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

#include <boost/algorithm/string/predicate.hpp>
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

static std::vector<std::unique_ptr<Request>> configuredSensors;

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

            if (requestIter == configuredSensors.end())
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

        processRequests(configuredSensors.begin());

        performReadings();
    });
}

void createSensors()
{
    // NM Statistics
    // Global power statistics
    configuredSensors.push_back(std::make_unique<GlobalPowerPlatform>(
        server, 0, 2040, "power", "Platform_PWR", globalPowerStats,
        entirePlatform, 0));
    configuredSensors.push_back(std::make_unique<GlobalPowerCpu>(
        server, 0, 510, "power", "CPU_PWR", globalPowerStats, cpuSubsystem, 0));
    configuredSensors.push_back(std::make_unique<GlobalPowerMemory>(
        server, 0, 255, "power", "Memory_PWR", globalPowerStats,
        memorySubsystem, 0));
}

void createAssociations()
{
    using GetSubTreeType = std::vector<std::pair<
        std::string,
        std::vector<std::pair<std::string, std::vector<std::string>>>>>;
    constexpr int32_t scanDepth = 0;
    std::vector<std::string> confPath{sensorConfPath};

    conn->async_method_call(
        [](boost::system::error_code ec, GetSubTreeType &subtree) {
            if (ec)
            {
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    "createAssociations: error async_method_call to "
                    "ObjectMapper");
                return;
            }

            if (subtree.empty())
            {
                phosphor::logging::log<phosphor::logging::level::INFO>(
                    "createAssociations: Object Mapper returned empty subtree");
                return;
            }

            if (!boost::algorithm::ends_with(subtree.front().first,
                                             std::string(sensorName)))
            {
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    "createAssociations: could not get configuration path for "
                    "sensor name");
                return;
            }

            // Create associations for all configured sensors
            for (auto &sensor : configuredSensors)
            {
                sensor->createAssociation(server, subtree.front().first);
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory/system", scanDepth, confPath);
}

/**
 * @brief Main
 */
int main(int argc, char *argv[])
{
    conn->request_name(nmdBus);
    createSensors();
    createAssociations();
    performReadings();

    static auto match = std::make_unique<sdbusplus::bus::match::match>(
        static_cast<sdbusplus::bus::bus &>(*conn),
        "type='signal',member='PropertiesChanged',"
        "arg0namespace='" +
            std::string(sensorConfPath) + "'",
        [](sdbusplus::message::message &message) { createAssociations(); });

    io.run();
    return 0;
}
