#pragma once
#include "xyz/openbmc_project/Chassis/Control/Chassis/error.hpp"
#include "xyz/openbmc_project/Chassis/Control/Chassis/server.hpp"

const static int32_t POWER_OFF = 0;
const static int32_t POWER_ON = 1;

struct EventDeleter
{
    void operator()(sd_event *event) const
    {
        event = sd_event_unref(event);
    }
};

using EventPtr = std::unique_ptr<sd_event, EventDeleter>;

struct ChassisControl
    : sdbusplus::server::object_t<
          sdbusplus::xyz::openbmc_project::Chassis::Control::server::Chassis>
{
    ChassisControl(sdbusplus::bus::bus& bus, const char* path, EventPtr &event)
        : sdbusplus::server::object_t<
            sdbusplus::xyz::openbmc_project::Chassis::Control::server::Chassis>(
            bus, path),
            mBus(bus),
            mState(POWER_OFF)
    {
        phosphor::logging::log<phosphor::logging::level::DEBUG>("ChassisControl is created.");
    }

    int32_t powerOn() override;
    int32_t powerOff() override;
    int32_t softPowerOff() override;
    int32_t reboot() override;
    int32_t softReboot() override;
    int32_t quiesce() override;
    int32_t getPowerState() override;



 private:
   sdbusplus::bus::bus& mBus;
   int32_t mState;
};

