/**
 * Copyright (c) 2017 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "argument.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <iterator>
#include <string>

ArgumentParser::ArgumentParser(int argc, char** argv)
{
    auto option = 0;

    while (-1 != (option = getopt_long(argc, argv, optionstr, options, NULL)))
    {
        if ((option == '?') || (option == 'h'))
        {
            usage(argv);
            exit(-1);
        }

        auto i = &options[0];
        while ((i->val != option) && (i->val != 0))
        {
            ++i;
        }

        if (i->val)
        {
            arguments[i->name] = (i->has_arg ? optarg : true_string);
        }
    }
}

const std::string& ArgumentParser::operator[](const std::string& opt)
{
    auto i = arguments.find(opt);
    if (i == arguments.end())
    {
        return empty_string;
    }
    else
    {
        return i->second;
    }
}

void ArgumentParser::usage(char** argv)
{
    std::cerr << "Usage: " << argv[0] << " [options]\n";
    std::cerr << "Options:\n";
    std::cerr << "    --help                Print this menu\n";
    std::cerr << "    --path=<Dbus Object path> Dbus object path.\n"
              << "                          Ex: /org/openbmc/control/power0\n";

    std::cerr
            << "    --service=<Dbus Service name> Dbus Service name.\n"
            << "                          Ex: org.openbmc.control.Power\n";
    std::cerr
           << "    --interface=<Dbus interface name> Dbus interface name.\n"
           << "                          Ex: org.openbmc.control.Power\n";
    std::cerr
            << "    --property=<Dbus property name> Dbus property name.\n"
            << "                          Ex: pgood\n";

    std::cerr
            << "    --expect=<expected value> Dbus property value.\n"
            << "                          Ex: 1\n";

    std::cerr << std::flush;
}

const option ArgumentParser::options[] =
{
    {"path", required_argument, NULL, 'p'},
    {"service", required_argument, NULL, 's'},
    {"interface", required_argument, NULL, 'i'},
    {"property", required_argument, NULL, 'r'},
    {"expect", required_argument, NULL, 'e'},
    {"help", no_argument, NULL, 'h'},
    {0, 0, 0, 0},
};

const char* ArgumentParser::optionstr = "p:s:i:r:e:?h";

const std::string ArgumentParser::true_string = "true";
const std::string ArgumentParser::empty_string = "";
