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
#include <map>

static constexpr uint16_t mdrSmbiosSize = 32 * 1024;    // 32K
static constexpr uint16_t mdrAcpiTableSize = 32 * 1024; // 32K
static constexpr uint16_t mdrMemMappingSize = 8 * 1024; // 8K
static constexpr uint16_t mdrScsiBootSize = 8 * 1024;   // 8K
static constexpr uint16_t mdrNvmeSize = 1 * 1024;       // 1K

static constexpr uint16_t smbiosTableStorageSize = 0xffff;

static constexpr uint8_t mdrVersion = 0x11; // MDR version 1.1

static constexpr const char *smbiosPath = "/etc/smbios";
static constexpr const char *mdrType1File = "/etc/smbios/smbios1";
static constexpr const char *mdrType2File = "/etc/smbios/smbios2";
static constexpr const char *mdrAcpiFile = "/etc/smbios/acpi";
static constexpr const char *mdrMemMapFile = "/etc/smbios/memmapping";
static constexpr const char *mdrScsiBootFile = "/etc/smbios/scsiboot";
static constexpr const char *mdrNvmeFile = "/etc/smbios/nvme";

typedef enum
{
    mdrNone = 0,
    mdrSmbios = 1,
    mdrAcpi = 2,
    mdrMemMap = 3,
    mdrScsiBoot = 4,
    mdrNvme = 5,
    maxMdrIndex = 6
} MDRRegionIndex;

typedef enum
{
    regionLockUnlocked = 0,
    regionLockStrict,
    regionLockPreemptable
} MDRLockType;

struct MDRState
{
    uint8_t mdrVersion;
    uint8_t regionId;
    uint8_t valid;
    uint8_t updateCount;
    uint8_t lockPolicy;
    uint16_t regionLength;
    uint16_t regionUsed;
    uint8_t crc8;
} __attribute__((packed));

struct ManagedDataRegion
{
    const char *flashName;
    uint8_t *regionData;
    uint16_t msTimeout;
    struct MDRState state;
    uint8_t sessionId;
} __attribute__((packed));

struct BIOSInfo
{
    uint8_t biosVersion;
};

static constexpr const char *dimmPath =
    "/xyz/openbmc_project/inventory/system/chassis/motherboard/dimm";

static constexpr const char *cpuPath =
    "/xyz/openbmc_project/inventory/system/chassis/motherboard/cpu";

static constexpr const char *systemPath =
    "/xyz/openbmc_project/inventory/system/chassis/motherboard/bios";

typedef enum
{
    biosType = 0,
    systemType = 1,
    baseboardType = 2,
    chassisType = 3,
    processorsType = 4,
    memoryControllerType = 5,
    memoryModuleInformationType = 6,
    cacheType = 7,
    portConnectorType = 8,
    systemSlots = 9,
    onBoardDevicesType = 10,
    oemStringsType = 11,
    systemCconfigurationOptionsType = 12,
    biosLanguageType = 13,
    groupAssociatonsType = 14,
    systemEventLogType = 15,
    physicalMemoryArrayType = 16,
    memoryDeviceType = 17,
} SmbiosType;

static constexpr uint8_t separateLen = 2;
// To get the point of next smbios item
static inline uint8_t *smbiosNextPtr(uint8_t *smbiosDataIn)
{
    if (smbiosDataIn == nullptr)
    {
        return nullptr;
    }
    uint8_t *smbiosData = smbiosDataIn + *(smbiosDataIn + 1);
    int len = 0;
    while ((*smbiosData | *(smbiosData + 1)) != 0)
    {
        smbiosData++;
        len++;
        if (len >= mdrSmbiosSize) // To avoid endless loop
        {
            return nullptr;
        }
    }
    return smbiosData + separateLen;
}

// When first time run smbiosTypePtr, need to send the RegionS[].regionData
// to smbiosDataIn
static inline uint8_t *smbiosTypePtr(uint8_t *smbiosDataIn, uint8_t typeId)
{
    if (smbiosDataIn == nullptr)
    {
        return nullptr;
    }
    char *smbiosData = reinterpret_cast<char *>(smbiosDataIn);
    while ((*smbiosData != '\0') || (*(smbiosData + 1) != '\0'))
    {
        if (*smbiosData != typeId)
        {
            uint32_t len = *(smbiosData + 1);
            smbiosData += len;
            while ((*smbiosData != '\0') || (*(smbiosData + 1) != '\0'))
            {
                smbiosData++;
                len++;
                if (len >= mdrSmbiosSize) // To avoid endless loop
                {
                    return nullptr;
                }
            }
            smbiosData += separateLen;
            continue;
        }
        return reinterpret_cast<uint8_t *>(smbiosData);
    }
    return nullptr;
}

static inline std::string positionToString(uint8_t positionNum,
                                           uint8_t structLen, uint8_t *dataIn)
{
    if (dataIn == nullptr)
    {
        return "";
    }
    char *target;
    uint8_t stringLen = 0;
    uint16_t limit = mdrSmbiosSize; // set a limit to avoid endless loop

    target = reinterpret_cast<char *>(dataIn + structLen);
    for (uint8_t index = 1; index < positionNum; index++)
    {
        for (; *target != '\0'; target++)
        {
            limit--;
            if (limit < 1)
            {
                return "";
            }
        }
        target++;
        if (*target == '\0')
        {
            return ""; // 0x00 0x00 means end of the entry.
        }
    }

    std::string result = target;
    return result;
}

// Find the specific string in smbios item
static inline std::string seekString(uint8_t *smbiosDataIn, uint8_t stringOrder)
{
    if (smbiosDataIn == nullptr)
        return "";
    uint8_t len = *(smbiosDataIn + 1);

    return positionToString(stringOrder, len, smbiosDataIn);
}
