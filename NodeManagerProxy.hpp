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

#include <boost/container/flat_set.hpp>
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
constexpr const char *nmdSensorIntf = "xyz.openbmc_project.Sensor.Value";
constexpr const char *nmdPowerCapIntf = "xyz.openbmc_project.Control.Power.Cap";
constexpr const char *nmdPowerMetricIntf =
    "xyz.openbmc_project.Power.PowerMetric";
constexpr const char *nmdMeVerIntf = "xyz.openbmc_project.Software.Version";

constexpr const char *ipmbBus = "xyz.openbmc_project.Ipmi.Channel.Ipmb";
constexpr const char *ipmbObj = "/xyz/openbmc_project/Ipmi/Channel/Ipmb";
constexpr const char *ipmbIntf = "org.openbmc.Ipmb";

constexpr const char *sensorConfPath =
    "xyz.openbmc_project.Configuration.NMSensor";
constexpr const char *sensorName = "Node_Manager_Sensor";
constexpr const char *associationInterface =
    "xyz.openbmc_project.Association.Definitions";

// this currently can be anything as it's only used to set the LED, might be
// good later to change it for redfish, but I'm not sure to what today
constexpr const char *meStatusPath = "/xyz/openbmc_project/status/me";

using Association = std::tuple<std::string, std::string, std::string>;

namespace power
{
const static constexpr char *busname = "xyz.openbmc_project.State.Host";
const static constexpr char *interface = "xyz.openbmc_project.State.Host";
const static constexpr char *path = "/xyz/openbmc_project/state/host0";
const static constexpr char *property = "CurrentHostState";
} // namespace power

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

/**
 * @brief Set Node Manager Policy defines
 */
constexpr uint8_t ipmiSetNmPolicyNetFn = 0x2E;
constexpr uint8_t ipmiSetNmPolicyLun = 0;
constexpr uint8_t ipmiSetNmPolicyCmd = 0xC1;

constexpr uint32_t ipmiSetNmPolicyLimitInWatts = 0;

/**
 * @brief Enable/Disable NM Policy Control defines
 */
constexpr uint8_t ipmiEnaDisNmPolicyCtrlNetFn = 0x2E;
constexpr uint8_t ipmiEnaDisNmPolicyCtrlLun = 0;
constexpr uint8_t ipmiEnaDisNmPolicyCtrlCmd = 0xC0;

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
 * @brief Get Device ID defines
 */
constexpr uint8_t ipmiGetDevIdNetFn = 0x6;
constexpr uint8_t ipmiGetDevIdLun = 0;
constexpr uint8_t ipmiGetDevIdCmd = 0x1;

/**
 * @brief Part of Get Device ID Command Response Payload
 */
typedef struct
{
    uint8_t fwMajorRev : 7,  // Binary encoded Major Version
        inUpgrade : 1;       // In Upgrade State
    uint8_t fwHotfixRev : 4, // BCD encoded Hotfix Version
        fwMinorRev : 4;      // BCD encoded Minor Version
} __attribute__((packed)) ipmiFwVerMajorMinor;

/**
 * @brief Part of Get Device ID Command Response Payload - Auxiliary
 * Firmware Revision Information
 */
typedef struct
{
    uint8_t nmVersion : 4, // Node Manager Version
        dcmiVersion : 4;   // DCMI Version
    uint8_t b : 4,         // BCD encoded Build Number - tens
        a : 4,             // BCD encoded Build Number - hundreds
        patch : 4,         // BCD encoded Patch Number
        c : 4;             // BCD encoded Build Number - digits
    uint8_t imageFlags;
} __attribute__((packed)) ipmiFwVerAux;

/**
 * @brief Get Device ID Command Full Response Payload
 */
typedef struct
{
    uint8_t deviceId;                 // Device ID
    uint8_t deviceRev : 4,            // Device Revision
        reserved0 : 3,                // Reserved bits
        sdrPresent : 1;               // SDR State
    ipmiFwVerMajorMinor fwMajorMinor; // Major and Minor Version
    uint8_t ipmiVersion;   // BCD encoded IPMI Version, reversed digit order
    uint8_t featureMask;   // Bitmask of supported features
    ipmiIana ianaId;       // Manufacturers ID
    uint8_t prodIdMinor;   // Product ID Minor Version
    uint8_t prodIdMajor;   // Product ID Major Version
    ipmiFwVerAux fwVerAux; // NmVersion, Build Number etc.
} __attribute__((packed)) ipmiGetDeviceIdResp;

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
 * @brief Set Node Manager Policy request format
 */
typedef struct
{
    ipmiIana iana;
    uint8_t domainId : 4, policyEnabled : 1, reservedByte4 : 3;
    uint8_t policyId;
    uint8_t triggerType : 4, configurationAction : 1, cpuPowerCorrection : 2,
        storageOption : 1;
    uint8_t failureAction : 2, reservedByte7 : 5, dcPowerDomain : 1;
    int16_t limit;
    uint32_t correctionTime;
    uint16_t triggerLimit;
    uint16_t statsPeriod;
} __attribute__((packed)) nmIpmiSetNmPolicyReq;

/**
 * @brief Enable/Disable NM Policy Control request format
 */

typedef struct
{
    ipmiIana iana;
    uint8_t flags : 3, reserved5b : 5;
    uint8_t domainId : 4, reserved4b : 4;
    uint8_t policyId;
} __attribute__((packed)) nmIpmiEnaDisNmPolicyCtrlReq;

/**
 * @brief Ipmb utils
 */
using IpmbDbusRspType =
    std::tuple<int, uint8_t, uint8_t, uint8_t, uint8_t, std::vector<uint8_t>>;

int ipmbSendRequest(sdbusplus::asio::connection &conn,
                    IpmbDbusRspType &ipmbResponse,
                    const std::vector<uint8_t> &dataToSend, uint8_t netFn,
                    uint8_t lun, uint8_t cmd)
{
    try
    {
        auto mesg =
            conn.new_method_call(ipmbBus, ipmbObj, ipmbIntf, "sendRequest");
        mesg.append(ipmbMeChannelNum, netFn, lun, cmd, dataToSend);
        auto ret = conn.call(mesg);
        ret.read(ipmbResponse);
        return 0;
    }
    catch (sdbusplus::exception::SdBusError &e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "ipmbSendRequest:, dbus call exception");
        return -1;
    }
}

/**
 * @brief PowerCap class declaration
 */
class PowerCap
{
  public:
    PowerCap(std::shared_ptr<sdbusplus::asio::connection> conn,
             sdbusplus::asio::object_server &server) :
        conn(conn)
    {
        iface = server.add_interface(
            "/xyz/openbmc_project/control/host0/power_cap", nmdPowerCapIntf);
        iface->register_property(
            "PowerCap", ipmiSetNmPolicyLimitInWatts,
            [this](const uint32_t &newVal, uint32_t &oldVal) {
                int cc = setPolicy(newVal);
                if (cc == 0)
                {
                    oldVal = newVal;
                }
                return 1;
            });
        iface->register_property("PowerCapEnable", false,
                                 [this](const bool &newVal, bool &oldVal) {
                                     int cc = enablePolicy(newVal);
                                     if (cc == 0)
                                     {
                                         oldVal = newVal;
                                     }
                                     return 1;
                                 });
        iface->initialize();
    }

    int setPolicy(uint32_t limitInWatts)
    {
        // prepare data to be sent
        std::vector<uint8_t> dataToSend;
        dataToSend.resize(sizeof(nmIpmiSetNmPolicyReq));

        auto nmSetPolicy =
            reinterpret_cast<nmIpmiSetNmPolicyReq *>(dataToSend.data());

        ipmiSetIntelIanaNumber(nmSetPolicy->iana);
        nmSetPolicy->domainId = 0x0;            // platform domain
        nmSetPolicy->policyEnabled = 0x0;       // enable policy during creation
        nmSetPolicy->reservedByte4 = 0x0;       // reserved
        nmSetPolicy->policyId = 0xA;            // policy number 10
        nmSetPolicy->triggerType = 0x0;         // boot time policy
        nmSetPolicy->configurationAction = 0x1; // override policy
        nmSetPolicy->cpuPowerCorrection = 0x2;  // automatic mode
        nmSetPolicy->storageOption = 0x0;       // non-volatile
        nmSetPolicy->failureAction = 0x0;       // no action
        nmSetPolicy->reservedByte7 = 0x0;       // reserved
        nmSetPolicy->dcPowerDomain = 0x0;       // primary power domain
        nmSetPolicy->limit = limitInWatts;
        nmSetPolicy->correctionTime = 0x2710; // 10000 milliseconds
        nmSetPolicy->triggerLimit = 0x0;      // ignored for platform domain
        nmSetPolicy->statsPeriod = 0xA;       // 10 seconds

        IpmbDbusRspType ipmbResponse;
        int sendStatus = ipmbSendRequest(
            *conn, ipmbResponse, dataToSend, ipmiSetNmPolicyNetFn,
            ipmiSetNmPolicyLun, ipmiSetNmPolicyCmd);

        if (sendStatus != 0)
            return sendStatus;

        const auto &[status, netfn, lun, cmd, cc, dataReceived] = ipmbResponse;

        if (status)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "setPolicy: non-zero response status ",
                phosphor::logging::entry("%d", status));
            return -1;
        }

        return cc;
    }

    int enablePolicy(bool enable)
    {
        // prepare data to be sent
        std::vector<uint8_t> dataToSend;
        dataToSend.resize(sizeof(nmIpmiEnaDisNmPolicyCtrlReq));

        auto nmEnablePolicy =
            reinterpret_cast<nmIpmiEnaDisNmPolicyCtrlReq *>(dataToSend.data());

        uint8_t flags = 0x4; // per policy disable policy for given domain
        if (enable)
        {
            flags = 0x5; // per policy enable policy for given domain
        }

        ipmiSetIntelIanaNumber(nmEnablePolicy->iana);
        nmEnablePolicy->flags = flags;
        nmEnablePolicy->reserved5b = 0x0; // reserved
        nmEnablePolicy->domainId = 0x0;   // platform domain
        nmEnablePolicy->reserved4b = 0x0; // reserved
        nmEnablePolicy->policyId = 0xA;   // policy number 10

        IpmbDbusRspType ipmbResponse;
        int sendStatus = ipmbSendRequest(
            *conn, ipmbResponse, dataToSend, ipmiEnaDisNmPolicyCtrlNetFn,
            ipmiEnaDisNmPolicyCtrlLun, ipmiEnaDisNmPolicyCtrlCmd);

        if (sendStatus != 0)
            return sendStatus;

        const auto &[status, netfn, lun, cmd, cc, dataReceived] = ipmbResponse;

        if (status)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "enablePolicy: non-zero response status ",
                phosphor::logging::entry("%d", status));
            return -1;
        }

        return cc;
    }

  private:
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;
    std::shared_ptr<sdbusplus::asio::connection> conn;
};

/**
 * @brief ME FW version class declaration
 */
class GetMeVer
{
  public:
    GetMeVer(std::shared_ptr<sdbusplus::asio::connection> conn,
             sdbusplus::asio::object_server &server) :
        conn(conn)
    {
        iface = server.add_interface("/xyz/openbmc_project/me_version",
                                     nmdMeVerIntf);

        iface->register_property(
            "Purpose",
            std::string(
                "xyz.openbmc_project.Software.Version.VersionPurpose.ME"));

        iface->register_property(
            "Version", std::string(""),
            [](const std::string &newVal, std::string &oldVal) { return 1; },
            [this](const std::string &val) { return getDevId(); });

        iface->initialize();
    }

    std::string getDevId()
    {
        constexpr const char *invalidMeVersion = "";
        std::vector<uint8_t> dataToSend;

        IpmbDbusRspType ipmbResponse;
        int sendStatus =
            ipmbSendRequest(*conn, ipmbResponse, dataToSend, ipmiGetDevIdNetFn,
                            ipmiGetDevIdLun, ipmiGetDevIdCmd);

        if (sendStatus)
        {
            return invalidMeVersion;
        }

        const auto &[status, netfn, lun, cmd, cc, dataReceived] = ipmbResponse;

        if (status)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "getDevId: ipmb non-zero response status ",
                phosphor::logging::entry("%d", status));
            return invalidMeVersion;
        }
        if (cc)
        {
            phosphor::logging::log<phosphor::logging::level::WARNING>(
                "getDevId: non-zero completion code ",
                phosphor::logging::entry("%d", cc));
            return invalidMeVersion;
        }
        if (dataReceived.size() != sizeof(ipmiGetDeviceIdResp))
        {
            phosphor::logging::log<phosphor::logging::level::WARNING>(
                "getDevId: response size does not match expected value");
            return invalidMeVersion;
        }

        auto getDevIdResp =
            reinterpret_cast<const ipmiGetDeviceIdResp *>(dataReceived.data());

        auto major = std::to_string(getDevIdResp->fwMajorMinor.fwMajorRev);
        auto minor = std::to_string(getDevIdResp->fwMajorMinor.fwMinorRev);
        auto hotfix = std::to_string(getDevIdResp->fwMajorMinor.fwHotfixRev);
        auto build = std::to_string(getDevIdResp->fwVerAux.a) +
                     std::to_string(getDevIdResp->fwVerAux.b) +
                     std::to_string(getDevIdResp->fwVerAux.c);
        auto patch = std::to_string(getDevIdResp->fwVerAux.patch);

        return major + '.' + minor + '.' + hotfix + '.' + build + '.' + patch;
    }

  private:
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;
    std::shared_ptr<sdbusplus::asio::connection> conn;
};

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
                                   const std::string &path){};

    virtual ~Request(){};

  protected:
    Request(){};

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;
    std::shared_ptr<sdbusplus::asio::dbus_interface> association;
};

/**
 * @brief PowerMetric class declaration
 */
class PowerMetric : public Request
{
  public:
    PowerMetric(sdbusplus::asio::object_server &server)
    {
        iface = server.add_interface("/xyz/openbmc_project/Power/PowerMetric",
                                     nmdPowerMetricIntf);

        iface->register_property("IntervalInMin", static_cast<uint64_t>(0));
        iface->register_property("MinConsumedWatts", static_cast<uint16_t>(0));
        iface->register_property("MaxConsumedWatts", static_cast<uint16_t>(0));
        iface->register_property("AverageConsumedWatts",
                                 static_cast<uint16_t>(0));
        iface->initialize();
    }

    void handleResponse(const uint8_t completionCode,
                        const std::vector<uint8_t> &dataReceived)
    {
        if (completionCode != 0)
            return;

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
            "IntervalInMin",
            static_cast<uint64_t>(getNmStatistics->statsReportPeriod));
        iface->set_property(
            "MinConsumedWatts",
            static_cast<uint16_t>(getNmStatistics->data.stats.min));
        iface->set_property(
            "MaxConsumedWatts",
            static_cast<uint16_t>(getNmStatistics->data.stats.max));
        iface->set_property(
            "AverageConsumedWatts",
            static_cast<uint16_t>(getNmStatistics->data.stats.avg));
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
        nmGetStatistics->mode = 1;
        nmGetStatistics->reserved3B = 0;
        nmGetStatistics->domainId = 0;
        nmGetStatistics->statsSide = 0;
        nmGetStatistics->reserved = 0;
        nmGetStatistics->perComponent = 0;
        nmGetStatistics->policyId = 0;
    }
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
        iface =
            server.add_interface(propObj + type + '/' + name, nmdSensorIntf);

        iface->register_property("MaxValue", static_cast<double>(maxValue));
        iface->register_property("MinValue", static_cast<double>(minValue));
        iface->register_property("Value", static_cast<uint16_t>(0));
        iface->register_property(
            "Unit", std::string("xyz.openbmc_project.Sensor.Value.Unit.Watts"));

        iface->initialize();
    }

    void createAssociation(sdbusplus::asio::object_server &server,
                           const std::string &path)
    {
        if (!association)
        {
            std::vector<Association> associations;
            associations.push_back(Association("chassis", "all_sensors", path));
            association = server.add_interface("/xyz/openbmc_project/sensors/" +
                                                   type + "/" + name,
                                               associationInterface);

            association->register_property("Associations", associations);
            association->initialize();
        }
    }

    void handleResponse(const uint8_t completionCode,
                        const std::vector<uint8_t> &dataReceived)
    {
        if (completionCode != 0)
            return;

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

struct HealthData
{
    HealthData(std::shared_ptr<sdbusplus::asio::dbus_interface> interface) :
        interface(interface)
    {
    }

    void set(const std::string &type, const std::string &level)
    {
        // todo: maybe look this up via mapper
        constexpr const char *globalInventoryPath =
            "/xyz/openbmc_project/CallbackManager";

        fatal.erase(type);
        critical.erase(type);
        warning.erase(type);

        if (level == "fatal")
        {
            fatal.insert(type);
        }
        else if (level == "critical")
        {
            critical.insert(type);
        }
        else if (level == "warning")
        {
            warning.insert(type);
        }
        else if (level != "ok")
        {
            throw std::invalid_argument(type);
        }

        std::vector<Association> association;
        if (fatal.size())
        {
            association.emplace_back("", "critical", globalInventoryPath);
            association.emplace_back("", "critical", meStatusPath);
        }
        else if (critical.size())
        {
            association.emplace_back("", "warning", globalInventoryPath);
            association.emplace_back("", "critical", meStatusPath);
        }
        else if (warning.size())
        {
            association.emplace_back("", "warning", globalInventoryPath);
            association.emplace_back("", "warning", meStatusPath);
        }
        interface->set_property("Associations", association);
    }

    void clear()
    {
        fatal.clear();
        critical.clear();
        warning.clear();
        interface->set_property("Associations", std::vector<Association>{});
    }

    std::shared_ptr<sdbusplus::asio::dbus_interface> interface;
    boost::container::flat_set<std::string> fatal;
    boost::container::flat_set<std::string> critical;
    boost::container::flat_set<std::string> warning;
};

#endif
