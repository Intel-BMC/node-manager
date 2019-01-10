/*
// Copyright (c) 2018 Intel Corporation
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

#include "gpiodaemon.hpp"

#include "gpioutils.hpp"

#include <boost/algorithm/string/replace.hpp>
#include <regex>

static constexpr const char* entityMgrService =
    "xyz.openbmc_project.EntityManager";
static constexpr const char* gpioService = "xyz.openbmc_project.Gpio";
static constexpr const char* gpioConfigInterface =
    "xyz.openbmc_project.Configuration.Gpio";
static constexpr const char* gpioInterface = "xyz.openbmc_project.Control.Gpio";
static constexpr const char* gpioPath = "/xyz/openbmc_project/control/gpio/";
static constexpr const char* sysGpioPath = "/sys/class/gpio/gpio";
static constexpr const char* propInterface = "org.freedesktop.DBus.Properties";

static constexpr const char* objPath = "/xyz/openbmc_project/object_mapper";
static constexpr const char* objService = "xyz.openbmc_project.ObjectMapper";
static constexpr const char* objInterface = "xyz.openbmc_project.ObjectMapper";

const static constexpr size_t waitTime = 2; // seconds

using BasicVariantType =
    sdbusplus::message::variant<std::string, int64_t, uint64_t, double, int32_t,
                                uint32_t, int16_t, uint16_t, uint8_t, bool>;

void GpioManager::addObject(const std::string& path)
{
    conn->async_method_call(
        [this,
         path](boost::system::error_code ec,
               const boost::container::flat_map<std::string, BasicVariantType>&
                   result) {
            if (ec)
            {
                phosphor::logging::log<phosphor::logging::level::INFO>(
                    ("ERROR with async_method_call path: " + path).c_str());
            }
            else
            {
                auto nameFind = result.find("Name");
                auto indexFind = result.find("Index");
                auto polarityFind = result.find("Polarity");

                if (nameFind == result.end() || indexFind == result.end() ||
                    polarityFind == result.end())
                {
                    phosphor::logging::log<phosphor::logging::level::INFO>(
                        "ERROR accessing entity manager.");
                    return;
                }

                uint64_t index = sdbusplus::message::variant_ns::get<uint64_t>(
                    indexFind->second);
                std::string gpioName =
                    sdbusplus::message::variant_ns::get<std::string>(
                        nameFind->second);
                bool inverted =
                    sdbusplus::message::variant_ns::get<std::string>(
                        polarityFind->second) == "Low";

                boost::replace_all(gpioName, " ", "_");

                // Add gpio entries to be managed by gpiodaemon
                gpioEnableList.push_back(
                    GpioEn(gpioName, (uint16_t)index, false));

                // Read present gpio data from /sys/class/gpio/...
                Gpio gpio = Gpio(std::to_string(index));

                // Add path to server object
                iface =
                    server.add_interface(gpioPath + gpioName, gpioInterface);

                /////////////////////////////
                // Set generic properties:
                //    - enabled - when set to false other properties like
                //    value and direction
                //                cannot be overriden
                //    - value
                //    - direction
                //    - polarity
                /////////////////////////////
                iface->register_property(
                    "Enabled", true,
                    [this, gpioName](const bool& req, bool& propertyValue) {
                        GpioEn* dbusGpio = findGpioObj(gpioName);
                        if (dbusGpio)
                        {
                            dbusGpio->setEnabled(req);
                            propertyValue = req;
                            return 1;
                        }
                        return 0;
                    });

                bool value = static_cast<bool>(gpio.getValue());
                if (inverted)
                {
                    value = !value;
                }
                iface->register_property(
                    "Value", value,
                    // Override set
                    [this, gpioName, inverted](const bool& req,
                                               bool& propertyValue) {
                        GpioEn* dbusGpio = findGpioObj(gpioName);

                        if (dbusGpio)
                        {
                            // If Gpio enabled property is set as true then
                            // reject set request
                            if (dbusGpio->getEnabled())
                            {
                                Gpio gpio =
                                    Gpio(std::to_string(dbusGpio->getNumber()));
                                bool setVal = req;
                                if (inverted)
                                {
                                    setVal = !setVal;
                                }
                                gpio.setValue(static_cast<GpioValue>(setVal));

                                propertyValue = setVal;
                                return 1;
                            }
                            return 0;
                        }
                        return 0;
                    },
                    // Override get
                    [this, gpioName](const bool& propertyValue) {
                        bool value;
                        if (getGpioStateValue(gpioName, value))
                        {
                            return value;
                        }
                        else // Gpio with direction "out" are not monitored.
                             // Return last know state.
                        {
                            GpioEn* dbuGpio = findGpioObj(gpioName);
                            if (dbuGpio)
                            {
                                return dbuGpio->getValue();
                            }
                        }
                    });

                iface->register_property(
                    "Direction", gpio.getDirection(),
                    // Override set
                    [this, gpioName](const std::string& req,
                                     std::string& propertyValue) {
                        GpioEn* dbusGpio = findGpioObj(gpioName);

                        if (dbusGpio)
                        {
                            // If Gpio enabled property is set as true than
                            // reject request
                            if (dbusGpio->getEnabled())
                            {
                                Gpio gpio =
                                    Gpio(std::to_string(dbusGpio->getNumber()));
                                gpio.setDirection(req);

                                propertyValue = req;
                                return 1;
                            }
                            return 0;
                        }
                        return 0;
                    },
                    // Override get
                    [this, index](const std::string& propertyDirection) {
                        // Read present gpio data from /sys/class/gpio/...
                        Gpio gpio = Gpio(std::to_string(index));
                        return gpio.getDirection();
                    });
                iface->initialize();

                // Monitor gpio value changes
                gpioMonitorList.push_back(std::make_unique<GpioState>(
                    gpioName, index, inverted, io, iface));
            }
        },
        entityMgrService, path, propInterface, "GetAll", gpioConfigInterface);
}

GpioManager::GpioManager(boost::asio::io_service& io_,
                         sdbusplus::asio::object_server& srv_,
                         std::shared_ptr<sdbusplus::asio::connection>& conn_) :
    io(io_),
    server(srv_), conn(conn_)
{
    using GetSubTreeType = std::vector<std::pair<
        std::string,
        std::vector<std::pair<std::string, std::vector<std::string>>>>>;
    constexpr int32_t scanDepth = 3;

    conn->async_method_call(
        [this](boost::system::error_code ec, GetSubTreeType& subtree) {
            if (ec)
            {
                phosphor::logging::log<phosphor::logging::level::INFO>(
                    "ERROR with async_method_call to ObjectMapper");
                return;
            }

            for (const auto& objectPath : subtree)
            {
                for (const auto& objectInterfaces : objectPath.second)
                {
                    for (const auto& interface : objectInterfaces.second)
                    {
                        if (gpioConfigInterface == interface)
                        {
                            addObject(objectPath.first);
                        }
                    }
                }
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/inventory/system", scanDepth,
        std::vector<std::string>());
}

GpioState::GpioState(const std::string gpioName_, const uint16_t gpioNumber,
                     const bool inverted_, boost::asio::io_service& io_,
                     std::shared_ptr<sdbusplus::asio::dbus_interface>& iface_) :
    gpioName(gpioName_),
    inverted(inverted_), inputDev(io_), iface(iface_)
{
    // todo: implement gpio device character access
    std::string device = sysGpioPath + std::to_string(gpioNumber);

    fdValue = open((device + "/value").c_str(), O_RDONLY);
    if (fdValue < 0)
    {
        phosphor::logging::log<phosphor::logging::level::INFO>(
            ("GpioState: Error opening " + device).c_str());
        return;
    }

    std::ofstream deviceFileEdge(device + "/edge");
    if (!deviceFileEdge.good())
    {
        phosphor::logging::log<phosphor::logging::level::INFO>(
            ("GpioState:Error setting edge for" + device).c_str());
        return;
    }
    deviceFileEdge << "both"; // Will success only for gpio.direction == "in"
    deviceFileEdge.close();

    inputDev.assign(boost::asio::ip::tcp::v4(), fdValue);
    monitor();
    readValue();
}

GpioState::~GpioState()
{
    inputDev.close();
    close(fdValue);
}

void GpioState::monitor(void)
{
    inputDev.async_wait(
        boost::asio::ip::tcp::socket::wait_error,
        [this](const boost::system::error_code& ec) {
            if (ec == boost::system::errc::bad_file_descriptor)
            {
                return; // we're being destroyed
            }
            else if (ec)
            {
                phosphor::logging::log<phosphor::logging::level::INFO>(
                    ("GpioState:monitor: Error on presence sensor socket"));
            }
            else
            {
                readValue();
            }
            monitor();
        });
}

void GpioState::readValue(void)
{
    constexpr size_t readSize = sizeof("0");

    std::string readBuf;
    readBuf.resize(readSize);
    lseek(fdValue, 0, SEEK_SET);

    size_t r = ::read(fdValue, readBuf.data(), readSize);
    bool value = std::stoi(readBuf);
    if (inverted)
    {
        value = !value;
    }
    state = value;

    // Broadcast value changed property signal
    iface->signal_property("Value");
}

int main()
{
    boost::asio::io_service io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    systemBus->request_name(gpioService);
    sdbusplus::asio::object_server server(systemBus);

    GpioManager gpioMgr(io, server, systemBus);

    static auto match = std::make_unique<sdbusplus::bus::match::match>(
        static_cast<sdbusplus::bus::bus&>(*systemBus),
        "type='signal',member='PropertiesChanged',"
        "arg0namespace='" +
            std::string(gpioConfigInterface) + "'",
        [&gpioMgr](sdbusplus::message::message& message) {
            phosphor::logging::log<phosphor::logging::level::INFO>(
                "New configuration detected. Updating gpiodaemon.");

            gpioMgr.addObject(std::string(message.get_path()));
            // TODO: Distinct and handle adequately whether entity-manager is
            // adding or deleting gpio related entries. For now just addObject.
        });

    io.run();
}
