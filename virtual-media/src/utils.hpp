#pragma once

#include <boost/process/async_pipe.hpp>
#include <boost/type_traits/has_dereference.hpp>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace fs = std::filesystem;

namespace utils
{
constexpr const size_t secretLimit = 1024;

template <typename T>
static void secureCleanup(T& value)
{
    auto raw = const_cast<typename T::value_type*>(&value[0]);
    explicit_bzero(raw, value.size() * sizeof(*raw));
}

class Credentials
{
  public:
    Credentials(std::string&& user, std::string&& password) :
        userBuf(std::move(user)), passBuf(std::move(password))
    {
    }

    ~Credentials()
    {
        secureCleanup(userBuf);
        secureCleanup(passBuf);
    }

    const std::string& user()
    {
        return userBuf;
    }

    const std::string& password()
    {
        return passBuf;
    }

  private:
    Credentials() = delete;
    Credentials(const Credentials&) = delete;
    Credentials& operator=(const Credentials&) = delete;

    std::string userBuf;
    std::string passBuf;
};

class CredentialsProvider
{
  public:
    template <typename T>
    struct Deleter
    {
        void operator()(T* buff) const
        {
            if (buff)
            {
                secureCleanup(*buff);
                delete buff;
            }
        }
    };

    using Buffer = std::vector<char>;
    using SecureBuffer = std::unique_ptr<Buffer, Deleter<Buffer>>;
    // Using explicit definition instead of std::function to avoid implicit
    // conversions eg. stack copy instead of reference for parameters
    using FormatterFunc = void(const std::string& username,
                               const std::string& password, Buffer& dest);

    CredentialsProvider(std::string&& user, std::string&& password) :
        credentials(std::move(user), std::move(password))
    {
    }

    const std::string& user()
    {
        return credentials.user();
    }

    const std::string& password()
    {
        return credentials.password();
    }

    SecureBuffer pack(const FormatterFunc formatter)
    {
        SecureBuffer packed{new Buffer{}};
        if (formatter)
        {
            formatter(credentials.user(), credentials.password(), *packed);
        }
        return packed;
    }

  private:
    Credentials credentials;
};

// Wrapper for boost::async_pipe ensuring proper pipe cleanup
template <typename Buffer>
class NamedPipe
{
  public:
    using unix_fd = sdbusplus::message::unix_fd;

    NamedPipe(boost::asio::io_context& io, const std::string name,
              Buffer&& buffer) :
        name(name),
        impl(io, name), buffer{std::move(buffer)}
    {
    }

    ~NamedPipe()
    {
        // Named pipe needs to be explicitly removed
        impl.close();
        ::unlink(name.c_str());
    }

    unix_fd fd()
    {
        return unix_fd{impl.native_sink()};
    }

    const std::string& file()
    {
        return name;
    }

    template <typename WriteHandler>
    void async_write(WriteHandler&& handler)
    {
        impl.async_write_some(data(), std::forward<WriteHandler>(handler));
    }

  private:
    // Specialization for pointer types
    template <typename B = Buffer>
    typename std::enable_if<boost::has_dereference<B>::value,
                            boost::asio::const_buffer>::type
        data()
    {
        return boost::asio::buffer(*buffer);
    }

    template <typename B = Buffer>
    typename std::enable_if<!boost::has_dereference<B>::value,
                            boost::asio::const_buffer>::type
        data()
    {
        return boost::asio::buffer(buffer);
    }

    const std::string name;
    boost::process::async_pipe impl;
    Buffer buffer;
};

class VolatileFile
{
    using Buffer = CredentialsProvider::SecureBuffer;

  public:
    VolatileFile(Buffer&& contents) :
        filePath(fs::temp_directory_path() / std::tmpnam(nullptr)),
        size(contents->size())
    {
        auto data = std::move(contents);
        create(filePath, data);
    }

    ~VolatileFile()
    {
        // Purge file contents
        std::array<char, secretLimit> buf;
        buf.fill('*');
        std::ofstream file(filePath);
        std::size_t bytesWritten = 0, bytesToWrite = 0;

        while (bytesWritten < size)
        {
            bytesToWrite = std::min(secretLimit, (size - bytesWritten));
            file.write(buf.data(), bytesToWrite);
            bytesWritten += bytesToWrite;
        }

        // Remove leftover file
        fs::remove(filePath);
    }

    const std::string& path()
    {
        return filePath;
    }

  private:
    static void create(const std::string& filePath, const Buffer& data)
    {
        // Create file
        std::ofstream file(filePath);

        // Limit permissions to owner only
        fs::permissions(filePath,
                        fs::perms::owner_read | fs::perms::owner_write,
                        fs::perm_options::replace);

        // Write contents
        file.write(data->data(), data->size());
    }

    const std::string filePath;
    const std::size_t size;
};
} // namespace utils
