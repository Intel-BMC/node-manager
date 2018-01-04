/**
 * Copyright (c) 2017 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// A tool used to wait for a dbus property(int32_t) to be the expected value
#include <unistd.h>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include "argument.hpp"

static bool Waiting = true;
static auto property = ArgumentParser::empty_string;
static auto expect_val = ArgumentParser::empty_string;

static int propChanged(sd_bus_message* msg, void* userData,
                       sd_bus_error* retError)
{
    std::string iface;
    std::map<std::string, sdbusplus::message::variant<int32_t>> data;

    auto sdPlusMsg = sdbusplus::message::message(msg);

    sdPlusMsg.read(iface, data);

    auto prop = data.find(property);

    if (prop != data.end())
    {
        int32_t prop_val = -1;
        prop_val = sdbusplus::message::variant_ns::get<int32_t>(prop->second);

        if (prop_val == std::stoi(expect_val))
        {
            Waiting = false;
        }
        else
        {
            Waiting = true;
        }
    }

    return 0;
}

static void ExitWithError(const char* err, char** argv)
{
    ArgumentParser::usage(argv);
    std::cerr << std::endl;
    std::cerr << "ERROR: " << err << std::endl;
    exit(-1);
}

int main(int argc, char* argv[])
{
    auto options = ArgumentParser(argc, argv);

    auto path = options["path"];
    if (path == ArgumentParser::empty_string)
    {
        ExitWithError("path not specified", argv);
    }

    auto service = options["service"];
    if (service == ArgumentParser::empty_string)
    {
        ExitWithError("service not specified", argv);
    }

    auto interface = options["interface"];
    if (interface == ArgumentParser::empty_string)
    {
        ExitWithError("interface not specified", argv);
    }

    property = options["property"];
    if (property == ArgumentParser::empty_string)
    {
        ExitWithError("property not specified", argv);
    }

    expect_val = options["expect"];
    if (expect_val == ArgumentParser::empty_string)
    {
        ExitWithError("expect value not specified", argv);
    }

    auto bus = sdbusplus::bus::new_default();

    std::string match_str =
        sdbusplus::bus::match::rules::propertiesChanged(path, interface);

    sdbusplus::message::variant<int32_t> prop_val = -1;

    auto method = bus.new_method_call(std::string(service).c_str(),
                                      std::string(path).c_str(),
                                      "org.freedesktop.DBus.Properties", "Get");

    method.append(interface, property);

    auto reply = bus.call(method);
    reply.read(prop_val);
    if (reply.is_method_error())
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error in method call to get property");
        return -1;
    }

    if (prop_val == std::stoi(expect_val))
    {
        return 0;
    }
    else
    {
        Waiting = true;
    }

    // Setup Signal Handler
    sdbusplus::bus::match::match waitSignals(bus, match_str.c_str(),
            propChanged, nullptr);
    // Wait for signal
    while (Waiting)
    {
        bus.process_discard();
        if (!Waiting)
        {
            break;
        }
        bus.wait();
    }

    return 0;
}
