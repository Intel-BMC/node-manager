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

#include <boost/asio/posix/stream_descriptor.hpp>
#include <gpiod.hpp>
#include <iostream>
#include <sdbusplus/asio/object_server.hpp>

namespace host_error_monitor
{
static boost::asio::io_service io;
static std::shared_ptr<sdbusplus::asio::connection> conn;

static bool hostOff = true;

const static constexpr int caterrTimeoutMs = 1000;
const static constexpr int crashdumpTimeoutS = 300;

// Timers
// Timer for CATERR asserted
static boost::asio::steady_timer caterrAssertTimer(io);

// GPIO Lines and Event Descriptors
static gpiod::line caterrLine;
static boost::asio::posix::stream_descriptor caterrEvent(io);
//----------------------------------
// PCH_BMC_THERMTRIP function related definition
//----------------------------------
// GPIO Lines and Event Descriptors
static gpiod::line pchThermtripLine;
static boost::asio::posix::stream_descriptor pchThermtripEvent(io);

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

            // No host events should fire while off, so cancel any pending
            // timers
            if (hostOff)
            {
                caterrAssertTimer.cancel();
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

static void caterrAssertHandler()
{
    std::cout << "CPU CATERR detected, starting timer\n";
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
            std::cout << "CATERR assert timer canceled\n";
            return;
        }
        std::cout << "CATERR asset timer completed\n";
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
static void pchThermtripHandler()
{
    if (!hostOff)
    {
        gpiod::line_event gpioLineEvent = pchThermtripLine.event_read();

        bool pchThermtrip =
            gpioLineEvent.event_type == gpiod::line_event::FALLING_EDGE;
        if (pchThermtrip)
        {
            std::cout << "PCH Thermtrip detected \n";
            // log to redfish, call API
            sd_journal_send("MESSAGE=SSBThermtrip: SSB Thermtrip",
                            "PRIORITY=%i", LOG_INFO, "REDFISH_MESSAGE_ID=%s",
                            "OpenBMC.0.1.SSBThermtrip", NULL);
        }
    }
    pchThermtripEvent.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [](const boost::system::error_code ec) {
            if (ec)
            {
                std::cerr << "PCH Thermtrip handler error: " << ec.message()
                          << "\n";
                return;
            }
            pchThermtripHandler();
        });
}

static void initializeErrorState()
{
    // Handle CPU_CATERR if it's asserted now
    if (caterrLine.get_value() == 0)
    {
        caterrAssertHandler();
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
