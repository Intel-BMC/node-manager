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

#include "NodeManagerProxy.hpp"

#include <boost/asio.hpp>
#include <tuple>
#include <unordered_map>
#include <vector>

static boost::asio::io_service io;
static auto conn = std::make_shared<sdbusplus::asio::connection>(io);
static boost::asio::steady_timer readingsSchedulingTimer(io);
static boost::asio::steady_timer framesDistributingTimer(io);

static sdbusplus::asio::object_server server =
    sdbusplus::asio::object_server(conn);

static std::vector<std::unique_ptr<Request>> configuredRequests;

/**
 * @brief Function distributing requests in time (burst prevention)
 */
void processRequests(
    std::vector<std::unique_ptr<Request>>::iterator requestIter)
{
    framesDistributingTimer.expires_after(
        std::chrono::milliseconds(framesInterval));
    framesDistributingTimer.async_wait(
        [requestIter](const boost::system::error_code &ec) mutable {
            if (ec)
            {
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    "processRequests: timer error");
                return;
            }

            if (requestIter == configuredRequests.end())
            {
                return;
            }

            // prepare data to be sent
            std::vector<uint8_t> data;
            uint8_t netFn = 0, lun = 0, cmd = 0;
            (*requestIter)->prepareRequest(netFn, lun, cmd, data);

            // send request to Ipmb
            conn->async_method_call(
                [requestIter](boost::system::error_code &ec,
                              std::tuple<int, uint8_t, uint8_t, uint8_t,
                                         uint8_t, std::vector<uint8_t>>
                                  response) {
                    if (ec)
                    {
                        phosphor::logging::log<phosphor::logging::level::ERR>(
                            "sendRequest: Error request response");
                        return;
                    }

                    std::vector<uint8_t> dataReceived;
                    int status = -1;
                    uint8_t netFn = 0, lun = 0, cmd = 0, cc = 0;

                    std::tie(status, netFn, lun, cmd, cc, dataReceived) =
                        response;

                    if (status)
                    {
                        phosphor::logging::log<phosphor::logging::level::ERR>(
                            "sendRequest: non-zero response status ",
                            phosphor::logging::entry("%d", status));
                        return;
                    }

                    (*requestIter)->handleResponse(cc, dataReceived);
                },
                ipmbBus, ipmbObj, ipmbIntf, "sendRequest", ipmbMeChannelNum,
                netFn, lun, cmd, data);

            requestIter++;
            processRequests(requestIter);
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

    iface->initialize();

    // NM Statistics
    // Global power statistics
    configuredRequests.push_back(std::make_unique<GlobalPowerPlatform>(
        server, "GlobalPowerPlatform", globalPowerStats, entirePlatform, 0));
    configuredRequests.push_back(std::make_unique<GlobalPowerCpu>(
        server, "GlobalPowerCpu", globalPowerStats, cpuSubsystem, 0));
    configuredRequests.push_back(std::make_unique<GlobalPowerMemory>(
        server, "GlobalPowerMemory", globalPowerStats, memorySubsystem, 0));
    configuredRequests.push_back(std::make_unique<GlobalPowerHwProtection>(
        server, "GlobalPowerHwProtection", globalPowerStats, hwProtection, 0));
    configuredRequests.push_back(std::make_unique<GlobalPowerIOsubsystem>(
        server, "GlobalPowerIOsubsystem", globalPowerStats,
        highPowerIOsubsystem, 0));

    // Global inlet temperature statistics
    configuredRequests.push_back(std::make_unique<GlobalInletTempPlatform>(
        server, "GlobalInletTempPlatform", globalInletTempStats, entirePlatform,
        0));
    configuredRequests.push_back(std::make_unique<GlobalInletTempCpu>(
        server, "GlobalInletTempCpu", globalInletTempStats, cpuSubsystem, 0));
    configuredRequests.push_back(std::make_unique<GlobalInletTempMemory>(
        server, "GlobalInletTempMemory", globalInletTempStats, memorySubsystem,
        0));
    configuredRequests.push_back(std::make_unique<GlobalInletTempHwProtection>(
        server, "GlobalInletTempHwProtection", globalInletTempStats,
        hwProtection, 0));
    configuredRequests.push_back(std::make_unique<GlobalInletTempIOsubsystem>(
        server, "GlobalInletTempIOsubsystem", globalInletTempStats,
        highPowerIOsubsystem, 0));

    // Global throttling statistics
    configuredRequests.push_back(std::make_unique<GlobalThrottlingPlatform>(
        server, "GlobalThrottlingPlatform", globalThrottlingStats,
        entirePlatform, 0));
    configuredRequests.push_back(std::make_unique<GlobalThrottlingCpu>(
        server, "GlobalThrottlingCpu", globalThrottlingStats, cpuSubsystem, 0));
    configuredRequests.push_back(std::make_unique<GlobalThrottlingMemory>(
        server, "GlobalThrottlingMemory", globalThrottlingStats,
        memorySubsystem, 0));
    configuredRequests.push_back(std::make_unique<GlobalThrottlingHwProtection>(
        server, "GlobalThrottlingHwProtection", globalThrottlingStats,
        hwProtection, 0));
    configuredRequests.push_back(std::make_unique<GlobalThrottlingIOsubsystem>(
        server, "GlobalThrottlingIOsubsystem", globalThrottlingStats,
        highPowerIOsubsystem, 0));

    // Global volumetric airflow statistics
    configuredRequests.push_back(std::make_unique<GlobalVolAirflowPlatform>(
        server, "GlobalVolAirflowPlatform", globalVolAirflowStats,
        entirePlatform, 0));
    configuredRequests.push_back(std::make_unique<GlobalVolAirflowCpu>(
        server, "GlobalVolAirflowCpu", globalVolAirflowStats, cpuSubsystem, 0));
    configuredRequests.push_back(std::make_unique<GlobalVolAirflowMemory>(
        server, "GlobalVolAirflowMemory", globalVolAirflowStats,
        memorySubsystem, 0));
    configuredRequests.push_back(std::make_unique<GlobalVolAirflowHwProtection>(
        server, "GlobalVolAirflowHwProtection", globalVolAirflowStats,
        hwProtection, 0));
    configuredRequests.push_back(std::make_unique<GlobalVolAirflowIOsubsystem>(
        server, "GlobalVolAirflowIOsubsystem", globalVolAirflowStats,
        highPowerIOsubsystem, 0));

    // Global outlet airflow temperature statistics
    configuredRequests.push_back(
        std::make_unique<GlobalOutletAirflowTempPlatform>(
            server, "GlobalOutletAirflowTempPlatform",
            globalOutletAirflowTempStats, entirePlatform, 0));
    configuredRequests.push_back(std::make_unique<GlobalOutletAirflowTempCpu>(
        server, "GlobalOutletAirflowTempCpu", globalOutletAirflowTempStats,
        cpuSubsystem, 0));
    configuredRequests.push_back(
        std::make_unique<GlobalOutletAirflowTempMemory>(
            server, "GlobalOutletAirflowTempMemory",
            globalOutletAirflowTempStats, memorySubsystem, 0));
    configuredRequests.push_back(
        std::make_unique<GlobalOutletAirflowTempHwProtection>(
            server, "GlobalOutletAirflowTempHwProtection",
            globalOutletAirflowTempStats, hwProtection, 0));
    configuredRequests.push_back(
        std::make_unique<GlobalOutletAirflowTempIOsubsystem>(
            server, "GlobalOutletAirflowTempIOsubsystem",
            globalOutletAirflowTempStats, highPowerIOsubsystem, 0));

    // Global chassis power statistics
    configuredRequests.push_back(std::make_unique<GlobalChassisPowerPlatform>(
        server, "GlobalChassisPowerPlatform", globalChassisPowerStats,
        entirePlatform, 0));
    configuredRequests.push_back(std::make_unique<GlobalChassisPowerCpu>(
        server, "GlobalChassisPowerCpu", globalChassisPowerStats, cpuSubsystem,
        0));
    configuredRequests.push_back(std::make_unique<GlobalChassisPowerMemory>(
        server, "GlobalChassisPowerMemory", globalChassisPowerStats,
        memorySubsystem, 0));
    configuredRequests.push_back(
        std::make_unique<GlobalChassisPowerHwProtection>(
            server, "GlobalChassisPowerHwProtection", globalChassisPowerStats,
            hwProtection, 0));
    configuredRequests.push_back(
        std::make_unique<GlobalChassisPowerIOsubsystem>(
            server, "GlobalChassisPowerIOsubsystem", globalChassisPowerStats,
            highPowerIOsubsystem, 0));

    performReadings();

    io.run();

    return 0;
}
