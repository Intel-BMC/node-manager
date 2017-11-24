#pragma once
#include "xyz/openbmc_project/Chassis/Buttons/Reset/error.hpp"
#include "xyz/openbmc_project/Chassis/Buttons/Reset/server.hpp"
#include "Common.hpp"
#include "Gpio.hpp"

const static constexpr char *RESET_BUTTON = "RESET_BUTTON";

struct ResetButton
    : sdbusplus::server::object::object<
          sdbusplus::xyz::openbmc_project::Chassis::Buttons::server::Reset>
{

    ResetButton(sdbusplus::bus::bus &bus, const char *path, EventPtr &event,
                sd_event_io_handler_t handler = ResetButton::EventHandler) :
        sdbusplus::server::object::object<
            sdbusplus::xyz::openbmc_project::Chassis::Buttons::server::Reset>(
            bus, path),
        fd(-1), bus(bus), event(event), callbackHandler(handler)
    {

        int ret;

        // config gpio
        ret = ::configGpio(RESET_BUTTON, &fd, bus);
        if (ret < 0)
        {
            throw std::runtime_error("failed to config GPIO");
        }

        ret = sd_event_add_io(event.get(), nullptr, fd, EPOLLPRI,
                              callbackHandler, this);
        if (ret < 0)
        {
            throw std::runtime_error("failed to add to event loop");
        }
    }

    ~ResetButton()
    {
        ::closeGpio(fd);
    }
    bool simPress() override;
    static int EventHandler(sd_event_source *es, int fd, uint32_t revents,
                            void *userdata)
    {

        int n;
        char buf;

        ResetButton *resetButton = static_cast<ResetButton *>(userdata);

        if (!resetButton)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "null pointer!");
            return -1;
        }

        n = ::lseek(fd, 0, SEEK_SET);

        if (n < 0)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "lseek error!");
            return n;
        }

        n = ::read(fd, &buf, 1);
        if (n < 0)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "read error!");
            return n;
        }

        if (buf == '0')
        {
            // emit pressed signal
            resetButton->pressed();
        }
        else
        {
            // released
            resetButton->released();
        }

        return 0;
    }

  private:
    int fd;
    sdbusplus::bus::bus &bus;
    EventPtr &event;
    sd_event_io_handler_t callbackHandler;
};
