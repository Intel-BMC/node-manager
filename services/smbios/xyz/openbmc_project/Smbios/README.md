# SMBIOS MDR V1

## Overview
SMBIOS MDR V1 service exposes D-Bus methods for SMBIOS Version 1 operations.

### SMBIOS MDR V1 Interface
SMBIOS MDR V1 interface `xyz.openbmc_project.Smbios.MDR_V1` provides following
methods.
#### methods
* RegionStatus - Get the region status with the given region ID.
* RegionComplete - Set complete status to region.
* RegionRead - Read Region Data from BMC Memory.
* RegionWrite - Write Region Data to BMC Memory.
* RegionLock - Lock Region.

#### properties
* RegionID - ID of Region, default: 0.
* SessionID - ID of session, default: 0.
* LockPolicy - The policy of the lock, 0 means unlocked, 1 means strict locked,
2 means preemptable locked.
* RegionUsed - If the region is used or not, default: 0.
