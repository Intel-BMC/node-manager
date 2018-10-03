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
#include "srvcfg_manager.hpp"

std::shared_ptr<boost::asio::deadline_timer> timer = nullptr;
std::map<std::string, std::shared_ptr<phosphor::service::ServiceConfig>>
    srvMgrObjects;

static std::map<std::string, std::string> serviceList = {
    {"netipmid", "phosphor-ipmi-net"}, {"web", "bmcweb"}, {"ssh", "dropbear"}};

int main()
{
    // setup connection to dbus
    boost::asio::io_service io;
    auto conn = std::make_shared<sdbusplus::asio::connection>(io);
    timer = std::make_shared<boost::asio::deadline_timer>(io);
    conn->request_name(phosphor::service::serviceConfigSrvName);
    auto server = sdbusplus::asio::object_server(conn);
    for (const auto &service : serviceList)
    {
        std::string objPath("/xyz/openbmc_project/control/service/" +
                            service.first);
        auto srvCfgObj = std::make_unique<phosphor::service::ServiceConfig>(
            server, conn, objPath, service.second);
        srvMgrObjects.emplace(
            std::make_pair(std::move(service.first), std::move(srvCfgObj)));
    }
    io.run();

    return 0;
}
