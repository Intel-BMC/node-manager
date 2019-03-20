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

#include <boost/algorithm/string.hpp>
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
                auto directionFind = result.find("Direction");

                if (nameFind == result.end() || indexFind == result.end() ||
                    polarityFind == result.end() ||
                    directionFind == result.end())
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
                std::string direction =
                    sdbusplus::message::variant_ns::get<std::string>(
                        directionFind->second);

                boost::replace_all(gpioName, " ", "_");
                boost::algorithm::to_lower(direction);
                if (gpioMonitorList.find(gpioName) != gpioMonitorList.end())
                {
                    phosphor::logging::log<phosphor::logging::level::ERR>(
                        "ERROR Object already preset",
                        phosphor::logging::entry("GPIO_NAME=%s",
                                                 gpioName.c_str()));
                    return;
                }
                // Add path to server object
                iface =
                    server.add_interface(gpioPath + gpioName, gpioInterface);

                // Monitor gpio value changes
                gpioMonitorList.emplace(gpioName, std::make_unique<GpioState>(
                                                      gpioName, index, inverted,
                                                      direction, io, iface));
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

GpioState::GpioState(const std::string& name_, const uint64_t& number_,
                     const bool inverted_, const std::string& direction_,
                     boost::asio::io_service& io_,
                     std::shared_ptr<sdbusplus::asio::dbus_interface>& iface_) :
    name(name_),
    number(number_), inverted(inverted_), direction(direction_), inputDev(io_),
    iface(iface_), gpio(std::to_string(number_))
{

    // Initialize gpio direction as specified by entity-manager
    gpio.setDirection(direction);

    // Property used to control whether gpio pin monitoring / control
    // has to be ignored. Once set to true, gpio daemon will
    // return the cached value. Any real sampling of data can be perfomed
    // by reading the gpio lower level API directly.
    // TODO: When set to ignore disable pass-through by default??
    iface->register_property(
        "Ignore", ignore,
        // Override set
        [this](const bool& req, bool& propertyValue) {
            if (ignore != req)
            {
                ignore = req;
                propertyValue = req;
                return 1;
            }
            return 0;
        },
        // Override get
        [this](const bool& propertyDirection) { return ignore; });
    iface->register_property(
        "Direction", direction,
        // Override set
        [this](const std::string& req, std::string& propertyValue) {
            if (!ignore && direction != req)
            {
                direction = req;
                this->gpio.setDirection(req);

                propertyValue = req;
                return 1;
            }
            return 0;
        },
        // Override get
        [this](const std::string& propertyValue) {
            if (!ignore)
            {
                direction = this->gpio.getDirection();
            }
            return direction;
        });

    value = static_cast<bool>(gpio.getValue());
    if (inverted)
    {
        value = !value;
    }
    iface->register_property(
        "Value", value,
        // Override set
        [this](const bool& req, bool& propertyValue) {
            if ((!ignore && gpio.getDirection() == "out") || internalSet)
            {
                bool setVal = req;
                if (value != setVal)
                {
                    value = setVal;
                    propertyValue = setVal;
                    if (!internalSet)
                    {
                        if (inverted)
                        {
                            setVal = !setVal;
                        }
                        gpio.setValue(static_cast<GpioValue>(setVal));
                    }
                    return 1;
                }
            }
            return 0;
        },
        // Override get
        [this](const bool& propertyValue) { return value; });
    iface->register_property(
        "SampledValue", value,
        // Override set - ignore set
        [this](const bool& req, bool& propertyValue) { return 0; },
        // Override get
        [this](const bool& propertyValue) {
            return (inverted ? !static_cast<bool>(gpio.getValue())
                             : static_cast<bool>(gpio.getValue()));
        });

    iface->initialize(true);

    // TODO: implement gpio device character access in gpioutils.cpp
    // and make use of gpioutils file to read directly
    std::string device = sysGpioPath + std::to_string(number);
    fdValue = open((device + "/value").c_str(), O_RDONLY);
    if (fdValue < 0)
    {
        phosphor::logging::log<phosphor::logging::level::INFO>(
            ("GpioState: Error opening " + device).c_str());
        return;
    }
    if (gpio.getDirection() == "in")
    {
        std::ofstream deviceFileEdge(device + "/edge");
        if (!deviceFileEdge.good())
        {
            phosphor::logging::log<phosphor::logging::level::INFO>(
                ("GpioState:Error setting edge for" + device).c_str());
            return;
        }
        deviceFileEdge << "both";
        deviceFileEdge.close();
    }
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
    if (ignore)
    {
        return;
    }
    constexpr size_t readSize = sizeof("0");

    std::string readBuf;
    readBuf.resize(readSize);
    lseek(fdValue, 0, SEEK_SET);
    size_t r = ::read(fdValue, readBuf.data(), readSize);
    bool state = std::stoi(readBuf);
    internalSet = true;
    // Update the property value
    iface->set_property("Value", inverted ? !state : state);
    internalSet = false;
}

int main()
{
    boost::asio::io_service io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    systemBus->request_name(gpioService);
    sdbusplus::asio::object_server server(systemBus);

    GpioManager gpioMgr(io, server, systemBus);
    // TODO: Expose a manager object to handle disable / enable passthrough ?

    static auto match = std::make_unique<sdbusplus::bus::match::match>(
        static_cast<sdbusplus::bus::bus&>(*systemBus),
        "type='signal',member='InterfacesAdded',sender='xyz.openbmc_project."
        "EntityManager'",
        [&gpioMgr](sdbusplus::message::message& message) {
            using EntityMgrObjectTypes =
                std::map<std::string,
                         std::vector<std::pair<std::string, BasicVariantType>>>;
            sdbusplus::message::object_path objectPath;
            EntityMgrObjectTypes objects;
            message.read(objectPath, objects);
            for (const auto& objectInterfaces : objects)
            {
                if (gpioConfigInterface == objectInterfaces.first)
                {
                    phosphor::logging::log<phosphor::logging::level::INFO>(
                        "New configuration detected. Updating gpiodaemon.");
                    gpioMgr.addObject(objectPath.str);
                }
            }
            // TODO: Distinct and handle adequately whether entity-manager is
            // adding or deleting gpio related entries. For now just registered
            // for
            //.InterfacesAdded signal alone.
        });

    io.run();
}
