#include <experimental/filesystem>
#include <fcntl.h>
#include <phosphor-logging/elog-errors.hpp>
#include <unistd.h>
#include <xyz/openbmc_project/Common/error.hpp>
#include "I2c.hpp"

int i2cSet(uint8_t bus, uint8_t slaveAddr, uint8_t regAddr, uint8_t value)
{
    unsigned long funcs;
    std::string devPath = "/dev/i2c-" + std::to_string(bus);
    std::experimental::filesystem::path fullPath(devPath);

    if (!std::experimental::filesystem::exists(fullPath))
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "i2c bus des not exist!",
            phosphor::logging::entry("PATH=%s", devPath.c_str()),
            phosphor::logging::entry("slaveAddr=%d", slaveAddr));
        return -1;
    }

    int fd = ::open(devPath.c_str(), O_RDWR);
    if (fd < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error in open!",
            phosphor::logging::entry("PATH=%s", devPath.c_str()),
            phosphor::logging::entry("slaveAddr=%d", slaveAddr));
        return -1;
    }

    if (::ioctl(fd, I2C_FUNCS, &funcs) < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error in I2C_FUNCS!",
            phosphor::logging::entry("PATH=%s", devPath.c_str()),
            phosphor::logging::entry("slaveAddr=%d", slaveAddr));

        ::close(fd);
        return -1;
    }

    if (!(funcs & I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
    {

        phosphor::logging::log<phosphor::logging::level::ERR>(
            "i2c bus des not support write!",
            phosphor::logging::entry("PATH=%s", devPath.c_str()),
            phosphor::logging::entry("slaveAddr=%d", slaveAddr));
        ::close(fd);
        return -1;
    }

    if (::ioctl(fd, I2C_SLAVE_FORCE, slaveAddr) < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error in I2C_SLAVE_FORCE!",
            phosphor::logging::entry("PATH=%s", devPath.c_str()),
            phosphor::logging::entry("slaveAddr=%d", slaveAddr));
        ::close(fd);
        return -1;
    }

    if (::i2c_smbus_write_byte_data(fd, regAddr, value) < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error in i2c write!",
            phosphor::logging::entry("PATH=%s", devPath.c_str()),
            phosphor::logging::entry("slaveAddr=%d", slaveAddr));
        ::close(fd);
        return -1;
    }

    phosphor::logging::log<phosphor::logging::level::DEBUG>(
        "i2cset successfully",
        phosphor::logging::entry("PATH=%s", devPath.c_str()),
        phosphor::logging::entry("slaveAddr=0x%x", slaveAddr),
        phosphor::logging::entry("regAddr=0x%x", regAddr),
        phosphor::logging::entry("value=0x%x", value));
    ::close(fd);
    return 0;
}