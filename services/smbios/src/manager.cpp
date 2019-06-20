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
#include <algorithm>
#include <sdbusplus/exception.hpp>
#include "xyz/openbmc_project/Smbios/MDR_V1/error.hpp"
#include <smbios.hpp>
#include <string>
#include <phosphor-logging/elog-errors.hpp>
#include <fstream>

namespace phosphor
{
namespace smbios
{

void MDR_V1::regionUpdateTimeout(uint8_t u8Data)
{
    uint8_t regionId = u8Data;
    if ((regionId == 0) || (regionId >= maxMdrIndex))
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "timeout callback failure - region Id invalid");
        return;
    }
    regionId--;

    // TODO: Create a SEL Log

    // unlock the region
    if (regionS[regionId].state.lockPolicy != regionLockUnlocked)
    {
        regionS[regionId].state.valid = false;
        regionUpdateCounter(&(regionS[regionId].state.updateCount));
        regionS[regionId].sessionId = 0xFF;
        regionS[regionId].state.lockPolicy = regionLockUnlocked;
        regionS[regionId].state.regionUsed = 0;
    } // If locked
}

std::vector<uint8_t> MDR_V1::regionStatus(uint8_t regionId)
{
    uint8_t *state;
    std::vector<uint8_t> result;
    if (regionId >= maxRegion)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "region status error - invalid regionId");
        throw sdbusplus::xyz::openbmc_project::Smbios::MDR_V1::Error::
            InvalidParameter();
        return result;
    }
    state = &(regionS[regionId].state.mdrVersion);

    for (int index = 0; index < sizeof(struct MDRState); index++)
    {
        result.push_back(state[index]);
    }

    return result;
}

static constexpr int msb = 0x8000;
static constexpr uint32_t polynomial = 0x1070;
uint8_t MDR_V1::calcCRC8(const uint8_t regionId)
{
    uint8_t crc = 0;

    for (int count = 0; count < regionS[regionId].state.regionUsed; count++)
    {
        int data = ((crc ^ regionS[regionId].regionData[count]) << 8);
        for (int index = 0; index < 8; index++)
        {
            if (data & msb)
            {
                data = (data ^ (polynomial << 3));
            }
            data <<= 1;
        }
        crc = (data >> 8);
    }

    return crc;
}

uint8_t MDR_V1::getTotalDimmSlot()
{
    uint8_t *dataIn = regionS[0].regionData;
    uint8_t num = 0;

    if (dataIn == nullptr)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "get dimm total slot failed - no region data");
        return 0;
    }

    int limit = 0xff;
    while (limit > 0)
    {
        dataIn = smbiosTypePtr(dataIn, memoryDeviceType);
        if (dataIn == nullptr)
        {
            break;
        }
        num++;
        dataIn = smbiosNextPtr(dataIn);
        if (dataIn == nullptr)
        {
            break;
        }
        limit--;
    }

    return num;
}

constexpr int limitEntryLen = 0xff;
uint8_t MDR_V1::getTotalCpuSlot()
{
    uint8_t *dataIn = regionS[0].regionData;
    uint8_t num = 0;

    if (dataIn == nullptr)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "get cpu total slot failed - no region data");
        return 0;
    }

    int limit = limitEntryLen;
    while (limit > 0)
    {
        dataIn = smbiosTypePtr(dataIn, processorsType);
        if (dataIn == nullptr)
        {
            break;
        }
        num++;
        dataIn = smbiosNextPtr(dataIn);
        if (dataIn == nullptr)
        {
            break;
        }
        limit--;
    }

    return num;
}

void MDR_V1::systemInfoUpdate()
{
    uint8_t num = 0;
    std::string path;

    num = getTotalDimmSlot();

    // Clear all dimm cpu interface first
    std::vector<std::unique_ptr<Dimm>>().swap(dimms);
    std::vector<std::unique_ptr<Cpu>>().swap(cpus);

    for (int index = 0; index < num; index++)
    {
        path = dimmPath + std::to_string(index);
        dimms.emplace_back(std::make_unique<phosphor::smbios::Dimm>(
            bus, path, index, &regionS[0]));
    }

    num = 0;
    num = getTotalCpuSlot();

    for (int index = 0; index < num; index++)
    {
        path = cpuPath + std::to_string(index);
        cpus.emplace_back(std::make_unique<phosphor::smbios::Cpu>(
            bus, path, index, &regionS[0]));
    }
}

bool MDR_V1::readDataFromFlash(uint8_t *data, const char *file)
{
    std::ifstream filePtr(file, std::ios_base::binary);
    if (!filePtr.good())
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Read data from flash error - Open MDRV1 table file failure");
        return false;
    }
    filePtr.clear();
    filePtr.seekg(0, std::ios_base::beg);
    ManagedDataRegion *pRegionS = (ManagedDataRegion *)data;
    filePtr.read(reinterpret_cast<char *>(&(pRegionS->state)),
                 sizeof(MDRState));
    filePtr.read(reinterpret_cast<char *>(pRegionS->regionData),
                 (pRegionS->state.regionLength < smbiosTableStorageSize
                      ? pRegionS->state.regionLength
                      : smbiosTableStorageSize));
    return true;
}

bool MDR_V1::storeDataToFlash(uint8_t *data, const char *file)
{
    std::ofstream filePtr(file, std::ios_base::binary);
    if (!filePtr.good())
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Write data from flash error - Open MDRV1 table file failure");
        return false;
    }

    filePtr.clear();
    filePtr.seekp(0, std::ios_base::beg);
    ManagedDataRegion *pRegionS = (ManagedDataRegion *)data;

    filePtr.write(reinterpret_cast<char *>(&(pRegionS->state)),
                  sizeof(MDRState));
    filePtr.write(reinterpret_cast<char *>(pRegionS->regionData),
                  pRegionS->state.regionLength);

    return true;
}

void MDR_V1::regionComplete(uint8_t regionId)
{
    uint8_t tempRegionId = 0;
    uint8_t tempUpdateCount = 0;

    if (regionId >= maxRegion)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "region complete failed - invalid regionId");
        throw sdbusplus::xyz::openbmc_project::Smbios::MDR_V1::Error::
            InvalidParameter();
        return;
    }

    regionS[regionId].state.valid = true;
    regionS[regionId].state.lockPolicy = regionLockUnlocked;
    regionS[regionId].sessionId = 0;
    regionS[regionId].state.crc8 = calcCRC8(regionId);
    tempRegionId = regionS[regionId].state.regionId;
    tempUpdateCount = regionS[regionId].state.updateCount;

    lockPolicy(regionS[regionId].state.lockPolicy);
    sessionId(regionS[regionId].sessionId);

    timers[regionId]->setEnabled<std::false_type>();

    //  TODO: Create a SEL Log
    systemInfoUpdate(); // Update CPU and DIMM information

    // If BMC try to restore region data from BMC flash
    // no need to store the data to flash again.
    if (restoreRegion)
    {
        restoreRegion = false;
        return;
    }

    if (access(smbiosPath, F_OK) == -1)
    {
        if (0 != mkdir(smbiosPath, S_IRWXU))
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "create folder failed for writting smbios file");
            return;
        }
    }
    if (!storeDataToFlash(reinterpret_cast<uint8_t *>(&regionS[regionId]),
                          regionS[regionId].flashName))
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Store data to flash failed");
        throw sdbusplus::xyz::openbmc_project::Smbios::MDR_V1::Error::IOError();
    }
}

std::vector<uint8_t> MDR_V1::regionRead(uint8_t regionId, uint8_t length,
                                        uint16_t offset)
{
    std::vector<uint8_t> result;

    if (regionId >= maxRegion)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "region read failed - invalid regionId");
        throw sdbusplus::xyz::openbmc_project::Smbios::MDR_V1::Error::
            InvalidParameter();
        return result;
    }
    if (regionS[regionId].state.regionUsed < offset + length)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "region read failed - invalid offset/length");
        throw sdbusplus::xyz::openbmc_project::Smbios::MDR_V1::Error::
            InvalidParameter();
        return result;
    }

    result.push_back(length);
    result.push_back(regionS[regionId].state.updateCount);
    for (uint16_t index = 0; index < length; index++)
    {
        result.push_back(regionS[regionId].regionData[index + offset]);
    }
    return result;
}

std::string MDR_V1::regionWrite(std::vector<uint8_t> wData)
{
    uint8_t regionId = wData[0];
    if (regionId >= maxRegion)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "region write failed - invalid regionId");
        throw sdbusplus::xyz::openbmc_project::Smbios::MDR_V1::Error::
            InvalidParameter();
        return "failed";
    }
    uint8_t length = wData[1];
    uint16_t offset = wData[3] << 8 | (wData[2]);
    uint8_t *dest;
    std::vector<uint8_t>::iterator iter;
    std::chrono::microseconds usec =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::milliseconds(regionS[regionId].msTimeout));

    dest = &(regionS[regionId].regionData[offset]);

    iter = wData.begin();
    iter += 4;
    std::copy(iter, wData.end(), dest);
    regionS[regionId].state.regionUsed = std::max(
        regionS[regionId].state.regionUsed, (uint16_t)(offset + length));
    regionUsed(regionS[regionId].state.regionUsed);
    timers[regionId]->start(usec);
    timers[regionId]->setEnabled<std::true_type>();

    return "Success";
}

uint32_t MDR_V1::getOsRunningTime(void)
{
    int s32Result;
    struct timespec sTime;

    s32Result = clock_gettime(CLOCK_MONOTONIC, &sTime);
    if (s32Result != 0)
    {
        return 0;
    }

    return (sTime.tv_sec);
}

uint8_t MDR_V1::genMdrSessionId(void)
{
    uint32_t now = 0;
    uint8_t id = 0;

    do
    {
        now = getOsRunningTime();
        id = (uint8_t)(now ^ (now >> 8) ^ (now >> 16) ^ (now >> 24));
    } while ((id == 0x00) || (id == 0xFF));

    return id;
}

void MDR_V1::regionUpdateCounter(uint8_t *count)
{
    if (count != nullptr)
    {
        if (++(*count) == 0)
        {
            (*count)++;
        }
    }
}

uint8_t MDR_V1::regionLock(uint8_t u8SessionId, uint8_t regionId,
                           uint8_t u8LockPolicy, uint16_t msTimeout)
{
    uint8_t reqSession;

    if (regionId >= maxRegion)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "region lock failed - invalid regionId");
        throw sdbusplus::xyz::openbmc_project::Smbios::MDR_V1::Error::
            InvalidParameter();
        return 0;
    }

    if (u8LockPolicy != regionLockUnlocked)
    {
        reqSession = genMdrSessionId();
    }
    else
    {
        reqSession = 0;
    }

    regionS[regionId].state.valid = false;
    regionUpdateCounter(&(regionS[regionId].state.updateCount));
    regionS[regionId].sessionId = reqSession;
    regionS[regionId].state.lockPolicy = u8LockPolicy;
    regionS[regionId].state.regionUsed = 0;
    regionS[regionId].msTimeout = msTimeout;
    regionUsed(regionS[regionId].state.regionUsed);
    lockPolicy(regionS[regionId].state.lockPolicy);
    sessionId(regionS[regionId].sessionId);

    if (regionS[regionId].msTimeout != 0)
    {
        auto usec = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::milliseconds(1000));
        timers[regionId]->start(usec);
        timers[regionId]->setEnabled<std::true_type>();
    }

    return regionS[regionId].sessionId;
}

uint8_t MDR_V1::regionId(uint8_t value)
{
    globalRegionId = value;

    sdbusplus::xyz::openbmc_project::Smbios::server::MDR_V1::lockPolicy(
        regionS[value].state.lockPolicy);

    sdbusplus::xyz::openbmc_project::Smbios::server::MDR_V1::regionUsed(
        regionS[value].state.regionUsed);

    sdbusplus::xyz::openbmc_project::Smbios::server::MDR_V1::sessionId(
        regionS[value].sessionId);

    return sdbusplus::xyz::openbmc_project::Smbios::server::MDR_V1::regionId(
        value);
}

uint8_t MDR_V1::lockPolicy(uint8_t value)
{
    return sdbusplus::xyz::openbmc_project::Smbios::server::MDR_V1::lockPolicy(
        value);
}

uint16_t MDR_V1::regionUsed(uint16_t value)
{
    return sdbusplus::xyz::openbmc_project::Smbios::server::MDR_V1::regionUsed(
        value);
}

uint8_t MDR_V1::sessionId(uint8_t value)
{
    return sdbusplus::xyz::openbmc_project::Smbios::server::MDR_V1::sessionId(
        value);
}

} // namespace smbios
} // namespace phosphor
