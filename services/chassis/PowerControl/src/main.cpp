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
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <systemd/sd-event.h>
#include <unistd.h>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <chrono>
#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include "PowerControl.hpp"

int main(int argc, char* argv[]) {
  int ret = 0;

  phosphor::logging::log<phosphor::logging::level::INFO>(
      "Start Chassis power control service...");

  sd_event* event = nullptr;
  ret = sd_event_default(&event);
  if (ret < 0) {
    phosphor::logging::log<phosphor::logging::level::ERR>(
        "Error creating a default sd_event handler");
    return ret;
  }
  phosphor::watchdog::EventPtr eventP{event,
                                      phosphor::watchdog::EventDeleter()};
  event = nullptr;

  auto bus = sdbusplus::bus::new_default();
  sdbusplus::server::manager_t m{bus, DBUS_OBJECT_NAME};

  bus.request_name(DBUS_INTF_NAME);

  PowerControl powerControl{bus, DBUS_OBJECT_NAME, eventP};

  auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now());

  try {
    bus.attach_event(eventP.get(), SD_EVENT_PRIORITY_NORMAL);
    while (true) {
      ret = sd_event_run(eventP.get(), (uint64_t)-1);
      if (ret < 0) {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error waiting for events");
      }
    }
  }

  catch (std::exception& e) {
    phosphor::logging::log<phosphor::logging::level::ERR>(e.what());
    return -1;
  }
  return 0;
}
