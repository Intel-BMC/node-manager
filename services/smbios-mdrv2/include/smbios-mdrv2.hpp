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

#include <array>

static constexpr const char *mdrType2File = "/etc/smbios/smbios2";
static constexpr const char *smbiosPath = "/etc/smbios";

static constexpr uint16_t mdrSmbiosSize = 32 * 1024;

constexpr uint16_t smbiosAgentId = 0x0101;
constexpr int firstAgentIndex = 1;

constexpr uint8_t maxDirEntries = 4;
constexpr uint32_t mdr2SMSize = 0x00100000;
constexpr uint32_t mdr2SMBaseAddress = 0x9FF00000;

constexpr uint8_t mdrTypeII = 2;

constexpr uint8_t mdr2Version = 2;
constexpr uint8_t smbiosAgentVersion = 1;

constexpr uint32_t pageMask = 0xf000;
constexpr int smbiosDirIndex = 0;

constexpr uint32_t smbiosTableVersion = 15;
constexpr uint32_t smbiosTableTimestamp = 0x45464748;
constexpr uint32_t smbiosSMMemoryOffset = 0;
constexpr uint32_t smbiosSMMemorySize = 1024 * 1024;
constexpr uint32_t smbiosTableStorageSize = 64 * 1024;
constexpr uint32_t defaultTimeout = 200;

enum class MDR2SMBIOSStatusEnum
{
    mdr2Init = 0,
    mdr2Loaded = 1,
    mdr2Updated = 2,
    mdr2Updating = 3
};

enum class MDR2DirLockEnum
{
    mdr2DirUnlock = 0,
    mdr2DirLock = 1
};

enum class DirDataRequestEnum
{
    dirDataNotRequested = 0x00,
    dirDataRequested = 0x01
};

enum class FlagStatus
{
    flagIsInvalid = 0,
    flagIsValid = 1,
    flagIsLocked = 2
};

typedef struct
{
    uint8_t dataInfo[16];
} DataIdStruct;

typedef struct
{
    DataIdStruct id;
    uint32_t size;
    uint32_t dataSetSize;
    uint32_t dataVersion;
    uint32_t timestamp;
} Mdr2DirEntry;

typedef struct
{
    Mdr2DirEntry common;
    MDR2SMBIOSStatusEnum stage;
    MDR2DirLockEnum lock;
    uint16_t lockHandle;
    uint32_t xferBuff;
    uint32_t xferSize;
    uint32_t maxDataSize;
    uint8_t *dataStorage;
} Mdr2DirLocalStruct;

typedef struct
{
    uint8_t agentVersion;
    uint8_t dirVersion;
    uint8_t dirEntries;
    uint8_t status; // valid / locked / etc
    uint8_t remoteDirVersion;
    uint16_t sessionHandle;
    Mdr2DirLocalStruct dir[maxDirEntries];
} Mdr2DirStruct;

struct MDRSmbiosHeader
{
    uint8_t dirVer;
    uint8_t mdrType;
    uint32_t timestamp;
    uint32_t dataSize;
} __attribute__((packed));
