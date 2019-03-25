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
#include "gpioutils.hpp"

#include <boost/asio.hpp>
#include <experimental/filesystem>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/asio/object_server.hpp>

class GpioState
{
    const std::string name;
    uint64_t number;
    const bool inverted;
    bool value;
    std::string direction;
    bool ignore = false;
    bool internalSet = false;

    int fdValue;
    boost::asio::ip::tcp::socket inputDev;
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;

    void monitor(void);
    void readValue(void);

  public:
    GpioState(const std::string& name_, const uint64_t& number_,
              const bool inverted_, const std::string& direction_,
              boost::asio::io_service& io_,
              std::shared_ptr<sdbusplus::asio::dbus_interface>& iface_);
    ~GpioState();
    Gpio gpio;
};

class GpioManager
{
    boost::asio::io_service& io;
    sdbusplus::asio::object_server& server;
    std::shared_ptr<sdbusplus::asio::connection> conn;
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;

    std::map<std::string, std::unique_ptr<GpioState>> gpioMonitorList;

  public:
    GpioManager(boost::asio::io_service& io,
                sdbusplus::asio::object_server& srv,
                std::shared_ptr<sdbusplus::asio::connection>& conn);

    void addObject(const std::string& path);
};
