/*
// Copyright (c) 2019 Intel Corporation
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

#include "pfr_mgr.hpp"

static std::array<std::string, 5> listVersionPaths = {
    "bmc_active", "bmc_recovery", "bios_active", "bios_recovery", "cpld"};

int main()
{
    // setup connection to dbus
    boost::asio::io_service io;
    auto conn = std::make_shared<sdbusplus::asio::connection>(io);
    conn->request_name("xyz.openbmc_project.Intel.PFR.Manager");
    auto server = sdbusplus::asio::object_server(conn, true);

    // Create Intel PFR attributes object and interface
    intel::pfr::PfrConfig obj(server, conn);

    // Create Software objects using Versions interface
    for (const auto& path : listVersionPaths)
    {
        intel::pfr::PfrVersion obj(server, conn, path);
    }

    phosphor::logging::log<phosphor::logging::level::INFO>(
        "Intel PFR service started successfully");

    io.run();

    return 0;
}
