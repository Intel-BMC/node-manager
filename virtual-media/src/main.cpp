#include <sys/mount.h>

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/process.hpp>
#include <filesystem>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

namespace fs = std::filesystem;

namespace udev
{
#include <libudev.h>
struct udev;
struct udev_monitor;
struct udev_device;

struct udevDeleter
{
    void operator()(udev* desc) const
    {
        udev_unref(desc);
    }
};

struct monitorDeleter
{
    void operator()(udev_monitor* desc) const
    {
        udev_monitor_unref(desc);
    };
};

struct deviceDeleter
{
    void operator()(udev_device* desc) const
    {
        udev_device_unref(desc);
    };
};
} // namespace udev

enum class StateChange
{
    notmonitored,
    unknown,
    removed,
    inserted
};

class DeviceMonitor
{
  public:
    DeviceMonitor(boost::asio::io_context& ioc) : ioc(ioc), monitorSd(ioc)
    {
        udev = std::unique_ptr<udev::udev, udev::udevDeleter>(udev::udev_new());
        if (!udev)
        {
            throw std::system_error(EFAULT, std::generic_category(),
                                    "Unable to create uDev handler.");
        }

        monitor = std::unique_ptr<udev::udev_monitor, udev::monitorDeleter>(
            udev::udev_monitor_new_from_netlink(udev.get(), "kernel"));

        if (!monitor)
        {
            throw std::system_error(EFAULT, std::generic_category(),
                                    "Unable to create uDev Monitor handler.");
        }
        int rc = udev_monitor_filter_add_match_subsystem_devtype(
            monitor.get(), "block", "disk");

        if (rc)
        {
            throw std::system_error(EFAULT, std::generic_category(),
                                    "Could not apply filters.");
        }
        rc = udev_monitor_enable_receiving(monitor.get());
        if (rc)
        {
            throw std::system_error(EFAULT, std::generic_category(),
                                    "Enable receiving failed.");
        }
        monitorSd.assign(udev_monitor_get_fd(monitor.get()));
    }

    DeviceMonitor(const DeviceMonitor&) = delete;
    DeviceMonitor(DeviceMonitor&&) = delete;

    DeviceMonitor& operator=(const DeviceMonitor&) = delete;
    DeviceMonitor& operator=(DeviceMonitor&&) = delete;

    template <typename DeviceChangeStateCb>
    void run(DeviceChangeStateCb callback)
    {
        boost::asio::spawn(ioc, [this,
                                 callback](boost::asio::yield_context yield) {
            boost::system::error_code ec;
            while (1)
            {
                monitorSd.async_wait(
                    boost::asio::posix::stream_descriptor::wait_read,
                    yield[ec]);

                std::unique_ptr<udev::udev_device, udev::deviceDeleter> device =
                    std::unique_ptr<udev::udev_device, udev::deviceDeleter>(
                        udev::udev_monitor_receive_device(monitor.get()));
                if (device)
                {
                    const char* devAction =
                        udev_device_get_action(device.get());
                    if (devAction == nullptr)
                    {
                        std::cerr << "Received NULL action.\n";
                        continue;
                    }
                    if (strcmp(devAction, "change") == 0)
                    {
                        const char* sysname =
                            udev_device_get_sysname(device.get());
                        if (sysname == nullptr)
                        {
                            std::cerr << "Received NULL sysname.\n";
                            continue;
                        }
                        auto monitoredDevice = devices.find(sysname);
                        if (monitoredDevice != devices.cend())
                        {
                            const char* size_str =
                                udev_device_get_sysattr_value(device.get(),
                                                              "size");
                            if (size_str == nullptr)
                            {
                                std::cerr << "Received NULL size.\n";
                                continue;
                            }
                            uint64_t size = 0;
                            try
                            {
                                size = std::stoul(size_str, 0, 0);
                            }
                            catch (const std::exception& e)
                            {
                                std::cerr
                                    << "Could not convert size to integer.\n";
                                continue;
                            }
                            if (size > 0 && monitoredDevice->second !=
                                                StateChange::inserted)
                            {
                                std::cout << sysname << " inserted.\n";
                                monitoredDevice->second = StateChange::inserted;
                                callback(sysname, StateChange::inserted);
                            }
                            else if (size == 0 && monitoredDevice->second !=
                                                      StateChange::removed)
                            {
                                std::cout << sysname << " removed.\n";
                                monitoredDevice->second = StateChange::removed;
                                callback(sysname, StateChange::removed);
                            }
                        };
                    }
                }
            }
        });
    }

    void addDevice(const std::string& device)
    {
        std::cout << "Watch on /dev/" << device << '\n';
        devices.insert(
            std::pair<std::string, StateChange>(device, StateChange::unknown));
    }

    StateChange getState(const std::string& device)
    {
        auto monitoredDevice = devices.find(device);
        if (monitoredDevice != devices.cend())
        {
            return monitoredDevice->second;
        }
        return StateChange::notmonitored;
    }

  private:
    boost::asio::io_context& ioc;
    boost::asio::posix::stream_descriptor monitorSd;

    std::unique_ptr<udev::udev, udev::udevDeleter> udev;
    std::unique_ptr<udev::udev_monitor, udev::monitorDeleter> monitor;

    boost::container::flat_map<std::string, StateChange> devices;
};

class Process : public std::enable_shared_from_this<Process>
{
  public:
    Process(boost::asio::io_context& ioc, const std::string& name) :
        ioc(ioc), pipe(ioc), name(name)
    {
    }

    template <typename ExitCb>
    bool spawn(const std::vector<std::string>& args, ExitCb&& onExit)
    {
        std::error_code ec;
        child = boost::process::child(
            "/usr/sbin/nbd-client",
            //"/sbin/nbd-client", "-N", "otherexport", "127.0.0.1", "/dev/nbd0",
            //"-n",
            boost::process::args(args),
            boost::process::on_exit(
                [name(this->name), onExit{std::move(onExit)}](
                    int exit, const std::error_code& ecIn) {
                    std::cout << "- child " << name << "(" << exit << " "
                              << ecIn << ")\n";
                    auto process = processList.find(name);
                    if (process != processList.end() && process->second)
                    {
                        std::cout << "Removing process from processList\n";
                        process->second.reset();
                    }
                    onExit(name);
                }),
            (boost::process::std_out & boost::process::std_err) > pipe, ec,
            ioc);

        if (ec)
        {
            std::cerr << "Error while creating child process: " << ec << '\n';
            return false;
        }

        boost::asio::spawn(ioc, [this, self = shared_from_this()](
                                    boost::asio::yield_context yield) {
            boost::system::error_code bec;
            std::string line;
            boost::asio::dynamic_string_buffer buffer{line};
            std::cout << "Entering COUT Loop\n";
            while (1)
            {
                auto x = boost::asio::async_read_until(pipe, std::move(buffer),
                                                       '\n', yield[bec]);
                std::cout << "-[" << name << "]-> " << line;
                buffer.consume(x);
                if (bec)
                {
                    std::cerr << "Loop Error: " << bec << '\n';
                    break;
                }
            }
            std::cout << "Exiting from COUT Loop\n";
        });
        return true;
    }

    void terminate(int signal = SIGTERM)
    {
        int rc = kill(child.id(), SIGTERM);
        if (rc)
        {
            return;
        }
        child.wait();
    }

    static void addProcess(const std::string& name)
    {
        processList[name] = nullptr;
    }

    template <typename ExitCb>
    static bool spawn(boost::asio::io_context& ioc, const std::string& name,
                      const std::vector<std::string>& args, ExitCb&& onExit)
    {
        auto process = processList.find(name);
        if (process != processList.end() && !process->second)
        {
            std::cout << "Spawning process for " << name << '\n';

            auto newProcess = std::make_shared<Process>(ioc, name);
            if (newProcess->spawn(args, onExit))
            {
                process->second = newProcess;
                return true;
            }
            std::cout << "Process (" << name << ") exited immediately.\n";
            return false;
        }
        std::cout << "No process (" << name << ") to be spawned.\n";
        return false;
    }

    static bool terminate(const std::string& name, int signal = SIGTERM)
    {
        auto process = processList.find(name);
        if (process != processList.end() && process->second)
        {
            std::cout << "Terminating " << name << '\n';
            process->second->terminate(signal);
            return true;
        }
        std::cout << "No process (" << name << ") for termination.\n";
        return false;
    }

    static bool isActive(const std::string& name)
    {
        auto process = processList.find(name);
        if (process != processList.end())
        {
            if (process->second)
            {
                return true;
            }
        }
        return false;
    }

  private:
    boost::asio::io_context& ioc;
    boost::process::child child;
    boost::process::async_pipe pipe;
    std::string name;
    inline static boost::container::flat_map<std::string,
                                             std::shared_ptr<Process>>
        processList;
};

class Configuration
{
  public:
    enum class Mode
    {
        // Proxy mode - works directly from browser and uses JavaScript/HTML5
        // to communicate over Secure WebSockets directly to HTTPS endpoint
        // hosted by bmcweb on BMC
        Proxy = 0,

        // Legacy mode - is initiated from browser using Redfish defined
        // VirtualMedia schemas, then BMC process connects to external
        // CIFS/HTTPS image pointed during initialization.
        Legacy = 1,
    };

    struct MountPoint
    {
        std::string nbdDevice;
        std::string unixSocket;
        std::string endPointId;
        std::optional<int> timeout;
        std::optional<int> blocksize;
        Mode mode;

        static std::vector<std::string> toArgs(const MountPoint& mp)
        {
            std::vector<std::string> args = {
                "-N",          "otherexport",          "-u",
                mp.unixSocket, "/dev/" + mp.nbdDevice, "-n"};
            return args;
        }
    };

    bool valid = false;
    boost::container::flat_map<std::string, MountPoint> mountPoints;

    Configuration(const std::string& file)
    {
        valid = loadConfiguration(file);
    }

  private:
    bool loadConfiguration(const std::string& file) noexcept
    {
        std::ifstream configFile(file);
        if (!configFile.is_open())
        {
            std::cout << "Could not open configuration file\n";
            return false;
        }
        try
        {
            auto data = nlohmann::json::parse(configFile, nullptr);
            setupVariables(data);
        }
        catch (nlohmann::json::exception& e)
        {
            std::cerr << "Error parsing JSON file\n";
            return false;
        }
        catch (std::out_of_range& e)
        {
            std::cerr << "Error parsing JSON file\n";
            return false;
        }
        return true;
    }

    bool setupVariables(const nlohmann::json& config)
    {
        for (const auto& item : config.items())
        {
            if (item.key() == "MountPoints")
            {
                for (const auto& mountpoint : item.value().items())
                {
                    MountPoint mp;
                    const auto nbdDeviceIter =
                        mountpoint.value().find("NBDDevice");
                    if (nbdDeviceIter != mountpoint.value().cend())
                    {
                        const std::string* value =
                            nbdDeviceIter->get_ptr<const std::string*>();
                        if (value)
                        {
                            mp.nbdDevice = *value;
                        }
                        else
                        {
                            std::cerr << "NBDDevice required, not set\n";
                            continue;
                        }
                    };
                    const auto unixSocketIter =
                        mountpoint.value().find("UnixSocket");
                    if (unixSocketIter != mountpoint.value().cend())
                    {
                        const std::string* value =
                            unixSocketIter->get_ptr<const std::string*>();
                        if (value)
                        {
                            mp.unixSocket = *value;
                        }
                        else
                        {
                            std::cerr << "UnixSocket required, not set\n";
                            continue;
                        }
                    }
                    const auto endPointIdIter =
                        mountpoint.value().find("EndpointId");
                    if (endPointIdIter != mountpoint.value().cend())
                    {
                        const std::string* value =
                            endPointIdIter->get_ptr<const std::string*>();
                        if (value)
                        {
                            mp.endPointId = *value;
                        }
                        else
                        {
                            std::cerr << "EndpointId required, not set\n";
                            continue;
                        }
                    }
                    const auto timeoutIter = mountpoint.value().find("Timeout");
                    if (timeoutIter != mountpoint.value().cend())
                    {
                        const uint64_t* value =
                            timeoutIter->get_ptr<const uint64_t*>();
                        if (value)
                        {
                            mp.timeout = *value;
                        }
                        else
                        {
                            std::cout << "Timeout not set, use default\n";
                        }
                    }
                    const auto blocksizeIter =
                        mountpoint.value().find("BlockSize");
                    if (blocksizeIter != mountpoint.value().cend())
                    {
                        const uint64_t* value =
                            blocksizeIter->get_ptr<const uint64_t*>();
                        if (value)
                        {
                            mp.blocksize = *value;
                        }
                        else
                        {
                            std::cout << "BlockSize not set, use default\n";
                        }
                    }
                    const auto modeIter = mountpoint.value().find("Mode");
                    if (modeIter != mountpoint.value().cend())
                    {
                        const uint64_t* value =
                            modeIter->get_ptr<const uint64_t*>();
                        if (value)
                        {
                            if (*value == 0)
                            {
                                mp.mode = Configuration::Mode::Proxy;
                            }
                            else if (*value == 1)
                            {
                                mp.mode = Configuration::Mode::Legacy;
                            }
                            else
                            {
                                std::cerr << "Incorrect Mode, skip this mount "
                                             "point\n";
                                continue;
                            }
                        }
                        else
                        {
                            std::cerr
                                << "Mode not set, skip this mount point\n";
                            continue;
                        }
                    }
                    else
                    {
                        std::cerr
                            << "Mode does not exist, skip this mount point\n";
                        continue;
                    }
                    mountPoints[mountpoint.key()] = std::move(mp);
                }
            }
        }
        return true;
    }
};

class ParametersManager
{
  public:
    struct Parameters
    {
        // Legacy mode specific parameters
        std::string imageUrl;
        std::string mountDirectoryPath;
    };

    ParametersManager()
    {
    }

    void addMountPoint(const std::string& name)
    {
        Parameters parameters;
        parameters.imageUrl.clear();
        parameters.mountDirectoryPath.clear();
        mountPoints[name] = std::move(parameters);
    }

    Parameters* getMountPoint(const std::string& name)
    {
        if (mountPoints.find(name) != mountPoints.end())
        {
            return &mountPoints[name];
        }
        else
        {
            return nullptr;
        }
    }

  private:
    boost::container::flat_map<std::string, Parameters> mountPoints;
};

class App
{
  public:
    App(boost::asio::io_context& ioc, const Configuration& config,
        sd_bus* custom_bus = nullptr) :
        ioc(ioc),
        devMonitor(ioc), config(config), paramMgr()
    {
        if (!custom_bus)
        {
            bus = std::make_shared<sdbusplus::asio::connection>(ioc);
        }
        else
        {
            bus =
                std::make_shared<sdbusplus::asio::connection>(ioc, custom_bus);
        }
        objServer = std::make_shared<sdbusplus::asio::object_server>(bus);
        bus->request_name("xyz.openbmc_project.VirtualMedia");
        objManager = std::make_shared<sdbusplus::server::manager::manager>(
            *bus, "/xyz/openbmc_project/VirtualMedia");
        for (const auto& entry : config.mountPoints)
        {
            if (entry.second.mode == Configuration::Mode::Proxy)
            {
                devMonitor.addDevice(entry.second.nbdDevice);
                Process::addProcess(entry.first);
            }

            addMountPointInterface(entry.first, entry.second);

            if (entry.second.mode == Configuration::Mode::Proxy)
            {
                addProxyInterface(entry.first, entry.second);
                addProcessInterface(entry.first);
            }
            else
            {
                addLegacyInterface(entry.first);
            }

            paramMgr.addMountPoint(entry.first);
        }
        devMonitor.run([this](const std::string& device, StateChange change) {
            configureUsbGadget(device, change);
        });
    };

  private:
    bool echoToFile(const fs::path& fname, const std::string& content)
    {
        std::ofstream fileWriter;
        fileWriter.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        fileWriter.open(fname, std::ios::out | std::ios::app);
        fileWriter << content << std::endl; // Make sure for new line and flush
        fileWriter.close();
        return true;
    }

    int configureUsbGadget(const std::string& device, StateChange change,
                           const std::string& lunFile = "")
    {
        int result = 0;
        std::error_code ec;
        if (change == StateChange::unknown)
        {
            std::cerr << "Change to unknown state is not possible\n";
            return -1;
        }

        // If lun file provided just use it to inject to the lun.0/file
        // instead of /dev/<device>
        std::string lunFileInternal;
        if (lunFile.empty())
        {
            lunFileInternal = "/dev/" + device;
        }
        else
        {
            lunFileInternal = lunFile;
        }

        const fs::path gadgetDir =
            "/sys/kernel/config/usb_gadget/mass-storage-" + device;
        const fs::path funcMassStorageDir =
            gadgetDir / "functions/mass_storage.usb0";
        const fs::path stringsDir = gadgetDir / "strings/0x409";
        const fs::path configDir = gadgetDir / "configs/c.1";
        const fs::path massStorageDir = configDir / "mass_storage.usb0";
        const fs::path configStringsDir = configDir / "strings/0x409";

        if (change == StateChange::inserted)
        {
            try
            {
                fs::create_directories(gadgetDir);
                echoToFile(gadgetDir / "idVendor", "0x1d6b");
                echoToFile(gadgetDir / "idProduct", "0x0104");
                fs::create_directories(stringsDir);
                echoToFile(stringsDir / "manufacturer", "OpenBMC");
                echoToFile(stringsDir / "product", "Virtual Media Device");
                fs::create_directories(configStringsDir);
                echoToFile(configStringsDir / "configuration", "config 1");
                fs::create_directories(funcMassStorageDir);
                fs::create_directories(funcMassStorageDir / "lun.0");
                fs::create_directory_symlink(funcMassStorageDir,
                                             massStorageDir);
                echoToFile(funcMassStorageDir / "lun.0/removable", "1");
                echoToFile(funcMassStorageDir / "lun.0/ro", "1");
                echoToFile(funcMassStorageDir / "lun.0/cdrom", "0");
                echoToFile(funcMassStorageDir / "lun.0/file", lunFileInternal);

                for (const auto& port : fs::directory_iterator(
                         "/sys/bus/platform/devices/1e6a0000.usb-vhub"))
                {
                    if (fs::is_directory(port) && !fs::is_symlink(port) &&
                        !fs::exists(port.path() / "gadget/suspended"))
                    {
                        const std::string portId = port.path().filename();
                        std::cout << "Use port : " << port.path().filename()
                                  << '\n';
                        echoToFile(gadgetDir / "UDC", portId);
                        return 0;
                    }
                }
            }
            catch (fs::filesystem_error& e)
            {
                // Got error perform cleanup
                std::cerr << e.what() << '\n';
                result = -1;
            }
            catch (std::ofstream::failure& e)
            {
                // Got error perform cleanup
                std::cerr << e.what() << '\n';
                result = -1;
            }
        }
        // StateChange: unknown, notmonitored, inserted were handler
        // earlier. We'll get here only for removed, or cleanup

        fs::remove_all(massStorageDir, ec);
        if (ec)
        {
            std::cerr << ec.message() << '\n';
        }
        fs::remove_all(funcMassStorageDir, ec);
        if (ec)
        {
            std::cerr << ec.message() << '\n';
        }
        fs::remove_all(configStringsDir, ec);
        if (ec)
        {
            std::cerr << ec.message() << '\n';
        }
        fs::remove_all(configDir, ec);
        if (ec)
        {
            std::cerr << ec.message() << '\n';
        }
        fs::remove_all(stringsDir, ec);
        if (ec)
        {
            std::cerr << ec.message() << '\n';
        }
        fs::remove_all(gadgetDir, ec);
        if (ec)
        {
            std::cerr << ec.message() << '\n';
            result = -1;
        }

        return result;
    }

    void addMountPointInterface(const std::string& name,
                                const Configuration::MountPoint& mp)
    {
        std::string objPath;
        if (mp.mode == Configuration::Mode::Proxy)
        {
            objPath = "/xyz/openbmc_project/VirtualMedia/Proxy/";
        }
        else
        {
            objPath = "/xyz/openbmc_project/VirtualMedia/Legacy/";
        }

        auto iface = objServer->add_interface(
            objPath + name, "xyz.openbmc_project.VirtualMedia.MountPoint");
        iface->register_property("Device", mp.nbdDevice);
        iface->register_property("EndpointId", mp.endPointId);
        iface->register_property("Socket", mp.unixSocket);
        iface->register_property("Timeout", *mp.timeout);
        iface->register_property("BlockSize", *mp.blocksize);
        iface->register_property(
            "ImageUrl", std::string(""),
            [](const std::string& req, std::string& property) {
                throw sdbusplus::exception::SdBusError(
                    EPERM, "Setting ImageUrl property is not allowed");
                return -1;
            },
            [this, name](const std::string& property) {
                auto parameters = paramMgr.getMountPoint(name);
                if (parameters == nullptr)
                {
                    return std::string("");
                }
                else
                {
                    return parameters->imageUrl;
                }
            });
        iface->initialize();
    };

    void addProxyInterface(const std::string& name,
                           const Configuration::MountPoint& mp)
    {
        auto iface = objServer->add_interface(
            "/xyz/openbmc_project/VirtualMedia/Proxy/" + name,
            "xyz.openbmc_project.VirtualMedia.Proxy");
        iface->register_method(
            "Mount", [&, name, args(Configuration::MountPoint::toArgs(mp))]() {
                return Process::spawn(
                    ioc, name, args, [this](const std::string& name) {
                        configureUsbGadget(name, StateChange::removed);
                    });
            });
        iface->register_method("Unmount", [this, name]() {
            const auto mp = config.mountPoints.find(name);
            std::string nbdDevice = mp->second.nbdDevice;
            configureUsbGadget(nbdDevice, StateChange::removed);
            return Process::terminate(name);
        });
        iface->initialize();
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
        if (urlScheme.compare(imageUrl.substr(0, urlScheme.size())) == 0)
        {
            return true;
        }
        else
        {
            return false;
        }
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
                std::cerr << "Invalid parameter provied\n";
                return false;
            }
        }
        else
        {
            std::cerr << "Provied url does not match scheme\n";
            return false;
        }
    }

    bool checkHttpsUrl(const std::string& imageUrl)
    {
        return checkUrl("https://", imageUrl);
    }

    bool getImagePathFromHttpsUrl(const std::string& imageUrl,
                                  std::string* imagePath)
    {
        return getImagePathFromUrl("https://", imageUrl, imagePath);
    }

    bool checkCifsUrl(const std::string& imageUrl)
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
            std::cerr << "Unrecognized url's scheme encountered\n";
            return fs::path("");
        }
    }

    int mountCifsUrlForLegcyMode(const std::string& name,
                                 const std::string& imageUrl,
                                 ParametersManager::Parameters* parameters)
    {
        int result = -1;
        fs::path imageUrlPath = getImagePath(imageUrl);
        const std::string imageUrlParentPath =
            "/" + imageUrlPath.parent_path().string();
        std::string mountDirectoryPath;
        result = prepareTempDirForLegacyMode(mountDirectoryPath);
        if (result != 0)
        {
            std::cerr << "Failed to create tmp directory\n";
            return result;
        }

        result = mount(imageUrlParentPath.c_str(), mountDirectoryPath.c_str(),
                       "cifs", 0,
                       "username=,password=,nolock,sec="
                       "ntlmsspi,seal,vers=3.0");
        if (result != 0)
        {
            std::cerr << "Failed to mount the url\n";
            fs::remove_all(fs::path(mountDirectoryPath));
            return result;
        }

        const std::string imageMountPath =
            mountDirectoryPath + "/" + imageUrlPath.filename().string();
        result =
            configureUsbGadget(name, StateChange::inserted, imageMountPath);
        if (result != 0)
        {
            std::cerr << "Failed to run usb gadget\n";
            umount(mountDirectoryPath.c_str());
            fs::remove_all(fs::path(mountDirectoryPath));
            return result;
        }

        parameters->mountDirectoryPath = mountDirectoryPath;

        return result;
    }

    int umountCifsUrlForLegcyMode(const std::string& name,
                                  ParametersManager::Parameters* parameters)
    {
        int result = -1;

        result = configureUsbGadget(name, StateChange::removed);
        if (result != 0)
        {
            std::cerr << "Failed to unmount resource\n";
            return result;
        }

        umount(parameters->mountDirectoryPath.c_str());
        fs::remove_all(fs::path(parameters->mountDirectoryPath));
        parameters->mountDirectoryPath.clear();

        return result;
    }

    void addLegacyInterface(const std::string& name)
    {
        auto iface = objServer->add_interface(
            "/xyz/openbmc_project/VirtualMedia/Legacy/" + name,
            "xyz.openbmc_project.VirtualMedia.Legacy");
        iface->register_method(
            "Mount", [this, name](const std::string& imageUrl) {
                auto parameters = paramMgr.getMountPoint(name);
                if (parameters == nullptr)
                {
                    throw sdbusplus::exception::SdBusError(
                        ENOENT, "Failed to find the node.");
                }

                if (!parameters->imageUrl.empty())
                {
                    throw sdbusplus::exception::SdBusError(
                        ETXTBSY, "Node already used and resource mounted.");
                }

                if (checkCifsUrl(imageUrl))
                {
                    int result =
                        mountCifsUrlForLegcyMode(name, imageUrl, parameters);
                    if (result != 0)
                    {
                        throw sdbusplus::exception::SdBusError(
                            -result, "Failed to mount cifs url.");
                    }
                }
                else
                {
                    throw sdbusplus::exception::SdBusError(
                        EINVAL, "Not supported url's scheme.");
                }

                parameters->imageUrl = imageUrl;
            });
        iface->register_method("Unmount", [this, name]() {
            auto parameters = paramMgr.getMountPoint(name);
            if (parameters == nullptr)
            {
                throw sdbusplus::exception::SdBusError(
                    ENOENT, "Failed to find the node.");
            }

            if (parameters->imageUrl.empty())
            {
                throw sdbusplus::exception::SdBusError(
                    ENOENT, "Node is not used and no resource mounted.");
            }

            if (checkCifsUrl(parameters->imageUrl))
            {
                int result = umountCifsUrlForLegcyMode(name, parameters);
                if (result != 0)
                {
                    throw sdbusplus::exception::SdBusError(
                        -result, "Failed to unmount cifs resource.");
                }
            }
            else
            {
                throw sdbusplus::exception::SdBusError(
                    EINVAL, "Not supported url's scheme.");
            }

            parameters->imageUrl.clear();
        });
        iface->initialize();
    }

    void addProcessInterface(const std::string& name)
    {
        auto iface = objServer->add_interface(
            "/xyz/openbmc_project/VirtualMedia/Proxy/" + name,
            "xyz.openbmc_project.VirtualMedia.Process");
        iface->register_property(
            "Active", bool(false),
            [](const bool& req, bool& property) { return 0; },
            [name](const bool& property) { return Process::isActive(name); });
        iface->initialize();
    }

    boost::asio::io_context& ioc;
    DeviceMonitor devMonitor;
    std::shared_ptr<sdbusplus::asio::connection> bus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    std::shared_ptr<sdbusplus::server::manager::manager> objManager;
    const Configuration& config;
    ParametersManager paramMgr;
};

int main()
{
    Configuration config("/etc/virtual-media.json");
    if (!config.valid)
        return -1;

    boost::asio::io_context ioc;
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&ioc](const boost::system::error_code&, const int&) { ioc.stop(); });

    sd_bus* b = nullptr;
#if defined(CUSTOM_DBUS_PATH)
#pragma message("You are using custom DBUS path set to " CUSTOM_DBUS_PATH)
    sd_bus_new(&b);
    sd_bus_set_bus_client(b, true);
    sd_bus_set_address(b, CUSTOM_DBUS_PATH);
    sd_bus_start(b);
#endif
    App app(ioc, config, b);

    ioc.run();

    return 0;
}
