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

#ifndef NODEMANAGERD_HPP
#define NODEMANAGERD_HPP

/**
 * @brief Dbus
 */
constexpr const char *nmdBus = "xyz.openbmc_project.Ipmi.NodeManagerd";
constexpr const char *nmdObj = "/xyz/openbmc_project/Ipmi/NodeManagerd";
constexpr const char *ipmbIntf = "org.openbmc.Ipmb";

constexpr const char *nmdIntf = "org.openbmc.Readings";
constexpr const char *propObj = "/xyz/openbmc_project/Ipmi/NodeManagerd/";

/**
 * @brief NMd defines
 */
constexpr uint32_t readingsInterval = 10; // seconds
constexpr uint32_t framesInterval =
    100; // msec - number of frames per reading * framesInterval should be 2x
         // less than readingsInterval

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
    virtual void sendRequest(uint8_t &netFn, uint8_t &lun, uint8_t &cmd,
                             std::vector<uint8_t> &dataToSend) = 0;

    // virtual function for handling responses from Ipmb
    virtual void handleResponse(std::vector<uint8_t> &dataReceived) = 0;

    virtual ~Request(){};

  protected:
    Request(){};

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;
};

class getNmStatistics : public Request
{
  public:
    uint8_t mode;
    uint8_t domainId;
    uint8_t policyId;

    getNmStatistics(sdbusplus::asio::object_server &server, std::string name,
                    uint8_t mode, uint8_t domainId, uint8_t policyId) :
        mode(mode),
        domainId(domainId), policyId(policyId)
    {
        iface = server.add_interface(propObj + name, nmdIntf);

        iface->register_property("Current", static_cast<uint16_t>(0));
        iface->register_property("Minimum", static_cast<uint16_t>(0));
        iface->register_property("Maximum", static_cast<uint16_t>(0));
        iface->register_property("Average", static_cast<uint16_t>(0));
        iface->register_property("TimeStamp", static_cast<uint32_t>(0));
        iface->register_property("StatsReportPeriod", static_cast<uint32_t>(0));
        iface->register_property("DomainId", static_cast<uint8_t>(0));
        iface->register_property("PolicyGlobalState", static_cast<uint8_t>(0));
        iface->register_property("PolicyOperationalState",
                                 static_cast<uint8_t>(0));
        iface->register_property("MeasurmentsState", static_cast<uint8_t>(0));
        iface->register_property("PolicyActivationState",
                                 static_cast<uint8_t>(0));

        iface->initialize();
    }

    void handleResponse(std::vector<uint8_t> &dataReceived)
    {
        if (dataReceived.size() != sizeof(nmIpmiGetNmStatisticsResp))
        {
            phosphor::logging::log<phosphor::logging::level::WARNING>(
                "handleResponse: response size does not match expected value");
            return;
        }

        auto getNmStatistics =
            reinterpret_cast<nmIpmiGetNmStatisticsResp *>(dataReceived.data());

        iface->set_property("Current", getNmStatistics->data.stats.cur);
        iface->set_property("Minimum", getNmStatistics->data.stats.min);
        iface->set_property("Maximum", getNmStatistics->data.stats.max);
        iface->set_property("Average", getNmStatistics->data.stats.avg);
        iface->set_property("TimeStamp", getNmStatistics->timeStamp);
        iface->set_property("StatsReportPeriod",
                            getNmStatistics->statsReportPeriod);
        iface->set_property("DomainId", getNmStatistics->domainId);
        iface->set_property("PolicyGlobalState",
                            getNmStatistics->policyGlobalState);
        iface->set_property("PolicyOperationalState",
                            getNmStatistics->policyOperationalState);
        iface->set_property("MeasurmentsState",
                            getNmStatistics->measurmentsState);
        iface->set_property("PolicyActivationState",
                            getNmStatistics->policyActivationState);
    }

    void sendRequest(uint8_t &netFn, uint8_t &lun, uint8_t &cmd,
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

class GlobalPowerIOsubsystem : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

/**
 * @brief Global inlet temperature statistics [Celsius]
 */
class GlobalInletTempPlatform : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalInletTempCpu : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalInletTempMemory : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalInletTempHwProtection : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalInletTempIOsubsystem : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

/**
 * @brief Global throttling statistics [%]
 */
class GlobalThrottlingPlatform : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalThrottlingCpu : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalThrottlingMemory : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalThrottlingHwProtection : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalThrottlingIOsubsystem : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

/**
 * @brief Global volumetric airflow statistics [1/10 of CFM]
 */
class GlobalVolAirflowPlatform : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalVolAirflowCpu : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalVolAirflowMemory : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalVolAirflowHwProtection : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalVolAirflowIOsubsystem : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

/**
 * @brief Global outlet airflow temperature statistics [Celsius]
 */
class GlobalOutletAirflowTempPlatform : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalOutletAirflowTempCpu : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalOutletAirflowTempMemory : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalOutletAirflowTempHwProtection : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalOutletAirflowTempIOsubsystem : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

/**
 * @brief Global outlet airflow temperature statistics [Celsius]
 */
class GlobalChassisPowerPlatform : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalChassisPowerCpu : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalChassisPowerMemory : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalChassisPowerHwProtection : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

class GlobalChassisPowerIOsubsystem : public getNmStatistics
{
  public:
    using getNmStatistics::getNmStatistics;
};

#endif
