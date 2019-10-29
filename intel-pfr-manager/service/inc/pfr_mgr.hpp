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

#include <string>
#include <sdbusplus/asio/object_server.hpp>
#include <phosphor-logging/log.hpp>
#include <boost/asio.hpp>

#include "pfr.hpp"

namespace intel
{
namespace pfr
{

static constexpr const char *versionPurposeBMC =
    "xyz.openbmc_project.Software.Version.VersionPurpose.BMC";
static constexpr const char *versionPurposeHost =
    "xyz.openbmc_project.Software.Version.VersionPurpose.Host";
static constexpr const char *versionPurposeOther =
    "xyz.openbmc_project.Software.Version.VersionPurpose.Other";

static constexpr const char *versionStr = "Version";
static constexpr const char *ufmProvisionedStr = "UfmProvisioned";
static constexpr const char *ufmLockedStr = "UfmLocked";

class PfrVersion
{
  public:
    PfrVersion(sdbusplus::asio::object_server &srv_,
               std::shared_ptr<sdbusplus::asio::connection> &conn_,
               const std::string &path_, const ImageType &imgType_,
               const std::string &purpose_);
    ~PfrVersion() = default;

    std::shared_ptr<sdbusplus::asio::connection> conn;

    void updateVersion();

  private:
    sdbusplus::asio::object_server &server;
    std::shared_ptr<sdbusplus::asio::dbus_interface> versionIface;
    bool internalSet = false;

    std::string path;
    std::string version;
    std::string purpose;
    ImageType imgType;
};

class PfrConfig
{
  public:
    PfrConfig(sdbusplus::asio::object_server &srv_,
              std::shared_ptr<sdbusplus::asio::connection> &conn_);
    ~PfrConfig() = default;

    std::shared_ptr<sdbusplus::asio::connection> conn;

    void updateProvisioningStatus();

  private:
    sdbusplus::asio::object_server &server;
    std::shared_ptr<sdbusplus::asio::dbus_interface> pfrCfgIface;
    bool internalSet = false;

    bool ufmProvisioned;
    bool ufmLocked;
};

} // namespace pfr
} // namespace intel
