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

#pragma once
#include <boost/container/flat_map.hpp>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <sdbusplus/asio/connection.hpp>

const constexpr char* entityManagerName = "xyz.openbmc_project.EntityManager";
static const constexpr char* redundancyInterface =
    "xyz.openbmc_project.Control.PowerSupplyRedundancy";
static const constexpr std::array<const char*, 1> psuEventInterface = {
    "xyz.openbmc_project.State.Decorator.OperationalStatus"};

using BasicVariantType =
    std::variant<std::vector<std::string>, std::vector<uint64_t>, std::string,
                 int64_t, uint64_t, double, int32_t, uint32_t, int16_t,
                 uint16_t, uint8_t, bool>;

using PropertyMapType =
    boost::container::flat_map<std::string, BasicVariantType>;

using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;

constexpr std::chrono::microseconds dbusTimeout(5000);

using Value = sdbusplus::message::variant<bool, uint8_t, int16_t, uint16_t,
                                          int32_t, uint32_t, int64_t, uint64_t,
                                          double, std::string>;

enum class PSUState
{
    normal,
    acLost
};

void getPSUEvent(
    const std::array<const char*, 1>& type,
    const std::shared_ptr<sdbusplus::asio::connection>& dbusConnection,
    const std::string& psuName, PSUState& state);

int i2cSet(uint8_t bus, uint8_t slaveAddr, uint8_t regAddr, uint8_t value);
int i2cGet(uint8_t bus, uint8_t slaveAddr, uint8_t regAddr, int& value);
