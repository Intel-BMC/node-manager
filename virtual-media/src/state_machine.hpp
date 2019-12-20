#pragma once

#include "configuration.hpp"
#include "logger.hpp"
#include "system.hpp"

#include <sys/mount.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <sdbusplus/asio/object_server.hpp>
#include <stdexcept>
#include <variant>

struct MountPointStateMachine
{
    struct InvalidStateError : std::runtime_error
    {
        InvalidStateError(const char* what) : std::runtime_error(what)
        {
        }
    };

    struct BasicState
    {
        BasicState(MountPointStateMachine& machine,
                   const char* stateName = nullptr) :
            machine{machine},
            stateName{stateName}
        {
            if (stateName != nullptr)
            {
                LogMsg(Logger::Debug, machine.name, " State changed to ",
                       stateName);
            }
        }

        BasicState(const BasicState& state) :
            machine{state.machine}, stateName{state.stateName}
        {
        }

        BasicState(const BasicState& state, const char* stateName) :
            machine{state.machine}, stateName{stateName}
        {
            LogMsg(Logger::Debug, machine.name, " State changed to ",
                   stateName);
        }

        BasicState& operator=(BasicState&& state)
        {
            machine = std::move(state.machine);
            stateName = std::move(state.stateName);
            return *this;
        }

        virtual void onEnter(){};

        MountPointStateMachine& machine;
        const char* stateName = nullptr;
    };

    struct InitialState : public BasicState
    {
        InitialState(const BasicState& state) :
            BasicState(state, __FUNCTION__){};
        InitialState(MountPointStateMachine& machine) :
            BasicState(machine, __FUNCTION__){};
    };

    struct ReadyState : public BasicState
    {
        ReadyState(const BasicState& state) : BasicState(state, __FUNCTION__){};

        virtual void onEnter()
        {
            if (machine.target)
            {
                machine.target.reset();
            }
        }
    };

    struct ActivatingState : public BasicState
    {
        ActivatingState(const BasicState& state) :
            BasicState(state, __FUNCTION__)
        {
        }

        virtual void onEnter()
        {
            machine.emitActivationStartedEvent();
        }
    };

    struct WaitingForGadgetState : public BasicState
    {
        WaitingForGadgetState(const BasicState& state) :
            BasicState(state, __FUNCTION__)
        {
        }

        std::weak_ptr<Process> process;
    };

    struct ActiveState : public BasicState
    {
        ActiveState(const BasicState& state) : BasicState(state, __FUNCTION__)
        {
        }
        ActiveState(const WaitingForGadgetState& state) :
            BasicState(state, __FUNCTION__), process{state.process} {};

        std::weak_ptr<Process> process;
    };

    struct WaitingForProcessEndState : public BasicState
    {
        WaitingForProcessEndState(const BasicState& state) :
            BasicState(state, __FUNCTION__)
        {
        }
        WaitingForProcessEndState(const ActiveState& state) :
            BasicState(state, __FUNCTION__), process{state.process}
        {
        }
        WaitingForProcessEndState(const WaitingForGadgetState& state) :
            BasicState(state, __FUNCTION__), process{state.process}
        {
        }

        std::weak_ptr<Process> process;
    };

    using State = std::variant<InitialState, ReadyState, ActivatingState,
                               WaitingForGadgetState, ActiveState,
                               WaitingForProcessEndState>;

    struct BasicEvent
    {
        BasicEvent(const char* eventName) : eventName(eventName)
        {
        }

        inline void transitionError(const char* en, const BasicState& state)
        {
            LogMsg(Logger::Critical, state.machine.name, " Unexpected event ",
                   eventName, " received in ", state.stateName,
                   "state. Review and correct state transisions.");
        }
        virtual State operator()(const InitialState& state)
        {
            transitionError(eventName, state);
            return state;
        }
        virtual State operator()(const ReadyState& state)
        {
            transitionError(eventName, state);
            return state;
        }
        virtual State operator()(const ActivatingState& state)
        {
            transitionError(eventName, state);
            return state;
        }
        virtual State operator()(const WaitingForGadgetState& state)
        {
            transitionError(eventName, state);
            return state;
        }
        virtual State operator()(const ActiveState& state)
        {
            transitionError(eventName, state);
            return state;
        }
        virtual State operator()(const WaitingForProcessEndState& state)
        {
            transitionError(eventName, state);
            return state;
        }
        const char* eventName;
    };

    struct RegisterDbusEvent : public BasicEvent
    {
        RegisterDbusEvent(
            std::shared_ptr<sdbusplus::asio::connection> bus,
            std::shared_ptr<sdbusplus::asio::object_server> objServer) :
            BasicEvent(__FUNCTION__),
            bus(bus), objServer(objServer),
            emitMountEvent(std::move(emitMountEvent))
        {
        }

        State operator()(const InitialState& state)
        {
            const bool isLegacy =
                (state.machine.config.mode == Configuration::Mode::legacy);
            addMountPointInterface(state);
            addProcessInterface(state);
            addServiceInterface(state, isLegacy);
            return ReadyState(state);
        }

        template <typename AnyState>
        State operator()(const AnyState& state)
        {
            LogMsg(Logger::Critical, state.machine.name,
                   " If you receiving this error, this means "
                   "your FSM is broken. Rethink!");
            return InitialState(state);
        }

      private:
        std::string getObjectPath(const MountPointStateMachine& machine)
        {
            LogMsg(Logger::Debug, "getObjectPath entry()");
            std::string objPath;
            if (machine.config.mode == Configuration::Mode::proxy)
            {
                objPath = "/xyz/openbmc_project/VirtualMedia/Proxy/";
            }
            else
            {
                objPath = "/xyz/openbmc_project/VirtualMedia/Legacy/";
            }
            return objPath;
        }

        std::string getObjectPath(const InitialState& state)
        {
            return getObjectPath(state.machine);
        }

        void addProcessInterface(const InitialState& state)
        {
            std::string objPath = getObjectPath(state);

            auto processIface = objServer->add_interface(
                objPath + state.machine.name,
                "xyz.openbmc_project.VirtualMedia.Process");

            processIface->register_property(
                "Active", bool(false),
                [](const bool& req, bool& property) { return 0; },
                [& machine = state.machine](const bool& property) {
                    if (std::get_if<ActiveState>(&machine.state))
                    {
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                });
            processIface->register_property(
                "ExitCode", uint8_t(0),
                [](const uint8_t& req, uint8_t& property) { return 0; },
                [& machine = state.machine](const uint8_t& property) {
                    // TODO: indicate real value instead of success
                    return uint8_t(255);
                });
            processIface->initialize();
        }

        void addMountPointInterface(const InitialState& state)
        {
            std::string objPath = getObjectPath(state);

            auto iface = objServer->add_interface(
                objPath + state.machine.name,
                "xyz.openbmc_project.VirtualMedia.MountPoint");
            iface->register_property(
                "Device", state.machine.config.nbdDevice.to_string());
            iface->register_property("EndpointId",
                                     state.machine.config.endPointId);
            iface->register_property("Socket", state.machine.config.unixSocket);
            iface->initialize();
        }

        void addServiceInterface(const InitialState& state, const bool isLegacy)
        {
            const std::string name = "xyz.openbmc_project.VirtualMedia." +
                                     std::string(isLegacy ? "Legacy" : "Proxy");

            const std::string path = getObjectPath(state) + state.machine.name;

            auto iface = objServer->add_interface(path, name);

            // Common unmount
            iface->register_method(
                "Unmount",
                [& machine = state.machine](boost::asio::yield_context yield) {
                    LogMsg(Logger::Info, "[App]: Unmount called on ",
                           machine.name);
                    try
                    {
                        machine.emitUnmountEvent();
                    }
                    catch (InvalidStateError& e)
                    {
                        throw sdbusplus::exception::SdBusError(EPERM, e.what());
                        return false;
                    }

                    boost::asio::steady_timer timer(machine.ioc.get());
                    int waitCnt = 120;
                    while (waitCnt > 0)
                    {
                        if (std::get_if<ReadyState>(&machine.state))
                        {
                            break;
                        }
                        boost::system::error_code ignored_ec;
                        timer.expires_from_now(std::chrono::milliseconds(100));
                        timer.async_wait(yield[ignored_ec]);
                        waitCnt--;
                    }
                    return true;
                });

            // Common mount
            const auto handleMount = [](boost::asio::yield_context yield,
                                        MountPointStateMachine& machine) {
                try
                {
                    machine.emitMountEvent();
                }
                catch (InvalidStateError& e)
                {
                    throw sdbusplus::exception::SdBusError(EPERM, e.what());
                    return false;
                }

                boost::asio::steady_timer timer(machine.ioc.get());
                int waitCnt = 120;
                while (waitCnt > 0)
                {
                    if (std::get_if<ReadyState>(&machine.state))
                    {
                        return false;
                    }
                    if (std::get_if<ActiveState>(&machine.state))
                    {
                        return true;
                    }
                    boost::system::error_code ignored_ec;
                    timer.expires_from_now(std::chrono::milliseconds(100));
                    timer.async_wait(yield[ignored_ec]);
                    waitCnt--;
                }
                return false;
            };

            // Mount specialization
            if (isLegacy)
            {
                iface->register_method(
                    "Mount", [& machine = state.machine, this,
                              handleMount](boost::asio::yield_context yield,
                                           std::string imgUrl, bool rw) {
                        LogMsg(Logger::Info, "[App]: Mount called on ",
                               getObjectPath(machine), machine.name);

                        machine.target = {imgUrl};
                        return handleMount(yield, machine);
                    });
            }
            else
            {
                iface->register_method(
                    "Mount", [& machine = state.machine, this,
                              handleMount](boost::asio::yield_context yield) {
                        LogMsg(Logger::Info, "[App]: Mount called on ",
                               getObjectPath(machine), machine.name);

                        return handleMount(yield, machine);
                    });
            }

            iface->initialize();
        }

        std::shared_ptr<sdbusplus::asio::connection> bus;
        std::shared_ptr<sdbusplus::asio::object_server> objServer;
        std::function<void(void)> emitMountEvent;
    };

    struct MountEvent : public BasicEvent
    {
        MountEvent() : BasicEvent(__FUNCTION__)
        {
        }
        State operator()(const ReadyState& state)
        {
            return ActivatingState(state);
        }

        template <typename AnyState>
        State operator()(const AnyState& state)
        {
            throw InvalidStateError("Could not mount on not empty slot");
        }
    };

    struct UnmountEvent : public BasicEvent
    {
        UnmountEvent() : BasicEvent(__FUNCTION__)
        {
        }
        State operator()(const ActivatingState& state)
        {
            return ReadyState(state);
        }
        State operator()(const WaitingForGadgetState& state)
        {
            state.machine.stopProcess(state.process);
            return WaitingForProcessEndState(state);
        }
        State operator()(const ActiveState& state)
        {
            if (!state.machine.removeUsbGadget(state))
            {
                return ReadyState(state);
            }
            state.machine.stopProcess(state.process);
            return WaitingForProcessEndState(state);
        }
        State operator()(const WaitingForProcessEndState& state)
        {
            throw InvalidStateError("Could not unmount on empty slot");
        }
        State operator()(const ReadyState& state)
        {
            throw InvalidStateError("Could not unmount on empty slot");
        }
    };

    struct SubprocessStoppedEvent : public BasicEvent
    {
        SubprocessStoppedEvent() : BasicEvent(__FUNCTION__)
        {
        }
        State operator()(const ActivatingState& state)
        {
            return ReadyState(state);
        }
        State operator()(const WaitingForGadgetState& state)
        {
            state.machine.stopProcess(state.process);
            return ReadyState(state);
        }
        State operator()(const ActiveState& state)
        {
            if (!state.machine.removeUsbGadget(state))
            {
                return ReadyState(state);
            }
            return ReadyState(state);
        }
        State operator()(const WaitingForProcessEndState& state)
        {
            return ReadyState(state);
        }
    };

    struct ActivationStartedEvent : public BasicEvent
    {
        ActivationStartedEvent() : BasicEvent(__FUNCTION__)
        {
        }
        State operator()(const ActivatingState& state)
        {
            if (state.machine.config.mode == Configuration::Mode::proxy)
            {
                return activateProxyMode(state);
            }
            return activateLegacyMode(state);
        }

        State activateProxyMode(const ActivatingState& state)
        {
            auto process = std::make_shared<Process>(
                state.machine.ioc.get(), state.machine.name,
                state.machine.config.nbdDevice);
            if (!process)
            {
                LogMsg(Logger::Error, state.machine.name,
                       " Failed to create Process for: ", state.machine.name);
                return ReadyState(state);
            }
            if (!process->spawn(
                    Configuration::MountPoint::toArgs(state.machine.config),
                    [& machine = state.machine](int exitCode, bool isReady) {
                        LogMsg(Logger::Info, machine.name, " process ended.");
                        machine.emitSubprocessStoppedEvent();
                    }))
            {
                LogMsg(Logger::Error, state.machine.name,
                       " Failed to spawn Process for: ", state.machine.name);
                return ReadyState(state);
            }
            auto newState = WaitingForGadgetState(state);
            newState.process = process;
            return newState;
        }

        State activateLegacyMode(const ActivatingState& state)
        {
            // Check if imgUrl is not emptry
            if (isCifsUrl(state.machine.target->imgUrl))
            {
                auto newState = ActiveState(state);

                return newState;
            }
            else
            {
                throw sdbusplus::exception::SdBusError(
                    EINVAL, "Not supported url's scheme.");
            }
        }

        int prepareTempDirForLegacyMode(std::string& path)
        {
            int result = -1;
            char mountPathTemplate[] = "/tmp/vm_legacy.XXXXXX";
            const char* tmpPath = mkdtemp(mountPathTemplate);
            if (tmpPath != nullptr)
            {
                path = tmpPath;
                result = 0;
            }

            return result;
        }

        bool checkUrl(const std::string& urlScheme, const std::string& imageUrl)
        {
            return (urlScheme.compare(imageUrl.substr(0, urlScheme.size())) ==
                    0);
        }

        bool getImagePathFromUrl(const std::string& urlScheme,
                                 const std::string& imageUrl,
                                 std::string* imagePath)
        {
            if (checkUrl(urlScheme, imageUrl))
            {
                if (imagePath != nullptr)
                {
                    *imagePath = imageUrl.substr(urlScheme.size() - 1);
                    return true;
                }
                else
                {
                    LogMsg(Logger::Error, "Invalid parameter provied");
                    return false;
                }
            }
            else
            {
                LogMsg(Logger::Error, "Provied url does not match scheme");
                return false;
            }
        }

        bool isHttpsUrl(const std::string& imageUrl)
        {
            return checkUrl("https://", imageUrl);
        }

        bool getImagePathFromHttpsUrl(const std::string& imageUrl,
                                      std::string* imagePath)
        {
            return getImagePathFromUrl("https://", imageUrl, imagePath);
        }

        bool isCifsUrl(const std::string& imageUrl)
        {
            return checkUrl("smb://", imageUrl);
        }

        bool getImagePathFromCifsUrl(const std::string& imageUrl,
                                     std::string* imagePath)
        {
            return getImagePathFromUrl("smb://", imageUrl, imagePath);
        }

        fs::path getImagePath(const std::string& imageUrl)
        {
            std::string imagePath;

            if (getImagePathFromHttpsUrl(imageUrl, &imagePath))
            {
                return fs::path(imagePath);
            }
            else if (getImagePathFromCifsUrl(imageUrl, &imagePath))
            {
                return fs::path(imagePath);
            }
            else
            {
                LogMsg(Logger::Error, "Unrecognized url's scheme encountered");
                return fs::path("");
            }
        }
    };

    struct UdevStateChangeEvent : public BasicEvent
    {
        UdevStateChangeEvent(const StateChange& devState) :
            BasicEvent(__FUNCTION__), devState{devState}
        {
        }
        State operator()(const WaitingForGadgetState& state)
        {
            if (devState == StateChange::inserted)
            {
                int32_t ret = UsbGadget::configure(
                    state.machine.name, state.machine.config.nbdDevice,
                    devState);
                if (ret == 0)
                {
                    return ActiveState(state);
                }
                return ReadyState(state);
            }
            return ReadyState(state);
        }

        State operator()(const ReadyState& state)
        {
            if (devState == StateChange::removed)
            {
                LogMsg(Logger::Debug, state.machine.name,
                       " This is acceptable since udev notification is often "
                       "after process is being killed");
            }
            return state;
        }

        template <typename AnyState>
        State operator()(const AnyState& state)
        {
            LogMsg(Logger::Info, name,
                   " Udev State: ", static_cast<int>(devState));
            LogMsg(Logger::Critical, name,
                   " If you receiving this error, this means "
                   "your FSM is broken. Rethink!");
            return state;
        }
        StateChange devState;
    };

    // Helper functions
    bool removeUsbGadget(const BasicState& state)
    {
        int32_t ret = UsbGadget::configure(state.machine.name,
                                           state.machine.config.nbdDevice,
                                           StateChange::removed);
        if (ret != 0)
        {
            // This shouldn't ever happen, perhaps best is to restart app
            LogMsg(Logger::Critical, name, " Some serious failrue happen!");
            return false;
        }
        return true;
    }
    void stopProcess(std::weak_ptr<Process> process)
    {
        if (auto ptr = process.lock())
        {
            ptr->stop();
            return;
        }
        LogMsg(Logger::Info, name, " No process to stop");
    }

    MountPointStateMachine(boost::asio::io_context& ioc,
                           DeviceMonitor& devMonitor, const std::string& name,
                           const Configuration::MountPoint& config) :
        ioc{ioc},
        name{name}, config{config}, state{InitialState(*this)}
    {
        devMonitor.addDevice(config.nbdDevice);
    }

    MountPointStateMachine& operator=(MountPointStateMachine&& machine)
    {
        if (this != &machine)
        {
            state = std::move(machine.state);
            name = std::move(machine.name);
            ioc = machine.ioc;
            config = std::move(machine.config);
        }
        return *this;
    }

    void emitEvent(BasicEvent&& event)
    {
        std::string stateName = std::visit(
            [](const BasicState& state) { return state.stateName; }, state);

        LogMsg(Logger::Debug, name, " received ", event.eventName, " while in ",
               stateName);

        state = std::visit(event, state);
        std::visit([](BasicState& state) { state.onEnter(); }, state);
    }

    void emitRegisterDBusEvent(
        std::shared_ptr<sdbusplus::asio::connection> bus,
        std::shared_ptr<sdbusplus::asio::object_server> objServer)
    {
        emitEvent(RegisterDbusEvent(bus, objServer));
    }

    void emitMountEvent()
    {
        emitEvent(MountEvent());
    }

    void emitUnmountEvent()
    {
        emitEvent(UnmountEvent());
    }

    void emitActivationStartedEvent()
    {
        emitEvent(ActivationStartedEvent());
    }

    void emitSubprocessStoppedEvent()
    {
        emitEvent(SubprocessStoppedEvent());
    }

    void emitUdevStateChangeEvent(const NBDDevice& dev, StateChange devState)
    {
        if (config.nbdDevice == dev)
        {
            emitEvent(UdevStateChangeEvent(devState));
        }
        else
        {
            LogMsg(Logger::Debug, name, " Ignoring request.");
        }
    }

    struct Target
    {
        std::string imgUrl;
    };

    std::reference_wrapper<boost::asio::io_context> ioc;
    std::string name;
    Configuration::MountPoint config;

    std::optional<Target> target;
    State state;
};
