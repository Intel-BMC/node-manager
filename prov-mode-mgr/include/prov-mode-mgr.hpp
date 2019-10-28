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

#pragma once

#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Control/Security/RestrictionMode/server.hpp>

static constexpr const char* provModeMgrService =
    "xyz.openbmc_project.RestrictionMode.Manager";
static constexpr const char* provModeIntf =
    "xyz.openbmc_project.Control.Security.RestrictionMode";
static constexpr const char* provModePath =
    "/xyz/openbmc_project/control/security/restriction_mode";

static constexpr const char* uBootEnvMgrService =
    "xyz.openbmc_project.U_Boot.Environment.Manager";
static constexpr const char* uBootEnvMgrPath =
    "/xyz/openbmc_project/u_boot/environment/mgr";
static constexpr const char* uBootEnvMgrIntf =
    "xyz.openbmc_project.U_Boot.Environment.Manager";
static constexpr const char* uBootEnvMgrReadMethod = "Read";
static constexpr const char* uBootEnvMgrWriteMethod = "Write";
static constexpr const char* uBootEnvProvision = "provision";

class ProvModeMgr
{
    boost::asio::io_service& io;
    sdbusplus::asio::object_server& server;
    std::shared_ptr<sdbusplus::asio::connection> conn;
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;
    sdbusplus::xyz::openbmc_project::Control::Security::server::
        RestrictionMode::Modes provMode;

    sdbusplus::xyz::openbmc_project::Control::Security::server::
        RestrictionMode::Modes
        convertModesFromString(const std::string& s);
    std::string
        convertForMessage(sdbusplus::xyz::openbmc_project::Control::Security::
                              server::RestrictionMode::Modes v);
    void init();
    void updateProvModeProperty(
        sdbusplus::xyz::openbmc_project::Control::Security::server::
            RestrictionMode::Modes mode);
    void logEvent(sdbusplus::xyz::openbmc_project::Control::Security::server::
                      RestrictionMode::Modes mode);

  public:
    ProvModeMgr(boost::asio::io_service& io,
                sdbusplus::asio::object_server& srv,
                std::shared_ptr<sdbusplus::asio::connection>& conn);
};
