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

#pragma once
#include "smbios-mdrv2.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Smbios/MDR_V2/server.hpp>

namespace phosphor
{
namespace smbios
{

static constexpr const char *mdrV2Path = "/xyz/openbmc_project/Smbios/MDR_V2";

class MDR_V2 : sdbusplus::xyz::openbmc_project::Smbios::server::MDR_V2
{
  public:
    MDR_V2() = delete;
    MDR_V2(const MDR_V2 &) = delete;
    MDR_V2 &operator=(const MDR_V2 &) = delete;
    MDR_V2(MDR_V2 &&) = delete;
    MDR_V2 &operator=(MDR_V2 &&) = delete;
    ~MDR_V2() = default;

    MDR_V2(sdbusplus::bus::bus &bus, const char *path) :
        sdbusplus::xyz::openbmc_project::Smbios::server::MDR_V2(bus, path),
        bus(bus)
    {

        smbiosDir.agentVersion = smbiosAgentVersion;
        smbiosDir.dirVersion = 1;
        smbiosDir.dirEntries = 1;
        directoryEntries(smbiosDir.dirEntries);
        smbiosDir.status = 1;
        smbiosDir.remoteDirVersion = 0;

        std::copy(smbiosTableId.begin(), smbiosTableId.end(),
                  smbiosDir.dir[smbiosDirIndex].common.id.dataInfo);
    }

    std::vector<uint8_t> getDirectoryInformation(uint8_t dirIndex) override;

    std::vector<uint8_t> getDataInformation(uint8_t idIndex) override;

    bool sendDirectoryInformation(uint8_t dirVersion, uint8_t dirIndex,
                                  uint8_t returnedEntries,
                                  uint8_t remainingEntries,
                                  std::vector<uint8_t> dirEntry) override;

    std::vector<uint8_t> getDataOffer() override;

    bool sendDataInformation(uint8_t idIndex, uint8_t flag, uint32_t dataLen,
                             uint32_t dataVer, uint32_t timeStamp) override;

    int findIdIndex(std::vector<uint8_t> dataInfo) override;

    bool agentSynchronizeData() override;

    std::vector<uint32_t>
        synchronizeDirectoryCommonData(uint8_t idIndex, uint32_t size) override;

    uint8_t directoryEntries(uint8_t value) override;

  private:
    sdbusplus::bus::bus &bus;

    Mdr2DirStruct smbiosDir;

    const std::array<uint8_t, 16> smbiosTableId{
        40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 0x42};
    uint8_t smbiosTableStorage[smbiosTableStorageSize];

    bool smbiosIsAvailForUpdate(uint8_t index);
    inline uint8_t smbiosValidFlag(uint8_t index);
};

} // namespace smbios
} // namespace phosphor
