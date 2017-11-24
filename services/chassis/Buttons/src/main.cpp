#include <chrono>
#include <experimental/filesystem>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <sys/stat.h>
#include <sys/types.h>
#include <systemd/sd-event.h>
#include <unistd.h>
#include <xyz/openbmc_project/Common/error.hpp>
#include "ResetButton.hpp"
#include "PowerButton.hpp"

int main(int argc, char *argv[])
{
    int ret = 0;

    phosphor::logging::log<phosphor::logging::level::INFO>(
        "Start power button service...");

    sd_event *event = nullptr;
    ret = sd_event_default(&event);
    if (ret < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error creating a default sd_event handler");
        return ret;
    }
    EventPtr eventP{event};
    event = nullptr;

    auto bus = sdbusplus::bus::new_default();
    sdbusplus::server::manager::manager objManager{
        bus, "/xyz/openbmc_project/Chassis/Buttons"};

    bus.request_name("xyz.openbmc_project.Chassis.Buttons");

    PowerButton powerButton{bus, POWER_DBUS_OBJECT_NAME, eventP};

    ResetButton resetButton{bus, RESET_DBUS_OBJECT_NAME, eventP};

    try
    {
        bus.attach_event(eventP.get(), SD_EVENT_PRIORITY_NORMAL);

        while (true)
        {
            ret = sd_event_run(eventP.get(), (uint64_t)-1);
            if (ret < 0)
            {
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    "Error waiting for events");
                break;
            }
        }
    }

    catch (std::exception &e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(e.what());
        ret = -1;
    }
    return ret;
}
