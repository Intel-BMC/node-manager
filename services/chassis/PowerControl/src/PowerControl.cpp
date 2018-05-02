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
#include "PowerControl.hpp"

int32_t PowerControl::forcePowerOff() {
  int ret = 0;
  ret = i2cSet(PCH_DEVICE_BUS_ADDRESS, PCH_DEVICE_SLAVE_ADDRESS,
               PCH_CMD_REGISTER, PCH_POWER_DOWN_CMD);

  if (ret < 0) {
    throw sdbusplus::xyz::openbmc_project::Chassis::Control::Power::Error::
        IOError();
  }
  return 0;
}

int32_t PowerControl::setPowerState(int32_t newState) {
  int ret = 0;
  int count = 0;
  char buf;

  phosphor::logging::log<phosphor::logging::level::DEBUG>(
      "setPowerState", phosphor::logging::entry("newState=%d", newState));

  if (state() == newState) {
    phosphor::logging::log<phosphor::logging::level::DEBUG>(
        "Same powerstate", phosphor::logging::entry("newState=%d", newState));
  }

  if (newState == 1) {
    gotoSystemState(std::string("HOST_POWERING_ON"));
  } else {
    gotoSystemState(std::string("HOST_POWERING_OFF"));
  }

  state(newState);

  ret = ::lseek(power_up_fd, 0, SEEK_SET);
  if (ret < 0) {
    phosphor::logging::log<phosphor::logging::level::ERR>("lseek error!");
    throw sdbusplus::xyz::openbmc_project::Chassis::Control::Power::Error::
        IOError();
    return ret;
  }

  buf = '0';
  ret = ::write(power_up_fd, &buf, 1);
  if (ret < 0) {
    phosphor::logging::log<phosphor::logging::level::ERR>("write error!");
    throw sdbusplus::xyz::openbmc_project::Chassis::Control::Power::Error::
        IOError();
    return ret;
  }

  if (1 == newState) {  // start power on
    std::this_thread::sleep_for(
        std::chrono::milliseconds(POWER_ON_PULSE_TIME_MS));
  } else {  // start power off
    std::this_thread::sleep_for(
        std::chrono::milliseconds(POWER_OFF_PULSE_TIME_MS));
  }

  buf = '1';
  ret = ::write(power_up_fd, &buf, 1);
  if (ret < 0) {
    phosphor::logging::log<phosphor::logging::level::ERR>("write error!");
    throw sdbusplus::xyz::openbmc_project::Chassis::Control::Power::Error::
        IOError();
    return ret;
  }

  if (0 == newState) {
    // For power off, currently there is a known issue, the “long-press” power
    // button cannot power off the host, a workaround is perform force power off
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    if (1 == pgood()) {  // still on, force off!
      phosphor::logging::log<phosphor::logging::level::DEBUG>(
          "Perform force power off");
      count = 0;
      do {
        if (count++ > 5) {
          phosphor::logging::log<phosphor::logging::level::ERR>(
              "forcePowerOff error!");
          throw sdbusplus::xyz::openbmc_project::Chassis::Control::Power::
              Error::IOError();
          return ret;
        }
        ret = forcePowerOff();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      } while (ret != 0);
    }
  }

  return 0;
}

int32_t PowerControl::getPowerState() { return state(); }
