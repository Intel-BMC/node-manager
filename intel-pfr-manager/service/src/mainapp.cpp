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

#include <systemd/sd-journal.h>

#include "pfr_mgr.hpp"
#include "pfr.hpp"

static std::array<std::string, 5> listVersionPaths = {
    "bmc_active", "bmc_recovery", "bios_active", "bios_recovery", "cpld"};

// Caches the last Recovery/Panic Count to
// identify any new Recovery/panic actions.
/* TODO: When BMC Reset's, these values will be lost
 * Persist this info using settingsd */
static uint8_t lastRecoveryCount = 0;
static uint8_t lastPanicCount = 0;
static uint8_t lastMajorErr = 0;
static uint8_t lastMinorErr = 0;

static bool stateTimerRunning = false;
std::unique_ptr<boost::asio::steady_timer> stateTimer = nullptr;

// Recovery reason map. { <CPLD association>, <Recovery Reason> }
static std::map<uint8_t, std::string> recoveryReasonMap = {
    {0x01, "PCH active authentication failure"},
    {0x02, "PCH recovery authentication failure"},
    {0x03, "ACM launch failure"},
    {0x04, "IBB launch failure"},
    {0x05, "OBB launch failure"},
    {0x06, "BMC active authentication failure"},
    {0x07, "BMC recovery authentication failure"},
    {0x08, "BMC launch failure"},
    {0x09, "CPLD watchdog expired"}};

// Panic Reason map. { <CPLD association>, <Panic reason> }
static std::map<uint8_t, std::string> panicReasonMap = {
    {0x01, "CPLD WDT expired"},
    {0x02, "BMC WDT expired"},
    {0x03, "ME WDT expired"},
    {0x04, "ACM WDT expired"},
    {0x05, "IBB WDT expired"},
    {0x06, "OBB WDT expired"},
    {0x07, "BMC active authentication failure"},
    {0x08, "BMC recovery authentication failure"},
    {0x09, "PCH active authentication failure"},
    {0x0A, "PCH recovery authentication failure"},
    {0x0B, "IBB authentication failure"},
    {0x0C, "OBB authentication failure"},
    {0x0D, "BMC authentication failure"},
    {0x0E, "PCH active update intent"},
    {0x0F, "BMC active update intent"},
    {0x10, "PCH recovery update intent"},
    {0x11, "BMC recovery update intent"}};

static void logLastRecoveryEvent()
{
    uint8_t reason = 0;
    if (0 !=
        intel::pfr::readCpldReg(intel::pfr::ActionType::recoveryReason, reason))
    {
        return;
    }

    std::map<uint8_t, std::string>::const_iterator it =
        recoveryReasonMap.find(reason);
    if (it == recoveryReasonMap.end())
    {
        // No matching found. So just return without logging event.
        return;
    }

    sd_journal_send("MESSAGE=%s", "Platform firmware recovered.", "PRIORITY=%i",
                    LOG_ERR, "REDFISH_MESSAGE_ID=%s",
                    "OpenBMC.0.1.PlatformFWRecovered",
                    "REDFISH_MESSAGE_ARGS=%s", it->second.c_str(), NULL);
}

static void logLastPanicEvent()
{
    uint8_t reason = 0;
    if (0 !=
        intel::pfr::readCpldReg(intel::pfr::ActionType::panicReason, reason))
    {
        return;
    }

    std::map<uint8_t, std::string>::const_iterator it =
        panicReasonMap.find(reason);
    if (it == panicReasonMap.end())
    {
        // No matching found. So just return without logging event.
        return;
    }

    sd_journal_send("MESSAGE=%s", "Platform panic event triggered.",
                    "PRIORITY=%i", LOG_ERR, "REDFISH_MESSAGE_ID=%s",
                    "OpenBMC.0.1.PlatformFWPanicTriggered",
                    "REDFISH_MESSAGE_ARGS=%s", it->second.c_str(), NULL);
}

static void checkAndLogEvents()
{
    uint8_t currPanicCount = 0;
    if (0 == intel::pfr::readCpldReg(intel::pfr::ActionType::panicCount,
                                     currPanicCount))
    {
        if (lastPanicCount != currPanicCount)
        {
            // Update cached data and log redfish event by reading reason.
            lastPanicCount = currPanicCount;
            logLastPanicEvent();
        }
    }

    uint8_t currRecoveryCount = 0;
    if (0 == intel::pfr::readCpldReg(intel::pfr::ActionType::recoveryCount,
                                     currRecoveryCount))
    {
        if (lastRecoveryCount != currRecoveryCount)
        {
            // Update cached data and log redfish event by reading reason.
            lastRecoveryCount = currRecoveryCount;
            logLastRecoveryEvent();
        }
    }

    uint8_t majorErr = 0;
    uint8_t minorErr = 0;
    if ((0 == intel::pfr::readCpldReg(intel::pfr::ActionType::majorError,
                                      majorErr)) ||
        (0 ==
         intel::pfr::readCpldReg(intel::pfr::ActionType::minorError, minorErr)))
    {
        if ((lastMajorErr != majorErr) || (lastMinorErr != minorErr))
        {
            lastMajorErr = majorErr;
            lastMinorErr = minorErr;

            if (majorErr || minorErr)
            {
                sd_journal_send(
                    "MESSAGE=%s", "Error occurred on platform firmware.",
                    "PRIORITY=%i", LOG_ERR, "REDFISH_MESSAGE_ID=%s",
                    "OpenBMC.0.1.PlatformFWErrorOccurred",
                    "REDFISH_MESSAGE_ARGS=%i,%i", majorErr, minorErr, NULL);
            }
        }
    }
}

static void monitorPlatformStateChange(
    sdbusplus::asio::object_server& server,
    std::shared_ptr<sdbusplus::asio::connection>& conn)
{
    constexpr size_t pollTimeout = 10; // seconds
    stateTimer->expires_after(std::chrono::seconds(pollTimeout));
    stateTimer->async_wait(
        [&server, &conn](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted)
            {
                // Timer reset.
                return;
            }
            if (ec)
            {
                // Platform State Monitor - Timer cancelled.
                return;
            }
            checkAndLogEvents();
            monitorPlatformStateChange(server, conn);
        });
}

int main()
{
    // setup connection to dbus
    boost::asio::io_service io;
    auto conn = std::make_shared<sdbusplus::asio::connection>(io);
    stateTimer = std::make_unique<boost::asio::steady_timer>(io);
    conn->request_name("xyz.openbmc_project.Intel.PFR.Manager");
    auto server = sdbusplus::asio::object_server(conn, true);

    // Create Intel PFR attributes object and interface
    intel::pfr::PfrConfig obj(server, conn);

    // Create Software objects using Versions interface
    for (const auto& path : listVersionPaths)
    {
        intel::pfr::PfrVersion obj(server, conn, path);
    }

    // Capture the Chassis state and Start the monitor timer
    // if state changed to 'On'. Run timer until  OS boot.
    // Stop timer if state changed to 'Off'.
    static auto matchChassisState = sdbusplus::bus::match::match(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged', "
        "interface='org.freedesktop.DBus.Properties', "
        "sender='xyz.openbmc_project.State.Chassis', "
        "arg0namespace='xyz.openbmc_project.State.Chassis'",
        [&server, &conn](sdbusplus::message::message& message) {
            std::string intfName;
            std::map<std::string, std::variant<std::string>> properties;
            message.read(intfName, properties);

            const auto it = properties.find("CurrentPowerState");
            if (it != properties.end())
            {
                const std::string* state =
                    std::get_if<std::string>(&it->second);
                if (state != nullptr)
                {
                    if ((*state ==
                         "xyz.openbmc_project.State.Chassis.PowerState.On") &&
                        (!stateTimerRunning))
                    {
                        stateTimerRunning = true;
                        monitorPlatformStateChange(server, conn);
                    }
                    else if ((*state == "xyz.openbmc_project.State.Chassis."
                                        "PowerState.Off") &&
                             (stateTimerRunning))
                    {
                        stateTimer->cancel();
                        checkAndLogEvents();
                        stateTimerRunning = false;
                    }
                }
            }
        });

    // Capture the Host state and Start the monitor timer
    // if state changed to 'Running'. Run timer until OS boot.
    // Stop timer if state changed to 'Off'.
    static auto matchHostState = sdbusplus::bus::match::match(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged', "
        "interface='org.freedesktop.DBus.Properties', "
        "sender='xyz.openbmc_project.State.Chassis', "
        "arg0namespace='xyz.openbmc_project.State.Host'",
        [&server, &conn](sdbusplus::message::message& message) {
            std::string intfName;
            std::map<std::string, std::variant<std::string>> properties;
            message.read(intfName, properties);

            const auto it = properties.find("CurrentHostState");
            if (it != properties.end())
            {
                const std::string* state =
                    std::get_if<std::string>(&it->second);
                if (state != nullptr)
                {
                    if ((*state ==
                         "xyz.openbmc_project.State.Host.HostState.Running") &&
                        (!stateTimerRunning))
                    {
                        stateTimerRunning = true;
                        monitorPlatformStateChange(server, conn);
                    }
                    else if (((*state == "xyz.openbmc_project.State.Host."
                                         "HostState.Off") ||
                              (*state == "xyz.openbmc_project.State.Host."
                                         "HostState.Quiesced")) &&
                             (stateTimerRunning))
                    {
                        stateTimer->cancel();
                        checkAndLogEvents();
                        stateTimerRunning = false;
                    }
                }
            }
        });

    // Capture the OS state change and stop monitor timer
    // if OS boots completly or becomes Inactive.
    // start timer in other cases to mnitor states.
    static auto matchOsState = sdbusplus::bus::match::match(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged', "
        "interface='org.freedesktop.DBus.Properties', "
        "sender='xyz.openbmc_project.State.Chassis', "
        "arg0namespace='xyz.openbmc_project.State.OperatingSystem.Status'",
        [&server, &conn](sdbusplus::message::message& message) {
            std::string intfName;
            std::map<std::string, std::variant<std::string>> properties;
            message.read(intfName, properties);

            const auto it = properties.find("OperatingSystemState");
            if (it != properties.end())
            {
                const std::string* state =
                    std::get_if<std::string>(&it->second);
                if (state != nullptr)
                {
                    if (((*state == "BootComplete") ||
                         (*state == "Inactive")) &&
                        (stateTimerRunning))
                    {
                        stateTimer->cancel();
                        checkAndLogEvents();
                        stateTimerRunning = false;
                    }
                    else if (!stateTimerRunning)
                    {
                        stateTimerRunning = true;
                        monitorPlatformStateChange(server, conn);
                    }
                }
            }
        });

    // First time, check and log events if any.
    checkAndLogEvents();

    phosphor::logging::log<phosphor::logging::level::INFO>(
        "Intel PFR service started successfully");

    io.run();

    return 0;
}
