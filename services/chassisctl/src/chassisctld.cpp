/*
// Copyright (c) 2017 Intel Corporation
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

#include <error.hpp>
#include <server.hpp>
#include <chrono>

#include "timer.hpp"

namespace Log = phosphor::logging;
namespace Watchdog = phosphor::watchdog;

using Chassisctl_inherit = sdbusplus::server::object_t<
    sdbusplus::org::openbmc::control::server::Power>;

struct Chassisctl : Chassisctl_inherit {
  Chassisctl(sdbusplus::bus::bus& bus, const char* path,
             Watchdog::EventPtr event)
      : Chassisctl_inherit(bus, path),
        bus(bus),
        timer(event, std::bind(&Chassisctl::timeOutHandler, this)) {
    timer.start(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::milliseconds(500)));
    timer.setEnabled<std::true_type>();
    Log::log<Log::level::DEBUG>("Enable timer");
  }

  uint32_t forcePowerOff() override {
    // TODO
    throw sdbusplus::org::openbmc::control::Power::Error::UnsupportedCommand();
    return 0;
  }

  uint32_t setPowerState(uint32_t newState) override {
    state(newState);
    return 0;
  }

  uint32_t getPowerState() override { return state(); }

  void timeOutHandler() {
    Log::log<Log::level::DEBUG>("timeOutHandler...");
    // TODO check acpi status
  }

  Watchdog::Timer timer;

 private:
  sdbusplus::bus::bus& bus;
};

int main(int argc, char* argv[]) {
  int ret = 0;

  Log::log<Log::level::INFO>("Start Chassis service...");

  constexpr const char* path = "/org/openbmc/control/power0";

  sd_event* event = nullptr;
  ret = sd_event_default(&event);
  if (ret < 0) {
    Log::log<Log::level::ERR>("Error creating a default sd_event handler");
    return ret;
  }
  Watchdog::EventPtr eventP{event, Watchdog::EventDeleter()};
  event = nullptr;

  auto bus = sdbusplus::bus::new_default();
  sdbusplus::server::manager_t m{bus, path};

  bus.request_name("org.openbmc.control.Power");

  Chassisctl c1{bus, path, eventP};

  try {
    bus.attach_event(eventP.get(), SD_EVENT_PRIORITY_NORMAL);

    while (true) {
      ret = sd_event_run(eventP.get(), (uint64_t)-1);
      if (ret < 0) {
        Log::log<Log::level::ERR>("Error waiting for events");
      }
      if (c1.timer.expired()) {
        Log::log<Log::level::DEBUG>("timer expired restart...");
        c1.timer.start(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::milliseconds(1000 * 1)));
        c1.timer.setEnabled<std::true_type>();
      }
    }
  }

  catch (std::exception& e) {
    Log::log<Log::level::ERR>(e.what());
    return -1;
  }
  return 0;
}
