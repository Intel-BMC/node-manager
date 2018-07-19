#pragma once

#include <functional>
#include <memory>
#include <chrono>
#include <systemd/sd-event.h>
namespace phosphor
{
namespace watchdog
{

/* Need a custom deleter for freeing up sd_event */
struct EventDeleter
{
    void operator()(sd_event* event) const
    {
        event = sd_event_unref(event);
    }
};
using EventPtr = std::shared_ptr<sd_event>;

/* Need a custom deleter for freeing up sd_event_source */
struct EventSourceDeleter
{
    void operator()(sd_event_source* eventSource) const
    {
        eventSource = sd_event_source_unref(eventSource);
    }
};
using EventSourcePtr = std::unique_ptr<sd_event_source, EventSourceDeleter>;

/** @class Timer
 *  @brief Manages starting timers and handling timeouts
 */
class Timer
{
  public:
    Timer() = delete;
    ~Timer() = default;
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(Timer&&) = delete;

    /** @brief Constructs timer object
     *
     *  @param[in] event        - sd_event unique pointer
     *  @param[in] userCallBack - Optional function callback
     *                            for timer expiration
     */
    Timer(EventPtr event, uint8_t id,
          std::function<void(uint8_t)> userCallBack = nullptr) :
        event(event),
        regionId(id), userCallBack(userCallBack)
    {
        // Initialize the timer
        initialize();
    }

    void clearExpired(void)
    {
        expire = false;
    }

    /** @brief Tells whether the timer is expired or not */
    inline auto expired() const
    {
        return expire;
    }

    /** @brief Returns the current Timer enablement type */
    int getEnabled() const;

    /** @brief Enables / disables the timer.
     *         <T> is an integral constant boolean
     */
    template <typename T> void setEnabled()
    {
        constexpr auto type = T::value ? SD_EVENT_ONESHOT : SD_EVENT_OFF;
        setEnabled(type);
    }

    /** @brief Returns time remaining in usec before expiration
     *         which is an offset to current steady clock
     */
    std::chrono::microseconds getRemaining() const;

    /** @brief Starts the timer with specified expiration value.
     *         std::steady_clock is used for base time.
     *
     *  @param[in] usec - Microseconds from the current time
     *                    before expiration.
     *
     *  @return None.
     *
     *  @error Throws exception
     */
    void start(std::chrono::microseconds usec);

    /** @brief Gets the current time from steady clock */
    static std::chrono::microseconds getCurrentTime();

  private:
    uint8_t regionId;

    /** @brief Reference to sd_event unique pointer */
    EventPtr event;

    /** @brief event source */
    EventSourcePtr eventSource;

    /** @brief Set to true when the timeoutHandler is called into */
    bool expire = false;

    /** @brief Optional function to call on timer expiration
     *         This is called from timeout handler.
     */
    std::function<void(uint8_t)> userCallBack;

    /** @brief Initializes the timer object with infinite
     *         expiration time and sets up the callback handler
     *
     *  @return None.
     *
     *  @error Throws exception
     */
    void initialize();

    /** @brief Callback function when timer goes off
     *
     *  @param[in] eventSource - Source of the event
     *  @param[in] usec        - time in microseconds
     *  @param[in] userData    - User data pointer
     *
     */
    static int timeoutHandler(sd_event_source* eventSource, uint64_t usec,
                              void* userData);

    /** @brief Enables / disables the timer
     *
     *  @param[in] type - Timer type.
     *                    This implementation uses only SD_EVENT_OFF
     *                    and SD_EVENT_ONESHOT
     */
    void setEnabled(int type);
};

} // namespace watchdog
} // namespace phosphor
