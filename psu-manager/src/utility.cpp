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

#include "utility.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <phosphor-logging/elog-errors.hpp>

extern "C" {
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
}

int i2cSet(uint8_t bus, uint8_t slaveAddr, uint8_t regAddr, uint8_t value)
{
    unsigned long funcs = 0;
    std::string devPath = "/dev/i2c-" + std::to_string(bus);

    int fd = ::open(devPath.c_str(), O_RDWR);
    if (fd < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error in open!",
            phosphor::logging::entry("PATH=%s", devPath.c_str()),
            phosphor::logging::entry("SLAVEADDR=0x%x", slaveAddr));
        return -1;
    }

    if (::ioctl(fd, I2C_FUNCS, &funcs) < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error in I2C_FUNCS!",
            phosphor::logging::entry("PATH=%s", devPath.c_str()),
            phosphor::logging::entry("SLAVEADDR=0x%x", slaveAddr));
        ::close(fd);
        return -1;
    }

    if (!(funcs & I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "i2c bus does not support write!",
            phosphor::logging::entry("PATH=%s", devPath.c_str()),
            phosphor::logging::entry("SLAVEADDR=0x%x", slaveAddr));
        ::close(fd);
        return -1;
    }

    if (::ioctl(fd, I2C_SLAVE_FORCE, slaveAddr) < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error in I2C_SLAVE_FORCE!",
            phosphor::logging::entry("PATH=%s", devPath.c_str()),
            phosphor::logging::entry("SLAVEADDR=0x%x", slaveAddr));
        ::close(fd);
        return -1;
    }

    if (::i2c_smbus_write_byte_data(fd, regAddr, value) < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error in i2c write!",
            phosphor::logging::entry("PATH=%s", devPath.c_str()),
            phosphor::logging::entry("SLAVEADDR=0x%x", slaveAddr));
        ::close(fd);
        return -1;
    }

    ::close(fd);
    return 0;
}

int i2cGet(uint8_t bus, uint8_t slaveAddr, uint8_t regAddr, uint8_t& value)
{
    unsigned long funcs = 0;
    std::string devPath = "/dev/i2c-" + std::to_string(bus);

    int fd = ::open(devPath.c_str(), O_RDWR);
    if (fd < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error in open!",
            phosphor::logging::entry("PATH=%s", devPath.c_str()),
            phosphor::logging::entry("SLAVEADDR=0x%x", slaveAddr));
        return -1;
    }

    if (::ioctl(fd, I2C_FUNCS, &funcs) < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error in I2C_FUNCS!",
            phosphor::logging::entry("PATH=%s", devPath.c_str()),
            phosphor::logging::entry("SLAVEADDR=0x%x", slaveAddr));
        ::close(fd);
        return -1;
    }

    if (!(funcs & I2C_FUNC_SMBUS_READ_BYTE_DATA))
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "i2c bus does not support read!",
            phosphor::logging::entry("PATH=%s", devPath.c_str()),
            phosphor::logging::entry("SLAVEADDR=0x%x", slaveAddr));
        ::close(fd);
        return -1;
    }

    if (::ioctl(fd, I2C_SLAVE_FORCE, slaveAddr) < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error in I2C_SLAVE_FORCE!",
            phosphor::logging::entry("PATH=%s", devPath.c_str()),
            phosphor::logging::entry("SLAVEADDR=0x%x", slaveAddr));
        ::close(fd);
        return -1;
    }

    value = ::i2c_smbus_read_byte_data(fd, regAddr);
    if (value < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error in i2c read!",
            phosphor::logging::entry("PATH=%s", devPath.c_str()),
            phosphor::logging::entry("SLAVEADDR=0x%x", slaveAddr));
        ::close(fd);
        return -1;
    }

    ::close(fd);
    return 0;
}

void getPSUEvent(const std::array<const char*, 1>& configTypes,
                 const std::shared_ptr<sdbusplus::asio::connection>& conn,
                 const std::string& psuName, PSUState& state)
{

    conn->async_method_call(
        [&conn, &state, &psuName, &configTypes](
            const boost::system::error_code ec, GetSubTreeType subtree) {
            if (ec)
            {
                std::cerr << "Exception happened when communicating to "
                             "ObjectMapper\n";
                return;
            }
            state = PSUState::normal;

            for (const auto& object : subtree)
            {
                std::string pathStr = object.first;

                // /State/Decorator/PSUx_OperationalStatus
                auto slantingPos = pathStr.find_last_of("/\\") + 1;
                std::string statePSUName = pathStr.substr(slantingPos);
                auto hypenPos = statePSUName.find("_");

                std::string name = statePSUName.substr(0, hypenPos);
                if (name != psuName)
                {
                    continue;
                }
                for (const auto& serviceIface : object.second)
                {
                    std::string serviceName = serviceIface.first;
                    for (const auto& interface : serviceIface.second)
                    {
                        // only get property of matched interface
                        bool isIfaceMatched = false;
                        for (const auto& type : configTypes)
                        {
                            if (type == interface)
                            {
                                isIfaceMatched = true;
                                break;
                            }
                        }
                        if (!isIfaceMatched)
                            continue;

                        conn->async_method_call(
                            [&conn, &state](const boost::system::error_code ec,
                                            const bool& result) {
                                if (ec)
                                {
                                    std::cerr << "Exception happened when get "
                                                 "functional propert\n";
                                    return;
                                }

                                if (!result)
                                {
                                    state = PSUState::acLost;
                                }
                                return;
                            },
                            serviceName.c_str(), pathStr.c_str(),
                            "org.freedesktop.DBus.Properties", "Get",
                            interface.c_str(), "functional");
                    }
                }
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/State/Decorator", 1, configTypes);
}
