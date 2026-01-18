// Copyright (c) 2025 Elias Bachaalany
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <copilot/transport.hpp>
#include <copilot/types.hpp>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace copilot
{

// =============================================================================
// JSON-RPC 2.0 Exceptions
// =============================================================================

/// JSON-RPC error codes (standard and custom)
enum class JsonRpcErrorCode : int
{
    // Standard JSON-RPC 2.0 errors
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,

    // Server errors (-32000 to -32099)
    ServerError = -32000,

    // Custom errors
    RequestCancelled = -32800,
    ConnectionClosed = -32801,
    Timeout = -32802,
};

/// Exception for JSON-RPC errors
class JsonRpcError : public std::runtime_error
{
  public:
    JsonRpcError(
        JsonRpcErrorCode code, const std::string& message, const json& data = nullptr
    )
        : std::runtime_error(message), code_(code), data_(data)
    {
    }

    JsonRpcErrorCode code() const
    {
        return code_;
    }
    const json& data() const
    {
        return data_;
    }

  private:
    JsonRpcErrorCode code_;
    json data_;
};

// =============================================================================
// JSON-RPC 2.0 Message Types
// =============================================================================

/// JSON-RPC request ID (can be string or integer)
using JsonRpcId = std::variant<std::string, int64_t>;

/// Convert JsonRpcId to JSON
inline json id_to_json(const JsonRpcId& id)
{
    return std::visit([](const auto& v) -> json { return v; }, id);
}

/// Parse JsonRpcId from JSON
inline JsonRpcId id_from_json(const json& j)
{
    if (j.is_string())
        return j.get<std::string>();
    else if (j.is_number_integer())
        return j.get<int64_t>();
    throw std::runtime_error("Invalid JSON-RPC id type");
}

/// JSON-RPC 2.0 Request
struct JsonRpcRequest
{
    std::string method;
    json params;
    std::optional<JsonRpcId> id; // nullopt for notifications

    json to_json() const
    {
        json j = {{"jsonrpc", "2.0"}, {"method", method}};
        if (!params.is_null())
            j["params"] = params;
        if (id)
            j["id"] = id_to_json(*id);
        return j;
    }

    static JsonRpcRequest from_json(const json& j)
    {
        JsonRpcRequest req;
        req.method = j.at("method").get<std::string>();
        if (j.contains("params"))
            req.params = j.at("params");
        if (j.contains("id") && !j.at("id").is_null())
            req.id = id_from_json(j.at("id"));
        return req;
    }

    bool is_notification() const
    {
        return !id.has_value();
    }
};

/// JSON-RPC 2.0 Error object
struct JsonRpcErrorObject
{
    int code;
    std::string message;
    json data;

    json to_json() const
    {
        json j = {{"code", code}, {"message", message}};
        if (!data.is_null())
            j["data"] = data;
        return j;
    }

    static JsonRpcErrorObject from_json(const json& j)
    {
        JsonRpcErrorObject err;
        err.code = j.at("code").get<int>();
        err.message = j.at("message").get<std::string>();
        if (j.contains("data"))
            err.data = j.at("data");
        return err;
    }
};

/// JSON-RPC 2.0 Response
struct JsonRpcResponse
{
    JsonRpcId id;
    std::optional<json> result;
    std::optional<JsonRpcErrorObject> error;

    json to_json() const
    {
        json j = {{"jsonrpc", "2.0"}, {"id", id_to_json(id)}};
        if (result)
            j["result"] = *result;
        if (error)
            j["error"] = error->to_json();
        return j;
    }

    static JsonRpcResponse from_json(const json& j)
    {
        JsonRpcResponse resp;
        if (j.contains("id") && !j.at("id").is_null())
            resp.id = id_from_json(j.at("id"));
        if (j.contains("result"))
            resp.result = j.at("result");
        if (j.contains("error"))
            resp.error = JsonRpcErrorObject::from_json(j.at("error"));
        return resp;
    }

    bool is_error() const
    {
        return error.has_value();
    }
};

// =============================================================================
// Pending Request Tracking
// =============================================================================

/// Holds state for a pending request awaiting response
struct PendingRequest
{
    std::promise<json> promise;
    std::chrono::steady_clock::time_point deadline;

    PendingRequest(std::chrono::milliseconds timeout = std::chrono::milliseconds{0})
        : deadline(
              timeout.count() > 0 ? std::chrono::steady_clock::now() + timeout
                                  : std::chrono::steady_clock::time_point::max()
          )
    {
    }
};

// =============================================================================
// JSON-RPC Client
// =============================================================================

/// Handler for incoming notifications
using NotificationHandler =
    std::function<void(const std::string& method, const json& params)>;

/// Handler for incoming requests (returns response result or throws)
using RequestHandler =
    std::function<json(const std::string& method, const json& params)>;

/// JSON-RPC 2.0 client with bidirectional communication
///
/// Features:
/// - Send requests and await responses (with timeout)
/// - Send notifications (fire-and-forget)
/// - Handle incoming notifications via callback
/// - Handle incoming requests (server-to-client calls) via callback
/// - Background read loop with automatic dispatch
class JsonRpcClient
{
  public:
    /// Construct client with transport and message framer
    /// @param transport The underlying transport (takes ownership)
    explicit JsonRpcClient(std::unique_ptr<ITransport> transport)
        : transport_(std::move(transport)), framer_(*transport_), next_id_(1), running_(false)
    {
    }

    ~JsonRpcClient()
    {
        stop();
    }

    // Non-copyable, non-movable (due to mutex/thread)
    JsonRpcClient(const JsonRpcClient&) = delete;
    JsonRpcClient& operator=(const JsonRpcClient&) = delete;
    JsonRpcClient(JsonRpcClient&&) = delete;
    JsonRpcClient& operator=(JsonRpcClient&&) = delete;

    /// Start the background read loop
    void start()
    {
        if (running_.exchange(true))
            return; // Already running

        read_thread_ = std::thread([this] { read_loop(); });
        timeout_thread_ = std::thread([this] { timeout_loop(); });
    }

    /// Stop the client and close connection
    void stop()
    {
        bool was_running = running_.exchange(false);

        pending_cv_.notify_all();

        // Close transport to unblock read
        if (was_running && transport_)
            transport_->close();

        // Wait for read thread
        if (read_thread_.joinable())
            read_thread_.join();

        if (timeout_thread_.joinable())
            timeout_thread_.join();

        // Fail all pending requests
        fail_all_pending(JsonRpcErrorCode::ConnectionClosed, "Connection closed");
    }

    /// Check if client is running
    bool is_running() const
    {
        return running_;
    }

    /// Set handler for incoming notifications
    void set_notification_handler(NotificationHandler handler)
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        notification_handler_ = std::move(handler);
    }

    /// Set handler for incoming requests
    void set_request_handler(RequestHandler handler)
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        request_handler_ = std::move(handler);
    }

    /// Send a request and await response
    /// @param method The method name
    /// @param params The parameters (can be object or array)
    /// @param timeout Request timeout (0 = no timeout)
    /// @return Future that resolves to the result
    std::future<json> invoke(
        const std::string& method,
        const json& params = nullptr,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{30000}
    )
    {
        auto id = next_id_++;

        auto pending = std::make_shared<PendingRequest>(timeout);
        auto future = pending->promise.get_future();

        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_requests_[id] = pending;
        }
        pending_cv_.notify_all();

        // Build and send request
        JsonRpcRequest request{method, params, JsonRpcId{id}};

        try
        {
            send_message(request.to_json());
        }
        catch (...)
        {
            // Remove pending request on send failure
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_requests_.erase(id);
            throw;
        }

        return future;
    }

    /// Send a request and wait for response synchronously
    template <typename T = json>
    T invoke_sync(
        const std::string& method,
        const json& params = nullptr,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{30000}
    )
    {
        auto [id, future] = invoke_with_id(method, params, timeout);

        if (timeout.count() > 0)
        {
            auto status = future.wait_for(timeout);
            if (status == std::future_status::timeout)
            {
                fail_pending_request(id, JsonRpcErrorCode::Timeout, "Request timed out");
                throw JsonRpcError(JsonRpcErrorCode::Timeout, "Request timed out");
            }
        }

        auto result = future.get();

        if constexpr (std::is_same_v<T, json>)
            return result;
        else
            return result.get<T>();
    }

    /// Send a notification (no response expected)
    void notify(const std::string& method, const json& params = nullptr)
    {
        JsonRpcRequest request{method, params, std::nullopt};
        send_message(request.to_json());
    }

    /// Send a response to an incoming request
    void send_response(const JsonRpcId& id, const json& result)
    {
        JsonRpcResponse response{id, result, std::nullopt};
        send_message(response.to_json());
    }

    /// Send an error response to an incoming request
    void send_error_response(
        const JsonRpcId& id,
        int code,
        const std::string& message,
        const json& data = nullptr
    )
    {
        JsonRpcResponse response{id, std::nullopt, JsonRpcErrorObject{code, message, data}};
        send_message(response.to_json());
    }

  private:
    /// Internal invoke that returns both request ID and future for cleanup purposes
    std::pair<int64_t, std::future<json>> invoke_with_id(
        const std::string& method, const json& params, std::chrono::milliseconds timeout
    )
    {
        auto id = next_id_++;

        auto pending = std::make_shared<PendingRequest>(timeout);
        auto future = pending->promise.get_future();

        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_requests_[id] = pending;
        }
        pending_cv_.notify_all();

        JsonRpcRequest request{method, params, JsonRpcId{id}};

        try
        {
            send_message(request.to_json());
        }
        catch (...)
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_requests_.erase(id);
            throw;
        }

        return {id, std::move(future)};
    }

    void timeout_loop()
    {
        using clock = std::chrono::steady_clock;

        while (running_)
        {
            std::vector<std::shared_ptr<PendingRequest>> expired;
            auto now = clock::now();
            auto next_deadline = clock::time_point::max();

            {
                std::unique_lock<std::mutex> lock(pending_mutex_);
                for (auto it = pending_requests_.begin(); it != pending_requests_.end();)
                {
                    const auto& pending = it->second;
                    if (pending->deadline <= now)
                    {
                        expired.push_back(pending);
                        it = pending_requests_.erase(it);
                    }
                    else
                    {
                        next_deadline = std::min(next_deadline, pending->deadline);
                        ++it;
                    }
                }

                if (expired.empty())
                {
                    if (!running_)
                        break;
                    if (next_deadline == clock::time_point::max())
                    {
                        pending_cv_.wait_for(
                            lock,
                            std::chrono::milliseconds(250),
                            [this] { return !running_.load(); }
                        );
                    }
                    else
                    {
                        pending_cv_.wait_until(
                            lock, next_deadline, [this] { return !running_.load(); }
                        );
                    }
                    continue;
                }
            }

            for (auto& pending : expired)
            {
                try
                {
                    pending->promise.set_exception(
                        std::make_exception_ptr(
                            JsonRpcError(JsonRpcErrorCode::Timeout, "Request timed out")
                        )
                    );
                }
                catch (...)
                {
                    // Ignore if already fulfilled
                }
            }
        }
    }

    void fail_pending_request(int64_t id, JsonRpcErrorCode code, const std::string& message)
    {
        std::shared_ptr<PendingRequest> pending;
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            auto it = pending_requests_.find(id);
            if (it == pending_requests_.end())
                return;
            pending = it->second;
            pending_requests_.erase(it);
        }
        try
        {
            pending->promise.set_exception(std::make_exception_ptr(JsonRpcError(code, message)));
        }
        catch (...)
        {
            // Ignore if already fulfilled
        }
    }

    void send_message(const json& message)
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        framer_.write_message(message.dump());
    }

    void read_loop()
    {
        while (running_)
        {
            try
            {
                auto message_str = framer_.read_message();
                auto message = json::parse(message_str);
                dispatch_message(message);
            }
            catch (const ConnectionClosedError&)
            {
                running_ = false;
                fail_all_pending(JsonRpcErrorCode::ConnectionClosed, "Connection closed");
                break;
            }
            catch (const json::exception& e)
            {
                // JSON parse error - log and continue
                // In a real implementation, would use a logger
                continue;
            }
            catch (const std::exception& e)
            {
                // Other errors - continue if still running
                if (!running_)
                    break;
            }
        }
    }

    void dispatch_message(const json& message)
    {
        // Check if it's a response (has id and result/error, no method)
        if (message.contains("id") && !message.at("id").is_null() &&
            (message.contains("result") || message.contains("error")) &&
            !message.contains("method"))
        {
            handle_response(message);
            return;
        }

        // Check if it's a request or notification (has method)
        if (message.contains("method"))
        {
            auto request = JsonRpcRequest::from_json(message);
            if (request.is_notification())
                handle_notification(request);
            else
                handle_request(request);
            return;
        }

        // Unknown message format - ignore
    }

    void handle_response(const json& message)
    {
        auto response = JsonRpcResponse::from_json(message);

        // Find pending request by ID
        int64_t id = 0;
        if (auto* int_id = std::get_if<int64_t>(&response.id))
        {
            id = *int_id;
        }
        else
        {
            // String IDs not used for our outgoing requests
            return;
        }

        std::shared_ptr<PendingRequest> pending;
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            auto it = pending_requests_.find(id);
            if (it == pending_requests_.end())
                return; // Unknown request ID
            pending = it->second;
            pending_requests_.erase(it);
        }

        // Resolve or reject the promise
        if (response.is_error())
        {
            auto& err = *response.error;
            pending->promise.set_exception(
                std::make_exception_ptr(
                    JsonRpcError(static_cast<JsonRpcErrorCode>(err.code), err.message, err.data)
                )
            );
        }
        else
        {
            pending->promise.set_value(response.result.value_or(nullptr));
        }
    }

    void handle_notification(const JsonRpcRequest& request)
    {
        NotificationHandler handler;
        {
            std::lock_guard<std::mutex> lock(handlers_mutex_);
            handler = notification_handler_;
        }

        if (handler)
        {
            try
            {
                handler(request.method, request.params);
            }
            catch (...)
            {
                // Notification handlers should not throw
            }
        }
    }

    void handle_request(const JsonRpcRequest& request)
    {
        RequestHandler handler;
        {
            std::lock_guard<std::mutex> lock(handlers_mutex_);
            handler = request_handler_;
        }

        if (!handler)
        {
            // No handler - respond with method not found
            send_error_response(
                *request.id,
                static_cast<int>(JsonRpcErrorCode::MethodNotFound),
                "Method not found: " + request.method
            );
            return;
        }

        try
        {
            auto result = handler(request.method, request.params);
            send_response(*request.id, result);
        }
        catch (const JsonRpcError& e)
        {
            send_error_response(*request.id, static_cast<int>(e.code()), e.what(), e.data());
        }
        catch (const std::exception& e)
        {
            send_error_response(
                *request.id, static_cast<int>(JsonRpcErrorCode::InternalError), e.what()
            );
        }
    }

    void fail_all_pending(JsonRpcErrorCode code, const std::string& message)
    {
        std::vector<std::shared_ptr<PendingRequest>> to_fail;
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            to_fail.reserve(pending_requests_.size());
            for (auto& [id, pending] : pending_requests_)
                to_fail.push_back(pending);
            pending_requests_.clear();
        }
        for (auto& pending : to_fail)
        {
            try
            {
                pending->promise.set_exception(
                    std::make_exception_ptr(JsonRpcError(code, message))
                );
            }
            catch (...)
            {
                // Ignore if already fulfilled
            }
        }
    }

    std::unique_ptr<ITransport> transport_;
    MessageFramer framer_;
    std::atomic<int64_t> next_id_;
    std::atomic<bool> running_;

    std::thread read_thread_;
    std::thread timeout_thread_;
    std::mutex write_mutex_;

    std::mutex pending_mutex_;
    std::condition_variable pending_cv_;
    std::map<int64_t, std::shared_ptr<PendingRequest>> pending_requests_;

    std::mutex handlers_mutex_;
    NotificationHandler notification_handler_;
    RequestHandler request_handler_;
};

} // namespace copilot
