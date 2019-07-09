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

constexpr const char *prefix = "/var/lib/phosphor-settings-manager/settings";

// because some paths collide with others
// i.e. /xyz/openbmc_project/host collides with /xyz/openbmc/host/foo
constexpr const char *trail = "_";

#include "utils.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

struct SettingsInterface
{

    SettingsInterface(sdbusplus::asio::object_server &objectServer,
                      const std::string &objectPath, const std::string &iface) :
        objectServer(objectServer),
        path(std::make_shared<std::string>(objectPath + trail))
    {
        interface = objectServer.add_interface(objectPath, iface);
    }
    ~SettingsInterface()
    {
        objectServer.remove_interface(interface);
    }

    template <typename T>
    std::optional<T> getOptional(const nlohmann::json &data)
    {
        std::optional<T> resp;

        if constexpr (std::is_floating_point_v<T>)
        {
            const double *val = data.get_ptr<const double *>();
            if (val != nullptr)
            {
                resp = *val;
            }
            return resp;
        }
        else if constexpr (std::is_same_v<bool, std::decay_t<T>>)
        {
            const bool *val = data.get_ptr<const bool *>();
            if (val != nullptr)
            {
                resp = *val;
            }
            return resp;
        }
        else if constexpr (std::is_unsigned_v<T>)
        {
            const uint64_t *val = data.get_ptr<const uint64_t *>();
            if (val != nullptr)
            {
                resp = *val;
            }
            return resp;
        }
        else if constexpr (std::is_signed_v<T>)
        {
            const int64_t *val = data.get_ptr<const int64_t *>();
            if (val != nullptr)
            {
                resp = *val;
            }
            return resp;
        }
        else if constexpr (is_vector_v<T>)
        {
            resp = {};
            for (const auto &val : data)
            {
                using SubType = typename T::value_type;

                std::optional<SubType> opt = getOptional<SubType>(val);
                if (!opt)
                {
                    return std::optional<T>(std::nullopt);
                }
                resp->emplace_back(*opt);
            }
            return resp;
        }
        else if constexpr (std::is_same_v<std::string, std::decay_t<T>>)
        {
            const std::string *val = data.get_ptr<const std::string *>();
            if (val != nullptr)
            {
                resp = *val;
            }
            return resp;
        }
        else
        {
            static_assert(!std::is_same_v<T, T>, "Unsupported Type");
        }
    }

    // specialization for char * as std::string is the nlohmann type
    void addProperty(const std::string &name, const char *value,
                     const bool persist = true)
    {
        addProperty(name, std::string(value), persist);
    }

    template <typename T>
    void addProperty(const std::string &name, T value,
                     const bool persist = true)
    {

        std::ifstream current(std::string(prefix) + *path);
        if (current.good())
        {
            nlohmann::json data =
                nlohmann::json::parse(current, nullptr, false);
            if (data.is_discarded())
            {
                std::cerr << "Persisted data at " << *path << " corrupted\n";
            }
            else
            {
                auto find = data.find(name);
                if (find != data.end())
                {
                    auto val = getOptional<T>(*find);
                    if (!val)
                    {
                        std::cerr << "Persisted data at " << *path
                                  << " corrupted, type changed for " << name
                                  << "\n";
                    }
                    else
                    {
                        value = *val;
                    }
                }
            }
        }

        interface->register_property(
            name, value,
            [path = std::shared_ptr<std::string>(path), name,
             persist](const T &req, T &old) {
                nlohmann::json data;

                { // context is here for raii to close the file
                    std::ifstream current(std::string(prefix) + *path);
                    if (current.good())
                    {
                        data = nlohmann::json::parse(current, nullptr, false);
                        if (data.is_discarded())
                        {
                            std::cerr << "Persisted data at " << *path
                                      << " corrupted\n";
                            throw std::runtime_error("Persisting Error");
                        }
                    }
                }

                if (persist)
                {
                    data[name] = req;
                    std::filesystem::create_directories(
                        std::filesystem::path(std::string(prefix) + *path)
                            .parent_path());
                    std::ofstream output(std::string(prefix) + *path);
                    if (!output.good())
                    {
                        std::cerr << "Cannot write data at " << *path << "\n";
                        throw std::runtime_error("Persisting Error");
                    }
                    output << data;
                }

                old = req;

                return 1;
            });
    }
    void initialize()
    {
        interface->initialize();
    }

    sdbusplus::asio::object_server &objectServer;
    std::shared_ptr<std::string> path; // shared ptr so this object can be moved
    std::shared_ptr<sdbusplus::asio::dbus_interface> interface;
};
