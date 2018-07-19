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

#include <xyz/openbmc_project/Common/UUID/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Revision/server.hpp>
#include "smbios.hpp"

namespace phosphor
{

namespace smbios
{

class System : sdbusplus::server::object::object<
                   sdbusplus::xyz::openbmc_project::Common::server::UUID>,
               sdbusplus::server::object::object<
                   sdbusplus::xyz::openbmc_project::Inventory::Decorator::
                       server::Revision>
{
  public:
    System() = delete;
    ~System() = default;
    System(const System &) = delete;
    System &operator=(const System &) = delete;
    System(System &&) = default;
    System &operator=(System &&) = default;

    System(sdbusplus::bus::bus &bus, const std::string &objPath,
           struct ManagedDataRegion *region) :
        sdbusplus::server::object::object<
            sdbusplus::xyz::openbmc_project::Common::server::UUID>(
            bus, objPath.c_str()),
        sdbusplus::server::object::object<
            sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::
                Revision>(bus, objPath.c_str()),
        path(objPath), regionS(region)
    {
        std::string input = "0";
        uUID(input);
        version("0.00");
    }

    std::string uUID(std::string value) override;

    std::string version(std::string value) override;

  private:
    /** @brief Path of the group instance */
    std::string path;

    struct ManagedDataRegion *regionS;
};

} // namespace smbios

} // namespace phosphor
