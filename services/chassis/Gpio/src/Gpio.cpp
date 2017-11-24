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
#include "Gpio.hpp"

const static constexpr char *SYSMGR_SERVICE = "org.openbmc.managers.System";
const static constexpr char *SYSMGR_OBJ_PATH = "/org/openbmc/managers/System";
const static constexpr char *SYSMGR_INTERFACE = "org.openbmc.managers.System";

int closeGpio(int fd)
{
    if (fd > 0)
    {
        ::close(fd);
    }
    return 0;
}

int configGpio(const char *gpioName, int *fd, sdbusplus::bus::bus &bus)
{
    auto method = bus.new_method_call(SYSMGR_SERVICE, SYSMGR_OBJ_PATH,
                                      SYSMGR_INTERFACE, "gpioInit");

    method.append(gpioName);

    auto result = bus.call(method);

    if (result.is_method_error())
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "bus call error!");
        return -1;
    }

    int32_t gpioNum;
    std::string gpioDev;
    std::string gpioDirection;

    result.read(gpioDev, gpioNum, gpioDirection);

    std::string devPath;

    std::fstream stream;

    stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    devPath.clear();
    devPath = gpioDev + "/gpio" + std::to_string(gpioNum) + "/value";

    std::experimental::filesystem::path fullPath(devPath);

    if (std::experimental::filesystem::exists(fullPath))
    {
        phosphor::logging::log<phosphor::logging::level::INFO>("GPIO exported");
    }
    else
    {
        devPath.clear();
        devPath = gpioDev + "/export";

        stream.open(devPath, std::fstream::out);
        try
        {
            stream << gpioNum;
            stream.close();
        }

        catch (const std::exception &e)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Error in writing!",
                phosphor::logging::entry("PATH=%s", devPath.c_str()),
                phosphor::logging::entry("NUM=%d", gpioNum));
            return -1;
        }
    }

    if (gpioDirection == "out")
    {
        devPath.clear();
        devPath = gpioDev + "/gpio" + std::to_string(gpioNum) + "/value";

        uint32_t currentValue;

        stream.open(devPath, std::fstream::in);
        try
        {
            stream >> currentValue;
            stream.close();
        }

        catch (const std::exception &e)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Error in reading!",
                phosphor::logging::entry("PATH=%s", devPath.c_str()));
            return -1;
        }

        const char *direction = currentValue ? "high" : "low";

        devPath.clear();
        devPath = gpioDev + "/gpio" + std::to_string(gpioNum) + "/direction";

        stream.open(devPath, std::fstream::out);
        try
        {
            stream << direction;
            stream.close();
        }

        catch (const std::exception &e)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Error in writing!");
            return -1;
        }
    }
    else if (gpioDirection == "in")
    {
        devPath.clear();
        devPath = gpioDev + "/gpio" + std::to_string(gpioNum) + "/direction";

        stream.open(devPath, std::fstream::out);
        try
        {
            stream << gpioDirection;
            stream.close();
        }

        catch (const std::exception &e)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Error in writing!");
            return -1;
        }
    }
    else if ((gpioDirection == "both"))
    {

        // For gpio configured as ‘both’, it is an interrupt pin and trigged on
        // both rising and falling signals
        devPath.clear();
        devPath = gpioDev + "/gpio" + std::to_string(gpioNum) + "/edge";

        stream.open(devPath, std::fstream::out);
        try
        {
            stream << gpioDirection;
            stream.close();
        }

        catch (const std::exception &e)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Error in writing!");
            return -1;
        }
    }

    devPath.clear();
    devPath = gpioDev + "/gpio" + std::to_string(gpioNum) + "/value";

    *fd = ::open(devPath.c_str(), O_RDWR | O_NONBLOCK);

    if (*fd < 0)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>("open error!");
        return -1;
    }

    return 0;
}
