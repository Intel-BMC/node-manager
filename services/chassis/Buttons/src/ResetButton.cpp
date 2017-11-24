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
#include <chrono>
#include <experimental/filesystem>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <sys/stat.h>
#include <sys/types.h>
#include <systemd/sd-event.h>
#include <unistd.h>
#include <xyz/openbmc_project/Common/error.hpp>
#include "ResetButton.hpp"

const static constexpr char *SYSMGR_SERVICE = "org.openbmc.managers.System";
const static constexpr char *SYSMGR_OBJ_PATH = "/org/openbmc/managers/System";
const static constexpr char *SYSMGR_INTERFACE = "org.openbmc.managers.System";

bool ResetButton::simPress()
{
    pressed();
    return true;
}