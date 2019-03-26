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

#include <phosphor-logging/log.hpp>
#include <sdbusplus/asio/object_server.hpp>

#ifndef NODEMANAGERPROXY_HPP
#define NODEMANAGERPROXY_HPP

/**
 * @brief Dbus
 */
constexpr const char *nmdBus = "xyz.openbmc_project.NodeManagerProxy";
constexpr const char *nmdObj = "/xyz/openbmc_project/NodeManagerProxy";
constexpr const char *propObj = "/xyz/openbmc_project/sensors/";
constexpr const char *nmdIntf = "xyz.openbmc_project.Sensor.Value";

constexpr const char *ipmbBus = "xyz.openbmc_project.Ipmi.Channel.Ipmb";
constexpr const char *ipmbObj = "/xyz/openbmc_project/Ipmi/Channel/Ipmb";
constexpr const char *ipmbIntf = "org.openbmc.Ipmb";

constexpr const char *sensorConfPath =
    "xyz.openbmc_project.Configuration.NMSensor";
constexpr const char *sensorName = "Node_Manager_Sensor";
/**
 * @brief NMd defines
 */
constexpr uint32_t readingsInterval = 10; // seconds
constexpr uint32_t framesInterval =
    100; // msec - number of frames per reading * framesInterval should be 2x
         // less than readingsInterval

/**
 * @brief Ipmb defines
 */
constexpr uint8_t ipmbMeChannelNum = 1;

/**
 * @brief Ipmi defines
 */
constexpr uint32_t ipmiIanaIntel = 0x157;

typedef struct
{
    uint8_t b0;
    uint8_t b1;
    uint8_t b2;
} __attribute__((packed)) ipmiIana;
static_assert(sizeof(ipmiIana) == 3);

constexpr void ipmiSetIntelIanaNumber(ipmiIana &iana)
{
    iana.b0 = static_cast<uint8_t>(ipmiIanaIntel & 0xFF);
    iana.b1 = static_cast<uint8_t>((ipmiIanaIntel >> 8) & 0xFF);
    iana.b2 = static_cast<uint8_t>((ipmiIanaIntel >> 16) & 0xFF);
}

/**
 * @brief Get Node Manager Statistics defines
 */
constexpr uint8_t ipmiGetNmStatisticsNetFn = 0x2E;
constexpr uint8_t ipmiGetNmStatisticsLun = 0;
constexpr uint8_t ipmiGetNmStatisticsCmd = 0xC8;

// Mode
constexpr uint8_t globalPowerStats = 0x1;
constexpr uint8_t globalInletTempStats = 0x2;
constexpr uint8_t globalThrottlingStats = 0x3;
constexpr uint8_t globalVolAirflowStats = 0x4;
constexpr uint8_t globalOutletAirflowTempStats = 0x5;
constexpr uint8_t globalChassisPowerStats = 0x6;
constexpr uint8_t globalHostUnhandleReqStats = 0x1B;
constexpr uint8_t globalHostResponseTimeStats = 0x1C;
constexpr uint8_t globalHostCommFailureStats = 0x1F;

// Domain Id
constexpr uint8_t entirePlatform = 0x0;
constexpr uint8_t cpuSubsystem = 0x1;
constexpr uint8_t memorySubsystem = 0x2;
constexpr uint8_t hwProtection = 0x3;
constexpr uint8_t highPowerIOsubsystem = 0x4;

/**
 * @brief Get Node Manager Statistics request format
 */
typedef struct
{
    ipmiIana iana;
    uint8_t mode : 5, reserved3B : 3;

    uint8_t domainId : 4, statsSide : 1, reserved : 2, perComponent : 1;
    union
    {
        uint8_t policyId;
        uint8_t componentId;
    };
} __attribute__((packed)) nmIpmiGetNmStatisticsReq;
static_assert(sizeof(nmIpmiGetNmStatisticsReq) == 6);

/**
 * @brief Get Node Manager Statistics response format
 */
typedef struct
{
    ipmiIana iana;
    union
    {
        struct
        {
            uint16_t cur;
            uint16_t min;
            uint16_t max;
            uint16_t avg;
        } stats;
        uint64_t energyAccumulator;
    } data;
    uint32_t timeStamp;
    uint32_t statsReportPeriod;
    uint8_t domainId : 4, policyGlobalState : 1, policyOperationalState : 1,
        measurmentsState : 1, policyActivationState : 1;
} __attribute__((packed)) nmIpmiGetNmStatisticsResp;
static_assert(sizeof(nmIpmiGetNmStatisticsResp) == 20);

/**
 * @brief Request class declaration
 */
class Request
{
  public:
    // virtual function for sending requests to Ipmb
    virtual void prepareRequest(uint8_t &netFn, uint8_t &lun, uint8_t &cmd,
                                std::vector<uint8_t> &dataToSend) = 0;

    // virtual function for handling responses from Ipmb
    virtual void handleResponse(const uint8_t completionCode,
                                const std::vector<uint8_t> &dataReceived) = 0;

    virtual void createAssociation(sdbusplus::asio::object_server &server,
                                   std::string &path) = 0;

    virtual ~Request(){};

  protected:
    Request(){};

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;
    std::shared_ptr<sdbusplus::asio::dbus_interface> association;
};

class getNmStatistics : public Request
{
  public:
    getNmStatistics(sdbusplus::asio::object_server &server, double minValue,
                    double maxValue, std::string type, std::string name,
                    uint8_t mode, uint8_t domainId, uint8_t policyId) :
        mode(mode),
        domainId(domainId), policyId(policyId), type(type), name(name)
    {
        iface = server.add_interface(propObj + type + '/' + name, nmdIntf);

        iface->register_property("MaxValue", static_cast<double>(maxValue));
        iface->register_property("MinValue", static_cast<double>(minValue));
        iface->register_property("Value", static_cast<uint16_t>(0));

        iface->initialize();
    }

    void createAssociation(sdbusplus::asio::object_server &server,
                           std::string &path)
    {
        if (!association)
        {
            using Association =
                std::tuple<std::string, std::string, std::string>;
            std::vector<Association> associations;
            associations.push_back(Association("inventory", "sensors", path));
            association = server.add_interface("/xyz/openbmc_project/sensors/" +
                                                   type + "/" + name,
                                               "org.openbmc.Associations");

            association->register_property("associations", associations);
            association->initialize();
        }
    }

    void handleResponse(const uint8_t completionCode,
                        const std::vector<uint8_t> &dataReceived)
    {
        if (completionCode != 0)
        {
            return;
        }

        if (dataReceived.size() != sizeof(nmIpmiGetNmStatisticsResp))
        {
            phosphor::logging::log<phosphor::logging::level::WARNING>(
                "handleResponse: response size does not match expected value");
            return;
        }

        auto getNmStatistics =
            reinterpret_cast<const nmIpmiGetNmStatisticsResp *>(
                dataReceived.data());

        iface->set_property(
            "Value", static_cast<uint16_t>(getNmStatistics->data.stats.cur));
    }

    void prepareRequest(uint8_t &netFn, uint8_t &lun, uint8_t &cmd,
                        std::vector<uint8_t> &dataToSend)
    {
        dataToSend.resize(sizeof(nmIpmiGetNmStatisticsReq));

        auto nmGetStatistics =
            reinterpret_cast<nmIpmiGetNmStatisticsReq *>(dataToSend.data());

        netFn = ipmiGetNmStatisticsNetFn;
        lun = ipmiGetNmStatisticsLun;
        cmd = ipmiGetNmStatisticsCmd;

        ipmiSetIntelIanaNumber(nmGetStatistics->iana);
        nmGetStatistics->mode = mode;
        nmGetStatistics->reserved3B = 0;
        nmGetStatistics->domainId = domainId;
        nmGetStatistics->statsSide = 0;
        nmGetStatistics->reserved = 0;
        nmGetStatistics->perComponent = 0;
        nmGetStatistics->policyId = policyId;
    }

  private:
    uint8_t mode;
    uint8_t domainId;
    uint8_t policyId;
    std::string type;
    std::string name;
};

/**
 * @brief Global power statistics [Watts]
 */
class GlobalPowerPlatform : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalPowerCpu : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalPowerMemory : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalPowerHwProtection : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

#endif
