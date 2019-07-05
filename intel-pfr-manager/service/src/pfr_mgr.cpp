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
#include "pfr.hpp"

namespace intel
{
namespace pfr
{

static constexpr uint8_t activeImage = 0;
static constexpr uint8_t recoveryImage = 1;

PfrVersion::PfrVersion(sdbusplus::asio::object_server &srv_,
                       std::shared_ptr<sdbusplus::asio::connection> &conn_,
                       const std::string &path_) :
    server(srv_),
    conn(conn_), path(path_)
{
    if (path == "bmc_active")
    {
        purpose = "xyz.openbmc_project.Software.Version.VersionPurpose.BMC";
        ImageType imgType = ImageType::bmcActive;
        version = getVersionInfoCPLD(imgType);
    }
    else if (path == "bmc_recovery")
    {
        purpose = "xyz.openbmc_project.Software.Version.VersionPurpose.BMC";
        ImageType imgType = ImageType::bmcRecovery;
        version = getVersionInfoCPLD(imgType);
    }
    else if (path == "bios_active")
    {
        purpose = "xyz.openbmc_project.Software.Version.VersionPurpose.Host";
        ImageType imgType = ImageType::biosActive;
        version = getVersionInfoCPLD(imgType);
    }
    else if (path == "bios_recovery")
    {
        purpose = "xyz.openbmc_project.Software.Version.VersionPurpose.Host";
        ImageType imgType = ImageType::biosRecovery;
        version = getVersionInfoCPLD(imgType);
    }
    else if (path == "cpld")
    {
        purpose = "xyz.openbmc_project.Software.Version.VersionPurpose.Other";
        ImageType imgType = ImageType::cpld;
        version = getVersionInfoCPLD(imgType);
    }
    else
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Invalid path specified for PfrVersion");
        return;
    }

    std::string objPath = "/xyz/openbmc_project/software/" + path;
    auto iface =
        server.add_interface(objPath, "xyz.openbmc_project.Software.Version");
    iface->register_property("Purpose", purpose);
    iface->register_property("Version", version);

    iface->initialize();
}

bool PfrConfig::getPFRProvisionedState()
{
    bool ufmProvisioned = false;
    bool ufmLocked = false;
    getProvisioningStatus(ufmLocked, ufmProvisioned);

    return ufmProvisioned;
}

PfrConfig::PfrConfig(sdbusplus::asio::object_server &srv_,
                     std::shared_ptr<sdbusplus::asio::connection> &conn_) :
    server(srv_),
    conn(conn_)
{
    auto pfrIntf =
        server.add_interface("/xyz/openbmc_project/intel_pfr",
                             "xyz.openbmc_project.Intel_PFR.Attributes");

    pfrIntf->register_property("provisioned_state", getPFRProvisionedState());

    pfrIntf->initialize();
}

} // namespace pfr
} // namespace intel
