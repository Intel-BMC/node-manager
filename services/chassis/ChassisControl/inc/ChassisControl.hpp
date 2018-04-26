#pragma once
#include "xyz/openbmc_project/Chassis/Control/Chassis/error.hpp"
#include "xyz/openbmc_project/Chassis/Control/Chassis/server.hpp"
#include <phosphor-logging/log.hpp>
#include <systemd/sd-event.h>

constexpr auto POWER_BUTTON_PATH =
    "/xyz/openbmc_project/Chassis/Buttons/Power0";
constexpr auto POWER_BUTTON_INTF = "xyz.openbmc_project.Chassis.Buttons.Power";

const static int32_t POWER_OFF = 0;
const static int32_t POWER_ON = 1;
namespace sdbusRule = sdbusplus::bus::match::rules;

struct EventDeleter {
  void operator()(sd_event *event) const { event = sd_event_unref(event); }
};

using EventPtr = std::unique_ptr<sd_event, EventDeleter>;

struct ChassisControl
    : sdbusplus::server::object_t<
          sdbusplus::xyz::openbmc_project::Chassis::Control::server::Chassis> {
  ChassisControl(sdbusplus::bus::bus &bus, const char *path, EventPtr &event)
      : sdbusplus::server::object_t<
            sdbusplus::xyz::openbmc_project::Chassis::Control::server::Chassis>(
            bus, path),
        mBus(bus),
        powerButtonPressedSignal(
            bus,
            sdbusRule::type::signal() + sdbusRule::member("Pressed") +
                sdbusRule::path(POWER_BUTTON_PATH) +
                sdbusRule::interface(POWER_BUTTON_INTF),
            std::bind(std::mem_fn(&ChassisControl::powerButtonPressed), this,
                      std::placeholders::_1)) {
    phosphor::logging::log<phosphor::logging::level::DEBUG>(
        "ChassisControl is created.");
  }

  int32_t powerOn() override;
  int32_t powerOff() override;
  int32_t softPowerOff() override;
  int32_t reboot() override;
  int32_t softReboot() override;
  int32_t quiesce() override;
  int32_t getPowerState() override;

private:
  sdbusplus::bus::bus &mBus;

  sdbusplus::bus::match_t powerButtonPressedSignal;
  void powerButtonPressed(sdbusplus::message::message &m);
};
