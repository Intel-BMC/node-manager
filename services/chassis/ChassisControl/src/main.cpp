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
#include "ChassisControl.hpp"

int main(int argc, char *argv[])
{
    int ret = 0;

    phosphor::logging::log<phosphor::logging::level::INFO>(
        "Start Chassis Control service...");

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
        bus, "/xyz/openbmc_project/Chassis/Control/Chassis"};

    bus.request_name("xyz.openbmc_project.Chassis.Control.Chassis");

    ChassisControl chassisControl{bus, DBUS_OBJECT_NAME, eventP};

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
