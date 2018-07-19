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

#include "smbios.hpp"
#include "timer.hpp"
#include "xyz/openbmc_project/Smbios/MDR_V1/server.hpp"
#include "cpu.hpp"
#include "dimm.hpp"
#include "system.hpp"

namespace phosphor
{
namespace smbios
{

static constexpr const char *mdrV1Path = "/xyz/openbmc_project/Smbios/MDR_V1";

class MDR_V1 : sdbusplus::xyz::openbmc_project::Smbios::server::MDR_V1
{
  public:
    MDR_V1() = delete;
    MDR_V1(const MDR_V1 &) = delete;
    MDR_V1 &operator=(const MDR_V1 &) = delete;
    MDR_V1(MDR_V1 &&) = delete;
    MDR_V1 &operator=(MDR_V1 &&) = delete;
    ~MDR_V1() = default;

    MDR_V1(sdbusplus::bus::bus &bus, const char *path,
           struct ManagedDataRegion *region,
           phosphor::watchdog::EventPtr event) :
        sdbusplus::xyz::openbmc_project::Smbios::server::MDR_V1(bus, path),
        bus(bus)
    {
        for (uint8_t index = 0; index < 4; index++)
        {
            timers[index] = std::make_unique<phosphor::watchdog::Timer>(
                event, index, [&](uint8_t index) {
                    return MDR_V1::regionUpdateTimeout(index);
                });
        }

        std::copy(&region[0], &region[3], regionS);
        for (int index = 0; index < maxMdrIndex - 1; index++)
        {
            readDataFromFlash(reinterpret_cast<uint8_t *>(&regionS[index]));
        }

        system = std::make_unique<System>(bus, systemPath, &regionS[0]);
    }

    std::vector<uint8_t> regionStatus(uint8_t regionId) override;

    void regionComplete(uint8_t regionId) override;

    std::vector<uint8_t> regionRead(uint8_t regionId, uint8_t length,
                                    uint16_t offset) override;

    std::string regionWrite(std::vector<uint8_t> regionData) override;

    uint8_t regionLock(uint8_t sessionId, uint8_t regionId, uint8_t lockPolicy,
                       uint16_t timeout) override;

    void regionUpdateTimeout(uint8_t regionId);
    uint8_t calcCRC8(const uint8_t regionId);

    std::map<uint8_t, std::unique_ptr<phosphor::watchdog::Timer>> timers;

    uint8_t regionId(uint8_t value) override;

    uint8_t lockPolicy(uint8_t value) override;

    uint8_t sessionId(uint8_t value) override;

    uint16_t regionUsed(uint16_t value) override;

    static constexpr uint8_t maxRegion = 5;

    struct ManagedDataRegion regionS[maxRegion];

  private:
    sdbusplus::bus::bus &bus;

    bool storeDataToFlash(uint8_t *data);
    bool readDataFromFlash(uint8_t *data);

    uint8_t globalRegionId;

    void regionUpdateCounter(uint8_t *count);

    std::vector<std::unique_ptr<Dimm>> dimms;
    std::vector<std::unique_ptr<Cpu>> cpus;
    std::unique_ptr<System> system;

    void systemInfoUpdate(void);

    uint8_t getTotalDimmSlot(void);
    uint8_t getTotalCpuSlot(void);

    uint32_t getOsRunningTime(void);
    uint8_t genMdrSessionId(void);
};

} // namespace smbios
} // namespace phosphor
