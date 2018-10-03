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
#include <ctime>
#include <chrono>
#include <string>
#include <sdbusplus/asio/object_server.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <experimental/filesystem>
#include <boost/asio.hpp>

static constexpr const char *sysdActionStartUnit = "StartUnit";
static constexpr const char *sysdActionStopUnit = "StopUnit";

void systemdDaemonReload(
    const std::shared_ptr<sdbusplus::asio::connection> &conn);

void systemdUnitAction(const std::shared_ptr<sdbusplus::asio::connection> &conn,
                       const std::string &unitName,
                       const std::string &actionMethod);

void systemdUnitFileStateChange(
    const std::shared_ptr<sdbusplus::asio::connection> &conn,
    const std::string &unitName, const std::string &unitState);

bool checkSystemdUnitExist(const std::string &unitName);
