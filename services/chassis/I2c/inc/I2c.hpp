#pragma once
#include <cstdint>
#include <linux/i2c-dev-user.h>
int i2cSet(uint8_t bus, uint8_t slaveAddr, uint8_t regAddr, uint8_t value);