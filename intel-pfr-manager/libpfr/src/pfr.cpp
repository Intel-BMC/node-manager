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

#include <unistd.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "pfr.hpp"
#include "file.hpp"

namespace intel
{
namespace pfr
{
// TODO: Dynamically pull these values from configuration
// entity-manager, when needed
static constexpr int i2cBusNumber = 4;
static constexpr int i2cSlaveAddress = 0x70;

// CPLD mailbox registers
static constexpr uint8_t cpldROTVersion = 0x01;
static constexpr uint8_t cpldROTSvn = 0x02;
static constexpr uint8_t platformState = 0x03;
static constexpr uint8_t recoveryCount = 0x04;
static constexpr uint8_t lastRecoveryReason = 0x05;
static constexpr uint8_t panicEventCount = 0x06;
static constexpr uint8_t panicEventReason = 0x07;
static constexpr uint8_t majorErrorCode = 0x08;
static constexpr uint8_t minorErrorCode = 0x09;
static constexpr uint8_t provisioningStatus = 0x0A;
static constexpr uint8_t pchActiveMajorVersion = 0x17;
static constexpr uint8_t pchActiveMinorVersion = 0x18;
static constexpr uint8_t bmcActiveMajorVersion = 0x19;
static constexpr uint8_t bmcActiveMinorVersion = 0x1A;
static constexpr uint8_t pchRecoveryMajorVersion = 0x1C;
static constexpr uint8_t pchRecoveryMinorVersion = 0x1D;
static constexpr uint8_t bmcRecoveryMajorVersion = 0x1E;
static constexpr uint8_t bmcRecoveryMinorVersion = 0x1F;

static constexpr uint8_t ufmLockedMask = (0x1 << 0x04);
static constexpr uint8_t ufmProvisionedMask = (0x1 << 0x05);

template <typename T> std::string int_to_hexstring(T i)
{
    std::stringstream stream;
    stream << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex
           << static_cast<int>(i);
    return stream.str();
}

std::string getVersionInfoCPLD(ImageType& imgType)
{
    try
    {
        uint8_t majorReg;
        uint8_t minorReg;

        switch (imgType)
        {
            case (ImageType::cpld):
            {
                majorReg = cpldROTVersion;
                minorReg = cpldROTSvn;
                break;
            }
            case (ImageType::biosActive):
            {
                majorReg = pchActiveMajorVersion;
                minorReg = pchActiveMinorVersion;
                break;
            }
            case (ImageType::biosRecovery):
            {
                majorReg = pchRecoveryMajorVersion;
                minorReg = pchRecoveryMinorVersion;
                break;
            }
            case (ImageType::bmcActive):
            {
                majorReg = bmcActiveMajorVersion;
                minorReg = bmcActiveMinorVersion;
                break;
            }
            case (ImageType::bmcRecovery):
            {
                majorReg = bmcRecoveryMajorVersion;
                minorReg = bmcRecoveryMinorVersion;
                break;
            }
            default:
                // Invalid image Type.
                return "";
        }

        I2CFile cpldDev(i2cBusNumber, i2cSlaveAddress, O_RDWR | O_CLOEXEC);
        uint8_t majorVer = cpldDev.i2cReadByteData(majorReg);
        uint8_t minorVer = cpldDev.i2cReadByteData(minorReg);
        std::string version =
            int_to_hexstring(majorVer) + "." + int_to_hexstring(minorVer);
        return version;
    }
    catch (const std::exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Exception caught in getVersionInfoCPLD.",
            phosphor::logging::entry("MSG=%s", e.what()));
        return "";
    }
}

int getProvisioningStatus(bool& ufmLocked, bool& ufmProvisioned)
{
    try
    {
        I2CFile cpldDev(i2cBusNumber, i2cSlaveAddress, O_RDWR | O_CLOEXEC);
        uint8_t provStatus = cpldDev.i2cReadByteData(provisioningStatus);
        ufmLocked = (provStatus & ufmLockedMask);
        ufmProvisioned = (provStatus & ufmProvisionedMask);
        return 0;
    }
    catch (const std::exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Exception caught in getProvisioningStatus.",
            phosphor::logging::entry("MSG=%s", e.what()));
        return -1;
    }
}

} // namespace pfr
} // namespace intel
