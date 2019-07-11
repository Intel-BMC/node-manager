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

#include <boost/asio/deadline_timer.hpp>
#include <sdbusplus/asio/object_server.hpp>

static constexpr const char* strSpecialMode = "SpecialMode";

enum SpecialMode : uint8_t
{
    None = 0,
    ManufacturingExpired = 1,
    ManufacturingMode = 2
};

class SpecialModeMgr
{
    boost::asio::io_service& io;
    sdbusplus::asio::object_server& server;
    std::shared_ptr<sdbusplus::asio::connection> conn;
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;
    uint8_t specialMode = None;
    std::unique_ptr<boost::asio::deadline_timer> timer = nullptr;
    void AddSpecialModeProperty();

  public:
    uint8_t SetSpecialModeValue(uint8_t value) const
    {
        return iface->set_property(strSpecialMode, value);
    }
    SpecialModeMgr(boost::asio::io_service& io,
                   sdbusplus::asio::object_server& srv,
                   std::shared_ptr<sdbusplus::asio::connection>& conn);
};
