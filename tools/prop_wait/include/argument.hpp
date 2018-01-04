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

#pragma once

#include <getopt.h>
#include <map>
#include <string>

/**
 * Parses command line arguments.
 */
class ArgumentParser
{
    public:
        ArgumentParser(int argc, char** argv);
        ArgumentParser() = delete;
        ArgumentParser(const ArgumentParser&) = delete;
        ArgumentParser(ArgumentParser&&) = default;
        ArgumentParser& operator=(const ArgumentParser&) = delete;
        ArgumentParser& operator=(ArgumentParser&&) = default;
        ~ArgumentParser() = default;
        const std::string& operator[](const std::string& opt);

        static void usage(char** argv);

        static const std::string true_string;
        static const std::string empty_string;

    private:
        std::map<const std::string, std::string> arguments;

        static const option options[];
        static const char* optionstr;
};
