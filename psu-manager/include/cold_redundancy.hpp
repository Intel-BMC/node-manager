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

#include <sdbusplus/asio/object_server.hpp>
#include <utility.hpp>
#include <xyz/openbmc_project/Control/PowerSupplyRedundancy/server.hpp>

const constexpr char* psuInterface =
    "/xyz/openbmc_project/inventory/system/powersupply/";
const constexpr int secondsInOneDay = 86400;
const constexpr uint8_t bmcSpecific = 0;

class ColdRedundancy
    : sdbusplus::xyz::openbmc_project::Control::server::PowerSupplyRedundancy
{
  public:
    ColdRedundancy(
        boost::asio::io_service& io,
        sdbusplus::asio::object_server& objectServer,
        std::shared_ptr<sdbusplus::asio::connection>& dbusConnection,
        std::vector<std::unique_ptr<sdbusplus::bus::match::match>>& matches);
    ~ColdRedundancy() = default;

    uint8_t pSUNumber() const override;
    void
        createPSU(boost::asio::io_service& io,
                  sdbusplus::asio::object_server& objectServer,
                  std::shared_ptr<sdbusplus::asio::connection>& dbusConnection);

  private:
    bool crSupported = true;
    bool crEnabled = true;
    bool rotationEnabled = true;
    std::string rotationAlgo =
        "xyz.openbmc_project.Control.PowerSupplyRedundancy.Algo.bmcSpecific";
    uint8_t psOrder;
    uint8_t numberOfPSU = 0;
    uint32_t rotationPeriod = 7 * secondsInOneDay;

    void startRotateCR(void);
    void startCRCheck(void);
    void rotateCR(void);
    void configCR(bool reConfig);
    void checkCR(void);
    void reRanking(void);
    void putWarmRedundant(void);
    void keepAliveCheck(void);

    std::shared_ptr<sdbusplus::asio::connection>& systemBus;

    boost::asio::steady_timer timerRotation;
    boost::asio::steady_timer timerCheck;
    boost::asio::steady_timer warmRedundantTimer1;
    boost::asio::steady_timer warmRedundantTimer2;
    boost::asio::steady_timer keepAliveTimer;
    boost::asio::steady_timer filterTimer;
};

constexpr const uint8_t pmbusCmdCRSupport = 0xd0;

class PowerSupply
{
  public:
    PowerSupply(
        std::string name, uint8_t bus, uint8_t address,
        const std::shared_ptr<sdbusplus::asio::connection>& dbusConnection);
    ~PowerSupply();
    std::string name;
    uint8_t order = 0;
    uint8_t bus;
    uint8_t address;
    PSUState state = PSUState::normal;
};
