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
#include "utils.hpp"

void systemdDaemonReload(
    const std::shared_ptr<sdbusplus::asio::connection> &conn)
{
    try
    {
        conn->async_method_call(
            [](boost::system::error_code ec) {
                if (ec)
                {
                    phosphor::logging::log<phosphor::logging::level::ERR>(
                        "async error: Failed to do systemd reload.");
                    return;
                }
            },
            "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager", "Reload");
    }
    catch (const sdbusplus::exception::SdBusError &e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "daemon-reload operation failed.");
        phosphor::logging::elog<
            sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure>();
    }

    return;
}

void systemdUnitAction(const std::shared_ptr<sdbusplus::asio::connection> &conn,
                       const std::string &unitName,
                       const std::string &actionMethod)
{
    try
    {
        conn->async_method_call(
            [](boost::system::error_code ec,
               const sdbusplus::message::object_path &res) {
                if (ec)
                {
                    phosphor::logging::log<phosphor::logging::level::ERR>(
                        "async error: Failed to do systemd action");
                    return;
                }
            },
            "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager", actionMethod, unitName,
            "replace");
    }
    catch (const sdbusplus::exception::SdBusError &e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Systemd operation failed.",
            phosphor::logging::entry("ACTION=%s", actionMethod.c_str()));
        phosphor::logging::elog<
            sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure>();
    }

    return;
}

void systemdUnitFileStateChange(
    const std::shared_ptr<sdbusplus::asio::connection> &conn,
    const std::string &unitName, const std::string &unitState)
{
    try
    {
        // For now, we support two states(enabled & disabled). we can extend
        // to other states (enable-runtime, mask, link etc..) if needed.
        std::vector<std::string> unitFiles = {unitName};
        if (unitState == "enabled")
        {
            conn->async_method_call(
                [](boost::system::error_code ec) {
                    if (ec)
                    {
                        phosphor::logging::log<phosphor::logging::level::ERR>(
                            "async error: Failed to do systemd reload.");
                        return;
                    }
                },
                "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
                "org.freedesktop.systemd1.Manager", "EnableUnitFiles",
                unitFiles, false, false);
        }
        else if (unitState == "disabled")
        {
            conn->async_method_call(
                [](boost::system::error_code ec) {
                    if (ec)
                    {
                        phosphor::logging::log<phosphor::logging::level::ERR>(
                            "async error: Failed to do systemd reload.");
                        return;
                    }
                },
                "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
                "org.freedesktop.systemd1.Manager", "DisableUnitFiles",
                unitFiles, false);
        }
        else
        {
            // Not supported unit State
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "invalid Unit state",
                phosphor::logging::entry("STATE=%s", unitState.c_str()));
            phosphor::logging::elog<sdbusplus::xyz::openbmc_project::Common::
                                        Error::InternalFailure>();
        }
    }
    catch (const sdbusplus::exception::SdBusError &e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Systemd state change operation failed.");
        phosphor::logging::elog<
            sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure>();
    }

    return;
}

bool checkSystemdUnitExist(const std::string &unitName)
{
    std::experimental::filesystem::path unitFilePath(
        std::string("/lib/systemd/system/") + unitName);
    return std::experimental::filesystem::exists(unitFilePath);
}
