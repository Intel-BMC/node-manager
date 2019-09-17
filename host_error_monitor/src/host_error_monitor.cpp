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
#include <peci.h>
#include <systemd/sd-journal.h>

#include <bitset>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <gpiod.hpp>
#include <iostream>
#include <sdbusplus/asio/object_server.hpp>
#include <variant>

namespace host_error_monitor
{
static boost::asio::io_service io;
static std::shared_ptr<sdbusplus::asio::connection> conn;

static bool hostOff = true;

const static constexpr int caterrTimeoutMs = 2000;
const static constexpr int errTimeoutMs = 90000;
const static constexpr int smiTimeoutMs = 90000;
const static constexpr int crashdumpTimeoutS = 300;

// Timers
// Timer for CATERR asserted
static boost::asio::steady_timer caterrAssertTimer(io);
// Timer for ERR0 asserted
static boost::asio::steady_timer err0AssertTimer(io);
// Timer for ERR1 asserted
static boost::asio::steady_timer err1AssertTimer(io);
// Timer for ERR2 asserted
static boost::asio::steady_timer err2AssertTimer(io);
// Timer for SMI asserted
static boost::asio::steady_timer smiAssertTimer(io);

// GPIO Lines and Event Descriptors
static gpiod::line caterrLine;
static boost::asio::posix::stream_descriptor caterrEvent(io);
static gpiod::line err0Line;
static boost::asio::posix::stream_descriptor err0Event(io);
static gpiod::line err1Line;
static boost::asio::posix::stream_descriptor err1Event(io);
static gpiod::line err2Line;
static boost::asio::posix::stream_descriptor err2Event(io);
static gpiod::line smiLine;
static boost::asio::posix::stream_descriptor smiEvent(io);
static gpiod::line cpu1FIVRFaultLine;
static gpiod::line cpu1ThermtripLine;
static boost::asio::posix::stream_descriptor cpu1ThermtripEvent(io);
static gpiod::line cpu2FIVRFaultLine;
static gpiod::line cpu2ThermtripLine;
static boost::asio::posix::stream_descriptor cpu2ThermtripEvent(io);
static gpiod::line cpu1VRHotLine;
static boost::asio::posix::stream_descriptor cpu1VRHotEvent(io);
static gpiod::line cpu2VRHotLine;
static boost::asio::posix::stream_descriptor cpu1MemABCDVRHotEvent(io);
static gpiod::line cpu1MemEFGHVRHotLine;
static boost::asio::posix::stream_descriptor cpu1MemEFGHVRHotEvent(io);
static gpiod::line cpu2MemABCDVRHotLine;
static boost::asio::posix::stream_descriptor cpu2VRHotEvent(io);
static gpiod::line cpu1MemABCDVRHotLine;
static boost::asio::posix::stream_descriptor cpu2MemABCDVRHotEvent(io);
static gpiod::line cpu2MemEFGHVRHotLine;
static boost::asio::posix::stream_descriptor cpu2MemEFGHVRHotEvent(io);
//----------------------------------
// PCH_BMC_THERMTRIP function related definition
//----------------------------------
static gpiod::line pchThermtripLine;
static boost::asio::posix::stream_descriptor pchThermtripEvent(io);

static void cpuIERRLog()
{
    sd_journal_send("MESSAGE=HostError: IERR", "PRIORITY=%i", LOG_INFO,
                    "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.CPUError",
                    "REDFISH_MESSAGE_ARGS=%s", "IERR", NULL);
}

static void cpuIERRLog(const int cpuNum)
{
    std::string msg = "IERR on CPU " + std::to_string(cpuNum + 1);

    sd_journal_send("MESSAGE=HostError: %s", msg.c_str(), "PRIORITY=%i",
                    LOG_INFO, "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.CPUError",
                    "REDFISH_MESSAGE_ARGS=%s", msg.c_str(), NULL);
}

static void cpuIERRLog(const int cpuNum, const std::string& type)
{
    std::string msg = type + " IERR on CPU " + std::to_string(cpuNum + 1);

    sd_journal_send("MESSAGE=HostError: %s", msg.c_str(), "PRIORITY=%i",
                    LOG_INFO, "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.CPUError",
                    "REDFISH_MESSAGE_ARGS=%s", msg.c_str(), NULL);
}

static void cpuERRXLog(const int errPin)
{
    std::string msg = "ERR" + std::to_string(errPin) + " Timeout";

    sd_journal_send("MESSAGE=HostError: %s", msg.c_str(), "PRIORITY=%i",
                    LOG_INFO, "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.CPUError",
                    "REDFISH_MESSAGE_ARGS=%s", msg.c_str(), NULL);
}

static void cpuERRXLog(const int errPin, const int cpuNum)
{
    std::string msg = "ERR" + std::to_string(errPin) + " Timeout on CPU " +
                      std::to_string(cpuNum + 1);

    sd_journal_send("MESSAGE=HostError: %s", msg.c_str(), "PRIORITY=%i",
                    LOG_INFO, "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.CPUError",
                    "REDFISH_MESSAGE_ARGS=%s", msg.c_str(), NULL);
}

static void smiTimeoutLog()
{
    sd_journal_send("MESSAGE=HostError: SMI Timeout", "PRIORITY=%i", LOG_INFO,
                    "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.CPUError",
                    "REDFISH_MESSAGE_ARGS=%s", "SMI Timeout", NULL);
}

static void cpuBootFIVRFaultLog(const int cpuNum)
{
    std::string msg = "Boot FIVR Fault on CPU " + std::to_string(cpuNum);

    sd_journal_send("MESSAGE=HostError: %s", msg.c_str(), "PRIORITY=%i",
                    LOG_INFO, "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.CPUError",
                    "REDFISH_MESSAGE_ARGS=%s", msg.c_str(), NULL);
}

static void cpuThermTripLog(const int cpuNum)
{
    std::string msg = "CPU " + std::to_string(cpuNum) + " thermal trip";

    sd_journal_send("MESSAGE=HostError: %s", msg.c_str(), "PRIORITY=%i",
                    LOG_INFO, "REDFISH_MESSAGE_ID=%s",
                    "OpenBMC.0.1.CPUThermalTrip", "REDFISH_MESSAGE_ARGS=%d",
                    cpuNum, NULL);
}

static void cpuVRHotLog(const std::string& vr)
{
    std::string msg = vr + " Voltage Regulator Overheated.";

    sd_journal_send("MESSAGE=HostError: %s", msg.c_str(), "PRIORITY=%i",
                    LOG_INFO, "REDFISH_MESSAGE_ID=%s",
                    "OpenBMC.0.1.VoltageRegulatorOverheated",
                    "REDFISH_MESSAGE_ARGS=%s", vr.c_str(), NULL);
}

static void ssbThermTripLog()
{
    sd_journal_send("MESSAGE=HostError: SSB thermal trip", "PRIORITY=%i",
                    LOG_INFO, "REDFISH_MESSAGE_ID=%s",
                    "OpenBMC.0.1.SsbThermalTrip", NULL);
}

static void initializeErrorState();
static void initializeHostState()
{
    conn->async_method_call(
        [](boost::system::error_code ec,
           const std::variant<std::string>& property) {
            if (ec)
            {
                return;
            }
            const std::string* state = std::get_if<std::string>(&property);
            if (state == nullptr)
            {
                std::cerr << "Unable to read host state value\n";
                return;
            }
            hostOff = *state == "xyz.openbmc_project.State.Host.HostState.Off";
            // If the system is on, initialize the error state
            if (!hostOff)
            {
                initializeErrorState();
            }
        },
        "xyz.openbmc_project.State.Host", "/xyz/openbmc_project/state/host0",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.State.Host", "CurrentHostState");
}

static std::shared_ptr<sdbusplus::bus::match::match> startHostStateMonitor()
{
    return std::make_shared<sdbusplus::bus::match::match>(
        *conn,
        "type='signal',interface='org.freedesktop.DBus.Properties',"
        "member='PropertiesChanged',arg0namespace='xyz.openbmc_project.State."
        "Host'",
        [](sdbusplus::message::message& msg) {
            std::string interfaceName;
            boost::container::flat_map<std::string, std::variant<std::string>>
                propertiesChanged;
            std::string state;
            try
            {
                msg.read(interfaceName, propertiesChanged);
                state =
                    std::get<std::string>(propertiesChanged.begin()->second);
            }
            catch (std::exception& e)
            {
                std::cerr << "Unable to read host state\n";
                return;
            }
            hostOff = state == "xyz.openbmc_project.State.Host.HostState.Off";

            if (hostOff)
            {
                // No host events should fire while off, so cancel any pending
                // timers
                caterrAssertTimer.cancel();
                err0AssertTimer.cancel();
                err1AssertTimer.cancel();
                err2AssertTimer.cancel();
                smiAssertTimer.cancel();
            }
            else
            {
                // Handle any initial errors when the host turns on
                initializeErrorState();
            }
        });
}

static bool requestGPIOEvents(
    const std::string& name, const std::function<void()>& handler,
    gpiod::line& gpioLine,
    boost::asio::posix::stream_descriptor& gpioEventDescriptor)
{
    // Find the GPIO line
    gpioLine = gpiod::find_line(name);
    if (!gpioLine)
    {
        std::cerr << "Failed to find the " << name << " line\n";
        return false;
    }

    try
    {
        gpioLine.request(
            {"host-error-monitor", gpiod::line_request::EVENT_BOTH_EDGES});
    }
    catch (std::exception&)
    {
        std::cerr << "Failed to request events for " << name << "\n";
        return false;
    }

    int gpioLineFd = gpioLine.event_get_fd();
    if (gpioLineFd < 0)
    {
        std::cerr << "Failed to get " << name << " fd\n";
        return false;
    }

    gpioEventDescriptor.assign(gpioLineFd);

    gpioEventDescriptor.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [&name, handler](const boost::system::error_code ec) {
            if (ec)
            {
                std::cerr << name << " fd handler error: " << ec.message()
                          << "\n";
                return;
            }
            handler();
        });
    return true;
}

static bool requestGPIOInput(const std::string& name, gpiod::line& gpioLine)
{
    // Find the GPIO line
    gpioLine = gpiod::find_line(name);
    if (!gpioLine)
    {
        std::cerr << "Failed to find the " << name << " line.\n";
        return false;
    }

    // Request GPIO input
    try
    {
        gpioLine.request({__FUNCTION__, gpiod::line_request::DIRECTION_INPUT});
    }
    catch (std::exception&)
    {
        std::cerr << "Failed to request " << name << " input\n";
        return false;
    }

    return true;
}

static void startPowerCycle()
{
    conn->async_method_call(
        [](boost::system::error_code ec) {
            if (ec)
            {
                std::cerr << "failed to set Chassis State\n";
            }
        },
        "xyz.openbmc_project.State.Chassis",
        "/xyz/openbmc_project/state/chassis0",
        "org.freedesktop.DBus.Properties", "Set",
        "xyz.openbmc_project.State.Chassis", "RequestedPowerTransition",
        std::variant<std::string>{
            "xyz.openbmc_project.State.Chassis.Transition.PowerCycle"});
}

static void startCrashdumpAndRecovery(bool recoverSystem)
{
    std::cout << "Starting crashdump\n";
    static std::shared_ptr<sdbusplus::bus::match::match> crashdumpCompleteMatch;
    static boost::asio::steady_timer crashdumpTimer(io);

    crashdumpCompleteMatch = std::make_shared<sdbusplus::bus::match::match>(
        *conn,
        "type='signal',interface='org.freedesktop.DBus.Properties',"
        "member='PropertiesChanged',arg0namespace='com.intel.crashdump'",
        [recoverSystem](sdbusplus::message::message& msg) {
            crashdumpTimer.cancel();
            std::cout << "Crashdump completed\n";
            if (recoverSystem)
            {
                std::cout << "Recovering the system\n";
                startPowerCycle();
            }
            crashdumpCompleteMatch.reset();
        });

    crashdumpTimer.expires_after(std::chrono::seconds(crashdumpTimeoutS));
    crashdumpTimer.async_wait([](const boost::system::error_code ec) {
        if (ec)
        {
            // operation_aborted is expected if timer is canceled
            if (ec != boost::asio::error::operation_aborted)
            {
                std::cerr << "Crashdump async_wait failed: " << ec.message()
                          << "\n";
            }
            std::cout << "Crashdump timer canceled\n";
            return;
        }
        std::cerr << "Crashdump failed to complete before timeout\n";
        crashdumpCompleteMatch.reset();
    });

    conn->async_method_call(
        [](boost::system::error_code ec) {
            if (ec)
            {
                std::cerr << "failed to start Crashdump\n";
                crashdumpTimer.cancel();
                crashdumpCompleteMatch.reset();
            }
        },
        "com.intel.crashdump", "/com/intel/crashdump",
        "com.intel.crashdump.Stored", "GenerateStoredLog");
}

static void incrementCPUErrorCount(int cpuNum)
{
    std::string propertyName = "ErrorCountCPU" + std::to_string(cpuNum + 1);

    // Get the current count
    conn->async_method_call(
        [propertyName](boost::system::error_code ec,
                       const std::variant<uint8_t>& property) {
            if (ec)
            {
                std::cerr << "Failed to read " << propertyName << ": "
                          << ec.message() << "\n";
                return;
            }
            const uint8_t* errorCountVariant = std::get_if<uint8_t>(&property);
            if (errorCountVariant == nullptr)
            {
                std::cerr << propertyName << " invalid\n";
                return;
            }
            uint8_t errorCount = *errorCountVariant;
            if (errorCount == std::numeric_limits<uint8_t>::max())
            {
                std::cerr << "Maximum error count reached\n";
                return;
            }
            // Increment the count
            errorCount++;
            conn->async_method_call(
                [propertyName](boost::system::error_code ec) {
                    if (ec)
                    {
                        std::cerr << "Failed to set " << propertyName << ": "
                                  << ec.message() << "\n";
                    }
                },
                "xyz.openbmc_project.Settings",
                "/xyz/openbmc_project/control/processor_error_config",
                "org.freedesktop.DBus.Properties", "Set",
                "xyz.openbmc_project.Control.Processor.ErrConfig", propertyName,
                std::variant<uint8_t>{errorCount});
        },
        "xyz.openbmc_project.Settings",
        "/xyz/openbmc_project/control/processor_error_config",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Control.Processor.ErrConfig", propertyName);
}

static bool checkIERRCPUs()
{
    bool cpuIERRFound = false;
    for (int cpu = 0, addr = MIN_CLIENT_ADDR; addr <= MAX_CLIENT_ADDR;
         cpu++, addr++)
    {
        uint8_t cc = 0;
        CPUModel model{};
        if (peci_GetCPUID(addr, &model, &cc) != PECI_CC_SUCCESS)
        {
            std::cerr << "Cannot get CPUID!\n";
            continue;
        }

        switch (model)
        {
            case skx:
            {
                // First check the MCA_ERR_SRC_LOG to see if this is the CPU
                // that caused the IERR
                uint32_t mcaErrSrcLog = 0;
                if (peci_RdPkgConfig(addr, 0, 5, 4, (uint8_t*)&mcaErrSrcLog,
                                     &cc) != PECI_CC_SUCCESS)
                {
                    continue;
                }
                // Check MSMI_INTERNAL (20) and IERR_INTERNAL (27)
                if ((mcaErrSrcLog & (1 << 20)) || (mcaErrSrcLog & (1 << 27)))
                {
                    // TODO: Light the CPU fault LED?
                    cpuIERRFound = true;
                    incrementCPUErrorCount(cpu);
                    // Next check if it's a CPU/VR mismatch by reading the
                    // IA32_MC4_STATUS MSR (0x411)
                    uint64_t mc4Status = 0;
                    if (peci_RdIAMSR(addr, 0, 0x411, &mc4Status, &cc) !=
                        PECI_CC_SUCCESS)
                    {
                        continue;
                    }
                    // Check MSEC bits 31:24 for
                    // MCA_SVID_VCCIN_VR_ICC_MAX_FAILURE (0x40),
                    // MCA_SVID_VCCIN_VR_VOUT_FAILURE (0x42), or
                    // MCA_SVID_CPU_VR_CAPABILITY_ERROR (0x43)
                    if ((mc4Status & (0x40 << 24)) ||
                        (mc4Status & (0x42 << 24)) ||
                        (mc4Status & (0x43 << 24)))
                    {
                        cpuIERRLog(cpu, "CPU/VR Mismatch");
                        continue;
                    }

                    // Next check if it's a Core FIVR fault by looking for a
                    // non-zero value of CORE_FIVR_ERR_LOG (B(1) D30 F2 offset
                    // 80h)
                    uint32_t coreFIVRErrLog = 0;
                    if (peci_RdPCIConfigLocal(
                            addr, 1, 30, 2, 0x80, sizeof(uint32_t),
                            (uint8_t*)&coreFIVRErrLog, &cc) != PECI_CC_SUCCESS)
                    {
                        continue;
                    }
                    if (coreFIVRErrLog)
                    {
                        cpuIERRLog(cpu, "Core FIVR Fault");
                        continue;
                    }

                    // Next check if it's an Uncore FIVR fault by looking for a
                    // non-zero value of UNCORE_FIVR_ERR_LOG (B(1) D30 F2 offset
                    // 84h)
                    uint32_t uncoreFIVRErrLog = 0;
                    if (peci_RdPCIConfigLocal(addr, 1, 30, 2, 0x84,
                                              sizeof(uint32_t),
                                              (uint8_t*)&uncoreFIVRErrLog,
                                              &cc) != PECI_CC_SUCCESS)
                    {
                        continue;
                    }
                    if (uncoreFIVRErrLog)
                    {
                        cpuIERRLog(cpu, "Uncore FIVR Fault");
                        continue;
                    }

                    // Last if CORE_FIVR_ERR_LOG and UNCORE_FIVR_ERR_LOG are
                    // both zero, but MSEC bits 31:24 have either
                    // MCA_FIVR_CATAS_OVERVOL_FAULT (0x51) or
                    // MCA_FIVR_CATAS_OVERCUR_FAULT (0x52), then log it as an
                    // uncore FIVR fault
                    if (!coreFIVRErrLog && !uncoreFIVRErrLog &&
                        ((mc4Status & (0x51 << 24)) ||
                         (mc4Status & (0x52 << 24))))
                    {
                        cpuIERRLog(cpu, "Uncore FIVR Fault");
                        continue;
                    }
                    cpuIERRLog(cpu);
                }
                break;
            }
            case icx:
            {
                // First check the MCA_ERR_SRC_LOG to see if this is the CPU
                // that caused the IERR
                uint32_t mcaErrSrcLog = 0;
                if (peci_RdPkgConfig(addr, 0, 5, 4, (uint8_t*)&mcaErrSrcLog,
                                     &cc) != PECI_CC_SUCCESS)
                {
                    continue;
                }
                // Check MSMI_INTERNAL (20) and IERR_INTERNAL (27)
                if ((mcaErrSrcLog & (1 << 20)) || (mcaErrSrcLog & (1 << 27)))
                {
                    // TODO: Light the CPU fault LED?
                    cpuIERRFound = true;
                    incrementCPUErrorCount(cpu);
                    // Next check if it's a CPU/VR mismatch by reading the
                    // IA32_MC4_STATUS MSR (0x411)
                    uint64_t mc4Status = 0;
                    if (peci_RdIAMSR(addr, 0, 0x411, &mc4Status, &cc) !=
                        PECI_CC_SUCCESS)
                    {
                        continue;
                    }
                    // TODO: Update MSEC/MSCOD_31_24 check
                    // Check MSEC bits 31:24 for
                    // MCA_SVID_VCCIN_VR_ICC_MAX_FAILURE (0x40),
                    // MCA_SVID_VCCIN_VR_VOUT_FAILURE (0x42), or
                    // MCA_SVID_CPU_VR_CAPABILITY_ERROR (0x43)
                    if ((mc4Status & (0x40 << 24)) ||
                        (mc4Status & (0x42 << 24)) ||
                        (mc4Status & (0x43 << 24)))
                    {
                        cpuIERRLog(cpu, "CPU/VR Mismatch");
                        continue;
                    }

                    // Next check if it's a Core FIVR fault by looking for a
                    // non-zero value of CORE_FIVR_ERR_LOG (B(31) D30 F2 offsets
                    // C0h and C4h) (Note: Bus 31 is accessed on PECI as bus 14)
                    uint32_t coreFIVRErrLog0 = 0;
                    uint32_t coreFIVRErrLog1 = 0;
                    if (peci_RdEndPointConfigPciLocal(
                            addr, 0, 14, 30, 2, 0xC0, sizeof(uint32_t),
                            (uint8_t*)&coreFIVRErrLog0, &cc) != PECI_CC_SUCCESS)
                    {
                        continue;
                    }
                    if (peci_RdEndPointConfigPciLocal(
                            addr, 0, 14, 30, 2, 0xC4, sizeof(uint32_t),
                            (uint8_t*)&coreFIVRErrLog1, &cc) != PECI_CC_SUCCESS)
                    {
                        continue;
                    }
                    if (coreFIVRErrLog0 || coreFIVRErrLog1)
                    {
                        cpuIERRLog(cpu, "Core FIVR Fault");
                        continue;
                    }

                    // Next check if it's an Uncore FIVR fault by looking for a
                    // non-zero value of UNCORE_FIVR_ERR_LOG (B(31) D30 F2
                    // offset 84h) (Note: Bus 31 is accessed on PECI as bus 14)
                    uint32_t uncoreFIVRErrLog = 0;
                    if (peci_RdEndPointConfigPciLocal(
                            addr, 0, 14, 30, 2, 0x84, sizeof(uint32_t),
                            (uint8_t*)&uncoreFIVRErrLog,
                            &cc) != PECI_CC_SUCCESS)
                    {
                        continue;
                    }
                    if (uncoreFIVRErrLog)
                    {
                        cpuIERRLog(cpu, "Uncore FIVR Fault");
                        continue;
                    }

                    // TODO: Update MSEC/MSCOD_31_24 check
                    // Last if CORE_FIVR_ERR_LOG and UNCORE_FIVR_ERR_LOG are
                    // both zero, but MSEC bits 31:24 have either
                    // MCA_FIVR_CATAS_OVERVOL_FAULT (0x51) or
                    // MCA_FIVR_CATAS_OVERCUR_FAULT (0x52), then log it as an
                    // uncore FIVR fault
                    if (!coreFIVRErrLog0 && !coreFIVRErrLog1 &&
                        !uncoreFIVRErrLog &&
                        ((mc4Status & (0x51 << 24)) ||
                         (mc4Status & (0x52 << 24))))
                    {
                        cpuIERRLog(cpu, "Uncore FIVR Fault");
                        continue;
                    }
                    cpuIERRLog(cpu);
                }
                break;
            }
        }
    }
    return cpuIERRFound;
}

static void caterrAssertHandler()
{
    caterrAssertTimer.expires_after(std::chrono::milliseconds(caterrTimeoutMs));
    caterrAssertTimer.async_wait([](const boost::system::error_code ec) {
        if (ec)
        {
            // operation_aborted is expected if timer is canceled
            // before completion.
            if (ec != boost::asio::error::operation_aborted)
            {
                std::cerr << "caterr timeout async_wait failed: "
                          << ec.message() << "\n";
            }
            return;
        }
        std::cerr << "CATERR asserted for " << std::to_string(caterrTimeoutMs)
                  << " ms\n";
        if (!checkIERRCPUs())
        {
            cpuIERRLog();
        }
        conn->async_method_call(
            [](boost::system::error_code ec,
               const std::variant<bool>& property) {
                if (ec)
                {
                    return;
                }
                const bool* reset = std::get_if<bool>(&property);
                if (reset == nullptr)
                {
                    std::cerr << "Unable to read reset on CATERR value\n";
                    return;
                }
                startCrashdumpAndRecovery(*reset);
            },
            "xyz.openbmc_project.Settings",
            "/xyz/openbmc_project/control/processor_error_config",
            "org.freedesktop.DBus.Properties", "Get",
            "xyz.openbmc_project.Control.Processor.ErrConfig", "ResetOnCATERR");
    });
}

static void caterrHandler()
{
    if (!hostOff)
    {
        gpiod::line_event gpioLineEvent = caterrLine.event_read();

        bool caterr =
            gpioLineEvent.event_type == gpiod::line_event::FALLING_EDGE;
        if (caterr)
        {
            caterrAssertHandler();
        }
        else
        {
            caterrAssertTimer.cancel();
        }
    }
    caterrEvent.async_wait(boost::asio::posix::stream_descriptor::wait_read,
                           [](const boost::system::error_code ec) {
                               if (ec)
                               {
                                   std::cerr << "caterr handler error: "
                                             << ec.message() << "\n";
                                   return;
                               }
                               caterrHandler();
                           });
}

static void cpu1ThermtripAssertHandler()
{
    if (cpu1FIVRFaultLine.get_value() == 0)
    {
        cpuBootFIVRFaultLog(1);
    }
    else
    {
        cpuThermTripLog(1);
    }
}

static void cpu1ThermtripHandler()
{
    if (!hostOff)
    {
        gpiod::line_event gpioLineEvent = cpu1ThermtripLine.event_read();

        bool cpu1Thermtrip =
            gpioLineEvent.event_type == gpiod::line_event::FALLING_EDGE;
        if (cpu1Thermtrip)
        {
            cpu1ThermtripAssertHandler();
        }
    }
    cpu1ThermtripEvent.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [](const boost::system::error_code ec) {
            if (ec)
            {
                std::cerr << "CPU 1 Thermtrip handler error: " << ec.message()
                          << "\n";
                return;
            }
            cpu1ThermtripHandler();
        });
}

static void cpu2ThermtripAssertHandler()
{
    if (cpu2FIVRFaultLine.get_value() == 0)
    {
        cpuBootFIVRFaultLog(2);
    }
    else
    {
        cpuThermTripLog(2);
    }
}

static void cpu2ThermtripHandler()
{
    if (!hostOff)
    {
        gpiod::line_event gpioLineEvent = cpu2ThermtripLine.event_read();

        bool cpu2Thermtrip =
            gpioLineEvent.event_type == gpiod::line_event::FALLING_EDGE;
        if (cpu2Thermtrip)
        {
            cpu2ThermtripAssertHandler();
        }
    }
    cpu2ThermtripEvent.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [](const boost::system::error_code ec) {
            if (ec)
            {
                std::cerr << "CPU 2 Thermtrip handler error: " << ec.message()
                          << "\n";
                return;
            }
            cpu2ThermtripHandler();
        });
}

static void cpu1VRHotAssertHandler()
{
    cpuVRHotLog("CPU 1");
}

static void cpu1VRHotHandler()
{
    if (!hostOff)
    {
        gpiod::line_event gpioLineEvent = cpu1VRHotLine.event_read();

        bool cpu1VRHot =
            gpioLineEvent.event_type == gpiod::line_event::FALLING_EDGE;
        if (cpu1VRHot)
        {
            cpu1VRHotAssertHandler();
        }
    }
    cpu1VRHotEvent.async_wait(boost::asio::posix::stream_descriptor::wait_read,
                              [](const boost::system::error_code ec) {
                                  if (ec)
                                  {
                                      std::cerr << "CPU 1 VRHot handler error: "
                                                << ec.message() << "\n";
                                      return;
                                  }
                                  cpu1VRHotHandler();
                              });
}

static void cpu1MemABCDVRHotAssertHandler()
{
    cpuVRHotLog("CPU 1 Memory ABCD");
}

static void cpu1MemABCDVRHotHandler()
{
    if (!hostOff)
    {
        gpiod::line_event gpioLineEvent = cpu1MemABCDVRHotLine.event_read();

        bool cpu1MemABCDVRHot =
            gpioLineEvent.event_type == gpiod::line_event::FALLING_EDGE;
        if (cpu1MemABCDVRHot)
        {
            cpu1MemABCDVRHotAssertHandler();
        }
    }
    cpu1MemABCDVRHotEvent.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [](const boost::system::error_code ec) {
            if (ec)
            {
                std::cerr << "CPU 1 Memory ABCD VRHot handler error: "
                          << ec.message() << "\n";
                return;
            }
            cpu1MemABCDVRHotHandler();
        });
}

static void cpu1MemEFGHVRHotAssertHandler()
{
    cpuVRHotLog("CPU 1 Memory EFGH");
}

static void cpu1MemEFGHVRHotHandler()
{
    if (!hostOff)
    {
        gpiod::line_event gpioLineEvent = cpu1MemEFGHVRHotLine.event_read();

        bool cpu1MemEFGHVRHot =
            gpioLineEvent.event_type == gpiod::line_event::FALLING_EDGE;
        if (cpu1MemEFGHVRHot)
        {
            cpu1MemEFGHVRHotAssertHandler();
        }
    }
    cpu1MemEFGHVRHotEvent.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [](const boost::system::error_code ec) {
            if (ec)
            {
                std::cerr << "CPU 1 Memory EFGH VRHot handler error: "
                          << ec.message() << "\n";
                return;
            }
            cpu1MemEFGHVRHotHandler();
        });
}

static void cpu2VRHotAssertHandler()
{
    cpuVRHotLog("CPU 2");
}

static void cpu2VRHotHandler()
{
    if (!hostOff)
    {
        gpiod::line_event gpioLineEvent = cpu2VRHotLine.event_read();

        bool cpu2VRHot =
            gpioLineEvent.event_type == gpiod::line_event::FALLING_EDGE;
        if (cpu2VRHot)
        {
            cpu2VRHotAssertHandler();
        }
    }
    cpu2VRHotEvent.async_wait(boost::asio::posix::stream_descriptor::wait_read,
                              [](const boost::system::error_code ec) {
                                  if (ec)
                                  {
                                      std::cerr << "CPU 2 VRHot handler error: "
                                                << ec.message() << "\n";
                                      return;
                                  }
                                  cpu2VRHotHandler();
                              });
}

static void cpu2MemABCDVRHotAssertHandler()
{
    cpuVRHotLog("CPU 2 Memory ABCD");
}

static void cpu2MemABCDVRHotHandler()
{
    if (!hostOff)
    {
        gpiod::line_event gpioLineEvent = cpu2MemABCDVRHotLine.event_read();

        bool cpu2MemABCDVRHot =
            gpioLineEvent.event_type == gpiod::line_event::FALLING_EDGE;
        if (cpu2MemABCDVRHot)
        {
            cpu2MemABCDVRHotAssertHandler();
        }
    }
    cpu2MemABCDVRHotEvent.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [](const boost::system::error_code ec) {
            if (ec)
            {
                std::cerr << "CPU 2 Memory ABCD VRHot handler error: "
                          << ec.message() << "\n";
                return;
            }
            cpu2MemABCDVRHotHandler();
        });
}

static void cpu2MemEFGHVRHotAssertHandler()
{
    cpuVRHotLog("CPU 2 Memory EFGH");
}

static void cpu2MemEFGHVRHotHandler()
{
    if (!hostOff)
    {
        gpiod::line_event gpioLineEvent = cpu2MemEFGHVRHotLine.event_read();

        bool cpu2MemEFGHVRHot =
            gpioLineEvent.event_type == gpiod::line_event::FALLING_EDGE;
        if (cpu2MemEFGHVRHot)
        {
            cpu2MemEFGHVRHotAssertHandler();
        }
    }
    cpu2MemEFGHVRHotEvent.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [](const boost::system::error_code ec) {
            if (ec)
            {
                std::cerr << "CPU 2 Memory EFGH VRHot handler error: "
                          << ec.message() << "\n";
                return;
            }
            cpu2MemEFGHVRHotHandler();
        });
}

static void pchThermtripHandler()
{
    if (!hostOff)
    {
        gpiod::line_event gpioLineEvent = pchThermtripLine.event_read();

        bool pchThermtrip =
            gpioLineEvent.event_type == gpiod::line_event::FALLING_EDGE;
        if (pchThermtrip)
        {
            ssbThermTripLog();
        }
    }
    pchThermtripEvent.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [](const boost::system::error_code ec) {
            if (ec)
            {
                std::cerr << "PCH Thermal trip handler error: " << ec.message()
                          << "\n";
                return;
            }
            pchThermtripHandler();
        });
}

static std::bitset<MAX_CPUS> checkERRPinCPUs(const int errPin)
{
    int errPinSts = (1 << errPin);
    std::bitset<MAX_CPUS> errPinCPUs = 0;
    for (int cpu = 0, addr = MIN_CLIENT_ADDR; addr <= MAX_CLIENT_ADDR;
         cpu++, addr++)
    {
        if (peci_Ping(addr) == PECI_CC_SUCCESS)
        {
            uint8_t cc = 0;
            CPUModel model{};
            if (peci_GetCPUID(addr, &model, &cc) != PECI_CC_SUCCESS)
            {
                std::cerr << "Cannot get CPUID!\n";
                continue;
            }

            switch (model)
            {
                case skx:
                {
                    // Check the ERRPINSTS to see if this is the CPU that caused
                    // the ERRx (B(0) D8 F0 offset 210h)
                    uint32_t errpinsts = 0;
                    if (peci_RdPCIConfigLocal(
                            addr, 0, 8, 0, 0x210, sizeof(uint32_t),
                            (uint8_t*)&errpinsts, &cc) == PECI_CC_SUCCESS)
                    {
                        errPinCPUs[cpu] = (errpinsts & errPinSts) != 0;
                    }
                    break;
                }
                case icx:
                {
                    // Check the ERRPINSTS to see if this is the CPU that caused
                    // the ERRx (B(30) D0 F3 offset 274h) (Note: Bus 30 is
                    // accessed on PECI as bus 13)
                    uint32_t errpinsts = 0;
                    if (peci_RdEndPointConfigPciLocal(
                            addr, 0, 13, 0, 3, 0x274, sizeof(uint32_t),
                            (uint8_t*)&errpinsts, &cc) == PECI_CC_SUCCESS)
                    {
                        errPinCPUs[cpu] = (errpinsts & errPinSts) != 0;
                    }
                    break;
                }
            }
        }
    }
    return errPinCPUs;
}

static void errXAssertHandler(const int errPin,
                              boost::asio::steady_timer& errXAssertTimer)
{
    // ERRx status is not guaranteed through the timeout, so save which
    // CPUs have it asserted
    std::bitset<MAX_CPUS> errPinCPUs = checkERRPinCPUs(errPin);
    errXAssertTimer.expires_after(std::chrono::milliseconds(errTimeoutMs));
    errXAssertTimer.async_wait([errPin, errPinCPUs](
                                   const boost::system::error_code ec) {
        if (ec)
        {
            // operation_aborted is expected if timer is canceled before
            // completion.
            if (ec != boost::asio::error::operation_aborted)
            {
                std::cerr << "err2 timeout async_wait failed: " << ec.message()
                          << "\n";
            }
            return;
        }
        std::cerr << "ERR" << std::to_string(errPin) << " asserted for "
                  << std::to_string(errTimeoutMs) << " ms\n";
        if (errPinCPUs.count())
        {
            for (int i = 0; i < errPinCPUs.size(); i++)
            {
                if (errPinCPUs[i])
                {
                    cpuERRXLog(errPin, i);
                }
            }
        }
        else
        {
            cpuERRXLog(errPin);
        }
    });
}

static void err0AssertHandler()
{
    // Handle the standard ERR0 detection and logging
    const static constexpr int err0 = 0;
    errXAssertHandler(err0, err0AssertTimer);
}

static void err0Handler()
{
    if (!hostOff)
    {
        gpiod::line_event gpioLineEvent = err0Line.event_read();

        bool err0 = gpioLineEvent.event_type == gpiod::line_event::FALLING_EDGE;
        if (err0)
        {
            err0AssertHandler();
        }
        else
        {
            err0AssertTimer.cancel();
        }
    }
    err0Event.async_wait(boost::asio::posix::stream_descriptor::wait_read,
                         [](const boost::system::error_code ec) {
                             if (ec)
                             {
                                 std::cerr
                                     << "err0 handler error: " << ec.message()
                                     << "\n";
                                 return;
                             }
                             err0Handler();
                         });
}

static void err1AssertHandler()
{
    // Handle the standard ERR1 detection and logging
    const static constexpr int err1 = 1;
    errXAssertHandler(err1, err1AssertTimer);
}

static void err1Handler()
{
    if (!hostOff)
    {
        gpiod::line_event gpioLineEvent = err1Line.event_read();

        bool err1 = gpioLineEvent.event_type == gpiod::line_event::FALLING_EDGE;
        if (err1)
        {
            err1AssertHandler();
        }
        else
        {
            err1AssertTimer.cancel();
        }
    }
    err1Event.async_wait(boost::asio::posix::stream_descriptor::wait_read,
                         [](const boost::system::error_code ec) {
                             if (ec)
                             {
                                 std::cerr
                                     << "err1 handler error: " << ec.message()
                                     << "\n";
                                 return;
                             }
                             err1Handler();
                         });
}

static void err2AssertHandler()
{
    // Handle the standard ERR2 detection and logging
    const static constexpr int err2 = 2;
    errXAssertHandler(err2, err2AssertTimer);
    // Also handle reset for ERR2
    err2AssertTimer.async_wait([](const boost::system::error_code ec) {
        if (ec)
        {
            // operation_aborted is expected if timer is canceled before
            // completion.
            if (ec != boost::asio::error::operation_aborted)
            {
                std::cerr << "err2 timeout async_wait failed: " << ec.message()
                          << "\n";
            }
            return;
        }
        conn->async_method_call(
            [](boost::system::error_code ec,
               const std::variant<bool>& property) {
                if (ec)
                {
                    return;
                }
                const bool* reset = std::get_if<bool>(&property);
                if (reset == nullptr)
                {
                    std::cerr << "Unable to read reset on ERR2 value\n";
                    return;
                }
                startCrashdumpAndRecovery(*reset);
            },
            "xyz.openbmc_project.Settings",
            "/xyz/openbmc_project/control/processor_error_config",
            "org.freedesktop.DBus.Properties", "Get",
            "xyz.openbmc_project.Control.Processor.ErrConfig", "ResetOnERR2");
    });
}

static void err2Handler()
{
    if (!hostOff)
    {
        gpiod::line_event gpioLineEvent = err2Line.event_read();

        bool err2 = gpioLineEvent.event_type == gpiod::line_event::FALLING_EDGE;
        if (err2)
        {
            err2AssertHandler();
        }
        else
        {
            err2AssertTimer.cancel();
        }
    }
    err2Event.async_wait(boost::asio::posix::stream_descriptor::wait_read,
                         [](const boost::system::error_code ec) {
                             if (ec)
                             {
                                 std::cerr
                                     << "err2 handler error: " << ec.message()
                                     << "\n";
                                 return;
                             }
                             err2Handler();
                         });
}

static void smiAssertHandler()
{
    smiAssertTimer.expires_after(std::chrono::milliseconds(smiTimeoutMs));
    smiAssertTimer.async_wait([](const boost::system::error_code ec) {
        if (ec)
        {
            // operation_aborted is expected if timer is canceled before
            // completion.
            if (ec != boost::asio::error::operation_aborted)
            {
                std::cerr << "smi timeout async_wait failed: " << ec.message()
                          << "\n";
            }
            return;
        }
        std::cerr << "SMI asserted for " << std::to_string(smiTimeoutMs)
                  << " ms\n";
        smiTimeoutLog();
        conn->async_method_call(
            [](boost::system::error_code ec,
               const std::variant<bool>& property) {
                if (ec)
                {
                    return;
                }
                const bool* reset = std::get_if<bool>(&property);
                if (reset == nullptr)
                {
                    std::cerr << "Unable to read reset on SMI value\n";
                    return;
                }
                startCrashdumpAndRecovery(*reset);
            },
            "xyz.openbmc_project.Settings",
            "/xyz/openbmc_project/control/bmc_reset_disables",
            "org.freedesktop.DBus.Properties", "Get",
            "xyz.openbmc_project.Control.ResetDisables", "ResetOnSMI");
    });
}

static void smiHandler()
{
    if (!hostOff)
    {
        gpiod::line_event gpioLineEvent = smiLine.event_read();

        bool smi = gpioLineEvent.event_type == gpiod::line_event::FALLING_EDGE;
        if (smi)
        {
            smiAssertHandler();
        }
        else
        {
            smiAssertTimer.cancel();
        }
    }
    smiEvent.async_wait(boost::asio::posix::stream_descriptor::wait_read,
                        [](const boost::system::error_code ec) {
                            if (ec)
                            {
                                std::cerr
                                    << "smi handler error: " << ec.message()
                                    << "\n";
                                return;
                            }
                            smiHandler();
                        });
}

static void initializeErrorState()
{
    // Handle CPU_CATERR if it's asserted now
    if (caterrLine.get_value() == 0)
    {
        caterrAssertHandler();
    }

    // Handle CPU_ERR0 if it's asserted now
    if (err0Line.get_value() == 0)
    {
        err0AssertHandler();
    }

    // Handle CPU_ERR1 if it's asserted now
    if (err1Line.get_value() == 0)
    {
        err1AssertHandler();
    }

    // Handle CPU_ERR2 if it's asserted now
    if (err2Line.get_value() == 0)
    {
        err2AssertHandler();
    }

    // Handle SMI if it's asserted now
    if (smiLine.get_value() == 0)
    {
        smiAssertHandler();
    }

    // Handle CPU1_THERMTRIP if it's asserted now
    if (cpu1ThermtripLine.get_value() == 0)
    {
        cpu1ThermtripAssertHandler();
    }

    // Handle CPU2_THERMTRIP if it's asserted now
    if (cpu2ThermtripLine.get_value() == 0)
    {
        cpu2ThermtripAssertHandler();
    }

    // Handle CPU1_VRHOT if it's asserted now
    if (cpu1VRHotLine.get_value() == 0)
    {
        cpu1VRHotAssertHandler();
    }

    // Handle CPU1_MEM_ABCD_VRHOT if it's asserted now
    if (cpu1MemABCDVRHotLine.get_value() == 0)
    {
        cpu1MemABCDVRHotAssertHandler();
    }

    // Handle CPU1_MEM_EFGH_VRHOT if it's asserted now
    if (cpu1MemEFGHVRHotLine.get_value() == 0)
    {
        cpu1MemEFGHVRHotAssertHandler();
    }

    // Handle CPU2_VRHOT if it's asserted now
    if (cpu2VRHotLine.get_value() == 0)
    {
        cpu2VRHotAssertHandler();
    }

    // Handle CPU2_MEM_ABCD_VRHOT if it's asserted now
    if (cpu2MemABCDVRHotLine.get_value() == 0)
    {
        cpu2MemABCDVRHotAssertHandler();
    }

    // Handle CPU2_MEM_EFGH_VRHOT if it's asserted now
    if (cpu2MemEFGHVRHotLine.get_value() == 0)
    {
        cpu2MemEFGHVRHotAssertHandler();
    }

    // Handle PCH_BMC_THERMTRIP if it's asserted now
    if (pchThermtripLine.get_value() == 0)
    {
        ssbThermTripLog();
    }
}
} // namespace host_error_monitor

int main(int argc, char* argv[])
{
    // setup connection to dbus
    host_error_monitor::conn =
        std::make_shared<sdbusplus::asio::connection>(host_error_monitor::io);

    // Host Error Monitor Object
    host_error_monitor::conn->request_name(
        "xyz.openbmc_project.HostErrorMonitor");
    sdbusplus::asio::object_server server =
        sdbusplus::asio::object_server(host_error_monitor::conn);

    // Start tracking host state
    std::shared_ptr<sdbusplus::bus::match::match> hostStateMonitor =
        host_error_monitor::startHostStateMonitor();

    // Initialize the host state
    host_error_monitor::initializeHostState();

    // Request CPU_CATERR GPIO events
    if (!host_error_monitor::requestGPIOEvents(
            "CPU_CATERR", host_error_monitor::caterrHandler,
            host_error_monitor::caterrLine, host_error_monitor::caterrEvent))
    {
        return -1;
    }

    // Request CPU_ERR0 GPIO events
    if (!host_error_monitor::requestGPIOEvents(
            "CPU_ERR0", host_error_monitor::err0Handler,
            host_error_monitor::err0Line, host_error_monitor::err0Event))
    {
        return -1;
    }

    // Request CPU_ERR1 GPIO events
    if (!host_error_monitor::requestGPIOEvents(
            "CPU_ERR1", host_error_monitor::err1Handler,
            host_error_monitor::err1Line, host_error_monitor::err1Event))
    {
        return -1;
    }

    // Request CPU_ERR2 GPIO events
    if (!host_error_monitor::requestGPIOEvents(
            "CPU_ERR2", host_error_monitor::err2Handler,
            host_error_monitor::err2Line, host_error_monitor::err2Event))
    {
        return -1;
    }

    // Request SMI GPIO events
    if (!host_error_monitor::requestGPIOEvents(
            "SMI", host_error_monitor::smiHandler, host_error_monitor::smiLine,
            host_error_monitor::smiEvent))
    {
        return -1;
    }

    // Request CPU1_FIVR_FAULT GPIO input
    if (!host_error_monitor::requestGPIOInput(
            "CPU1_FIVR_FAULT", host_error_monitor::cpu1FIVRFaultLine))
    {
        return -1;
    }

    // Request CPU1_THERMTRIP GPIO events
    if (!host_error_monitor::requestGPIOEvents(
            "CPU1_THERMTRIP", host_error_monitor::cpu1ThermtripHandler,
            host_error_monitor::cpu1ThermtripLine,
            host_error_monitor::cpu1ThermtripEvent))
    {
        return -1;
    }

    // Request CPU2_FIVR_FAULT GPIO input
    if (!host_error_monitor::requestGPIOInput(
            "CPU2_FIVR_FAULT", host_error_monitor::cpu2FIVRFaultLine))
    {
        return -1;
    }

    // Request CPU2_THERMTRIP GPIO events
    if (!host_error_monitor::requestGPIOEvents(
            "CPU2_THERMTRIP", host_error_monitor::cpu2ThermtripHandler,
            host_error_monitor::cpu2ThermtripLine,
            host_error_monitor::cpu2ThermtripEvent))
    {
        return -1;
    }

    // Request CPU1_VRHOT GPIO events
    if (!host_error_monitor::requestGPIOEvents(
            "CPU1_VRHOT", host_error_monitor::cpu1VRHotHandler,
            host_error_monitor::cpu1VRHotLine,
            host_error_monitor::cpu1VRHotEvent))
    {
        return -1;
    }

    // Request CPU1_MEM_ABCD_VRHOT GPIO events
    if (!host_error_monitor::requestGPIOEvents(
            "CPU1_MEM_ABCD_VRHOT", host_error_monitor::cpu1MemABCDVRHotHandler,
            host_error_monitor::cpu1MemABCDVRHotLine,
            host_error_monitor::cpu1MemABCDVRHotEvent))
    {
        return -1;
    }

    // Request CPU1_MEM_EFGH_VRHOT GPIO events
    if (!host_error_monitor::requestGPIOEvents(
            "CPU1_MEM_EFGH_VRHOT", host_error_monitor::cpu1MemEFGHVRHotHandler,
            host_error_monitor::cpu1MemEFGHVRHotLine,
            host_error_monitor::cpu1MemEFGHVRHotEvent))
    {
        return -1;
    }

    // Request CPU2_VRHOT GPIO events
    if (!host_error_monitor::requestGPIOEvents(
            "CPU2_VRHOT", host_error_monitor::cpu2VRHotHandler,
            host_error_monitor::cpu2VRHotLine,
            host_error_monitor::cpu2VRHotEvent))
    {
        return -1;
    }

    // Request CPU2_MEM_ABCD_VRHOT GPIO events
    if (!host_error_monitor::requestGPIOEvents(
            "CPU2_MEM_ABCD_VRHOT", host_error_monitor::cpu2MemABCDVRHotHandler,
            host_error_monitor::cpu2MemABCDVRHotLine,
            host_error_monitor::cpu2MemABCDVRHotEvent))
    {
        return -1;
    }

    // Request CPU2_MEM_EFGH_VRHOT GPIO events
    if (!host_error_monitor::requestGPIOEvents(
            "CPU2_MEM_EFGH_VRHOT", host_error_monitor::cpu2MemEFGHVRHotHandler,
            host_error_monitor::cpu2MemEFGHVRHotLine,
            host_error_monitor::cpu2MemEFGHVRHotEvent))
    {
        return -1;
    }

    // Request PCH_BMC_THERMTRIP GPIO events
    if (!host_error_monitor::requestGPIOEvents(
            "PCH_BMC_THERMTRIP", host_error_monitor::pchThermtripHandler,
            host_error_monitor::pchThermtripLine,
            host_error_monitor::pchThermtripEvent))
    {
        return -1;
    }

    host_error_monitor::io.run();

    return 0;
}
