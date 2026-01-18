// Copyright (c) 2025 Elias Bachaalany
// SPDX-License-Identifier: MIT

#pragma once

#include <copilot/process.hpp>
#include <copilot/transport.hpp>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <unistd.h>
#endif

#include <algorithm>
#include <atomic>
#include <string>

namespace copilot
{

/// Transport that wraps stdio file handles (stdin/stdout or pipe handles)
///
/// This is used for the stdio JSON-RPC mode where the SDK communicates
/// with the CLI process via its stdin/stdout streams.
class StdioTransport : public ITransport
{
  public:
    /// Platform-specific handle type
#ifdef _WIN32
    using Handle = HANDLE;
    // Note: INVALID_HANDLE_VALUE is not constexpr-compatible on MinGW/GCC
    static inline Handle invalid_handle()
    {
        return INVALID_HANDLE_VALUE;
    }
#else
    using Handle = int;
    static constexpr Handle invalid_handle()
    {
        return -1;
    }
#endif

    /// Construct from read/write handles
    /// @param read_handle Handle to read from (e.g., process stdout)
    /// @param write_handle Handle to write to (e.g., process stdin)
    /// @param owns_handles If true, handles will be closed on destruction
    StdioTransport(Handle read_handle, Handle write_handle, bool owns_handles = true)
        : read_handle_(read_handle), write_handle_(write_handle), owns_handles_(owns_handles),
          open_(true)
    {
    }

    ~StdioTransport() override
    {
        close();
    }

    // Non-copyable
    StdioTransport(const StdioTransport&) = delete;
    StdioTransport& operator=(const StdioTransport&) = delete;

    // Movable
    StdioTransport(StdioTransport&& other) noexcept
        : read_handle_(other.read_handle_), write_handle_(other.write_handle_),
          owns_handles_(other.owns_handles_), open_(other.open_.load())
    {
        other.read_handle_ = invalid_handle();
        other.write_handle_ = invalid_handle();
        other.owns_handles_ = false;
        other.open_ = false;
    }

    StdioTransport& operator=(StdioTransport&& other) noexcept
    {
        if (this != &other)
        {
            close();
            read_handle_ = other.read_handle_;
            write_handle_ = other.write_handle_;
            owns_handles_ = other.owns_handles_;
            open_ = other.open_.load();
            other.read_handle_ = invalid_handle();
            other.write_handle_ = invalid_handle();
            other.owns_handles_ = false;
            other.open_ = false;
        }
        return *this;
    }

    size_t read(char* buffer, size_t size) override;
    void write(const char* data, size_t size) override;
    void close() override;
    bool is_open() const override
    {
        return open_;
    }

    Handle read_handle() const
    {
        return read_handle_;
    }
    Handle write_handle() const
    {
        return write_handle_;
    }

  private:
    Handle read_handle_;
    Handle write_handle_;
    bool owns_handles_;
    std::atomic<bool> open_;
};

// =============================================================================
// Platform-specific implementations
// =============================================================================

#ifdef _WIN32

inline size_t StdioTransport::read(char* buffer, size_t size)
{
    if (!open_)
        throw ConnectionClosedError();

    DWORD bytes_read = 0;
    if (!ReadFile(read_handle_, buffer, static_cast<DWORD>(size), &bytes_read, nullptr))
    {
        DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA)
        {
            open_ = false;
            return 0; // EOF
        }
        throw TransportError("ReadFile failed with error " + std::to_string(error));
    }

    if (bytes_read == 0)
        open_ = false;
    return bytes_read;
}

inline void StdioTransport::write(const char* data, size_t size)
{
    if (!open_)
        throw ConnectionClosedError();

    size_t total_written = 0;
    while (total_written < size)
    {
        DWORD bytes_written = 0;
        DWORD to_write = static_cast<DWORD>(std::min(size - total_written, size_t{MAXDWORD}));

        if (!WriteFile(write_handle_, data + total_written, to_write, &bytes_written, nullptr))
        {
            DWORD error = GetLastError();
            throw TransportError("WriteFile failed with error " + std::to_string(error));
        }

        total_written += bytes_written;
    }
}

inline void StdioTransport::close()
{
    if (!open_.exchange(false))
        return; // Already closed

    if (owns_handles_)
    {
        if (read_handle_ != invalid_handle())
            CloseHandle(read_handle_);
        if (write_handle_ != invalid_handle() && write_handle_ != read_handle_)
            CloseHandle(write_handle_);
    }
    read_handle_ = invalid_handle();
    write_handle_ = invalid_handle();
}

#else // POSIX

inline size_t StdioTransport::read(char* buffer, size_t size)
{
    if (!open_)
        throw ConnectionClosedError();

    ssize_t bytes_read = ::read(read_handle_, buffer, size);
    if (bytes_read < 0)
    {
        if (errno == EPIPE || errno == EBADF)
        {
            open_ = false;
            return 0;
        }
        throw TransportError("read() failed: " + std::string(strerror(errno)));
    }

    if (bytes_read == 0)
        open_ = false;
    return static_cast<size_t>(bytes_read);
}

inline void StdioTransport::write(const char* data, size_t size)
{
    if (!open_)
        throw ConnectionClosedError();

    size_t total_written = 0;
    while (total_written < size)
    {
        ssize_t bytes_written = ::write(write_handle_, data + total_written, size - total_written);
        if (bytes_written < 0)
            throw TransportError("write() failed: " + std::string(strerror(errno)));
        total_written += static_cast<size_t>(bytes_written);
    }
}

inline void StdioTransport::close()
{
    if (!open_.exchange(false))
        return;

    if (owns_handles_)
    {
        if (read_handle_ != invalid_handle())
            ::close(read_handle_);
        if (write_handle_ != invalid_handle() && write_handle_ != read_handle_)
            ::close(write_handle_);
    }
    read_handle_ = invalid_handle();
    write_handle_ = invalid_handle();
}

#endif // _WIN32

// =============================================================================
// PipeTransport - Transport adapter for Process pipes
// =============================================================================

/// Transport that wraps Process ReadPipe and WritePipe
///
/// This adapter allows using Process pipes with the JSON-RPC client.
/// The pipes are owned by the Process, so this transport doesn't close them.
class PipeTransport : public ITransport
{
  public:
    /// Construct from WritePipe (for writing) and ReadPipe (for reading)
    /// @param write_pipe Reference to the process stdin pipe
    /// @param read_pipe Reference to the process stdout pipe
    /// @note The pipes must outlive this transport
    PipeTransport(WritePipe& write_pipe, ReadPipe& read_pipe)
        : write_pipe_(&write_pipe), read_pipe_(&read_pipe), open_(true)
    {
    }

    ~PipeTransport() override
    {
        // Don't close pipes - they're owned by Process
        open_ = false;
    }

    // Non-copyable, non-movable (references to external pipes)
    PipeTransport(const PipeTransport&) = delete;
    PipeTransport& operator=(const PipeTransport&) = delete;
    PipeTransport(PipeTransport&&) = delete;
    PipeTransport& operator=(PipeTransport&&) = delete;

    size_t read(char* buffer, size_t size) override
    {
        if (!open_ || !read_pipe_)
            throw ConnectionClosedError();
        try
        {
            return read_pipe_->read(buffer, size);
        }
        catch (const std::exception&)
        {
            open_ = false;
            throw;
        }
    }

    void write(const char* data, size_t size) override
    {
        if (!open_ || !write_pipe_)
            throw ConnectionClosedError();
        try
        {
            write_pipe_->write(data, size);
        }
        catch (const std::exception&)
        {
            open_ = false;
            throw;
        }
    }

    void close() override
    {
        open_ = false;
        // Don't close pipes - they're owned by Process
    }

    bool is_open() const override
    {
        return open_;
    }

  private:
    WritePipe* write_pipe_;
    ReadPipe* read_pipe_;
    std::atomic<bool> open_;
};

} // namespace copilot
