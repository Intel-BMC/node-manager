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

#include "system.hpp"

#include "mdrv2.hpp"

namespace phosphor
{
namespace smbios
{

static std::string getNode(uint8_t *node, size_t len)
{
    std::string result;
    for (int index = 0; index < len; index++)
    {
        result += std::to_string(node[index]);
    }

    return result;
}

std::string System::uUID(std::string value)
{
    std::string result = "No UUID";
    uint8_t *dataIn = storage;
    dataIn = getSMBIOSTypePtr(dataIn, systemType);
    if (dataIn != nullptr)
    {
        auto systemInfo = reinterpret_cast<struct SystemInfo *>(dataIn);
        struct UUID uUID = systemInfo->uUID;
        std::string tempS = std::to_string(uUID.timeLow) + "-" +
                            std::to_string(uUID.timeMid) + "-" +
                            std::to_string(uUID.timeHiAndVer) + "-";
        tempS += std::to_string(uUID.clockSeqHi) +
                 std::to_string(uUID.clockSeqLow) + "-";
        tempS += getNode(uUID.node, sizeof(uUID.node) / sizeof(uUID.node[0]));

        result = tempS;
    }

    return sdbusplus::xyz::openbmc_project::Common::server::UUID::uUID(result);
}

std::string System::version(std::string value)
{
    std::string result = "No BIOS Version";
    uint8_t *dataIn = storage;
    dataIn = getSMBIOSTypePtr(dataIn, biosType);
    if (dataIn != nullptr)
    {
        auto biosInfo = reinterpret_cast<struct BIOSInfo *>(dataIn);
        uint8_t biosVerByte = biosInfo->biosVersion;
        std::string tempS =
            positionToString(biosInfo->biosVersion, biosInfo->length, dataIn);
        result = tempS;
    }

    return sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::
        Revision::version(result);
}

} // namespace smbios
} // namespace phosphor
