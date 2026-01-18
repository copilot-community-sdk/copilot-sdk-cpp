// Copyright (c) 2025 Elias Bachaalany
// SPDX-License-Identifier: MIT

#include <copilot/transport.hpp>
#include <copilot/transport_tcp.hpp>
#include <cstring>
#include <gtest/gtest.h>
#include <queue>
#include <sstream>

using namespace copilot;

// =============================================================================
// Mock Transport for Testing
// =============================================================================

/// A simple in-memory transport for testing MessageFramer
class MockTransport : public ITransport
{
  public:
    /// Queue data to be read
    void queue_read_data(const std::string& data)
    {
        for (char c : data)
            read_buffer_.push(c);
    }

    /// Get data that was written
    std::string get_written_data() const
    {
        return write_buffer_.str();
    }

    void clear_written()
    {
        write_buffer_.str("");
        write_buffer_.clear();
    }

    size_t read(char* buffer, size_t size) override
    {
        if (!open_)
            throw ConnectionClosedError();

        size_t bytes_read = 0;
        while (bytes_read < size && !read_buffer_.empty())
        {
            buffer[bytes_read++] = read_buffer_.front();
            read_buffer_.pop();
        }
        return bytes_read;
    }

    void write(const char* data, size_t size) override
    {
        if (!open_)
            throw ConnectionClosedError();
        write_buffer_.write(data, size);
    }

    void close() override
    {
        open_ = false;
    }

    bool is_open() const override
    {
        return open_;
    }

    void simulate_close()
    {
        open_ = false;
    }

  private:
    std::queue<char> read_buffer_;
    std::ostringstream write_buffer_;
    bool open_ = true;
};

// =============================================================================
// MessageFramer Tests
// =============================================================================

TEST(MessageFramerTest, ReadSimpleMessage)
{
    MockTransport transport;
    MessageFramer framer(transport);

    // Queue a properly framed message
    std::string message = R"({"jsonrpc":"2.0","method":"test"})";
    std::string frame = "Content-Length: " + std::to_string(message.size()) + "\r\n\r\n" + message;
    transport.queue_read_data(frame);

    auto result = framer.read_message();
    EXPECT_EQ(result, message);
}

TEST(MessageFramerTest, ReadMessageWithMultipleHeaders)
{
    MockTransport transport;
    MessageFramer framer(transport);

    std::string message = R"({"id":1})";
    std::string frame = "Content-Type: application/json\r\n"
                        "Content-Length: " +
                        std::to_string(message.size()) +
                        "\r\n"
                        "\r\n" +
                        message;
    transport.queue_read_data(frame);

    auto result = framer.read_message();
    EXPECT_EQ(result, message);
}

TEST(MessageFramerTest, ReadMessageCaseInsensitiveHeader)
{
    MockTransport transport;
    MessageFramer framer(transport);

    std::string message = "{}";
    // Use different case for Content-Length
    std::string frame = "content-length: 2\r\n\r\n{}";
    transport.queue_read_data(frame);

    auto result = framer.read_message();
    EXPECT_EQ(result, message);
}

TEST(MessageFramerTest, ReadMultipleMessages)
{
    MockTransport transport;
    MessageFramer framer(transport);

    std::string msg1 = R"({"id":1})";
    std::string msg2 = R"({"id":2})";
    std::string frame1 = "Content-Length: " + std::to_string(msg1.size()) + "\r\n\r\n" + msg1;
    std::string frame2 = "Content-Length: " + std::to_string(msg2.size()) + "\r\n\r\n" + msg2;

    transport.queue_read_data(frame1 + frame2);

    EXPECT_EQ(framer.read_message(), msg1);
    EXPECT_EQ(framer.read_message(), msg2);
}

TEST(MessageFramerTest, WriteMessage)
{
    MockTransport transport;
    MessageFramer framer(transport);

    std::string message = R"({"jsonrpc":"2.0","id":1})";
    framer.write_message(message);

    std::string expected =
        "Content-Length: " + std::to_string(message.size()) + "\r\n\r\n" + message;
    EXPECT_EQ(transport.get_written_data(), expected);
}

TEST(MessageFramerTest, RoundTrip)
{
    MockTransport transport;
    MessageFramer framer(transport);

    std::string original = R"({"method":"ping","params":{"message":"hello"}})";
    framer.write_message(original);

    // Queue the written data back for reading
    std::string written = transport.get_written_data();
    transport.clear_written();
    transport.queue_read_data(written);

    auto result = framer.read_message();
    EXPECT_EQ(result, original);
}

TEST(MessageFramerTest, LargeMessage)
{
    MockTransport transport;
    MessageFramer framer(transport);

    // Create a large message (100KB)
    std::string large_content(100 * 1024, 'x');
    std::string message = R"({"content":")" + large_content + R"("})";

    framer.write_message(message);

    std::string written = transport.get_written_data();
    transport.clear_written();
    transport.queue_read_data(written);

    auto result = framer.read_message();
    EXPECT_EQ(result, message);
}

TEST(MessageFramerTest, MissingContentLength)
{
    MockTransport transport;
    MessageFramer framer(transport);

    // Frame without Content-Length header
    transport.queue_read_data("Content-Type: application/json\r\n\r\n{}");

    EXPECT_THROW(framer.read_message(), TransportError);
}

TEST(MessageFramerTest, ConnectionClosedDuringHeader)
{
    MockTransport transport;
    MessageFramer framer(transport);

    // Partial header then close
    transport.queue_read_data("Content-Length: 10");
    transport.simulate_close();

    EXPECT_THROW(framer.read_message(), ConnectionClosedError);
}

TEST(MessageFramerTest, ConnectionClosedDuringBody)
{
    MockTransport transport;
    MessageFramer framer(transport);

    // Complete header but incomplete body
    transport.queue_read_data("Content-Length: 100\r\n\r\n{\"partial\":");

    EXPECT_THROW(framer.read_message(), ConnectionClosedError);
}

TEST(MessageFramerTest, EmptyMessage)
{
    MockTransport transport;
    MessageFramer framer(transport);

    transport.queue_read_data("Content-Length: 0\r\n\r\n");

    auto result = framer.read_message();
    EXPECT_EQ(result, "");
}

TEST(MessageFramerTest, UnixLineEndings)
{
    MockTransport transport;
    MessageFramer framer(transport);

    // Some implementations might send just \n instead of \r\n
    std::string message = "{}";
    std::string frame = "Content-Length: 2\n\n{}";
    transport.queue_read_data(frame);

    auto result = framer.read_message();
    EXPECT_EQ(result, message);
}

// =============================================================================
// Transport Interface Tests
// =============================================================================

TEST(TransportTest, MockTransportReadWrite)
{
    MockTransport transport;

    transport.queue_read_data("Hello");

    char buffer[10];
    size_t bytes_read = transport.read(buffer, 10);
    EXPECT_EQ(bytes_read, 5);
    EXPECT_EQ(std::string(buffer, bytes_read), "Hello");

    transport.write("World", 5);
    EXPECT_EQ(transport.get_written_data(), "World");
}

TEST(TransportTest, MockTransportClose)
{
    MockTransport transport;
    EXPECT_TRUE(transport.is_open());

    transport.close();
    EXPECT_FALSE(transport.is_open());

    char buffer[10];
    EXPECT_THROW(transport.read(buffer, 10), ConnectionClosedError);
    EXPECT_THROW(transport.write("test", 4), ConnectionClosedError);
}

TEST(TransportTest, TransportErrorMessage)
{
    TransportError error("Test error");
    EXPECT_STREQ(error.what(), "Test error");
}

TEST(TransportTest, ConnectionClosedErrorMessage)
{
    ConnectionClosedError error1;
    EXPECT_STREQ(error1.what(), "Connection closed");

    ConnectionClosedError error2("Custom close message");
    EXPECT_STREQ(error2.what(), "Custom close message");
}

// =============================================================================
// TCP Transport Tests (Unit tests that don't require network)
// =============================================================================

TEST(TcpTransportTest, DefaultConstruction)
{
    TcpTransport transport;
    EXPECT_FALSE(transport.is_open());
    EXPECT_EQ(transport.socket(), TcpTransport::kInvalidSocket);
}

TEST(TcpTransportTest, MoveConstruction)
{
    TcpTransport transport1;
    TcpTransport transport2(std::move(transport1));
    EXPECT_FALSE(transport1.is_open());
    EXPECT_FALSE(transport2.is_open());
}

TEST(TcpTransportTest, CloseUnconnected)
{
    TcpTransport transport;
    // Should not throw
    transport.close();
    EXPECT_FALSE(transport.is_open());
}

TEST(TcpTransportTest, ReadWriteWhenClosed)
{
    TcpTransport transport;
    char buffer[10];

    EXPECT_THROW(transport.read(buffer, 10), ConnectionClosedError);
    EXPECT_THROW(transport.write("test", 4), ConnectionClosedError);
}

TEST(TcpTransportTest, ConnectInvalidHost)
{
    TcpTransport transport;

    // Attempt to connect to an invalid host (should fail quickly)
    EXPECT_THROW(
        transport.connect("invalid.host.that.does.not.exist.example", 12345, 1000), TransportError
    );
}

// Note: Integration tests with actual TCP connections would require
// a test server or mocking at the system level. Those are better
// suited for E2E tests with the actual Copilot CLI.
