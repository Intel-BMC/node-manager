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
#include <sdbusplus/timer.hpp>
#include "utils.hpp"

namespace phosphor
{
namespace service
{

static constexpr const char *serviceConfigSrvName =
    "xyz.openbmc_project.Control.Service.Manager";
static constexpr const char *serviceConfigIntfName =
    "xyz.openbmc_project.Control.Service.Attributes";

enum class UpdatedProp
{
    port = 1,
    channel,
    state
};

class ServiceConfig
{
  public:
    ServiceConfig(sdbusplus::asio::object_server &srv_,
                  std::shared_ptr<sdbusplus::asio::connection> &conn_,
                  std::string objPath_, std::string unitName);
    ~ServiceConfig() = default;

    void applySystemDServiceConfig();
    void startServiceRestartTimer();

    std::shared_ptr<sdbusplus::asio::connection> conn;
    uint8_t updatedFlag;

  private:
    sdbusplus::asio::object_server &server;
    std::string objPath;

    uint16_t portNum;
    std::string protocol;
    std::string stateValue;
    std::vector<std::string> channelList;

    void registerProperties();
    std::string sysDUnitName;
    std::string unitSocketFilePath;
    std::string sysDSockObjPath;

    void syncWithSystemD1Properties();
};

} // namespace service
} // namespace phosphor
