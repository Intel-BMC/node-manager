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

namespace intel
{
namespace pfr
{

static constexpr uint8_t activeImage = 0;
static constexpr uint8_t recoveryImage = 1;

PfrVersion::PfrVersion(sdbusplus::asio::object_server &srv_,
                       std::shared_ptr<sdbusplus::asio::connection> &conn_,
                       const std::string &path_, const ImageType &imgType_,
                       const std::string &purpose_) :
    server(srv_),
    conn(conn_), path(path_), imgType(imgType_), purpose(purpose_)
{
    version = getFirmwareVersion(imgType);

    std::string objPath = "/xyz/openbmc_project/software/" + path;
    versionIface =
        server.add_interface(objPath, "xyz.openbmc_project.Software.Version");
    versionIface->register_property("Purpose", purpose);
    versionIface->register_property(
        versionStr, version,
        // Override set
        [this](const std::string &req, std::string &propertyValue) {
            if (internalSet)
            {
                if (req != propertyValue)
                {
                    version = req;
                    propertyValue = req;
                    return 1;
                }
            }
            return 0;
        });

    versionIface->initialize();

    /* Activation interface represents activation state for an associated
     * xyz.openbmc_project.Software.Version. since these versions are already
     * active, so we should set "activation" to Active and
     * "RequestedActivation" to None. */
    std::string activation;
    if (imgType == ImageType::bmcRecovery ||
        imgType == ImageType::biosRecovery ||
        imgType == ImageType::cpldRecovery)
    {
        activation =
            "xyz.openbmc_project.Software.Activation.Activations.StandbySpare";
    }
    else
    {
        activation =
            "xyz.openbmc_project.Software.Activation.Activations.Active";
    }

    std::string reqActNone =
        "xyz.openbmc_project.Software.Activation.RequestedActivations.None";
    auto activationIface = server.add_interface(
        objPath, "xyz.openbmc_project.Software.Activation");
    activationIface->register_property("Activation", activation);
    activationIface->register_property("RequestedActivation", reqActNone);

    activationIface->initialize();
}

void PfrVersion::updateVersion()
{
    if (versionIface && versionIface->is_initialized())
    {
        std::string ver = getFirmwareVersion(imgType);
        internalSet = true;
        versionIface->set_property(versionStr, ver);
        internalSet = false;
    }
    return;
}

PfrConfig::PfrConfig(sdbusplus::asio::object_server &srv_,
                     std::shared_ptr<sdbusplus::asio::connection> &conn_) :
    server(srv_),
    conn(conn_)
{
    pfrCfgIface = server.add_interface("/xyz/openbmc_project/pfr",
                                       "xyz.openbmc_project.PFR.Attributes");

    getProvisioningStatus(ufmLocked, ufmProvisioned);

    pfrCfgIface->register_property(ufmProvisionedStr, ufmProvisioned,
                                   // Override set
                                   [this](const bool req, bool propertyValue) {
                                       if (internalSet)
                                       {
                                           if (req != propertyValue)
                                           {
                                               ufmProvisioned = req;
                                               propertyValue = req;
                                               return 1;
                                           }
                                       }
                                       return 0;
                                   });

    pfrCfgIface->register_property(ufmLockedStr, ufmLocked,
                                   // Override set
                                   [this](const bool req, bool propertyValue) {
                                       if (internalSet)
                                       {
                                           if (req != propertyValue)
                                           {
                                               ufmLocked = req;
                                               propertyValue = req;
                                               return 1;
                                           }
                                       }
                                       return 0;
                                   });

    pfrCfgIface->initialize();
}

void PfrConfig::updateProvisioningStatus()
{
    if (pfrCfgIface && pfrCfgIface->is_initialized())
    {
        bool lockVal = false;
        bool provVal = false;
        getProvisioningStatus(lockVal, provVal);
        internalSet = true;
        pfrCfgIface->set_property(ufmProvisionedStr, provVal);
        pfrCfgIface->set_property(ufmLockedStr, lockVal);
        internalSet = false;
    }
    return;
}

} // namespace pfr
} // namespace intel
