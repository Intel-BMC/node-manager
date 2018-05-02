#pragma once
#include <unistd.h>
#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Chassis/Control/Power/error.hpp>
#include <xyz/openbmc_project/Chassis/Control/Power/server.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include "Gpio.hpp"
#include "I2c.hpp"
#include "timer.hpp"

static constexpr size_t POLLING_INTERVAL_MS = 500;

const static constexpr char* PGOOD_PIN = "PGOOD";
const static constexpr char* POWER_UP_PIN = "POWER_UP_PIN";

const static constexpr size_t PCH_DEVICE_BUS_ADDRESS = 3;
const static constexpr size_t PCH_DEVICE_SLAVE_ADDRESS = 0x44;
const static constexpr size_t PCH_CMD_REGISTER = 0;
const static constexpr size_t PCH_POWER_DOWN_CMD = 0x02;

const static constexpr size_t POWER_ON_PULSE_TIME_MS = 200;
const static constexpr size_t POWER_OFF_PULSE_TIME_MS = 4000;

using pwr_control =
    sdbusplus::xyz::openbmc_project::Chassis::Control::server::Power;

struct PowerControl : sdbusplus::server::object_t<pwr_control> {
  PowerControl(sdbusplus::bus::bus& bus, const char* path,
               phosphor::watchdog::EventPtr event,
               sd_event_io_handler_t handler = PowerControl::EventHandler)
      : sdbusplus::server::object_t<pwr_control>(bus, path),
        bus(bus),
        callbackHandler(handler),
        timer(event, std::bind(&PowerControl::timeOutHandler, this)) {
    int ret;
    char buf;

    // config gpio
    ret = configGpio(PGOOD_PIN, &pgood_fd, bus);
    if (ret < 0) {
      throw std::runtime_error("failed to config GPIO");
      return;
    }

    ret = configGpio(POWER_UP_PIN, &power_up_fd, bus);
    if (ret < 0) {
      throw std::runtime_error("failed to config GPIO");
      closeGpio(pgood_fd);
      return;
    }

    ret = sd_event_add_io(event.get(), nullptr, pgood_fd, EPOLLPRI,
                          callbackHandler, this);
    if (ret < 0) {
      closeGpio(pgood_fd);
      closeGpio(power_up_fd);
      throw std::runtime_error("failed to add to event loop");
    }

    timer.start(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::milliseconds(POLLING_INTERVAL_MS)));
    timer.setEnabled<std::true_type>();
    phosphor::logging::log<phosphor::logging::level::DEBUG>("Enable timer");
  }

  ~PowerControl() {
    closeGpio(pgood_fd);
    closeGpio(power_up_fd);
  }

  void timeOutHandler() {
    // TODO polling acpi status

    this->timer.start(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::milliseconds(POLLING_INTERVAL_MS)));
    this->timer.setEnabled<std::true_type>();
  }

  static int EventHandler(sd_event_source* es, int fd, uint32_t revents,
                          void* userdata) {
    // For the first event, only set the initial status,  do not emit signal
    // since is it not triggered by the real gpio change

    static bool first_event = true;
    int n;
    char buf;

    PowerControl* powercontrol = static_cast<PowerControl*>(userdata);

    if (!powercontrol) {
      phosphor::logging::log<phosphor::logging::level::ERR>("null pointer!");
      return -1;
    }

    n = ::lseek(fd, 0, SEEK_SET);
    if (n < 0) {
      phosphor::logging::log<phosphor::logging::level::ERR>("lseek error!");
      return n;
    }

    n = ::read(fd, &buf, 1);
    if (n < 0) {
      phosphor::logging::log<phosphor::logging::level::ERR>("read error!");
      return n;
    }

    if (buf == '0') {
      powercontrol->state(0);
      powercontrol->pgood(0);

      if (first_event) {
        first_event = false;
      } else {
        powercontrol->gotoSystemState(std::string("HOST_POWERED_OFF"));
      }
    } else {
      powercontrol->state(1);
      powercontrol->pgood(1);
      if (first_event) {
        first_event = false;
      } else {
        powercontrol->gotoSystemState(std::string("HOST_POWERED_ON"));
      }
    }

    return 0;
  }

  int32_t forcePowerOff() override;
  int32_t setPowerState(int32_t newState) override;
  int32_t getPowerState() override;

 private:
  int power_up_fd;
  int pgood_fd;
  phosphor::watchdog::Timer timer;
  sdbusplus::bus::bus& bus;
  sd_event_io_handler_t callbackHandler;
};
