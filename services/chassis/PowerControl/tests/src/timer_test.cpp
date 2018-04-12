#include <chrono>
#include <timer_test.hpp>

using namespace std::chrono;
using namespace std::chrono_literals;

/** @brief Starts the timer and expects it to
 *         expire in configured time and expects the
 *         deault callback handler to kick-in
 */
TEST_F(TimerTest, testTimerForExpirationDefaultTimeoutHandler)
{
    // Expect timer to expire in 2 seconds
    auto expireTime = seconds(2s);

    phosphor::watchdog::Timer timer(eventP);

    // Set the expiration and enable the timer
    timer.start(duration_cast<milliseconds>(expireTime));
    timer.setEnabled<std::true_type>();

    // Waiting 2 seconds to expect expiration
    int count = 0;
    while(count < expireTime.count() && !timer.expired())
    {
        // Returns -0- on timeout and positive number on dispatch
        auto sleepTime = duration_cast<microseconds>(seconds(1));
        if(!sd_event_run(eventP.get(), sleepTime.count()))
        {
            count++;
        }
    }
    EXPECT_EQ(true, timer.expired());
    EXPECT_EQ(expireTime.count() - 1, count);

    // Make sure secondary callback was not called.
    EXPECT_EQ(false, expired);
}

/** @brief Starts the timer and expects it to expire
 *         in configured time and expects the secondary
 *         callback to be called into along with default.
 */
TEST_F(TimerTest, testTimerForExpirationSecondCallBack)
{
    // Expect timer to expire in 2 seconds
    auto expireTime = seconds(2s);

    phosphor::watchdog::Timer timer(eventP,
                    std::bind(&TimerTest::timeOutHandler, this));

    // Set the expiration and enable the timer
    timer.start(duration_cast<milliseconds>(expireTime));
    timer.setEnabled<std::true_type>();

    // Waiting 2 seconds to expect expiration
    int count = 0;
    while(count < expireTime.count() && !timer.expired())
    {
        // Returns -0- on timeout and positive number on dispatch
        auto sleepTime = duration_cast<microseconds>(seconds(1));
        if(!sd_event_run(eventP.get(), sleepTime.count()))
        {
            count++;
        }
    }
    EXPECT_EQ(true, timer.expired());
    EXPECT_EQ(expireTime.count() - 1, count);

    // This gets set as part of secondary callback
    EXPECT_EQ(true, expired);
}
