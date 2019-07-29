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
#include "interface.hpp"

// this file is vendor specific, other vendors should replace this file using a
// bbappend

inline void loadSettings(sdbusplus::asio::object_server &objectServer,
                         std::vector<SettingsInterface> &settings)
{

    SettingsInterface *setting = nullptr;
    setting = &settings.emplace_back(
        objectServer,
        "/xyz/openbmc_project/control/minimum_ship_level_required",
        "xyz.openbmc_project.Control.MinimumShipLevel");

    setting->addProperty("MinimumShipLevelRequired", true);

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/control/host0/auto_reboot",
        "xyz.openbmc_project.Control.Boot.RebootPolicy");

    setting->addProperty("AutoReboot", false);

    setting = &settings.emplace_back(objectServer,
                                     "/xyz/openbmc_project/control/host0/boot",
                                     "xyz.openbmc_project.Control.Boot.Source");

    setting->addProperty(
        "BootSource",
        "xyz.openbmc_project.Control.Boot.Source.Sources.Default");

    setting = &settings.emplace_back(objectServer,
                                     "/xyz/openbmc_project/control/host0/boot",
                                     "xyz.openbmc_project.Control.Boot.Mode");

    setting->addProperty("BootMode",
                         "xyz.openbmc_project.Control.Boot.Mode.Modes.Regular");

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/control/host0/boot/one_time",
        "xyz.openbmc_project.Control.Boot.Source");

    setting->addProperty(
        "BootSource",
        "xyz.openbmc_project.Control.Boot.Source.Sources.Default");

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/control/host0/boot/one_time",
        "xyz.openbmc_project.Control.Boot.Mode");

    setting->addProperty("BootMode",
                         "xyz.openbmc_project.Control.Boot.Mode.Modes.Regular");

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/control/host0/boot/one_time",
        "xyz.openbmc_project.Object.Enable");

    setting->addProperty("Enabled", true);

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/control/host0/power_cap",
        "xyz.openbmc_project.Control.Power.Cap");

    setting->addProperty("PowerCap", static_cast<uint32_t>(0));
    setting->addProperty("PowerCapEnable", false);

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/control/host0/power_restore_policy",
        "xyz.openbmc_project.Control.Power.RestorePolicy");

    setting->addProperty(
        "PowerRestorePolicy",
        "xyz.openbmc_project.Control.Power.RestorePolicy.Policy.AlwaysOff");

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/control/power_restore_delay",
        "xyz.openbmc_project.Control.Power.RestoreDelay");

    setting->addProperty("PowerRestoreDelay", static_cast<uint16_t>(0));

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/control/host0/acpi_power_state",
        "xyz.openbmc_project.Control.Power.ACPIPowerState");

    setting->addProperty(
        "SysACPIStatus",
        "xyz.openbmc_project.Control.Power.ACPIPowerState.ACPI.Unknown");
    setting->addProperty(
        "DevACPIStatus",
        "xyz.openbmc_project.Control.Power.ACPIPowerState.ACPI.Unknown");

    setting =
        &settings.emplace_back(objectServer, "/xyz/openbmc_project/time/owner",
                               "xyz.openbmc_project.Time.Owner");

    setting->addProperty("TimeOwner",
                         "xyz.openbmc_project.Time.Owner.Owners.BMC");

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/time/sync_method",
        "xyz.openbmc_project.Time.Synchronization");

    setting->addProperty("TimeSyncMethod",
                         "xyz.openbmc_project.Time.Synchronization.Method.NTP");

    setting = &settings.emplace_back(objectServer,
                                     "/xyz/openbmc_project/network/host0/intf",
                                     "xyz.openbmc_project.Network.MACAddress");

    setting->addProperty("MACAddress", "00:00:00:00:00:00");

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/network/host0/intf/addr",
        "xyz.openbmc_project.Network.IP");

    setting->addProperty("Address", "0.0.0.0");
    setting->addProperty("PrefixLength", static_cast<uint8_t>(0));
    setting->addProperty("Origin",
                         "xyz.openbmc_project.Network.IP.AddressOrigin.Static");
    setting->addProperty("Gateway", "0.0.0.0");
    setting->addProperty("Type",
                         "xyz.openbmc_project.Network.IP.Protocol.IPv4");

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/control/host0/TPMEnable",
        "xyz.openbmc_project.Control.TPM.Policy");

    setting->addProperty("TPMEnable", false);

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/control/power_supply_redundancy",
        "xyz.openbmc_project.Control.PowerSupplyRedundancy");

    setting->addProperty("PowerSupplyRedundancyEnabled", true);

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/control/host0/turbo_allowed",
        "xyz.openbmc_project.Control.Host.TurboAllowed");

    setting->addProperty("TurboAllowed", true);

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/control/host0/systemGUID",
        "xyz.openbmc_project.Common.UUID");

    setting->addProperty("UUID", "00000000-0000-0000-0000-000000000000");

    setting = &settings.emplace_back(objectServer, "/xyz/openbmc_project/bios",
                                     "xyz.openbmc_project.Inventory.Item.Bios");

    setting->addProperty("BiosId", "NA");

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/control/processor_error_config",
        "xyz.openbmc_project.Control.Processor.ErrConfig");

    setting->addProperty("ResetOnCATERR", false);
    setting->addProperty("ResetOnERR2", false);
    setting->addProperty("ErrorCountCPU1", static_cast<uint8_t>(0));
    setting->addProperty("ErrorCountCPU2", static_cast<uint8_t>(0));
    setting->addProperty("ErrorCountCPU3", static_cast<uint8_t>(0));
    setting->addProperty("ErrorCountCPU4", static_cast<uint8_t>(0));
    setting->addProperty("CrashdumpCount", static_cast<uint8_t>(0));

    setting = &settings.emplace_back(
        objectServer, "/com/intel/control/ocotshutdown_policy_config",
        "com.intel.Control.OCOTShutdownPolicy");

    setting->addProperty(
        "OCOTPolicy",
        "com.intel.Control.OCOTShutdownPolicy.Policy.NoShutdownOnOCOT");

    setting =
        &settings.emplace_back(objectServer, "/com/intel/control/NMISource",
                               "com.intel.Control.NMISource");

    setting->addProperty("BMCSource",
                         "com.intel.Control.NMISource.BMCSourceSignal.None");

    setting->addProperty("Enabled", true);

    setting = &settings.emplace_back(objectServer,
                                     "/xyz/openbmc_project/state/chassis0",
                                     "xyz.openbmc_project.State.PowerOnHours");

    setting->addProperty("POHCounter", static_cast<uint32_t>(0));

    setting = &settings.emplace_back(
        objectServer,
        "/xyz/openbmc_project/control/chassis_capabilities_config",
        "xyz.openbmc_project.Control.ChassisCapabilities");

    setting->addProperty("CapabilitiesFlags", static_cast<uint8_t>(0));
    setting->addProperty("FRUDeviceAddress", static_cast<uint8_t>(32));
    setting->addProperty("SDRDeviceAddress", static_cast<uint8_t>(32));
    setting->addProperty("SELDeviceAddress", static_cast<uint8_t>(32));
    setting->addProperty("SMDeviceAddress", static_cast<uint8_t>(32));
    setting->addProperty("BridgeDeviceAddress", static_cast<uint8_t>(32));

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/control/thermal_mode",
        "xyz.openbmc_project.Control.ThermalMode");

    setting->addProperty("Current", "Performance");
    setting->addProperty("Supported",
                         std::vector<std::string>{"Acoustic", "Performance"});

    setting = &settings.emplace_back(objectServer,
                                     "/xyz/openbmc_project/control/cfm_limit",
                                     "xyz.openbmc_project.Control.CFMLimit");

    setting->addProperty("Limit", static_cast<double>(0));

    setting = &settings.emplace_back(objectServer,
                                     "/xyz/openbmc_project/ipmi/sol/eth0",
                                     "xyz.openbmc_project.Ipmi.SOL");

    setting->addProperty("Progress", static_cast<uint8_t>(0));
    setting->addProperty("Enable", false);
    setting->addProperty("ForceEncryption", true);
    setting->addProperty("ForceAuthentication", true);
    setting->addProperty("Privilege", static_cast<uint8_t>(4));
    setting->addProperty("AccumulateIntervalMS", static_cast<uint8_t>(12));
    setting->addProperty("Threshold", static_cast<uint8_t>(96));
    setting->addProperty("RetryCount", static_cast<uint8_t>(6));
    setting->addProperty("RetryIntervalMS", static_cast<uint8_t>(20));

    setting = &settings.emplace_back(objectServer,
                                     "/xyz/openbmc_project/ipmi/sol/eth1",
                                     "xyz.openbmc_project.Ipmi.SOL");

    setting->addProperty("Progress", static_cast<uint8_t>(0));
    setting->addProperty("Enable", false);
    setting->addProperty("ForceEncryption", true);
    setting->addProperty("ForceAuthentication", true);
    setting->addProperty("Privilege", static_cast<uint8_t>(4));
    setting->addProperty("AccumulateIntervalMS", static_cast<uint8_t>(12));
    setting->addProperty("Threshold", static_cast<uint8_t>(96));
    setting->addProperty("RetryCount", static_cast<uint8_t>(6));
    setting->addProperty("RetryIntervalMS", static_cast<uint8_t>(20));

    setting = &settings.emplace_back(objectServer,
                                     "/xyz/openbmc_project/ipmi/sol/eth2",
                                     "xyz.openbmc_project.Ipmi.SOL");

    setting->addProperty("Progress", static_cast<uint8_t>(0));
    setting->addProperty("Enable", false);
    setting->addProperty("ForceEncryption", true);
    setting->addProperty("ForceAuthentication", true);
    setting->addProperty("Privilege", static_cast<uint8_t>(4));
    setting->addProperty("AccumulateIntervalMS", static_cast<uint8_t>(12));
    setting->addProperty("Threshold", static_cast<uint8_t>(96));
    setting->addProperty("RetryCount", static_cast<uint8_t>(6));
    setting->addProperty("RetryIntervalMS", static_cast<uint8_t>(20));

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/control/host0/restart_cause",
        "xyz.openbmc_project.Common.RestartCause");

    setting->addProperty("RestartCause",
                         "xyz.openbmc_project.State.Host.RestartCause.Unknown");

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/control/host0/ac_boot",
        "xyz.openbmc_project.Common.ACBoot");

    setting->addProperty("ACBoot", "Unknown", false);

    setting = &settings.emplace_back(
        objectServer, "/xyz/openbmc_project/Inventory/Item/Dimm",
        "xyz.openbmc_project.Inventory.Item.Dimm.Offset");

    setting->addProperty("DimmOffset", std::vector<uint8_t>{});

    for (SettingsInterface &s : settings)
    {
        s.initialize();
    }
}
