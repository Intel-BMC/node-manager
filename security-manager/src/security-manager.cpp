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

#include "security-manager.hpp"

#include "file.hpp"

#include <pwd.h>
#include <shadow.h>
#include <systemd/sd-journal.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <gpiod.hpp>
#include <iostream>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <string>
#include <variant>
#include <vector>

namespace security_manager
{

static boost::asio::io_service io;
static std::shared_ptr<sdbusplus::asio::connection> conn;
static int inotifyFd = -1;
static int inotifyPwdFd = -1;

static bool atScaleDebugHarwareSupported = true;
static uint8_t currentAtScaleDebugState = 0xFF;
static constexpr uint8_t wolfPassBoardProductId = 0x7B;
static uint8_t systemBoardProductId = 0xFF;
static bool atScaleDebugHWInitDone = false;
static UserAssertedEventRecord assertedEvent = {0, 0, 0, 0};

std::unique_ptr<sdbusplus::bus::match_t> baseBoardUpdatedSignal;

//----------------------------------
// REMOTE_DEBUG_DETECTION function related definition
//----------------------------------
static gpiod::line remoteDebugDetectionLine;
static boost::asio::posix::stream_descriptor remoteDebugDetectEvent(io);
static boost::asio::posix::stream_descriptor fileChangeEvent(io);
static boost::asio::posix::stream_descriptor filePwdChangeEvent(io);

/** @brief implement Security event logging
 *  @param[in] Event message string
 *  @param[in] Event Severity
 *  @param[in] Event optional message
 *  @returns status
 */
static void securityEventlog(const std::string& msg, int severity)
{

    std::string eventStr = "OpenBMC.0.1." + msg;
    sd_journal_send("MESSAGE=Security Event: %s", eventStr.c_str(),
                    "PRIORITY=%i", severity, "REDFISH_MESSAGE_ID=%s",
                    eventStr.c_str(), NULL);
}

/** @brief implement check given user is active or not
 *  @param[in] username
 *  @param[out] algotype if user is enabled
 *  @returns user status
 */

static bool getUserStatusAndHashAlgoType(const std::string& username,
                                         uint8_t& algo)
{

    std::array<char, 4096> sbuffer{};
    struct spwd spwd;
    struct spwd* resultPtr = nullptr;
    // To get the shadow entry of asdbg user for verify the password is set or
    // not
    int status = getspnam_r(username.c_str(), &spwd, sbuffer.data(),
                            sbuffer.max_size(), &resultPtr);
    if (!status && (&spwd == resultPtr))
    {
        // Encrypted Password may be NULL or single character '!' if user is
        // disabled
        if (resultPtr->sp_pwdp[0] == 0 || resultPtr->sp_pwdp[1] == 0)
        {
            return false; // user pwd not set
        }
        algo = static_cast<int8_t>(std::atoi(&resultPtr->sp_pwdp[1]));
        return true; // user is enabled and password is set
    }
    return false; // assume user is disabled for any error.
}

/** @brief implement check any user related security breach
 *  @returns function status
 */

static void checkUserSecurityBreach()
{

    UserAssertedEventRecord currentAssertedEventState = {0, 0, 0, 0};
    uint8_t algoType = 0;

    std::vector<std::string> userList;
    std::vector<std::string> sshUsersList;

    struct passwd pw, *pwp = nullptr;
    std::array<char, 1024> buffer{};

    security_manager::File passwd("/etc/passwd", "r");
    if ((passwd)() == NULL)
    {
        std::cerr << "\nSecurity-manager :Error in opening passwd file \n";
    }

    while (true)
    {
        auto r = fgetpwent_r((passwd)(), &pw, buffer.data(), buffer.max_size(),
                             &pwp);
        if ((r != 0) || (pwp == NULL))
        {
            // break the loop. if reached EOF or any Error
            break;
        }

        std::string userName(pwp->pw_name);
        if (getUserStatusAndHashAlgoType(userName, algoType))
        {
            std::string loginShell(pwp->pw_shell);
            if ((loginShell == "/bin/zsh") || (loginShell == "/bin/csh") ||
                (loginShell == "/bin/ksh93") || (loginShell == "/bin/tcsh"))
            {
                if (!assertedEvent.unSupportedShellEvent)
                {
                    assertedEvent.unSupportedShellEvent = true;
                    securityEventlog("SecurityUserUnsupportedShellEnabled",
                                     LOG_CRIT);
                }
                currentAssertedEventState.unSupportedShellEvent = true;
            }
            if ((!("root" == userName)) && (pwp->pw_uid == 0))
            {
                if (!assertedEvent.uidZeroAssignedEvent)
                {
                    assertedEvent.uidZeroAssignedEvent = true;

                    securityEventlog("SecurityUserNonRootUidZeroAssigned",
                                     LOG_CRIT);
                }
                currentAssertedEventState.uidZeroAssignedEvent = true;
            }
            if (("root" == userName))
            {
                if (!assertedEvent.rootEnabledEvent)
                {
                    assertedEvent.rootEnabledEvent = true;

                    securityEventlog("SecurityUserRootEnabled", LOG_CRIT);
                }
                currentAssertedEventState.rootEnabledEvent = true;
            }
            if (algoType <=
                static_cast<uint8_t>(PasswordHashAlgorithm::hashAlgoNT))
            {
                if (!assertedEvent.weakHashAlgorithmEvent)
                {
                    assertedEvent.weakHashAlgorithmEvent = true;

                    securityEventlog("SecurityUserWeakHashAlgoEnabled",
                                     LOG_CRIT);
                }
                currentAssertedEventState.weakHashAlgorithmEvent = true;
            }
        }
    }

    endpwent();
    if (currentAssertedEventState.weakHashAlgorithmEvent == false &&
        assertedEvent.weakHashAlgorithmEvent == true)
    {
        securityEventlog("SecurityUserStrongHashAlgoRestored", LOG_INFO);
        assertedEvent.weakHashAlgorithmEvent = false;
    }

    if (currentAssertedEventState.rootEnabledEvent == false &&
        assertedEvent.rootEnabledEvent == true)
    {
        securityEventlog("SecurityUserRootDisabled", LOG_INFO);
        assertedEvent.rootEnabledEvent = false;
    }

    if (currentAssertedEventState.uidZeroAssignedEvent == false &&
        assertedEvent.uidZeroAssignedEvent == true)
    {
        securityEventlog("SecurityUserNonRootUidZeroRemoved", LOG_INFO);
        assertedEvent.uidZeroAssignedEvent = false;
    }

    if (currentAssertedEventState.unSupportedShellEvent == false &&
        assertedEvent.unSupportedShellEvent == true)
    {
        securityEventlog("SecurityUserUnsupportedShellRemoved", LOG_INFO);
        assertedEvent.unSupportedShellEvent = false;
    }

    return;
} // namespace security_manager

/** @brief implement check and control At-Scale Debug service
 *  @returns function status
 */

static void checkAndControlAtScaleDebugService()
{
    const static constexpr char* systemDService = "org.freedesktop.systemd1";
    const static constexpr char* systemDObjPath = "/org/freedesktop/systemd1";
    const static constexpr char* systemDMgrIntf =
        "org.freedesktop.systemd1.Manager";
    const static constexpr char* atScaleDebugService =
        "com.intel.AtScaleDebug.service";
    const std::string atScaleDebugUserName = "asdbg";

    bool flag = true;
    uint8_t algoType = 0;

    // We need to check the hardware at-ScaleDebug enabled or disabled.
    if (security_manager::atScaleDebugHarwareSupported)
    {
        flag = security_manager::remoteDebugDetectionLine.get_value();
    }

    // we need to enable at-ScaleDebug service only if special user is enabled.
    if (flag)
    {
        flag = getUserStatusAndHashAlgoType(atScaleDebugUserName, algoType);
        std::cerr << "Remote Debug Hardware Enabled \n";
    }
    // We have to avoid service start or stop if already in desired state.
    if (currentAtScaleDebugState == flag)
    {
        return;
    }
    currentAtScaleDebugState = flag;

    conn->async_method_call(
        [flag](const boost::system::error_code ec) {
            if (ec)
            {
                std::cerr << "\n Error in start or stop AtScaleDebug service:"
                          << ec.message() << "\n";
                // Keep Current AtScale Debug State by default
                currentAtScaleDebugState = 0xFF;
                return;
            }

            if (flag)
            {
                securityEventlog("AtScaleDebugFeatureEnabled", LOG_CRIT);
            }
            else
            {
                securityEventlog("AtScaleDebugFeatureDisabled", LOG_INFO);
            }
        },
        systemDService, systemDObjPath, systemDMgrIntf,
        flag ? "StartUnit" : "StopUnit", atScaleDebugService, "replace");

    return;
}

/** @brief implement register GPIO events
 *  @param[in] GPIO PIN name
 *  @param[in] Function  Callback handler
 *  @param[in] GPIO line
 *  @param[in] stream  Event descriptor
 *  @returns function status
 */

static bool requestGPIOEvents(
    const std::string& name, const std::function<void()>& handler,
    gpiod::line& gpioLine,
    boost::asio::posix::stream_descriptor& gpioEventDescriptor)
{
    // Find the GPIO line
    gpioLine = gpiod::find_line(name);
    if (!gpioLine)
    {
        std::cerr << "Failed to find the " << name << "GPIO line\n";
        return false;
    }

    try
    {
        gpioLine.request(
            {"security-manager", gpiod::line_request::EVENT_BOTH_EDGES, 0});
    }
    catch (std::exception&)
    {
        std::cerr << "Failed to request events for " << name << "\n";
        return false;
    }

    int gpioLineFd = gpioLine.event_get_fd();
    if (gpioLineFd < 0)
    {
        std::cerr << "Failed to get " << name << " fd\n";
        return false;
    }

    gpioEventDescriptor.assign(gpioLineFd);

    gpioEventDescriptor.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [handler](const boost::system::error_code ec) {
            if (ec)
            {
                std::cerr << "RemoteDebugEnable fd handler error: "
                          << ec.message() << "\n";
                return;
            }
            handler();
        });
    return true;
}

/** @brief implement check remote Debug on and log the event
 *  @returns function status
 */

static void remoteDebugDetectionHandler()
{
    gpiod::line_event gpioLineEvent = remoteDebugDetectionLine.event_read();

    bool remoteDebugEnable =
        (gpioLineEvent.event_type == gpiod::line_event::RISING_EDGE);

    if (remoteDebugEnable)
    {

        // Log the atScaleDebugHardware enable Event
        securityEventlog("AtScaleDebugFeatureEnabledAtHardware", LOG_CRIT);
    }
    else
    {
        // Log the atScaleDebugHardware disable Event
        securityEventlog("AtScaleDebugFeatureDisabledAtHardware", LOG_INFO);
    }

    checkAndControlAtScaleDebugService();
    remoteDebugDetectEvent.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [](const boost::system::error_code ec) {
            if (ec)
            {
                std::cerr << "RemoteDebugEnable handler error: " << ec.message()
                          << "\n";
                return;
            }
            remoteDebugDetectionHandler();
        });
}

/** @brief implement register file change Event
 *  @param[in] file descriptor
 *  @param[in] file name
 *  @param[in] Function  Callback handler
 *  @param[in] stream  Event descriptor
 *  @returns function status
 */

static bool requestFileChangeEvents(
    int& fd, const std::string& filename, const std::function<void()>& handler,
    boost::asio::posix::stream_descriptor& fileChangeEventDescriptor)
{

    fd = inotify_init1(IN_NONBLOCK);
    if (-1 == fd)
    {
        return false;
    }

    fileChangeEventDescriptor.assign(fd);
    auto wd = inotify_add_watch(fd, filename.c_str(),
                                IN_CLOSE_WRITE | IN_DELETE | IN_DELETE_SELF |
                                    IN_MOVE_SELF | IN_MODIFY);
    if (-1 == wd)
    {
        close(fd);
        return false;
    }

    fileChangeEventDescriptor.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [handler](const boost::system::error_code ec) {
            if (ec)
            {
                std::cerr << "shadow fd handler error: " << ec.message()
                          << "\n";
                return;
            }
            handler();
        });
    return true;
}

static void addInotifyWatch(int& fd, const std::string filename)
{

    auto wd = inotify_add_watch(fd, filename.c_str(),
                                IN_CLOSE_WRITE | IN_DELETE | IN_DELETE_SELF |
                                    IN_MOVE_SELF | IN_MODIFY);
    if (-1 == wd)
    {
        std::cerr << "Error in adding notify : " << filename.c_str() << "\n";
        return;
    }
}

/** @brief implement shadow file Event handler
    @Wait  for inotify event and Parse shadow file inotify event
    @Log the Event if any security breach
    @If asdbg user is set or disabled then control the AtScale Debug service.
 *  @returns function status
 */

static void fileShadowChangeHandler()
{

    std::array<char, 4096> buffer{};
    struct inotify_event* event = NULL;
    int index = 0;
    bool exec = 0;

    int len = read(inotifyFd, buffer.data(), sizeof(buffer));
    if (len < 0)
    {
        std::cerr << "fileChangeHandler length error: " << len;
    }

    event = reinterpret_cast<inotify_event*>(&buffer[index]);
    while (event != NULL)
    {

        event = reinterpret_cast<inotify_event*>(&buffer[index]);

        if (((event->mask & IN_MOVE_SELF) || (event->mask & IN_CLOSE_WRITE) ||
             (event->mask & IN_MODIFY) || (event->mask & IN_DELETE_SELF)))
        {
            exec = true;
        }

        if (event->mask & IN_IGNORED)
        {
            addInotifyWatch(inotifyFd, "/etc/shadow");
        }
        /* Move to next struct */
        len -= sizeof(inotify_event) + event->len;
        if (len > 0)
            index += (sizeof(inotify_event) + event->len);
        else
            event = NULL;
    }

    if (exec)
    {
        checkAndControlAtScaleDebugService();
        checkUserSecurityBreach();
    }

    fileChangeEvent.async_wait(boost::asio::posix::stream_descriptor::wait_read,
                               [](const boost::system::error_code ec) {
                                   if (ec)
                                   {
                                       std::cerr
                                           << "Recvd File read signal-error: "
                                           << ec.message() << "\n";
                                       return;
                                   }

                                   fileShadowChangeHandler();
                               });
}

/** @brief implement Passwd file Event handler
    @Wait  for inotify event and Parse passwd file inotify event
    @Log the Event if any security breach
 *  @returns function status
 */

static void filePwdChangeHandler()
{

    std::array<char, 4096> buffer{};
    struct inotify_event* event = NULL;
    int index = 0;
    bool exec = 0;

    int len = read(inotifyPwdFd, buffer.data(), sizeof(buffer));
    if (len < 0)
    {
        std::cerr << "filePwdChangeHandler length error: " << len;
    }

    event = reinterpret_cast<inotify_event*>(&buffer[index]);
    while (event != NULL)
    {

        event = reinterpret_cast<inotify_event*>(&buffer[index]);

        if (((event->mask & IN_MOVE_SELF) || (event->mask & IN_CLOSE_WRITE) ||
             (event->mask & IN_MODIFY) || (event->mask & IN_DELETE_SELF)))
        {
            exec = true;
        }

        if (event->mask & IN_IGNORED)
        {
            addInotifyWatch(inotifyPwdFd, "/etc/passwd");
        }
        /* Move to next struct */
        len -= sizeof(inotify_event) + event->len;
        if (len > 0)
            index += (sizeof(inotify_event) + event->len);
        else
            event = NULL;
    }

    if (exec)
    {
        checkUserSecurityBreach();
    }

    filePwdChangeEvent.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [](const boost::system::error_code ec) {
            if (ec)
            {
                std::cerr << "Recvd File read signal-error: " << ec.message()
                          << "\n";
                return;
            }

            filePwdChangeHandler();
        });
}

/** @brief implement manager function
    @Register for GPIO and file change event with callback function
 *  @returns function status
 */

static void coreMonitor()
{

    if (security_manager::atScaleDebugHarwareSupported)
    {

        // Request REMOTE_DEBUG_ENABLE  GPIO events
        if (!security_manager::requestGPIOEvents(
                "REMOTE_DEBUG_ENABLE",
                security_manager::remoteDebugDetectionHandler,
                security_manager::remoteDebugDetectionLine,
                security_manager::remoteDebugDetectEvent))
        {
            return;
        }
    }
    // We need to need enable or disable based on at-ScaleDebug special
    // user and Hardware status
    security_manager::checkAndControlAtScaleDebugService();

    // Request file change events
    if (!security_manager::requestFileChangeEvents(
            inotifyFd, "/etc/shadow", security_manager::fileShadowChangeHandler,
            security_manager::fileChangeEvent))
    {
        return;
    }

    // Request Pwd  file change events
    if (!security_manager::requestFileChangeEvents(
            inotifyPwdFd, "/etc/passwd", security_manager::filePwdChangeHandler,
            security_manager::filePwdChangeEvent))
    {
        return;
    }

    security_manager::checkUserSecurityBreach();

    atScaleDebugHWInitDone = true;
}
static void registerAtScaleDebugMonitor(const std::string& baseboardObjPath)
{

    // Get the Baseboard object to find the Product id

    conn->async_method_call(
        [](boost::system::error_code ec,
           const std::variant<std::uint64_t>& property) {
            if (ec)
            {
                return;
            }
            systemBoardProductId =
                static_cast<uint8_t>(std::get<uint64_t>(property));
            security_manager::atScaleDebugHarwareSupported =
                systemBoardProductId == wolfPassBoardProductId ? false : true;
            coreMonitor();
        },
        "xyz.openbmc_project.EntityManager", baseboardObjPath,
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.Inventory.Item.Board.Motherboard", "ProductId");

    return;
}

static bool startAtScaleDebugMonitor()
{

    // Get the all objects related with  inventory to find the Baseboard object
    // path
    conn->async_method_call(
        [](const boost::system::error_code ec,
           const std::vector<std::string>& subtree) {
            if (ec)
            {

                std::cerr << "Failed to get SubTree for "
                             "BaseBoard \n";
                return;
            }
            const std::string match = "board";

            for (const std::string& objpath : subtree)
            {
                // Iterate over all retrieved ObjectPaths.

                if (!boost::ends_with(objpath, match))
                {
                    continue;
                }

                // Baseboard object path found
                registerAtScaleDebugMonitor(objpath);
                return;
            }
            // Failed to get BaseBoard Object Path so waiting for EntityManager
            // Signal

            const static constexpr char* baseBoardInterface =
                "xyz.openbmc_project.Inventory.Item.Board.Motherboard";
            const static constexpr char* baseBoardObjBasePath =
                "/xyz/openbmc_project/inventory/system/board/";

            security_manager::baseBoardUpdatedSignal = std::make_unique<
                sdbusplus::bus::match_t>(
                *security_manager::conn,
                sdbusplus::bus::match::rules::interfacesAdded() +
                    sdbusplus::bus::match::rules::argNpath(
                        0, baseBoardObjBasePath),
                [](sdbusplus::message::message& msg) {
                    sdbusplus::message::object_path objPath;
                    boost::container::flat_map<
                        std::string,
                        boost::container::flat_map<
                            std::string, std::variant<std::string, uint64_t>>>
                        msgData;
                    msg.read(objPath, msgData);

                    // Check for xyz.openbmc_project.Inventory.Item.Board
                    // interface match
                    auto intfFound = msgData.find(baseBoardInterface);
                    if (msgData.end() != intfFound)
                    {
                        // Check for ProductId property match

                        auto prodIdFound = intfFound->second.find("ProductId");
                        if (intfFound->second.end() != prodIdFound)
                        {

                            systemBoardProductId = static_cast<uint8_t>(
                                std::get<uint64_t>(prodIdFound->second));
                            security_manager::atScaleDebugHarwareSupported =
                                systemBoardProductId == wolfPassBoardProductId
                                    ? false
                                    : true;

                            if (atScaleDebugHWInitDone == false)
                            {
                                coreMonitor();
                            }
                        }
                    }
                    return;
                });
            return;
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths",
        "/xyz/openbmc_project/inventory", 0,
        std::array<const char*, 1>{
            "xyz.openbmc_project.Inventory.Item.Board.Motherboard"});

    return true;
}

} // namespace security_manager

int main()
{

    const static constexpr char* securityManagerService =
        "xyz.openbmc_project.SecurityManager";

    // setup connection to dbus
    security_manager::conn =
        std::make_shared<sdbusplus::asio::connection>(security_manager::io);

    // Security manager  Object
    security_manager::conn->request_name(securityManagerService);
    sdbusplus::asio::object_server server =
        sdbusplus::asio::object_server(security_manager::conn);
    // Start the monitoring  based on the platform id
    security_manager::startAtScaleDebugMonitor();

    security_manager::io.run();

    return 0;
}
