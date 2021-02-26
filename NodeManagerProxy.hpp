/* Copyright 2018, 2021 Intel
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
constexpr const char *meSoftwareObjPath = "/xyz/openbmc_project/software/me";
constexpr const char *softwareVerIntf = "xyz.openbmc_project.Software.Version";
constexpr const char *softwareActivationIntf =
    "xyz.openbmc_project.Software.Activation";

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

/**
 * @brief Get Node Manager Policy defines
 */
constexpr uint8_t ipmiGetNmPolicyNetFn = 0x2E;
constexpr uint8_t ipmiGetNmPolicyLun = 0;
constexpr uint8_t ipmiGetNmPolicyCmd = 0xC2;

/**
 * @brief Get Node Manager Capabilites defines
 */
constexpr uint8_t ipmiGetNmCapabilitesNetFn = 0x2E;
constexpr uint8_t ipmiGetNmCapabilitesLun = 0;
constexpr uint8_t ipmiGetNmCapabilitesCmd = 0xC9;

// Mode
constexpr uint8_t globalPowerStats = 0x1;
constexpr uint8_t globalInletTempStats = 0x2;
constexpr uint8_t globalThrottlingStats = 0x3;
constexpr uint8_t globalVolAirflowStats = 0x4;
constexpr uint8_t globalOutletAirflowTempStats = 0x5;
constexpr uint8_t globalChassisPowerStats = 0x6;
constexpr uint8_t policyPowerStats = 0x11;
constexpr uint8_t globalHostUnhandleReqStats = 0x1B;
constexpr uint8_t globalHostResponseTimeStats = 0x1C;
constexpr uint8_t globalHostCommFailureStats = 0x1F;

// Domain Id
constexpr uint8_t entirePlatform = 0x0;
constexpr uint8_t cpuSubsystem = 0x1;
constexpr uint8_t memorySubsystem = 0x2;
constexpr uint8_t hwProtection = 0x3;
constexpr uint8_t highPowerIOsubsystem = 0x4;
constexpr uint8_t dcTotal = 0x5;

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
    uint8_t sendAlert : 1, shutdownSystem : 1, reservedByte7 : 6;
    int16_t limit;
    uint32_t correctionTime;
    uint16_t triggerLimit;
    uint16_t statsPeriod;
} __attribute__((packed)) nmIpmiSetNmPolicyReq;
static_assert(sizeof(nmIpmiSetNmPolicyReq) == 17);

/**
 * @brief Set Node Manager Policy response format
 */
typedef struct
{
    ipmiIana iana;
} __attribute__((packed)) nmIpmiSetNmPolicyResp;
static_assert(sizeof(nmIpmiSetNmPolicyResp) == 3);

/**
 * @brief Get Node Manager Policy request format
 */
typedef struct
{
    ipmiIana iana;
    uint8_t domainId : 4, reserved0 : 4;
    uint8_t policyId;
} __attribute__((packed)) nmIpmiGetNmPolicyReq;
static_assert(sizeof(nmIpmiGetNmPolicyReq) == 5);

/**
 * @brief Get Node Manager Policy response format
 */
typedef struct
{
    ipmiIana iana;
    uint8_t domainId : 4, policyEnabled : 1, domainEnabled : 1,
        globalEnabled : 1, external : 1;
    uint8_t triggerType : 4, policyType : 1, cpuPowerCorrection : 2,
        storageOption : 1;
    uint8_t sendAlert : 1, shutdownSystem : 1, reserved0 : 6;
    int16_t limit;
    uint32_t correctionTime;
    uint16_t triggerLimit;
    uint16_t statsPeriod;
} __attribute__((packed)) nmIpmiGetNmPolicyResp;
static_assert(sizeof(nmIpmiGetNmPolicyResp) == 16);

/**
 * @brief Get Node Manager Capabilites request format
 */
typedef struct
{
    ipmiIana iana;
    uint8_t domainId : 4, reserved0 : 4;
    uint8_t policyTriggerType : 4, policyType : 3, reserved1 : 1;
} __attribute__((packed)) nmIpmiGetNmCapabilitesReq;
static_assert(sizeof(nmIpmiGetNmCapabilitesReq) == 5);

/**
 * @brief Get Node Manager Capabilites response format
 */
typedef struct
{
    ipmiIana iana;
    uint8_t maxConcurentSettings;
    uint16_t maxLimit;
    uint16_t minLimit;
    uint32_t minCorrectionTime;
    uint32_t maxCorrectionTime;
    uint16_t minStatsReportingPeriod;
    uint16_t maxStatsReportingPeriod;
    uint8_t domainId : 4, reserved : 4;
} __attribute__((packed)) nmIpmiGetNmCapabilitesResp;
static_assert(sizeof(nmIpmiGetNmCapabilitesResp) == 21);

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
 * @brief ME FW version class declaration
 */
class GetMeVer
{
  public:
    GetMeVer(std::shared_ptr<sdbusplus::asio::connection> conn,
             sdbusplus::asio::object_server &server) :
        conn(conn)
    {
        iface = server.add_interface(meSoftwareObjPath, softwareVerIntf);

        iface->register_property(
            "Purpose",
            std::string(
                "xyz.openbmc_project.Software.Version.VersionPurpose.ME"));

        iface->register_property(
            "Version", std::string(""),
            [](const std::string &newVal, std::string &oldVal) { return 1; },
            [this](const std::string &val) { return getDevId(); });

        iface->initialize();

        /* Activation interface represents activation state for an associated
         * xyz.openbmc_project.Software.Version. since its are already active,
         * set "activation" to Active and "RequestedActivation" to None.
         */
        auto activationIface =
            server.add_interface(meSoftwareObjPath, softwareActivationIntf);

        activationIface->register_property(
            "Activation",
            std::string(
                "xyz.openbmc_project.Software.Activation.Activations.Active"));
        activationIface->register_property(
            "RequestedActivation",
            std::string("xyz.openbmc_project.Software.Activation."
                        "RequestedActivations.None"));

        activationIface->initialize();

        /* For all Active images, functional endpoints must be added. */
        std::vector<Association> associations;
        associations.push_back(
            Association("functional", "software_version", meSoftwareObjPath));
        auto associationsIface = server.add_interface(
            "/xyz/openbmc_project/software", associationInterface);
        associationsIface->register_property("Associations", associations);
        associationsIface->initialize();
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
        iface->register_property("Value", static_cast<double>(0));
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
            "Value", static_cast<double>(getNmStatistics->data.stats.cur));
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

/**
 * @brief DBus exception thrown in case any internal error
 */
struct InternalFailure final : public sdbusplus::exception_t
{
    static constexpr auto errName =
        "xyz.openbmc_project.Common.Error.InternalFailure";
    static constexpr auto errDesc = "The operation failed internally.";
    static constexpr auto errWhat =
        "xyz.openbmc_project.Common.Error.InternalFailure: The operation "
        "failed internally.";

    const char *name() const noexcept override
    {
        return errName;
    };
    const char *description() const noexcept override
    {
        return errDesc;
    };
    const char *what() const noexcept override
    {
        return errWhat;
    };
};

/**
 * @brief DBus exception thrown when got non-success IPMI completion code
 */
struct NonSuccessCompletionCode final : public sdbusplus::exception_t
{
    static constexpr auto errName =
        "xyz.openbmc_project.Common.Error.NonSuccessCompletionCode";
    static constexpr auto errDesc =
        "The operation failed. Got non-success completion code.";
    static constexpr auto errWhat =
        "xyz.openbmc_project.Common.Error.NonSuccessCompletionCode: The "
        "operation failed. Got non-success completion code.";

    const char *name() const noexcept override
    {
        return errName;
    };
    const char *description() const noexcept override
    {
        return errDesc;
    };
    const char *what() const noexcept override
    {
        return errWhat;
    };
};

/**
 * @brief DBus exception thrown when IPMI response size does not match expected
 * size
 */
struct WrongResponseSize final : public sdbusplus::exception_t
{
    static constexpr auto errName =
        "xyz.openbmc_project.NodeManager.Error.WrongResponseSize";
    static constexpr auto errDesc =
        "IPMB response size does not match expected value";
    static constexpr auto errWhat =
        "IPMB response size does not match expected value";
    const char *name() const noexcept override
    {
        return errName;
    };
    const char *description() const noexcept override
    {
        return errDesc;
    };
    const char *what() const noexcept override
    {
        return errWhat;
    };
};

/**
 * @brief Policy parameters structure
 */
struct PolicyParams
{
    uint32_t correctionInMs;
    uint16_t limit;
    uint16_t statReportingPeriod;
    int policyStorage;
    int powerCorrectionType;
    int limitException;
    std::vector<std::map<std::string,
                         std::variant<std::vector<std::string>, std::string>>>
        suspendPeriods;
    std::vector<std::map<std::string, uint32_t>> thresholds;
    uint8_t componentId;
    uint16_t triggerLimit;
    uint8_t triggerType;
};

/**
 * @brief Statistics type provided on DBus as return type for GetStatistics
 */
using StatValuesMap = std::map<std::string, std::variant<double, uint32_t>>;

/**
 * @brief Node Manager Statistics DBus interface
 * The following methods shall be supported:
 * * GetStatistics
 * * * return StatValuesMap - statistics collection
 */
constexpr const char *nmStatisitcsIf =
    "xyz.openbmc_project.NodeManager.Statistics";

/**
 * @brief Node Manager Policy Attributes DBus interface
 * The following properties shall be supported:
 * * uint16_t Limit
 */
constexpr const char *nmPolicyAttributesIf =
    "xyz.openbmc_project.NodeManager.PolicyAttributes";

/**
 * @brief Generic function used to send and receive IPMI message
 *
 * @tparam Req - IPMI request type
 * @tparam Resp - IPMI response type
 * @param conn  - DBus connection
 * @param netFnReq - IPMI Net Function
 * @param lunReq - IPMI LUN
 * @param cmdReq - IPMI command
 * @param req - IPMI request
 * @param resp - IPMI response
 */
template <typename Req, typename Resp>
void ipmiSendReceive(std::shared_ptr<sdbusplus::asio::connection> conn,
                     uint8_t netFnReq, uint8_t lunReq, uint8_t cmdReq,
                     const Req &req, Resp &resp)
{
    IpmbDbusRspType ipmbResponse;
    std::vector<uint8_t> dataToSend(reinterpret_cast<const uint8_t *>(&req),
                                    reinterpret_cast<const uint8_t *>(&req) +
                                        sizeof(req));

    int sendStatus = ipmbSendRequest(*conn, ipmbResponse, dataToSend, netFnReq,
                                     lunReq, cmdReq);
    if (sendStatus != 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "dbus error while sending IPMB request ",
            phosphor::logging::entry("%d", sendStatus));
        throw InternalFailure();
    }

    const auto &[status, netfnResp, lunResp, cmdResp, cc, dataReceived] =
        ipmbResponse;
    if (status)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "transport error while sending IPMB request ",
            phosphor::logging::entry("%d", status));
        throw InternalFailure();
    }

    if (cc != 0x00)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "error while sending IPMB request, wrong cc: ",
            phosphor::logging::entry("%d", cc));
        throw NonSuccessCompletionCode();
    }

    if (dataReceived.size() != sizeof(resp))
    {
        phosphor::logging::log<phosphor::logging::level::WARNING>(
            "wrong response size");
        throw WrongResponseSize();
    }

    std::copy(dataReceived.begin(), dataReceived.end(),
              reinterpret_cast<uint8_t *>(&resp));
}

/**
 * @brief Node Manager Policy
 */
class Policy
{
  public:
    Policy() = delete;
    Policy(const Policy &) = delete;
    Policy &operator=(const Policy &) = delete;
    Policy(Policy &&) = delete;
    Policy &operator=(Policy &&) = delete;

    Policy(std::shared_ptr<sdbusplus::asio::connection> connArg,
           sdbusplus::asio::object_server &server, std::string &domainDbusPath,
           uint8_t domainIdArg, uint8_t idArg) :
        conn(connArg),
        dbusPath(domainDbusPath + "/Policy/" + std::to_string(idArg)),
        domainId(domainIdArg), id(idArg)
    {
        createAttributesInterface(server);
        createStatisticsInterface(server);
    }

    uint8_t setOrUpdatePolicy(PolicyParams &params)
    {
        nmIpmiSetNmPolicyReq req = {0};

        ipmiSetIntelIanaNumber(req.iana);
        req.domainId = domainId;
        req.policyEnabled = 0x1; // Enable policy during creation
        req.policyId = id;
        req.triggerType = params.triggerType;
        req.configurationAction = 0x1; // Create or modify policy
        req.cpuPowerCorrection = params.powerCorrectionType;
        req.storageOption = params.policyStorage;
        uint8_t sendAlert, shutdownSystem;
        parseLimitException(params.limitException, sendAlert, shutdownSystem);
        req.sendAlert = sendAlert;
        req.shutdownSystem = shutdownSystem;
        req.limit = params.limit;
        req.correctionTime = params.correctionInMs;
        req.triggerLimit = params.triggerLimit;
        req.statsPeriod = params.statReportingPeriod;

        setPolicyIpmi(req);

        limit = params.limit;
        failureAction = params.limitException;
        correctionTime = params.correctionInMs;

        return id;
    }

    uint8_t getId() const
    {
        return id;
    }

  private:
    std::shared_ptr<sdbusplus::asio::connection> conn;
    std::string dbusPath;
    uint8_t domainId;
    uint8_t id;
    std::shared_ptr<sdbusplus::asio::dbus_interface> attributesIf;
    std::shared_ptr<sdbusplus::asio::dbus_interface> statisticsIf;
    uint16_t limit{0};
    int failureAction{0};
    uint32_t correctionTime{0};

    void createAttributesInterface(sdbusplus::asio::object_server &server)
    {
        attributesIf = server.add_interface(dbusPath, nmPolicyAttributesIf);
        attributesIf->register_property_rw(
            "Limit", uint16_t{0}, sdbusplus::vtable::property_::emits_change,
            [this](const auto &newPropertyValue, const auto &) {
                updatePolicyLimit(newPropertyValue);
                return 1;
            },
            [this](const auto &) { return limit; });
        attributesIf->register_property_rw(
            "LimitException", int{0},
            sdbusplus::vtable::property_::emits_change,
            [this](const auto &newPropertyValue, const auto &) {
                updatePolicyLimitException(newPropertyValue);
                return 1;
            },
            [this](const auto &) { return failureAction; });
        attributesIf->register_property_rw(
            "CorrectionInMs", uint32_t{0},
            sdbusplus::vtable::property_::emits_change,
            [this](const auto &newPropertyValue, const auto &) {
                updatePolicyCorrectionTime(newPropertyValue);
                return 1;
            },
            [this](const auto &) { return correctionTime; });
        attributesIf->initialize();
    }

    void createStatisticsInterface(sdbusplus::asio::object_server &server)
    {
        statisticsIf = server.add_interface(dbusPath, nmStatisitcsIf);
        statisticsIf->register_method("GetStatistics", [this]() {
            std::map<std::string, StatValuesMap> stats{
                {"Power", getPowerStatistics()}};
            return stats;
        });
        statisticsIf->initialize();
    }

    StatValuesMap getPowerStatistics()
    {
        nmIpmiGetNmStatisticsReq req = {0};
        nmIpmiGetNmStatisticsResp resp = {0};

        ipmiSetIntelIanaNumber(req.iana);
        req.mode = policyPowerStats;
        req.domainId = domainId;
        req.statsSide = 0;
        req.perComponent = 0; // Accumulated data from whole domain
        req.policyId = id;

        ipmiSendReceive<nmIpmiGetNmStatisticsReq, nmIpmiGetNmStatisticsResp>(
            conn, ipmiGetNmStatisticsNetFn, ipmiGetNmStatisticsLun,
            ipmiGetNmStatisticsCmd, req, resp);

        StatValuesMap stats{
            {"Current", static_cast<double>(resp.data.stats.cur)},
            {"Max", static_cast<double>(resp.data.stats.max)},
            {"Min", static_cast<double>(resp.data.stats.min)},
            {"Average", static_cast<double>(resp.data.stats.avg)},
            {"StatisticsReportingPeriod",
             static_cast<double>(resp.statsReportPeriod)}};

        return stats;
    }

    void setPolicyIpmi(const nmIpmiSetNmPolicyReq &req)
    {
        nmIpmiSetNmPolicyResp resp = {0};
        ipmiSendReceive<nmIpmiSetNmPolicyReq, nmIpmiSetNmPolicyResp>(
            conn, ipmiSetNmPolicyNetFn, ipmiSetNmPolicyLun, ipmiSetNmPolicyCmd,
            req, resp);
    }

    void getPolicyIpmi(const nmIpmiGetNmPolicyReq &req,
                       nmIpmiGetNmPolicyResp &resp)
    {
        ipmiSendReceive<nmIpmiGetNmPolicyReq, nmIpmiGetNmPolicyResp>(
            conn, ipmiGetNmPolicyNetFn, ipmiGetNmPolicyLun, ipmiGetNmPolicyCmd,
            req, resp);
    }

    void updatePolicy(std::function<void(nmIpmiSetNmPolicyReq &)> callback)
    {
        nmIpmiGetNmPolicyReq getPolicyReq = {0};
        nmIpmiGetNmPolicyResp getPolicyResp = {0};
        nmIpmiSetNmPolicyReq setPolicyReq = {0};

        ipmiSetIntelIanaNumber(getPolicyReq.iana);
        getPolicyReq.domainId = domainId;
        getPolicyReq.policyId = id;
        getPolicyIpmi(getPolicyReq, getPolicyResp);

        ipmiSetIntelIanaNumber(setPolicyReq.iana);
        setPolicyReq.domainId = getPolicyResp.domainId;
        setPolicyReq.policyEnabled = 0x1; // Enable policy during creation
        setPolicyReq.policyId = id;
        setPolicyReq.triggerType = getPolicyResp.triggerType;
        setPolicyReq.configurationAction = 0x1; // Create or modify policy
        setPolicyReq.cpuPowerCorrection = getPolicyResp.cpuPowerCorrection;
        setPolicyReq.storageOption = getPolicyResp.storageOption;
        setPolicyReq.sendAlert = getPolicyResp.sendAlert;
        setPolicyReq.shutdownSystem = getPolicyResp.shutdownSystem;
        setPolicyReq.limit = getPolicyResp.limit;
        setPolicyReq.correctionTime = getPolicyResp.correctionTime;
        setPolicyReq.triggerLimit = getPolicyResp.triggerLimit;
        setPolicyReq.statsPeriod = getPolicyResp.statsPeriod;

        callback(setPolicyReq);
        setPolicyIpmi(setPolicyReq);
    }

    void updatePolicyLimit(uint16_t newLimit)
    {
        updatePolicy([newLimit](nmIpmiSetNmPolicyReq &setPolicyReq) {
            setPolicyReq.limit = newLimit;
        });
        limit = newLimit;
    }

    void updatePolicyLimitException(uint8_t newLimitException)
    {
        updatePolicy([newLimitException,
                      this](nmIpmiSetNmPolicyReq &setPolicyReq) {
            uint8_t sendAlert, shutdownSystem;
            parseLimitException(newLimitException, sendAlert, shutdownSystem);
            setPolicyReq.sendAlert = sendAlert;
            setPolicyReq.shutdownSystem = shutdownSystem;
        });
        failureAction = newLimitException;
    }

    void updatePolicyCorrectionTime(uint32_t newCorrectionTime)
    {
        updatePolicy([newCorrectionTime](nmIpmiSetNmPolicyReq &setPolicyReq) {
            setPolicyReq.correctionTime = newCorrectionTime;
        });
        correctionTime = newCorrectionTime;
    }

    /**
     * @brief Maps limitException into sendAlert and shutdown bits.
     *  Possible limitException values are:
     *  noAction = 0,
     *  powerOff = 1,
     *  logEvent = 2,
     *  logEventAndPowerOff = 3
     */
    void parseLimitException(const int &limitException, uint8_t &sendAlert,
                             uint8_t &shutdown)
    {
        if (limitException == 0)
        {
            sendAlert = 0;
            shutdown = 0;
        }
        else if (limitException == 1)
        {
            sendAlert = 0;
            shutdown = 1;
        }
        else if (limitException == 2)
        {
            sendAlert = 1;
            shutdown = 0;
        }
        else if (limitException == 3)
        {
            sendAlert = 1;
            shutdown = 1;
        }
    }
};

/**
 * @brief Tuple describing Policy parameters
 */
using PolicyParamsTuple = std::tuple<
    uint32_t, // 0 - correctionInMs
    uint16_t, // 1 - limit
    uint16_t, // 2 - statReportingPeriod
    int,      // 3 - policyStorage
    int,      // 4 - powerCorrectionType
    int,      // 5 - limitException
    std::vector<
        std::map<std::string, std::variant<std::vector<std::string>,
                                           std::string>>>, // 6 - suspendPeriods
    std::vector<std::map<std::string, uint32_t>>,          // 7 - thresholds
    uint8_t,                                               // 8 - componentId
    uint16_t,                                              // 9- triggerLimit
    uint8_t                                                // 10- triggerType
    >;

/**
 * @brief Helper converting tuple to structure
 *
 * @tparam T
 * @tparam Tuple
 * @tparam Seq
 * @param tuple
 * @return T
 */
template <class T, class Tuple, size_t... Seq>
T makeFromTuple(Tuple &&tuple, std::index_sequence<Seq...>)
{
    return T{std::get<Seq>(std::forward<Tuple>(tuple))...};
}

/**
 * @brief Helper converting structure to tuple
 *
 * @tparam T
 * @tparam Tuple
 * @param tuple
 * @return T
 */
template <class T, class Tuple> T makeFromTuple(Tuple &&tuple)
{
    return makeFromTuple<T>(
        std::forward<Tuple>(tuple),
        std::make_index_sequence<
            std::tuple_size_v<std::remove_reference_t<Tuple>>>());
}

/**
 * @brief Node Manager Domain Capabilites DBus interface
 * The following properties shall be supported:
 * * double Min
 * * double Max
 */
constexpr const char *nmDomainCapabilitesIf =
    "xyz.openbmc_project.NodeManager.Capabilities";

/**
 * @brief Node Manager Domain Policy Manager DBus interface
 * The following method shall be supported:
 * * CreateWithId
 * * * uint8_t - Policy ID
 * * * PolicyParams - Policy Parameters
 * * * return uint8_t - Policy ID
 */
constexpr const char *nmDomainPolicyManagerIf =
    "xyz.openbmc_project.NodeManager.PolicyManager";

/**
 * @brief Node Manager Domain
 */
class Domain
{
  public:
    Domain() = delete;
    Domain(const Domain &) = delete;
    Domain &operator=(const Domain &) = delete;
    Domain(Domain &&) = delete;
    Domain &operator=(Domain &&) = delete;

    Domain(std::shared_ptr<sdbusplus::asio::connection> connArg,
           sdbusplus::asio::object_server &server, uint8_t idArg) :
        id(idArg),
        dbusPath("/xyz/openbmc_project/NodeManager/Domain/" +
                 std::to_string(idArg)),
        conn(connArg)
    {
        // SPS NM does not support DC Total so need to remap to AC Total (entire
        // platform)
        if (id == dcTotal)
        {
            id = entirePlatform;
        }
        createCapabilitesInterface(server);
        createPolicyManagerInterface(server);
        createStatisticsInterface(server);
    }

  private:
    uint8_t id;
    std::string dbusPath;
    std::shared_ptr<sdbusplus::asio::dbus_interface> capabilitesIf;
    std::shared_ptr<sdbusplus::asio::dbus_interface> policyManagerIf;
    std::shared_ptr<sdbusplus::asio::dbus_interface> statisticsIf;
    std::shared_ptr<sdbusplus::asio::connection> conn;
    std::vector<std::unique_ptr<Policy>> policies;

    void createCapabilitesInterface(sdbusplus::asio::object_server &server)
    {
        capabilitesIf = server.add_interface(dbusPath, nmDomainCapabilitesIf);
        capabilitesIf->register_property_r(
            "Min", double{0}, sdbusplus::vtable::property_::const_,
            [this](const auto &) { return getCapabilityMin(); });
        capabilitesIf->register_property_r(
            "Max", double{std::numeric_limits<double>::max()},
            sdbusplus::vtable::property_::const_,
            [this](const auto &) { return getCapabilityMax(); });
        capabilitesIf->initialize();
    }

    void createPolicyManagerInterface(sdbusplus::asio::object_server &server)
    {
        policyManagerIf =
            server.add_interface(dbusPath, nmDomainPolicyManagerIf);
        policyManagerIf->register_method(
            "CreateWithId",
            [this, &server](uint8_t policyId, PolicyParamsTuple t) {
                auto params = makeFromTuple<PolicyParams>(t);
                return createOrUpdatePolicy(server, policyId, params);
            });
        policyManagerIf->initialize();
    }

    void createStatisticsInterface(sdbusplus::asio::object_server &server)
    {
        statisticsIf = server.add_interface(dbusPath, nmStatisitcsIf);
        statisticsIf->register_method("GetStatistics", [this]() {
            std::map<std::string, StatValuesMap> stats{
                {"Power", getPowerStatistics()}};
            return stats;
        });
        statisticsIf->initialize();
    }

    double getCapabilityMin()
    {
        uint16_t min, max;
        getCapabilites(min, max);
        return static_cast<double>(min);
    }

    double getCapabilityMax()
    {
        uint16_t min, max;
        getCapabilites(min, max);
        return static_cast<double>(max);
    }

    uint8_t createOrUpdatePolicy(sdbusplus::asio::object_server &server,
                                 uint8_t policyId, PolicyParams &policyParams)
    {
        for (auto &policy : policies)
        {
            if (policy->getId() == policyId)
            {
                return policy->setOrUpdatePolicy(policyParams);
            }
        }
        auto policyTmp =
            std::make_unique<Policy>(conn, server, dbusPath, id, policyId);
        uint8_t policyIdTmp = policyTmp->setOrUpdatePolicy(policyParams);
        policies.emplace_back(std::move(policyTmp));
        return policyIdTmp;
    }

    void getCapabilites(uint16_t &minLimit, uint16_t &maxLimit)
    {
        nmIpmiGetNmCapabilitesReq req = {0};
        nmIpmiGetNmCapabilitesResp resp = {0};

        ipmiSetIntelIanaNumber(req.iana);
        req.domainId = id;
        req.policyTriggerType = 0; // No Policy Trigger
        req.policyType = 1;        // Power Control Policy

        ipmiSendReceive<nmIpmiGetNmCapabilitesReq, nmIpmiGetNmCapabilitesResp>(
            conn, ipmiGetNmCapabilitesNetFn, ipmiGetNmCapabilitesLun,
            ipmiGetNmCapabilitesCmd, req, resp);

        minLimit = resp.minLimit;
        maxLimit = resp.maxLimit;
    }

    StatValuesMap getPowerStatistics()
    {
        nmIpmiGetNmStatisticsReq req = {0};
        nmIpmiGetNmStatisticsResp resp = {0};

        ipmiSetIntelIanaNumber(req.iana);
        req.mode = globalPowerStats;
        req.domainId = id;
        req.statsSide = 0;
        req.perComponent = 0; // Accumulated data from whole domain
        req.policyId = 0;

        ipmiSendReceive<nmIpmiGetNmStatisticsReq, nmIpmiGetNmStatisticsResp>(
            conn, ipmiGetNmStatisticsNetFn, ipmiGetNmStatisticsLun,
            ipmiGetNmStatisticsCmd, req, resp);

        StatValuesMap stats{
            {"Current", static_cast<double>(resp.data.stats.cur)},
            {"Max", static_cast<double>(resp.data.stats.max)},
            {"Min", static_cast<double>(resp.data.stats.min)},
            {"Average", static_cast<double>(resp.data.stats.avg)},
            {"StatisticsReportingPeriod",
             static_cast<double>(resp.statsReportPeriod)}};

        return stats;
    }
};

#endif
