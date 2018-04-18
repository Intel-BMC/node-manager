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
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <chrono>
#include "ChassisControl.hpp"

constexpr auto SYSTEMD_SERVICE   = "org.freedesktop.systemd1";
constexpr auto SYSTEMD_OBJ_PATH  = "/org/freedesktop/systemd1";
constexpr auto SYSTEMD_INTERFACE = "org.freedesktop.systemd1.Manager";
constexpr auto HOST_START_TARGET = "obmc-intel-host-start@.target";

constexpr auto POWER_CONTROL_SERVICE   = "xyz.openbmc_project.Chassis.Control.Power";
constexpr auto POWER_CONTROL_OBJ_PATH  = "/xyz/openbmc_project/Chassis/Control/Power";
constexpr auto POWER_CONTROL_INTERFACE = "xyz.openbmc_project.Chassis.Control.Power";

int32_t ChassisControl::powerOn()
{
    auto method = mBus.new_method_call(SYSTEMD_SERVICE,
                                       SYSTEMD_OBJ_PATH,
                                       SYSTEMD_INTERFACE,
                                       "StartUnit");
    method.append(HOST_START_TARGET);
    method.append("replace");
    auto response = mBus.call(method);
    if (response.is_method_error())
    {
        phosphor::logging::log<phosphor::logging::level::ERR>
                    ("ERROR: Failed to run host start target");
        return -1;
    }

    return 0;
}
int32_t ChassisControl::powerOff()
{
    //TODO
    return 0;
}

int32_t ChassisControl::softPowerOff()
{
    //TODO
    return 0;
}
int32_t ChassisControl::reboot()
{
    //TODO
    return 0;
}
int32_t ChassisControl::softReboot()
{
    //TODO
    return 0;
}
int32_t ChassisControl::quiesce()
{
    //TODO
    return 0;
}
int32_t ChassisControl::getPowerState()
{
    auto method = mBus.new_method_call(POWER_CONTROL_SERVICE, POWER_CONTROL_OBJ_PATH,
                                       POWER_CONTROL_INTERFACE, "getPowerState");
    auto result = mBus.call(method);
    if (result.is_method_error())
    {
        phosphor::logging::log<phosphor::logging::level::ERR>
            ("ERROR: Failed to call power control method getPowerState");
        return -1;
    }
    result.read(mState);

    return mState;
}
