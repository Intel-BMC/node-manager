#pragma once
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Chassis/Control/Power/error.hpp>
#include <xyz/openbmc_project/Chassis/Control/Power/server.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <iostream>
#include "timer.hpp"

static constexpr size_t POLLING_INTERVAL_MS = 500;
using pwr_control =
    sdbusplus::xyz::openbmc_project::Chassis::Control::server::Power;

struct PowerControl : sdbusplus::server::object_t<pwr_control> {
  PowerControl(sdbusplus::bus::bus& bus, const char* path,
               phosphor::watchdog::EventPtr event)
      : sdbusplus::server::object_t<pwr_control>(bus, path),
        bus(bus),
        timer(event, std::bind(&PowerControl::timeOutHandler, this)) {
    timer.start(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::milliseconds(POLLING_INTERVAL_MS)));
    timer.setEnabled<std::true_type>();
    phosphor::logging::log<phosphor::logging::level::DEBUG>("Enable timer");
  }

  void timeOutHandler() {
    phosphor::logging::log<phosphor::logging::level::DEBUG>(
        "timeOutHandler...");

    // TODO polling acpi status

    this->timer.start(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::milliseconds(POLLING_INTERVAL_MS)));
    this->timer.setEnabled<std::true_type>();
  }

  int32_t forcePowerOff() override;
  int32_t setPowerState(int32_t newState) override;
  int32_t getPowerState() override;

 private:
  phosphor::watchdog::Timer timer;
  sdbusplus::bus::bus& bus;
};
