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

#include "manager.hpp"
#include "smbios.hpp"
#include <systemd/sd-event.h>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>

uint8_t mdrSmBiosData[mdrSmbiosSize];
uint8_t mdrAcpiTable[mdrAcpiTableSize];
uint8_t mdrMemoryMapping[mdrMemMappingSize];
uint8_t mdrSCSIBoot[mdrScsiBootSize];
uint8_t mdrNvmeData[mdrNvmeSize];

struct ManagedDataRegion regionS[] = {
    // SMBIOS table - matching the regionID order
    {mdrType1File,
     mdrSmBiosData,
     0,
     {mdrVersion, mdrSmbios, false, 0, regionLockUnlocked, mdrSmbiosSize, 0, 0},
     0},

    // ACPI tables - matching the regionID order
    {mdrAcpiFile,
     mdrAcpiTable,
     0,
     {mdrVersion, mdrAcpi, false, 0, regionLockUnlocked, 0, mdrAcpiTableSize,
      0},
     0},

    // Memory Mapping table - matching the regionID order
    {mdrMemMapFile,
     mdrMemoryMapping,
     0,
     {mdrVersion, mdrMemMap, false, 0, regionLockUnlocked, mdrMemMappingSize, 0,
      0},
     0},

    {mdrScsiBootFile,
     mdrSCSIBoot,
     0,
     {mdrVersion, mdrScsiBoot, false, 0, regionLockUnlocked, mdrScsiBootSize, 0,
      0},
     0},

    // NVMe table - matching the regionID order
    {mdrNvmeFile,
     mdrNvmeData,
     0,
     {mdrVersion, mdrNvme, false, 0, regionLockUnlocked, mdrNvmeSize, 0, 0},
     0},
};

int main(void)
{
    sd_event *events = nullptr;
    sd_event_default(&events);

    sdbusplus::bus::bus bus = sdbusplus::bus::new_default();
    sdbusplus::server::manager::manager objManager(bus, "/xyz/openbmc_project");
    phosphor::watchdog::EventPtr eventP{events,
                                        phosphor::watchdog::EventDeleter()};
    bus.attach_event(events, SD_EVENT_PRIORITY_NORMAL);
    bus.request_name("xyz.openbmc_project.Smbios.MDR_V1");

    phosphor::smbios::MDR_V1 mdrV1(bus, phosphor::smbios::mdrV1Path, regionS,
                                   eventP);

    while (true)
    {
        int r = sd_event_run(events, (uint64_t)-1);
        if (r < 0)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Failure processing request",
                phosphor::logging::entry("errno=0x%X", -r));
            return -1;
        }
    }

    return 0;
}
