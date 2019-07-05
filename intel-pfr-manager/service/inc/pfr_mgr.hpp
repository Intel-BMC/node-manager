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

#pragma once

#include <string>
#include <sdbusplus/asio/object_server.hpp>
#include <phosphor-logging/log.hpp>
#include <boost/asio.hpp>

namespace intel
{
namespace pfr
{

class PfrVersion
{
  public:
    PfrVersion(sdbusplus::asio::object_server &srv_,
               std::shared_ptr<sdbusplus::asio::connection> &conn_,
               const std::string &path_);
    ~PfrVersion() = default;

    std::shared_ptr<sdbusplus::asio::connection> conn;

  private:
    sdbusplus::asio::object_server &server;

    std::string path;
    std::string version;
    std::string purpose;
};

class PfrConfig
{
  public:
    PfrConfig(sdbusplus::asio::object_server &srv_,
              std::shared_ptr<sdbusplus::asio::connection> &conn_);
    ~PfrConfig() = default;

    std::shared_ptr<sdbusplus::asio::connection> conn;

  private:
    sdbusplus::asio::object_server &server;

    bool getPFRProvisionedState();
    std::string getBIOSVersion(uint8_t type);
    std::string getBMCVersion(uint8_t type);
};

} // namespace pfr
} // namespace intel
