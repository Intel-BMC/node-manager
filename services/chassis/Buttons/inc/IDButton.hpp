#pragma once
#include "xyz/openbmc_project/Chassis/Buttons/ID/error.hpp"
#include "xyz/openbmc_project/Chassis/Buttons/ID/server.hpp"
#include "Common.hpp"
#include "Gpio.hpp"

const static constexpr char *ID_BUTTON = "ID_BTN";

struct IDButton
    : sdbusplus::server::object::object<
          sdbusplus::xyz::openbmc_project::Chassis::Buttons::server::ID>
{

    IDButton(sdbusplus::bus::bus &bus, const char *path, EventPtr &event,
             sd_event_io_handler_t handler = IDButton::EventHandler) :
        sdbusplus::server::object::object<
            sdbusplus::xyz::openbmc_project::Chassis::Buttons::server::ID>(
            bus, path),
        fd(-1), bus(bus), event(event), callbackHandler(handler)
    {

        int ret;

        // config gpio
        ret = ::configGpio(ID_BUTTON, &fd, bus);
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

    ~IDButton()
    {
        ::closeGpio(fd);
    }
    bool simPress() override;
    static int EventHandler(sd_event_source *es, int fd, uint32_t revents,
                            void *userdata)
    {

        int n;
        char buf;

        IDButton *idButton = static_cast<IDButton *>(userdata);

        if (!idButton)
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
            idButton->pressed();
        }
        else
        {
            // released
            idButton->released();
        }

        return 0;
    }

  private:
    int fd;
    sdbusplus::bus::bus &bus;
    EventPtr &event;
    sd_event_io_handler_t callbackHandler;
};
