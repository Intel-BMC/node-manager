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

#pragma once
#include <boost/asio.hpp>
#include <experimental/filesystem>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/asio/object_server.hpp>

class GpioEn
{
    std::string name;
    uint16_t number;
    bool enabled;
    bool value;

  public:
    GpioEn(const std::string& name_, uint16_t number_, bool enabled_) :
        name(name_), number(number_), enabled(enabled_){};

    std::string getName() const
    {
        return name;
    }
    uint16_t getNumber() const
    {
        return number;
    }
    bool getEnabled() const
    {
        return enabled;
    }
    void setEnabled(bool value)
    {
        enabled = value;
    }
    bool getValue() const
    {
        return value;
    }
    void setvalue(bool val)
    {
        value = val;
    }
};

class GpioState
{
    bool state = true;
    const bool inverted;
    const std::string gpioName;
    int fdValue;

    boost::asio::ip::tcp::socket inputDev;
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;

    void monitor(void);
    void readValue(void);

  public:
    GpioState(const std::string gpioName, const uint16_t gpioNumber,
              const bool inverted, boost::asio::io_service& io,
              std::shared_ptr<sdbusplus::asio::dbus_interface>& iface);
    GpioState(GpioState&&) = default;
    GpioState& operator=(GpioState&&) = default;
    ~GpioState();

    std::string getName() const
    {
        return gpioName;
    }
    bool getValue(void) const
    {
        return state;
    }
};

class GpioManager
{
    boost::asio::io_service& io;
    sdbusplus::asio::object_server& server;
    std::shared_ptr<sdbusplus::asio::connection> conn;
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;

    std::vector<GpioEn> gpioEnableList;
    std::vector<std::unique_ptr<GpioState>> gpioMonitorList;

    GpioEn* findGpioObj(const std::string& gpioName)
    {
        auto el = std::find_if(gpioEnableList.begin(), gpioEnableList.end(),
                               [&gpioName](const GpioEn& obj) {
                                   return obj.getName() == gpioName;
                               });
        if (el != gpioEnableList.end())
        {
            return &(*el);
        }
        return nullptr;
    }

    bool getGpioStateValue(const std::string& gpioName, bool& value)
    {
        for (const auto& gpio : gpioMonitorList)
        {
            if (gpio->getName() == gpioName)
            {
                value = gpio->getValue();
                return true;
            }
        }
        return false;
    }

  public:
    GpioManager(boost::asio::io_service& io,
                sdbusplus::asio::object_server& srv,
                std::shared_ptr<sdbusplus::asio::connection>& conn);

    void addObject(const std::string& path);
};
