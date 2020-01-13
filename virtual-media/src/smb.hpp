#pragma once

#include "logger.hpp"
#include "utils.hpp"

#include <sys/mount.h>

#include <filesystem>
#include <optional>

namespace fs = std::filesystem;

class SmbShare
{
  public:
    SmbShare(const fs::path& mountDir) : mountDir(mountDir)
    {
    }

    bool mount(const fs::path& remote, bool rw,
               const std::unique_ptr<utils::CredentialsProvider>& credentials)
    {
        LogMsg(Logger::Debug, "Trying to mount remote : ", remote);

        const std::string params = "nolock,sec=ntlmsspi,seal,vers=3.0";
        const std::string perm = rw ? "rw" : "ro";
        auto options = params + "," + perm;
        LogMsg(Logger::Debug, "Mounting with options: ", options);

        std::string credentialsOpt;
        if (!credentials)
        {
            LogMsg(Logger::Info, "Mounting as Guest");
            credentialsOpt = "guest,user=OpenBmc";
        }
        else
        {
            LogMsg(Logger::Info, "Authenticating as ", credentials->user());
            credentialsOpt = "user=" + credentials->user() +
                             ",password=" + credentials->password();
        }

        options += "," + credentialsOpt;

        auto ec = ::mount(remote.c_str(), mountDir.c_str(), "cifs", 0,
                          options.c_str());

        utils::secureCleanup(options);
        utils::secureCleanup(credentialsOpt);

        if (ec)
        {
            LogMsg(Logger::Error, "Mount failed with ec = ", ec,
                   " errno = ", errno);
            return false;
        }

        return true;
    }

    static std::optional<fs::path> createMountDir(const fs::path& name)
    {
        auto destPath = fs::temp_directory_path() / name;
        std::error_code ec;

        if (fs::create_directory(destPath, ec))
        {
            return destPath;
        }

        LogMsg(Logger::Error, ec,
               " : Unable to create mount directory: ", destPath);
        return {};
    }

    static void unmount(const fs::path& mountDir)
    {
        int result;
        std::error_code ec;

        result = ::umount(mountDir.string().c_str());
        if (result)
        {
            LogMsg(Logger::Error, result, " : Unable to unmout directory ",
                   mountDir);
        }

        if (!fs::remove_all(mountDir, ec))
        {
            LogMsg(Logger::Error, ec, " : Unable to remove mount directory ",
                   mountDir);
        }
    }

  private:
    std::string mountDir;
};