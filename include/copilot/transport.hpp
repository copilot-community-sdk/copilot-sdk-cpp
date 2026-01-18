// Copyright (c) 2025 Elias Bachaalany
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace copilot
{

// =============================================================================
// Transport Exceptions
// =============================================================================

/// Exception thrown when transport operations fail
class TransportError : public std::runtime_error
{
  public:
    explicit TransportError(const std::string& message) : std::runtime_error(message) {}
};

/// Exception thrown when connection is closed
class ConnectionClosedError : public TransportError
{
  public:
    ConnectionClosedError() : TransportError("Connection closed") {}
    explicit ConnectionClosedError(const std::string& message) : TransportError(message) {}
};

// =============================================================================
// Transport Interface
// =============================================================================

/// Abstract interface for raw byte I/O transport
///
/// Implementations provide the underlying byte stream (stdio pipes, TCP sockets, etc.)
/// The transport is responsible for reading/writing raw bytes; framing is handled
/// separately by MessageFramer.
class ITransport
{
  public:
    virtual ~ITransport() = default;

    /// Read up to `size` bytes into buffer
    /// @param buffer Destination buffer
    /// @param size Maximum bytes to read
    /// @return Number of bytes actually read (0 indicates EOF)
    /// @throws TransportError on read failure
    virtual size_t read(char* buffer, size_t size) = 0;

    /// Write all bytes to the transport
    /// @param data Source data
    /// @param size Number of bytes to write
    /// @throws TransportError on write failure
    virtual void write(const char* data, size_t size) = 0;

    /// Close the transport
    virtual void close() = 0;

    /// Check if transport is open
    virtual bool is_open() const = 0;

    // Convenience overloads
    void write(const std::string& data)
    {
        write(data.data(), data.size());
    }

    void write(const std::vector<char>& data)
    {
        write(data.data(), data.size());
    }
};

// =============================================================================
// Content-Length Message Framer (LSP-style)
// =============================================================================

/// Handles Content-Length header framing for JSON-RPC messages
///
/// Message format:
/// ```
/// Content-Length: <length>\r\n
/// \r\n
/// <json-rpc-message>
/// ```
///
/// This is the standard LSP (Language Server Protocol) framing used by
/// StreamJsonRpc's HeaderDelimitedMessageHandler.
class MessageFramer
{
  public:
    explicit MessageFramer(ITransport& transport) : transport_(transport) {}

    /// Read a complete framed message
    /// @return The message content (without headers)
    /// @throws TransportError on read failure or invalid framing
    /// @throws ConnectionClosedError if connection is closed
    std::string read_message();

    /// Write a message with Content-Length framing
    /// @param message The message content to send
    /// @throws TransportError on write failure
    void write_message(const std::string& message);

  private:
    ITransport& transport_;
    std::vector<char> buffer_;
    size_t buffer_pos_ = 0;
    size_t buffer_len_ = 0;

    /// Read exactly n bytes from transport
    void read_exact(char* buffer, size_t n);

    /// Read a single line (up to \r\n or \n)
    std::string read_line();

    /// Ensure buffer has at least n bytes available
    void fill_buffer(size_t min_bytes);
};

// =============================================================================
// Inline implementations
// =============================================================================

inline std::string MessageFramer::read_message()
{
    // Read headers until empty line
    std::optional<size_t> content_length;

    while (true)
    {
        auto line = read_line();

        // Empty line signals end of headers
        if (line.empty())
            break;

        // Parse Content-Length header (case-insensitive)
        const std::string prefix = "content-length:";
        std::string lower_line = line;
        for (auto& c : lower_line)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (lower_line.compare(0, prefix.size(), prefix) == 0)
        {
            auto value_str = line.substr(prefix.size());
            // Trim whitespace
            size_t start = value_str.find_first_not_of(" \t");
            if (start != std::string::npos)
                value_str = value_str.substr(start);
            try
            {
                content_length = std::stoull(value_str);
            }
            catch (...)
            {
                throw TransportError("Invalid Content-Length value: " + value_str);
            }
        }
        // Ignore other headers (e.g., Content-Type)
    }

    if (!content_length)
        throw TransportError("Missing Content-Length header");

    // Read the message body
    std::string message(*content_length, '\0');
    read_exact(message.data(), *content_length);

    return message;
}

inline void MessageFramer::write_message(const std::string& message)
{
    std::string frame = "Content-Length: " + std::to_string(message.size()) + "\r\n\r\n" + message;
    transport_.write(frame);
}

inline void MessageFramer::read_exact(char* buffer, size_t n)
{
    size_t total_read = 0;

    // First, use any buffered data
    while (total_read < n && buffer_pos_ < buffer_len_)
        buffer[total_read++] = buffer_[buffer_pos_++];

    // Read remaining directly from transport
    while (total_read < n)
    {
        size_t bytes_read = transport_.read(buffer + total_read, n - total_read);
        if (bytes_read == 0)
            throw ConnectionClosedError("Connection closed while reading message body");
        total_read += bytes_read;
    }
}

inline std::string MessageFramer::read_line()
{
    std::string line;

    while (true)
    {
        // Refill buffer if empty
        if (buffer_pos_ >= buffer_len_)
        {
            fill_buffer(1);
            if (buffer_len_ == 0)
                throw ConnectionClosedError("Connection closed while reading header");
        }

        char c = buffer_[buffer_pos_++];

        if (c == '\n')
        {
            // Remove trailing \r if present
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            return line;
        }

        line += c;
    }
}

inline void MessageFramer::fill_buffer(size_t min_bytes)
{
    // Compact buffer if needed
    if (buffer_pos_ > 0)
    {
        if (buffer_pos_ < buffer_len_)
        {
            std::copy(
                buffer_.begin() + buffer_pos_, buffer_.begin() + buffer_len_, buffer_.begin()
            );
            buffer_len_ -= buffer_pos_;
        }
        else
        {
            buffer_len_ = 0;
        }
        buffer_pos_ = 0;
    }

    // Ensure buffer is large enough
    constexpr size_t kMinBufferSize = 4096;
    if (buffer_.size() < kMinBufferSize)
        buffer_.resize(kMinBufferSize);

    // Read more data
    while (buffer_len_ < min_bytes)
    {
        size_t bytes_read =
            transport_.read(buffer_.data() + buffer_len_, buffer_.size() - buffer_len_);

        if (bytes_read == 0)
        {
            // EOF - return what we have
            return;
        }

        buffer_len_ += bytes_read;
    }
}

} // namespace copilot
