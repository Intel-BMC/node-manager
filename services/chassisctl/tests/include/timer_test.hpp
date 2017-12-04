#include <iostream>
#include <gtest/gtest.h>
#include <timer.hpp>

// Base class for testing Timer
class TimerTest : public testing::Test
{
    public:
        // systemd event handler
        sd_event* events;

        // Need this so that events can be initialized.
        int rc;

        // Tells if the watchdog timer expired.
        bool expired = false;

        // Gets called as part of each TEST_F construction
        TimerTest()
            : rc(sd_event_default(&events)),
              eventP(events, phosphor::watchdog::EventDeleter())
        {
            // Check for successful creation of
            // event handler and bus handler
            EXPECT_GE(rc, 0);

            // Its already wrapped in eventP
            events = nullptr;
        }

        // unique_ptr for sd_event
        phosphor::watchdog::EventPtr eventP;

        // Handler called by timer expiration
        inline void timeOutHandler()
        {
            std::cout << "Time out handler called" << std::endl;
            expired = true;
        }
};
