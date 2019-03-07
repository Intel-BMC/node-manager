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

    // NM Statistics
    // Global power statistics
    configuredRequests.push_back(std::make_unique<GlobalPowerPlatform>(
        server, 0, 255, "power", "Platform", globalPowerStats, entirePlatform,
        0));
    configuredRequests.push_back(std::make_unique<GlobalPowerCpu>(
        server, 0, 255, "power", "CPU", globalPowerStats, cpuSubsystem, 0));
    configuredRequests.push_back(std::make_unique<GlobalPowerMemory>(
        server, 0, 255, "power", "Memory", globalPowerStats, memorySubsystem,
        0));
    configuredRequests.push_back(std::make_unique<GlobalPowerHwProtection>(
        server, 0, 255, "power", "HwProtection", globalPowerStats, hwProtection,
        0));

    performReadings();

    io.run();

    return 0;
}
