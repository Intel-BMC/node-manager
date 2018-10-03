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
#include <fstream>
#include <regex>
#include "srvcfg_manager.hpp"

extern std::shared_ptr<boost::asio::deadline_timer> timer;
extern std::map<std::string, std::shared_ptr<phosphor::service::ServiceConfig>>
    srvMgrObjects;

namespace phosphor
{
namespace service
{

static constexpr const char *overrideConfFileName = "override.conf";
static constexpr const size_t restartTimeout = 15; // seconds

static constexpr const char *systemd1UnitBasePath =
    "/org/freedesktop/systemd1/unit/";
static constexpr const char *systemdOverrideUnitBasePath =
    "/etc/systemd/system/";

void ServiceConfig::syncWithSystemD1Properties()
{
    // Read systemd1 socket/service property and load.
    conn->async_method_call(
        [this](boost::system::error_code ec,
               const sdbusplus::message::variant<
                   std::vector<std::tuple<std::string, std::string>>> &value) {
            if (ec)
            {
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    "async_method_call error: Failed to get property");
                return;
            }

            try
            {
                auto listenVal = sdbusplus::message::variant_ns::get<
                    std::vector<std::tuple<std::string, std::string>>>(value);
                protocol = std::get<0>(listenVal[0]);
                std::string port = std::get<1>(listenVal[0]);
                auto tmp = std::stoul(port.substr(port.find_last_of(":") + 1),
                                      nullptr, 10);
                if (tmp > std::numeric_limits<uint16_t>::max())
                {
                    throw std::out_of_range("Out of range");
                }
                portNum = tmp;
            }
            catch (const std::exception &e)
            {
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    "Exception for port number",
                    phosphor::logging::entry("WHAT=%s", e.what()));
                return;
            }
            conn->async_method_call(
                [](boost::system::error_code ec) {
                    if (ec)
                    {
                        phosphor::logging::log<phosphor::logging::level::ERR>(
                            "async_method_call error: Failed to set property");
                        return;
                    }
                },
                serviceConfigSrvName, objPath.c_str(),
                "org.freedesktop.DBus.Properties", "Set", serviceConfigIntfName,
                "Port", sdbusplus::message::variant<uint16_t>(portNum));
        },
        "org.freedesktop.systemd1", sysDSockObjPath.c_str(),
        "org.freedesktop.DBus.Properties", "Get",
        "org.freedesktop.systemd1.Socket", "Listen");

    conn->async_method_call(
        [this](boost::system::error_code ec,
               const sdbusplus::message::variant<std::string> &pValue) {
            if (ec)
            {
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    "async_method_call error: Failed to get property");
                return;
            }

            channelList.clear();
            std::istringstream stm(
                sdbusplus::message::variant_ns::get<std::string>(pValue));
            std::string token;
            while (std::getline(stm, token, ','))
            {
                channelList.push_back(token);
            }
            conn->async_method_call(
                [](boost::system::error_code ec) {
                    if (ec)
                    {
                        phosphor::logging::log<phosphor::logging::level::ERR>(
                            "async_method_call error: Failed to set property");
                        return;
                    }
                },
                serviceConfigSrvName, objPath.c_str(),
                "org.freedesktop.DBus.Properties", "Set", serviceConfigIntfName,
                "Channel",
                sdbusplus::message::variant<std::vector<std::string>>(
                    channelList));
        },
        "org.freedesktop.systemd1", sysDSockObjPath.c_str(),
        "org.freedesktop.DBus.Properties", "Get",
        "org.freedesktop.systemd1.Socket", "BindToDevice");

    std::string srvUnitName(sysDUnitName);
    if (srvUnitName == "dropbear")
    {
        srvUnitName.append("@");
    }
    srvUnitName.append(".service");
    conn->async_method_call(
        [this](boost::system::error_code ec, const std::string &pValue) {
            if (ec)
            {
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    "async_method_call error: Failed to get property");
                return;
            }
            stateValue = pValue;
            conn->async_method_call(
                [](boost::system::error_code ec) {
                    if (ec)
                    {
                        phosphor::logging::log<phosphor::logging::level::ERR>(
                            "async_method_call error: Failed to set property");
                        return;
                    }
                },
                serviceConfigSrvName, objPath.c_str(),
                "org.freedesktop.DBus.Properties", "Set", serviceConfigIntfName,
                "State", sdbusplus::message::variant<std::string>(stateValue));
        },
        "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager", "GetUnitFileState", srvUnitName);

    return;
}

ServiceConfig::ServiceConfig(
    sdbusplus::asio::object_server &srv_,
    std::shared_ptr<sdbusplus::asio::connection> &conn_, std::string objPath_,
    std::string unitName) :
    server(srv_),
    conn(conn_), objPath(objPath_), sysDUnitName(unitName)
{
    std::string socketUnitName(sysDUnitName + ".socket");
    // .socket systemd service files are handled.
    // Regular .service only files are ignored.
    if (!checkSystemdUnitExist(socketUnitName))
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Unit doesn't exist.",
            phosphor::logging::entry("UNITNAME=%s", socketUnitName.c_str()));
        phosphor::logging::elog<
            sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure>();
    }

    /// Check override socket directory exist, if not create it.
    std::experimental::filesystem::path ovrUnitFileDir(
        systemdOverrideUnitBasePath);
    ovrUnitFileDir += socketUnitName;
    ovrUnitFileDir += ".d";
    if (!std::experimental::filesystem::exists(ovrUnitFileDir))
    {
        if (!std::experimental::filesystem::create_directories(ovrUnitFileDir))
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Unable to create the directory.",
                phosphor::logging::entry("DIR=%s", ovrUnitFileDir.c_str()));
            phosphor::logging::elog<sdbusplus::xyz::openbmc_project::Common::
                                        Error::InternalFailure>();
        }
    }

    /* Store require info locally */
    unitSocketFilePath = std::string(ovrUnitFileDir);

    sysDSockObjPath = systemd1UnitBasePath;
    sysDSockObjPath.append(
        std::regex_replace(sysDUnitName, std::regex("-"), "_2d"));
    sysDSockObjPath.append("_2esocket");

    // Adds interface, object and Properties....
    registerProperties();

    syncWithSystemD1Properties();

    updatedFlag = 0;
    return;
}

void ServiceConfig::applySystemDServiceConfig()
{
    phosphor::logging::log<phosphor::logging::level::INFO>(
        "Applying new settings.",
        phosphor::logging::entry("OBJPATH=%s", objPath.c_str()));
    if (updatedFlag & ((1 << static_cast<uint8_t>(UpdatedProp::channel)) |
                       (1 << static_cast<uint8_t>(UpdatedProp::port))))
    {
        // Create override config file and write data.
        std::string ovrCfgFile{unitSocketFilePath + "/" + overrideConfFileName};
        std::string tmpFile{ovrCfgFile + "_tmp"};
        std::ofstream cfgFile(tmpFile, std::ios::out);
        if (!cfgFile.good())
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Failed to open override.conf_tmp file");
            phosphor::logging::elog<sdbusplus::xyz::openbmc_project::Common::
                                        Error::InternalFailure>();
        }

        // Write the socket header
        cfgFile << "[Socket]\n";
        // Listen
        cfgFile << "Listen" << protocol << "="
                << "\n";
        cfgFile << "Listen" << protocol << "=" << portNum << "\n";
        // BindToDevice
        bool firstElement = true;
        cfgFile << "BindToDevice=";
        for (const auto &it : channelList)
        {
            if (firstElement)
            {
                cfgFile << it;
                firstElement = false;
            }
            else
            {
                cfgFile << "," << it;
            }
        }
        cfgFile.close();

        if (std::rename(tmpFile.c_str(), ovrCfgFile.c_str()) != 0)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Failed to rename tmp file as override.conf");
            std::remove(tmpFile.c_str());
            phosphor::logging::elog<sdbusplus::xyz::openbmc_project::Common::
                                        Error::InternalFailure>();
        }

        // Systemd forcing explicit socket stop before reload...!
        std::string socketUnitName(sysDUnitName + ".socket");
        systemdUnitAction(conn, socketUnitName, sysdActionStopUnit);

        std::string srvUnitName(sysDUnitName + ".service");
        systemdUnitAction(conn, srvUnitName, sysdActionStopUnit);

        // Perform daemon reload to read new settings
        systemdDaemonReload(conn);

        // Restart the unit
        systemdUnitAction(conn, socketUnitName, sysdActionStartUnit);
        systemdUnitAction(conn, srvUnitName, sysdActionStartUnit);
    }

    if (updatedFlag & (1 << static_cast<uint8_t>(UpdatedProp::state)))
    {
        if ((stateValue == "enabled") || (stateValue == "disabled"))
        {
            systemdUnitFileStateChange(conn, sysDUnitName, stateValue);
        }
    }

    // Reset the flag
    updatedFlag = 0;

    // All done. Lets reload the properties which are applied on systemd1.
    // TODO: We need to capture the service restart signal and reload data
    // inside the signal handler. So that we can update the service properties
    // modified, outside of this service as well.
    syncWithSystemD1Properties();

    return;
}

void ServiceConfig::startServiceRestartTimer()
{
    timer->expires_from_now(boost::posix_time::seconds(restartTimeout));
    timer->async_wait([this](const boost::system::error_code &ec) {
        if (ec == boost::asio::error::operation_aborted)
        {
            // Timer reset.
            return;
        }
        else if (ec)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "async wait error.");
            return;
        }
        for (auto &srvMgrObj : srvMgrObjects)
        {
            auto &srvObj = srvMgrObj.second;
            if (srvObj->updatedFlag)
            {
                srvObj->applySystemDServiceConfig();
            }
        }
    });
}

void ServiceConfig::registerProperties()
{
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        server.add_interface(objPath, serviceConfigIntfName);

    iface->register_property(
        "Port", portNum, [this](const uint16_t &req, uint16_t &res) {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                " Inside register_property");
            if (req == res)
            {
                return 1;
            }
            portNum = req;
            updatedFlag |= (1 << static_cast<uint8_t>(UpdatedProp::port));
            startServiceRestartTimer();
            res = req;
            return 1;
        });

    iface->register_property(
        "Channel", channelList,
        [this](const std::vector<std::string> &req,
               std::vector<std::string> &res) {
            if (req == res)
            {
                return 1;
            }
            channelList.clear();
            std::copy(req.begin(), req.end(), back_inserter(channelList));

            updatedFlag |= (1 << static_cast<uint8_t>(UpdatedProp::channel));
            startServiceRestartTimer();
            res = req;
            return 1;
        });

    iface->register_property(
        "State", stateValue, [this](const std::string &req, std::string &res) {
            if (req == res)
            {
                return 1;
            }
            if ((req != "enabled") && (req != "disabled") && (req != "static"))
            {
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    "Invalid value specified");
                return -EINVAL;
            }
            stateValue = req;
            updatedFlag |= (1 << static_cast<uint8_t>(UpdatedProp::state));
            startServiceRestartTimer();
            res = req;
            return 1;
        });

    iface->initialize();
    return;
}

} // namespace service
} // namespace phosphor
