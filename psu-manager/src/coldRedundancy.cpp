/*
// Copyright (c) 2019 Intel Corporation
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

#include <array>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/container/flat_set.hpp>
#include <coldRedundancy.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <phosphor-logging/elog-errors.hpp>
#include <regex>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/asio/sd_event.hpp>
#include <utility.hpp>

static constexpr const bool debug = false;

static constexpr const std::array<const char*, 1> psuInterfaceTypes = {
    "xyz.openbmc_project.Configuration.pmbus"};
static const constexpr char* inventoryPath = "/xyz/openbmc_project/inventory";
static const constexpr char* eventPath = "/xyz/openbmc_project/State/Decorator";
static const constexpr char* coldRedundancyPath =
    "/xyz/openbmc_project/control/power_supply_redundancy";

static std::vector<std::unique_ptr<PowerSupply>> powerSupplies;

ColdRedundancy::ColdRedundancy(
    boost::asio::io_service& io, sdbusplus::asio::object_server& objectServer,
    std::shared_ptr<sdbusplus::asio::connection>& systemBus) :
    sdbusplus::xyz::openbmc_project::Control::server::PowerSupplyRedundancy(
        *systemBus, coldRedundancyPath),
    timerRotation(io), timerCheck(io), systemBus(systemBus)
{
    io.post([this, &io, &objectServer, &systemBus]() {
        createPSU(io, objectServer, systemBus);
    });
    std::function<void(sdbusplus::message::message&)> eventHandler =
        [this, &io, &objectServer,
         &systemBus](sdbusplus::message::message& message) {
            if (message.is_method_error())
            {
                std::cerr << "callback method error\n";
                return;
            }
            boost::asio::deadline_timer filterTimer(io);
            filterTimer.expires_from_now(boost::posix_time::seconds(1));
            filterTimer.async_wait([this, &io, &objectServer, &systemBus](
                                       const boost::system::error_code& ec) {
                if (ec == boost::asio::error::operation_aborted)
                {
                    return;
                }
                else if (ec)
                {
                    std::cerr << "timer error\n";
                }
                createPSU(io, objectServer, systemBus);
            });
        };
    for (const char* type : psuInterfaceTypes)
    {
        auto match = std::make_unique<sdbusplus::bus::match::match>(
            static_cast<sdbusplus::bus::bus&>(*systemBus),
            "type='signal',member='PropertiesChanged',path_namespace='" +
                std::string(inventoryPath) + "',arg0namespace='" + type + "'",
            eventHandler);
        matches.emplace_back(std::move(match));
    }

    io.run();
}

// Check PSU information from entity-manager D-Bus interface and use the bus
// address to create PSU Class for cold redundancy.
void ColdRedundancy::createPSU(
    boost::asio::io_service& io, sdbusplus::asio::object_server& objectServer,
    std::shared_ptr<sdbusplus::asio::connection>& conn)
{
    std::vector<PropertyMapType> sensorConfigs;
    numberOfPSU = 0;
    powerSupplies.clear();
    sensorConfigs.clear();

    // call mapper to get matched obj paths
    conn->async_method_call(
        [this, &conn, &sensorConfigs](const boost::system::error_code ec,
                                      GetSubTreeType subtree) {
            if (ec)
            {
                std::cerr << "Exception happened when communicating to "
                             "ObjectMapper\n";
                return;
            }
            if (debug)
            {
                std::cerr << "get valid subtree\n";
            }
            for (const auto& object : subtree)
            {
                std::string pathName = object.first;
                for (const auto& serviceIface : object.second)
                {
                    std::string serviceName = serviceIface.first;
                    for (const auto& interface : serviceIface.second)
                    {
                        // only get property of matched interface
                        bool isIfaceMatched = false;
                        for (const auto& type : psuInterfaceTypes)
                        {
                            if (type == interface)
                            {
                                isIfaceMatched = true;
                                break;
                            }
                        }
                        if (!isIfaceMatched)
                            continue;

                        conn->async_method_call(
                            [this, &conn,
                             &sensorConfigs](const boost::system::error_code ec,
                                             PropertyMapType propMap) {
                                if (ec)
                                {
                                    std::cerr
                                        << "Exception happened when get all "
                                           "properties\n";
                                    return;
                                }
                                if (debug)
                                {
                                    std::cerr << "get valid propMap\n";
                                }
                                // sensorConfigs.emplace_back(propMap);
                                auto configBus =
                                    std::get_if<uint64_t>(&propMap["Bus"]);
                                auto configAddress =
                                    std::get_if<uint64_t>(&propMap["Address"]);
                                auto configName =
                                    std::get_if<std::string>(&propMap["Name"]);

                                if (configBus == nullptr ||
                                    configAddress == nullptr ||
                                    configName == nullptr)
                                {
                                    std::cerr << "error finding necessary "
                                                 "entry in configuration\n";
                                    return;
                                }

                                powerSupplies.emplace_back(
                                    std::make_unique<PowerSupply>(
                                        *configName,
                                        static_cast<uint8_t>(*configBus),
                                        static_cast<uint8_t>(*configAddress),
                                        conn));
                                numberOfPSU++;
                            },
                            serviceName.c_str(), pathName.c_str(),
                            "org.freedesktop.DBus.Properties", "GetAll",
                            interface);
                    }
                }
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory/system/powersupply", 2,
        psuInterfaceTypes);
}

PowerSupply::PowerSupply(
    std::string name, uint8_t bus, uint8_t address,
    const std::shared_ptr<sdbusplus::asio::connection>& dbusConnection) :
    name(name),
    bus(bus), address(address)
{
    getPSUEvent(psuEventInterface, dbusConnection, name, state);
    if (debug)
    {
        std::cerr << "psu state " << static_cast<int>(state) << "\n";
    }
}

PowerSupply::~PowerSupply()
{
}
