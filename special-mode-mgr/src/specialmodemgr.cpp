/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "specialmodemgr.hpp"

#include <sys/sysinfo.h>

#include <fstream>
#include <phosphor-logging/log.hpp>
#include <string>

static constexpr const char* specialModeMgrService =
    "xyz.openbmc_project.SpecialMode";
static constexpr const char* specialModeIntf =
    "xyz.openbmc_project.Security.SpecialMode";
static constexpr const char* specialModePath =
    "/xyz/openbmc_project/security/specialMode";
static constexpr const char* provisioningMode =
    "xyz.openbmc_project.Control.Security.RestrictionMode.Modes.Provisioning";

static constexpr const char* restrictionModeProperty = "RestrictionMode";

using VariantValue =
    std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t,
                 uint64_t, double, std::string>;

SpecialModeMgr::SpecialModeMgr(
    boost::asio::io_service& io_, sdbusplus::asio::object_server& srv_,
    std::shared_ptr<sdbusplus::asio::connection>& conn_) :
    io(io_),
    server(srv_), conn(conn_),
    timer(std::make_unique<boost::asio::deadline_timer>(io))
{

    // Following condition must match to indicate specialMode.
    // Mark the mode as None for any failure.
    // 1. U-Boot detected power button press & indicated "special=mfg"
    // in command line parameter.
    // 2. BMC in Provisioning mode.
    // 3. BMC boot is due to AC cycle.
    // 4. Not crossed 12 hours in this special case.
    std::string cmdLineStr;
    std::ifstream cmdLineIfs("/proc/cmdline");
    getline(cmdLineIfs, cmdLineStr);
    static constexpr const char* specialModeStr = "special=mfg";
    static constexpr const char* acBootStr = "resetreason=0x11";
    if ((cmdLineStr.find(specialModeStr) != std::string::npos) &&
        (cmdLineStr.find(acBootStr) != std::string::npos))
    {
        conn->async_method_call(
            [this](boost::system::error_code ec, const VariantValue& mode) {
                if (ec)
                {
                    phosphor::logging::log<phosphor::logging::level::INFO>(
                        "ERROR with async_method_call");
                    AddSpecialModeProperty();
                    return;
                }
                if (std::get<std::string>(mode) != provisioningMode)
                {
                    AddSpecialModeProperty();
                    return;
                }
                struct sysinfo sysInfo = {};
                int ret = sysinfo(&sysInfo);
                if (ret != 0)
                {
                    phosphor::logging::log<phosphor::logging::level::INFO>(
                        "ERROR in getting sysinfo",
                        phosphor::logging::entry("RET = %d", ret));
                    AddSpecialModeProperty();
                    return;
                }
                constexpr int mtmAllowedTime = 12 * 60 * 60; // 12 hours
                int specialModeLockoutSeconds = 0;
                if (mtmAllowedTime > sysInfo.uptime)
                {
                    specialMode = ManufacturingMode;
                    specialModeLockoutSeconds = mtmAllowedTime - sysInfo.uptime;
                }
                AddSpecialModeProperty();
                if (!specialModeLockoutSeconds)
                {
                    return;
                }
                timer->expires_from_now(
                    boost::posix_time::seconds(specialModeLockoutSeconds));
                timer->async_wait([this](const boost::system::error_code& ec) {
                    if (ec == boost::asio::error::operation_aborted)
                    {
                        // timer aborted
                        return;
                    }
                    else if (ec)
                    {
                        phosphor::logging::log<phosphor::logging::level::ERR>(
                            "Error in special mode "
                            "timer");
                        return;
                    }
                    iface->set_property(
                        strSpecialMode,
                        static_cast<uint8_t>(ManufacturingExpired));
                });
            },
            "xyz.openbmc_project.RestrictionMode.Manager",
            "/xyz/openbmc_project/control/security/restriction_mode",
            "org.freedesktop.DBus.Properties", "Get",
            "xyz.openbmc_project.Control.Security.RestrictionMode",
            restrictionModeProperty);
    }
    else
    {
        AddSpecialModeProperty();
    }
}

void SpecialModeMgr::AddSpecialModeProperty()
{
    // Add path to server object
    iface = server.add_interface(specialModePath, specialModeIntf);
    iface->register_property(
        strSpecialMode, specialMode,
        // Ignore set
        [this](const uint8_t& req, uint8_t& propertyValue) {
            if (req == ManufacturingExpired && specialMode != req)
            {
                specialMode = req;
                propertyValue = req;
                return 1;
            }
            return 0;
        },
        // Override get
        [this](const uint8_t& mode) { return specialMode; });
    iface->initialize(true);
}

int main()
{
    boost::asio::io_service io;
    auto conn = std::make_shared<sdbusplus::asio::connection>(io);
    conn->request_name(specialModeMgrService);
    sdbusplus::asio::object_server server(conn);

    SpecialModeMgr specilModeMgr(io, server, conn);

    static auto match = sdbusplus::bus::match::match(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged', "
        "interface='org.freedesktop.DBus.Properties', "
        "arg0namespace='xyz.openbmc_project.Control.Security.RestrictionMode'",
        [&specilModeMgr](sdbusplus::message::message& message) {
            std::string intfName;
            std::map<std::string, std::variant<std::string>> properties;

            message.read(intfName,
                         properties); // skipping reading of 3rd argument

            std::variant<std::string> mode;

            try
            {
                mode = properties.at(restrictionModeProperty);
            }
            catch (const std::out_of_range& e)
            {
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    "Error in finding RestrictionMode property");

                throw std::out_of_range("Out of range");
            }
            if (std::get<std::string>(mode) != provisioningMode)
            {
                phosphor::logging::log<phosphor::logging::level::INFO>(
                    "Mode is not provisioning ");

                specilModeMgr.SetSpecialModeValue(ManufacturingExpired);
            }
        });

    io.run();
}
