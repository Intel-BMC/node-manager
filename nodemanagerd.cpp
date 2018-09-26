/* Copyright 2018 Intel
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "nodemanagerd.hpp"

#include <boost/asio.hpp>
#include <unordered_map>
#include <vector>

static boost::asio::io_service io;
static auto conn = std::make_shared<sdbusplus::asio::connection>(io);
static boost::asio::steady_timer readingsSchedulingTimer(io);
static boost::asio::steady_timer framesDistributingTimer(io);

static sdbusplus::asio::object_server server =
    sdbusplus::asio::object_server(conn);

static uint32_t gAppSeq = 0;
static std::vector<std::shared_ptr<Request>> configuredRequests;
static std::unordered_map<uint32_t, std::shared_ptr<Request>>
    pendingRequestsList;

static int64_t ipmbRequestTimeout(uint32_t appSeqReceived)
{
    int64_t status = 0;

    phosphor::logging::log<phosphor::logging::level::WARNING>(
        "ipmbRequestTimeout: timeout occurred");

    auto listElement = pendingRequestsList.find(appSeqReceived);
    if (listElement != pendingRequestsList.end())
    {
        pendingRequestsList.erase(listElement);
    }

    return status;
}

static int64_t ipmbResponseReturn(uint32_t appSeqReceived, uint8_t netfn,
                                  uint8_t lun, uint8_t cmd, uint8_t cc,
                                  std::vector<uint8_t> &dataReceived)
{
    int64_t status = 0;

    auto listElement = pendingRequestsList.find(appSeqReceived);
    if (listElement != pendingRequestsList.end())
    {
        if (cc != 0)
        {
            // do nothing
        }
        else
        {
            listElement->second->handleResponse(dataReceived);
        }
        pendingRequestsList.erase(listElement);
    }
    else
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "ipmbResponseReturn: could not find pending request");
    }

    return status;
}

/**
 * @brief Function distributing requests in time (burst prevention)
 */
void processRequests(std::vector<std::shared_ptr<Request>>::iterator iter)
{
    framesDistributingTimer.expires_after(
        std::chrono::milliseconds(framesInterval));
    framesDistributingTimer.async_wait(
        [iter](const boost::system::error_code &ec) mutable {
            if (ec)
            {
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    "processRequests: timer error");
                return;
            }

            std::vector<uint8_t> data(0);
            uint8_t netFn = 0, lun = 0, cmd = 0;

            // prepare data to be sent
            if (iter != configuredRequests.end())
            {
                (*iter)->sendRequest(netFn, lun, cmd, data);
            }
            else
            {
                return;
            }

            // send request to Ipmb
            auto mesg = conn->new_signal(nmdObj, ipmbIntf, "sendRequest");
            auto tempAppSeq = gAppSeq;
            gAppSeq++;
            mesg.append(tempAppSeq, netFn, lun, cmd, data);
            mesg.signal_send();

            // place request in pending request list to match it with response
            // later on
            if (false == pendingRequestsList.insert({tempAppSeq, *iter}).second)
            {
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    "processRequests: could not place request in pending "
                    "request list ");
            }

            iter++;

            processRequests(iter);
        });
}

void performReadings()
{
    readingsSchedulingTimer.expires_after(
        std::chrono::seconds(readingsInterval));
    readingsSchedulingTimer.async_wait([](const boost::system::error_code &ec) {
        if (ec)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "performReadings: timer error");
            return;
        }

        if (pendingRequestsList.size() > 0)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "performReadings: pending request list not empty");
            pendingRequestsList.clear();
        }

        processRequests(configuredRequests.begin());

        performReadings();
    });
}

/**
 * @brief Main
 */
int main(int argc, char *argv[])
{
    conn->request_name(nmdBus);

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        server.add_interface(nmdObj, ipmbIntf);

    iface->register_method("requestSendFailed", std::move(ipmbRequestTimeout));
    iface->register_method("returnResponse", std::move(ipmbResponseReturn));

    iface->initialize();

    // NM Statistics
    // Global power statistics
    configuredRequests.push_back(std::make_shared<GlobalPowerPlatform>(
        server, "GlobalPowerPlatform", globalPowerStats, entirePlatform, 0));
    configuredRequests.push_back(std::make_shared<GlobalPowerCpu>(
        server, "GlobalPowerCpu", globalPowerStats, cpuSubsystem, 0));
    configuredRequests.push_back(std::make_shared<GlobalPowerMemory>(
        server, "GlobalPowerMemory", globalPowerStats, memorySubsystem, 0));
    configuredRequests.push_back(std::make_shared<GlobalPowerHwProtection>(
        server, "GlobalPowerHwProtection", globalPowerStats, hwProtection, 0));
    configuredRequests.push_back(std::make_shared<GlobalPowerIOsubsystem>(
        server, "GlobalPowerIOsubsystem", globalPowerStats,
        highPowerIOsubsystem, 0));

    // Global inlet temperature statistics
    configuredRequests.push_back(std::make_shared<GlobalInletTempPlatform>(
        server, "GlobalInletTempPlatform", globalInletTempStats, entirePlatform,
        0));
    configuredRequests.push_back(std::make_shared<GlobalInletTempCpu>(
        server, "GlobalInletTempCpu", globalInletTempStats, cpuSubsystem, 0));
    configuredRequests.push_back(std::make_shared<GlobalInletTempMemory>(
        server, "GlobalInletTempMemory", globalInletTempStats, memorySubsystem,
        0));
    configuredRequests.push_back(std::make_shared<GlobalInletTempHwProtection>(
        server, "GlobalInletTempHwProtection", globalInletTempStats,
        hwProtection, 0));
    configuredRequests.push_back(std::make_shared<GlobalInletTempIOsubsystem>(
        server, "GlobalInletTempIOsubsystem", globalInletTempStats,
        highPowerIOsubsystem, 0));

    // Global throttling statistics
    configuredRequests.push_back(std::make_shared<GlobalThrottlingPlatform>(
        server, "GlobalThrottlingPlatform", globalThrottlingStats,
        entirePlatform, 0));
    configuredRequests.push_back(std::make_shared<GlobalThrottlingCpu>(
        server, "GlobalThrottlingCpu", globalThrottlingStats, cpuSubsystem, 0));
    configuredRequests.push_back(std::make_shared<GlobalThrottlingMemory>(
        server, "GlobalThrottlingMemory", globalThrottlingStats,
        memorySubsystem, 0));
    configuredRequests.push_back(std::make_shared<GlobalThrottlingHwProtection>(
        server, "GlobalThrottlingHwProtection", globalThrottlingStats,
        hwProtection, 0));
    configuredRequests.push_back(std::make_shared<GlobalThrottlingIOsubsystem>(
        server, "GlobalThrottlingIOsubsystem", globalThrottlingStats,
        highPowerIOsubsystem, 0));

    // Global volumetric airflow statistics
    configuredRequests.push_back(std::make_shared<GlobalVolAirflowPlatform>(
        server, "GlobalVolAirflowPlatform", globalVolAirflowStats,
        entirePlatform, 0));
    configuredRequests.push_back(std::make_shared<GlobalVolAirflowCpu>(
        server, "GlobalVolAirflowCpu", globalVolAirflowStats, cpuSubsystem, 0));
    configuredRequests.push_back(std::make_shared<GlobalVolAirflowMemory>(
        server, "GlobalVolAirflowMemory", globalVolAirflowStats,
        memorySubsystem, 0));
    configuredRequests.push_back(std::make_shared<GlobalVolAirflowHwProtection>(
        server, "GlobalVolAirflowHwProtection", globalVolAirflowStats,
        hwProtection, 0));
    configuredRequests.push_back(std::make_shared<GlobalVolAirflowIOsubsystem>(
        server, "GlobalVolAirflowIOsubsystem", globalVolAirflowStats,
        highPowerIOsubsystem, 0));

    // Global outlet airflow temperature statistics
    configuredRequests.push_back(
        std::make_shared<GlobalOutletAirflowTempPlatform>(
            server, "GlobalOutletAirflowTempPlatform",
            globalOutletAirflowTempStats, entirePlatform, 0));
    configuredRequests.push_back(std::make_shared<GlobalOutletAirflowTempCpu>(
        server, "GlobalOutletAirflowTempCpu", globalOutletAirflowTempStats,
        cpuSubsystem, 0));
    configuredRequests.push_back(
        std::make_shared<GlobalOutletAirflowTempMemory>(
            server, "GlobalOutletAirflowTempMemory",
            globalOutletAirflowTempStats, memorySubsystem, 0));
    configuredRequests.push_back(
        std::make_shared<GlobalOutletAirflowTempHwProtection>(
            server, "GlobalOutletAirflowTempHwProtection",
            globalOutletAirflowTempStats, hwProtection, 0));
    configuredRequests.push_back(
        std::make_shared<GlobalOutletAirflowTempIOsubsystem>(
            server, "GlobalOutletAirflowTempIOsubsystem",
            globalOutletAirflowTempStats, highPowerIOsubsystem, 0));

    // Global chassis power statistics
    configuredRequests.push_back(std::make_shared<GlobalChassisPowerPlatform>(
        server, "GlobalChassisPowerPlatform", globalChassisPowerStats,
        entirePlatform, 0));
    configuredRequests.push_back(std::make_shared<GlobalChassisPowerCpu>(
        server, "GlobalChassisPowerCpu", globalChassisPowerStats, cpuSubsystem,
        0));
    configuredRequests.push_back(std::make_shared<GlobalChassisPowerMemory>(
        server, "GlobalChassisPowerMemory", globalChassisPowerStats,
        memorySubsystem, 0));
    configuredRequests.push_back(
        std::make_shared<GlobalChassisPowerHwProtection>(
            server, "GlobalChassisPowerHwProtection", globalChassisPowerStats,
            hwProtection, 0));
    configuredRequests.push_back(
        std::make_shared<GlobalChassisPowerIOsubsystem>(
            server, "GlobalChassisPowerIOsubsystem", globalChassisPowerStats,
            highPowerIOsubsystem, 0));

    performReadings();

    io.run();

    return 0;
}
