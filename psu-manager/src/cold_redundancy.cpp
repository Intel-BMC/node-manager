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
#include <cold_redundancy.hpp>
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

static constexpr const std::array<const char*, 3> psuInterfaceTypes = {
    "xyz.openbmc_project.Configuration.pmbus",
    "xyz.openbmc_project.Configuration.PSUPresence",
    "xyz.openbmc_project.Configuration.PURedundancy"};
static const constexpr char* inventoryPath =
    "/xyz/openbmc_project/inventory/system";
static const constexpr char* eventPath = "/xyz/openbmc_project/State/Decorator";
static const constexpr char* coldRedundancyPath =
    "/xyz/openbmc_project/control/power_supply_redundancy";

static std::vector<std::unique_ptr<PowerSupply>> powerSupplies;
static uint8_t pmbusNum = 7;
static std::vector<uint64_t> addrTable = {0x50, 0x51};

ColdRedundancy::ColdRedundancy(
    boost::asio::io_service& io, sdbusplus::asio::object_server& objectServer,
    std::shared_ptr<sdbusplus::asio::connection>& systemBus,
    std::vector<std::unique_ptr<sdbusplus::bus::match::match>>& matches) :
    sdbusplus::xyz::openbmc_project::Control::server::PowerSupplyRedundancy(
        *systemBus, coldRedundancyPath),
    timerRotation(io), timerCheck(io), systemBus(systemBus),
    warmRedundantTimer1(io), warmRedundantTimer2(io), keepAliveTimer(io),
    filterTimer(io), puRedundantTimer(io)
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
            filterTimer.expires_after(std::chrono::seconds(1));
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

    std::function<void(sdbusplus::message::message&)> paramConfig =
        [this](sdbusplus::message::message& message) {
            std::string objectName;
            boost::container::flat_map<
                std::string, std::variant<bool, uint8_t, uint32_t, std::string,
                                          std::vector<uint8_t>>>
                values;
            message.read(objectName, values);

            for (auto& value : values)
            {
                if (value.first == "ColdRedundancyEnabled")
                {
                    bool* pCREnabled = std::get_if<bool>(&(value.second));
                    if (pCREnabled != nullptr)
                    {
                        crEnabled = *pCREnabled;
                        ColdRedundancy::configCR(false);
                    }
                    continue;
                }
                if (value.first == "RotationEnabled")
                {
                    bool* pRotationEnabled = std::get_if<bool>(&(value.second));
                    if (pRotationEnabled != nullptr)
                    {
                        rotationEnabled = *pRotationEnabled;
                        ColdRedundancy::configCR(false);
                    }
                    continue;
                }
                if (value.first == "RotationAlgorithm")
                {
                    std::string* pAlgo =
                        std::get_if<std::string>(&(value.second));
                    if (pAlgo != nullptr)
                    {
                        rotationAlgo = *pAlgo;
                    }
                    continue;
                }
                if (value.first == "RotationRankOrder")
                {
                    auto pRank =
                        std::get_if<std::vector<uint8_t>>(&(value.second));
                    if (pRank == nullptr)
                    {
                        continue;
                    }
                    uint8_t rankSize = pRank->size();
                    uint8_t index = 0;
                    for (auto& psu : powerSupplies)
                    {
                        if (index < rankSize)
                        {
                            psu->order = (*pRank)[index];
                        }
                        else
                        {
                            psu->order = 0;
                        }
                        index++;
                    }
                    ColdRedundancy::configCR(false);
                    continue;
                }
                if (value.first == "PeriodOfRotation")
                {
                    uint32_t* pPeriod = std::get_if<uint32_t>(&(value.second));
                    if (pPeriod != nullptr)
                    {
                        rotationPeriod = *pPeriod;
                        timerRotation.cancel();
                        startRotateCR();
                    }
                    continue;
                }
                std::cerr << "Unused property changed\n";
            }
        };

    std::function<void(sdbusplus::message::message&)> eventCollect =
        [&](sdbusplus::message::message& message) {
            std::string objectName;
            boost::container::flat_map<std::string, std::variant<bool>> values;
            std::string path = message.get_path();
            std::size_t slantingPos = path.find_last_of("/\\");
            if ((slantingPos == std::string::npos) ||
                ((slantingPos + 1) >= path.size()))
            {
                std::cerr << "Unable to get PSU state name from path\n";
                return;
            }
            std::string statePSUName = path.substr(slantingPos + 1);
            bool stateChanged = false;

            std::size_t hypenPos = statePSUName.find("_");
            if (hypenPos == std::string::npos)
            {
                std::cerr << "Unable to get PSU name from PSU path\n";
                return;
            }
            std::string psuName = statePSUName.substr(0, hypenPos);

            try
            {
                message.read(objectName, values);
            }
            catch (const sdbusplus::exception::exception& e)
            {
                std::cerr << "Failed to read message from PSU Event\n";
                return;
            }

            for (auto& psu : powerSupplies)
            {
                if (psu->name != psuName)
                {
                    continue;
                }

                std::string psuEventName = "functional";
                auto findEvent = values.find(psuEventName);
                if (findEvent != values.end())
                {
                    if (std::get<bool>(findEvent->second))
                    {
                        psu->state = PSUState::normal;
                    }
                    else
                    {
                        psu->state = PSUState::acLost;
                    }
                }
            }
            checkRedundancyEvent();
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

    for (const char* eventType : psuEventInterface)
    {
        auto eventMatch = std::make_unique<sdbusplus::bus::match::match>(
            static_cast<sdbusplus::bus::bus&>(*systemBus),
            "type='signal',member='PropertiesChanged',path_namespace='" +
                std::string(eventPath) + "',arg0namespace='" + eventType + "'",
            eventCollect);
        matches.emplace_back(std::move(eventMatch));
    }

    auto configParamMatch = std::make_unique<sdbusplus::bus::match::match>(
        static_cast<sdbusplus::bus::bus&>(*systemBus),
        "type='signal',member='PropertiesChanged',path_namespace='" +
            std::string(coldRedundancyPath) + "',arg0namespace='" +
            redundancyInterface + "'",
        paramConfig);
    matches.emplace_back(std::move(configParamMatch));

    io.run();
}

static std::set<uint8_t> psuPresence;
static const constexpr uint8_t fruOffsetZero = 0x00;

int pingPSU(const uint8_t& addr)
{
    int fruData = 0;
    return i2cGet(pmbusNum, addr, fruOffsetZero, fruData);
}

void rescanPSUEntityManager(
    std::shared_ptr<sdbusplus::asio::connection>& dbusConnection)
{
    sdbusplus::message::message method = dbusConnection->new_method_call(
        "xyz.openbmc_project.FruDevice", "/xyz/openbmc_project/FruDevice",
        "xyz.openbmc_project.FruDeviceManager", "ReScan");

    try
    {
        dbusConnection->call(method);
    }
    catch (const sdbusplus::exception::exception&)
    {
        std::cerr << "Failed to rescan entity manager\n";
    }
    return;
}

void keepAlive(std::shared_ptr<sdbusplus::asio::connection>& dbusConnection)
{
    bool newPSUFound = false;
    uint8_t psuNumber = 1;
    for (const auto& addr : addrTable)
    {
        if (0 == pingPSU(addr))
        {
            auto found = psuPresence.find(addr);
            if (found != psuPresence.end())
            {
                continue;
            }
            newPSUFound = true;
            psuPresence.emplace(addr);
            std::string psuNumStr = "PSU" + std::to_string(psuNumber);
            sd_journal_send("MESSAGE=%s", "New PSU is found", "PRIORITY=%i",
                            LOG_INFO, "REDFISH_MESSAGE_ID=%s",
                            "OpenBMC.0.1.PowerSupplyInserted",
                            "REDFISH_MESSAGE_ARGS=%s", psuNumStr.c_str(), NULL);
        }
        else
        {
            auto found = psuPresence.find(addr);
            if (found == psuPresence.end())
            {
                continue;
            }
            psuPresence.erase(addr);
            std::string psuNumStr = "PSU" + std::to_string(psuNumber);
            sd_journal_send("MESSAGE=%s", "One PSU is removed", "PRIORITY=%i",
                            LOG_INFO, "REDFISH_MESSAGE_ID=%s",
                            "OpenBMC.0.1.PowerSupplyRemoved",
                            "REDFISH_MESSAGE_ARGS=%s", psuNumStr.c_str(), NULL);
        }
        psuNumber++;
    }
    if (newPSUFound)
    {
        rescanPSUEntityManager(dbusConnection);
    }
}

static const constexpr int psuDepth = 3;
// Check PSU information from entity-manager D-Bus interface and use the bus
// address to create PSU Class for cold redundancy.
void ColdRedundancy::createPSU(
    boost::asio::io_service& io, sdbusplus::asio::object_server& objectServer,
    std::shared_ptr<sdbusplus::asio::connection>& conn)
{
    numberOfPSU = 0;
    powerSupplies.clear();

    // call mapper to get matched obj paths
    conn->async_method_call(
        [this, &conn](const boost::system::error_code ec,
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
                             interface](const boost::system::error_code ec,
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

                                auto configName =
                                    std::get_if<std::string>(&propMap["Name"]);
                                if (configName == nullptr)
                                {
                                    std::cerr << "error finding necessary "
                                                 "entry in configuration\n";
                                    return;
                                }

                                if (interface == "xyz.openbmc_project."
                                                 "Configuration.PURedundancy")
                                {
                                    uint64_t* redunancyCount =
                                        std::get_if<uint64_t>(
                                            &propMap["RedundantCount"]);
                                    if (redunancyCount != nullptr)
                                    {
                                        redundancyPSURequire =
                                            static_cast<uint8_t>(
                                                *redunancyCount);
                                    }
                                    else
                                    {
                                        std::cerr << "Failed to get Power Unit "
                                                     "Redundancy count, will "
                                                     "use default value\n";
                                    }
                                    return;
                                }
                                else if (interface ==
                                         "xyz.openbmc_project."
                                         "Configuration.PSUPresence")
                                {
                                    auto psuBus =
                                        std::get_if<uint64_t>(&propMap["Bus"]);
                                    auto psuAddress =
                                        std::get_if<std::vector<uint64_t>>(
                                            &propMap["Address"]);

                                    if (psuBus == nullptr ||
                                        psuAddress == nullptr)
                                    {
                                        std::cerr << "error finding necessary "
                                                     "entry in configuration\n";
                                        return;
                                    }
                                    pmbusNum = static_cast<uint8_t>(*psuBus);
                                    addrTable = *psuAddress;
                                    keepAliveCheck();
                                    return;
                                }

                                auto configBus =
                                    std::get_if<uint64_t>(&propMap["Bus"]);
                                auto configAddress =
                                    std::get_if<uint64_t>(&propMap["Address"]);

                                if (configBus == nullptr ||
                                    configAddress == nullptr)
                                {
                                    std::cerr << "error finding necessary "
                                                 "entry in configuration\n";
                                    return;
                                }
                                for (auto& psu : powerSupplies)
                                {
                                    if ((static_cast<uint8_t>(*configBus) ==
                                         psu->bus) &&
                                        (static_cast<uint8_t>(*configAddress) ==
                                         psu->address))
                                    {
                                        return;
                                    }
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
            checkRedundancyEvent();
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory/system", psuDepth, psuInterfaceTypes);
    startRotateCR();
    startCRCheck();
}

void ColdRedundancy::keepAliveCheck(void)
{
    keepAliveTimer.expires_after(std::chrono::seconds(2));
    keepAliveTimer.async_wait([&](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted)
        {
            return;
        }
        else if (ec)
        {
            std::cerr << "timer error\n";
        }
        keepAlive(systemBus);
        keepAliveCheck();
    });
}

uint8_t ColdRedundancy::pSUNumber() const
{
    return numberOfPSU;
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

// Reranking PSU orders with ascending order, if any of the PSU is not in
// normal state, changing rotation algo to bmc specific, and Reranking all
// other normal PSU. If all PSU are in normal state, and rotation algo is
// user specific, do nothing.
void ColdRedundancy::reRanking(void)
{
    uint8_t index = 1;
    if (rotationAlgo ==
        "xyz.openbmc_project.Control.PowerSupplyRedundancy.Algo.bmcSpecific")
    {
        for (auto& psu : powerSupplies)
        {
            if (psu->state == PSUState::normal)
            {
                psu->order = (index++);
            }
            else
            {
                psu->order = 0;
            }
        }
    }
    else
    {
        for (auto& psu : powerSupplies)
        {
            if (psu->state == PSUState::acLost)
            {
                rotationAlgo = "xyz.openbmc_project.Control."
                               "PowerSupplyRedundancy.Algo.bmcSpecific";
                reRanking();
                return;
            }
        }
    }
}

void ColdRedundancy::configCR(bool reConfig)
{
    if (!crSupported || !crEnabled)
    {
        return;
    }
    putWarmRedundant();
    warmRedundantTimer2.expires_after(std::chrono::seconds(5));
    warmRedundantTimer2.async_wait([this, reConfig](
                                       const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted)
        {
            return;
        }
        else if (ec)
        {
            std::cerr << "warm redundant timer error\n";
            return;
        }

        if (reConfig)
        {
            reRanking();
        }

        for (auto& psu : powerSupplies)
        {
            if (psu->state == PSUState::normal && psu->order != 0)
            {
                if (i2cSet(psu->bus, psu->address, pmbusCmdCRSupport,
                           psu->order))
                {
                    std::cerr << "Failed to change PSU Cold Redundancy order\n";
                }
            }
        }
    });
}

void ColdRedundancy::checkCR(void)
{
    if (!crSupported)
    {
        return;
    }
    if (!crEnabled)
    {
        putWarmRedundant();
        return;
    }

    for (auto& psu : powerSupplies)
    {
        if (psu->state == PSUState::normal)
        {
            int order = 0;
            if (i2cGet(psu->bus, psu->address, pmbusCmdCRSupport, order))
            {
                std::cerr << "Failed to get PSU Cold Redundancy order\n";
                continue;
            }
            if (order == 0)
            {
                configCR(true);
                return;
            }
        }
    }
}

void ColdRedundancy::startCRCheck()
{
    timerCheck.expires_after(std::chrono::seconds(60));
    timerCheck.async_wait([this](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted)
        {
            return;
        }
        else if (ec)
        {
            std::cerr << "timer error\n";
        }
        if (crSupported)
        {
            checkCR();
        }
        startCRCheck();
    });
}

// Rotate the orders of PSU redundancy. Each normal PSU will add one to its
// rank order. And the PSU with last rank order will become the rank order 1
void ColdRedundancy::rotateCR(void)
{
    if (!crSupported || !crEnabled)
    {
        return;
    }
    putWarmRedundant();
    warmRedundantTimer1.expires_after(std::chrono::seconds(5));
    warmRedundantTimer1.async_wait([this](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted)
        {
            return;
        }
        else if (ec)
        {
            std::cerr << "warm redundant timer error\n";
            return;
        }

        int goodPSUCount = 0;

        for (auto& psu : powerSupplies)
        {
            if (psu->state == PSUState::normal)
            {
                goodPSUCount++;
            }
        }

        for (auto& psu : powerSupplies)
        {
            if (psu->order == 0)
            {
                continue;
            }
            psu->order++;
            if (psu->order > goodPSUCount)
            {
                psu->order = 1;
            }
            if (i2cSet(psu->bus, psu->address, pmbusCmdCRSupport, psu->order))
            {
                std::cerr << "Failed to change PSU Cold Redundancy order\n";
            }
        }
    });
}

void ColdRedundancy::startRotateCR()
{
    timerRotation.expires_after(std::chrono::seconds(rotationPeriod));
    timerRotation.async_wait([this](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted)
        {
            return;
        }
        else if (ec)
        {
            std::cerr << "timer error\n";
        }
        if (crSupported && rotationEnabled)
        {
            rotateCR();
        }
        startRotateCR();
    });
}

void ColdRedundancy::putWarmRedundant(void)
{
    if (!crSupported)
    {
        return;
    }
    for (auto& psu : powerSupplies)
    {
        if (psu->state == PSUState::normal)
        {
            i2cSet(psu->bus, psu->address, pmbusCmdCRSupport, 0);
        }
    }
}

PowerSupply::~PowerSupply()
{
}

void ColdRedundancy::checkRedundancyEvent()
{
    puRedundantTimer.expires_after(std::chrono::seconds(2));
    puRedundantTimer.async_wait([this](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted)
        {
            return;
        }

        uint8_t psuWorkable = 0;
        static uint8_t psuPreviousWorkable = numberOfPSU;

        for (const auto& psu : powerSupplies)
        {
            if (psu->state == PSUState::normal)
            {
                psuWorkable++;
            }
        }

        if (psuWorkable > psuPreviousWorkable)
        {
            if (psuWorkable > redundancyPSURequire)
            {
                if (psuWorkable == numberOfPSU)
                {
                    // When all PSU are work correctly, it is full redundant
                    sd_journal_send(
                        "MESSAGE=%s", "Power Unit Full Redundancy Regained",
                        "PRIORITY=%i", LOG_INFO, "REDFISH_MESSAGE_ID=%s",
                        "OpenBMC.0.1.PowerUnitRedundancyRegained", NULL);
                }
                else if (psuPreviousWorkable <= redundancyPSURequire)
                {
                    // Not all PSU can work correctly but system still in
                    // redundancy mode and previous status is non redundant
                    sd_journal_send(
                        "MESSAGE=%s",
                        "Power Unit Redundancy Regained but not in Full "
                        "Redundancy",
                        "PRIORITY=%i", LOG_INFO, "REDFISH_MESSAGE_ID=%s",
                        "OpenBMC.0.1.PowerUnitDegradedFromNonRedundant", NULL);
                }
            }
            else if (psuPreviousWorkable == 0)
            {
                // Now system is not in redundancy mode but still some PSU are
                // workable and previously there is no any workable PSU in the
                // system
                sd_journal_send(
                    "MESSAGE=%s",
                    "Power Unit Redundancy Sufficient from insufficient",
                    "PRIORITY=%i", LOG_INFO, "REDFISH_MESSAGE_ID=%s",
                    "OpenBMC.0.1.PowerUnitNonRedundantFromInsufficient", NULL);
            }
        }
        else if (psuWorkable < psuPreviousWorkable)
        {
            if (psuWorkable > redundancyPSURequire)
            {
                // One PSU is now not workable, but other workable PSU can still
                // support redundancy mode.
                sd_journal_send(
                    "MESSAGE=%s", "Power Unit Redundancy Degraded",
                    "PRIORITY=%i", LOG_WARNING, "REDFISH_MESSAGE_ID=%s",
                    "OpenBMC.0.1.PowerUnitRedundancyDegraded", NULL);

                if (psuPreviousWorkable == numberOfPSU)
                {
                    // One PSU become not workable and system was in full
                    // redundancy mode.
                    sd_journal_send(
                        "MESSAGE=%s",
                        "Power Unit Redundancy Degraded from Full Redundant",
                        "PRIORITY=%i", LOG_WARNING, "REDFISH_MESSAGE_ID=%s",
                        "OpenBMC.0.1.PowerUnitDegradedFromRedundant", NULL);
                }
            }
            else
            {
                if (psuPreviousWorkable > redundancyPSURequire)
                {
                    // No enough workable PSU to support redundancy and
                    // previously system is in redundancy mode.
                    sd_journal_send(
                        "MESSAGE=%s", "Power Unit Redundancy Lost",
                        "PRIORITY=%i", LOG_ERR, "REDFISH_MESSAGE_ID=%s",
                        "OpenBMC.0.1.PowerUnitRedundancyLost", NULL);
                    if (psuWorkable > 0)
                    {
                        // There still some workable PSU, but system is not
                        // in redundancy mode.
                        sd_journal_send(
                            "MESSAGE=%s",
                            "Power Unit Redundancy NonRedundant Sufficient",
                            "PRIORITY=%i", LOG_WARNING, "REDFISH_MESSAGE_ID=%s",
                            "OpenBMC.0.1.PowerUnitNonRedundantSufficient",
                            NULL);
                    }
                }
                if (psuWorkable == 0)
                {
                    // No any workable PSU on the system.
                    sd_journal_send(
                        "MESSAGE=%s", "Power Unit Redundancy Insufficient",
                        "PRIORITY=%i", LOG_ERR, "REDFISH_MESSAGE_ID=%s",
                        "OpenBMC.0.1.PowerUnitNonRedundantInsufficient", NULL);
                }
            }
        }
        psuPreviousWorkable = psuWorkable;
    });
}
