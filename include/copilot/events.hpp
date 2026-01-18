// Copyright (c) 2025 Elias Bachaalany
// SPDX-License-Identifier: MIT

#pragma once

#include <chrono>
#include <copilot/types.hpp>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace copilot
{

// =============================================================================
// Event Data Types
// =============================================================================

// Forward declare all data types
struct SessionStartData;
struct SessionResumeData;
struct SessionErrorData;
struct SessionIdleData;
struct SessionInfoData;
struct SessionModelChangeData;
struct SessionHandoffData;
struct SessionTruncationData;
struct UserMessageData;
struct PendingMessagesModifiedData;
struct AssistantTurnStartData;
struct AssistantIntentData;
struct AssistantReasoningData;
struct AssistantReasoningDeltaData;
struct AssistantMessageData;
struct AssistantMessageDeltaData;
struct AssistantTurnEndData;
struct AssistantUsageData;
struct AbortData;
struct ToolUserRequestedData;
struct ToolExecutionStartData;
struct ToolExecutionPartialResultData;
struct ToolExecutionCompleteData;
struct CustomAgentStartedData;
struct CustomAgentCompletedData;
struct CustomAgentFailedData;
struct CustomAgentSelectedData;
struct HookStartData;
struct HookEndData;
struct SystemMessageData;

// =============================================================================
// Nested Types
// =============================================================================

/// Handoff source type
enum class HandoffSourceType
{
    Remote,
    Local
};

NLOHMANN_JSON_SERIALIZE_ENUM(
    HandoffSourceType,
    {
        {HandoffSourceType::Remote, "remote"},
        {HandoffSourceType::Local, "local"},
    }
)

/// Attachment type for user messages
enum class UserAttachmentType
{
    File,
    Directory
};

NLOHMANN_JSON_SERIALIZE_ENUM(
    UserAttachmentType,
    {
        {UserAttachmentType::File, "file"},
        {UserAttachmentType::Directory, "directory"},
    }
)

/// System message role
enum class SystemMessageRole
{
    System,
    Developer
};

NLOHMANN_JSON_SERIALIZE_ENUM(
    SystemMessageRole,
    {
        {SystemMessageRole::System, "system"},
        {SystemMessageRole::Developer, "developer"},
    }
)

/// Repository info for handoff
struct RepositoryInfo
{
    std::string owner;
    std::string name;
    std::optional<std::string> branch;
};

inline void to_json(json& j, const RepositoryInfo& r)
{
    j = json{{"owner", r.owner}, {"name", r.name}};
    if (r.branch)
        j["branch"] = *r.branch;
}

inline void from_json(const json& j, RepositoryInfo& r)
{
    j.at("owner").get_to(r.owner);
    j.at("name").get_to(r.name);
    if (j.contains("branch"))
        r.branch = j.at("branch").get<std::string>();
}

/// Attachment in user message
struct UserMessageAttachmentItem
{
    UserAttachmentType type;
    std::string path;
    std::string display_name;
};

inline void to_json(json& j, const UserMessageAttachmentItem& a)
{
    j = json{{"type", a.type}, {"path", a.path}, {"displayName", a.display_name}};
}

inline void from_json(const json& j, UserMessageAttachmentItem& a)
{
    j.at("type").get_to(a.type);
    j.at("path").get_to(a.path);
    j.at("displayName").get_to(a.display_name);
}

/// Tool request in assistant message
struct ToolRequestItem
{
    std::string tool_call_id;
    std::string name;
    std::optional<json> arguments;
};

inline void to_json(json& j, const ToolRequestItem& t)
{
    j = json{{"toolCallId", t.tool_call_id}, {"name", t.name}};
    if (t.arguments)
        j["arguments"] = *t.arguments;
}

inline void from_json(const json& j, ToolRequestItem& t)
{
    j.at("toolCallId").get_to(t.tool_call_id);
    j.at("name").get_to(t.name);
    if (j.contains("arguments"))
        t.arguments = j.at("arguments");
}

/// Tool execution result content
struct ToolResultContent
{
    std::string content;
};

inline void to_json(json& j, const ToolResultContent& r)
{
    j = json{{"content", r.content}};
}

inline void from_json(const json& j, ToolResultContent& r)
{
    j.at("content").get_to(r.content);
}

/// Tool execution error
struct ToolExecutionError
{
    std::string message;
    std::optional<std::string> code;
};

inline void to_json(json& j, const ToolExecutionError& e)
{
    j = json{{"message", e.message}};
    if (e.code)
        j["code"] = *e.code;
}

inline void from_json(const json& j, ToolExecutionError& e)
{
    j.at("message").get_to(e.message);
    if (j.contains("code"))
        e.code = j.at("code").get<std::string>();
}

/// Hook error
struct HookError
{
    std::string message;
    std::optional<std::string> stack;
};

inline void to_json(json& j, const HookError& e)
{
    j = json{{"message", e.message}};
    if (e.stack)
        j["stack"] = *e.stack;
}

inline void from_json(const json& j, HookError& e)
{
    j.at("message").get_to(e.message);
    if (j.contains("stack"))
        e.stack = j.at("stack").get<std::string>();
}

/// System message metadata
struct SystemMessageMetadata
{
    std::optional<std::string> prompt_version;
    std::optional<std::map<std::string, json>> variables;
};

inline void to_json(json& j, const SystemMessageMetadata& m)
{
    j = json::object();
    if (m.prompt_version)
        j["promptVersion"] = *m.prompt_version;
    if (m.variables)
        j["variables"] = *m.variables;
}

inline void from_json(const json& j, SystemMessageMetadata& m)
{
    if (j.contains("promptVersion"))
        m.prompt_version = j.at("promptVersion").get<std::string>();
    if (j.contains("variables"))
        m.variables = j.at("variables").get<std::map<std::string, json>>();
}

// =============================================================================
// Event Data Definitions
// =============================================================================

struct SessionStartData
{
    std::string session_id;
    double version;
    std::string producer;
    std::string copilot_version;
    std::string start_time; // ISO 8601
    std::optional<std::string> selected_model;
};

inline void from_json(const json& j, SessionStartData& d)
{
    j.at("sessionId").get_to(d.session_id);
    j.at("version").get_to(d.version);
    j.at("producer").get_to(d.producer);
    j.at("copilotVersion").get_to(d.copilot_version);
    j.at("startTime").get_to(d.start_time);
    if (j.contains("selectedModel"))
        d.selected_model = j.at("selectedModel").get<std::string>();
}

struct SessionResumeData
{
    std::string resume_time; // ISO 8601
    double event_count;
};

inline void from_json(const json& j, SessionResumeData& d)
{
    j.at("resumeTime").get_to(d.resume_time);
    j.at("eventCount").get_to(d.event_count);
}

struct SessionErrorData
{
    std::string error_type;
    std::string message;
    std::optional<std::string> stack;
};

inline void from_json(const json& j, SessionErrorData& d)
{
    j.at("errorType").get_to(d.error_type);
    j.at("message").get_to(d.message);
    if (j.contains("stack"))
        d.stack = j.at("stack").get<std::string>();
}

struct SessionIdleData
{
};

inline void from_json(const json&, SessionIdleData&) {}

struct SessionInfoData
{
    std::string info_type;
    std::string message;
};

inline void from_json(const json& j, SessionInfoData& d)
{
    j.at("infoType").get_to(d.info_type);
    j.at("message").get_to(d.message);
}

struct SessionModelChangeData
{
    std::optional<std::string> previous_model;
    std::string new_model;
};

inline void from_json(const json& j, SessionModelChangeData& d)
{
    if (j.contains("previousModel"))
        d.previous_model = j.at("previousModel").get<std::string>();
    j.at("newModel").get_to(d.new_model);
}

struct SessionHandoffData
{
    std::string handoff_time; // ISO 8601
    HandoffSourceType source_type;
    std::optional<RepositoryInfo> repository;
    std::optional<std::string> context;
    std::optional<std::string> summary;
    std::optional<std::string> remote_session_id;
};

inline void from_json(const json& j, SessionHandoffData& d)
{
    j.at("handoffTime").get_to(d.handoff_time);
    j.at("sourceType").get_to(d.source_type);
    if (j.contains("repository"))
        d.repository = j.at("repository").get<RepositoryInfo>();
    if (j.contains("context"))
        d.context = j.at("context").get<std::string>();
    if (j.contains("summary"))
        d.summary = j.at("summary").get<std::string>();
    if (j.contains("remoteSessionId"))
        d.remote_session_id = j.at("remoteSessionId").get<std::string>();
}

struct SessionTruncationData
{
    double token_limit;
    double pre_truncation_tokens_in_messages;
    double pre_truncation_messages_length;
    double post_truncation_tokens_in_messages;
    double post_truncation_messages_length;
    double tokens_removed_during_truncation;
    double messages_removed_during_truncation;
    std::string performed_by;
};

inline void from_json(const json& j, SessionTruncationData& d)
{
    j.at("tokenLimit").get_to(d.token_limit);
    j.at("preTruncationTokensInMessages").get_to(d.pre_truncation_tokens_in_messages);
    j.at("preTruncationMessagesLength").get_to(d.pre_truncation_messages_length);
    j.at("postTruncationTokensInMessages").get_to(d.post_truncation_tokens_in_messages);
    j.at("postTruncationMessagesLength").get_to(d.post_truncation_messages_length);
    j.at("tokensRemovedDuringTruncation").get_to(d.tokens_removed_during_truncation);
    j.at("messagesRemovedDuringTruncation").get_to(d.messages_removed_during_truncation);
    j.at("performedBy").get_to(d.performed_by);
}

struct UserMessageData
{
    std::string content;
    std::optional<std::string> transformed_content;
    std::optional<std::vector<UserMessageAttachmentItem>> attachments;
    std::optional<std::string> source;
};

inline void from_json(const json& j, UserMessageData& d)
{
    j.at("content").get_to(d.content);
    if (j.contains("transformedContent"))
        d.transformed_content = j.at("transformedContent").get<std::string>();
    if (j.contains("attachments"))
        d.attachments = j.at("attachments").get<std::vector<UserMessageAttachmentItem>>();
    if (j.contains("source"))
        d.source = j.at("source").get<std::string>();
}

struct PendingMessagesModifiedData
{
};

inline void from_json(const json&, PendingMessagesModifiedData&) {}

struct AssistantTurnStartData
{
    std::string turn_id;
};

inline void from_json(const json& j, AssistantTurnStartData& d)
{
    j.at("turnId").get_to(d.turn_id);
}

struct AssistantIntentData
{
    std::string intent;
};

inline void from_json(const json& j, AssistantIntentData& d)
{
    j.at("intent").get_to(d.intent);
}

struct AssistantReasoningData
{
    std::string reasoning_id;
    std::string content;
    std::optional<std::string> chunk_content;
};

inline void from_json(const json& j, AssistantReasoningData& d)
{
    j.at("reasoningId").get_to(d.reasoning_id);
    j.at("content").get_to(d.content);
    if (j.contains("chunkContent"))
        d.chunk_content = j.at("chunkContent").get<std::string>();
}

struct AssistantReasoningDeltaData
{
    std::string reasoning_id;
    std::string delta_content;
};

inline void from_json(const json& j, AssistantReasoningDeltaData& d)
{
    j.at("reasoningId").get_to(d.reasoning_id);
    j.at("deltaContent").get_to(d.delta_content);
}

struct AssistantMessageData
{
    std::string message_id;
    std::string content;
    std::optional<std::string> chunk_content;
    std::optional<double> total_response_size_bytes;
    std::optional<std::vector<ToolRequestItem>> tool_requests;
    std::optional<std::string> parent_tool_call_id;
};

inline void from_json(const json& j, AssistantMessageData& d)
{
    j.at("messageId").get_to(d.message_id);
    j.at("content").get_to(d.content);
    if (j.contains("chunkContent"))
        d.chunk_content = j.at("chunkContent").get<std::string>();
    if (j.contains("totalResponseSizeBytes"))
        d.total_response_size_bytes = j.at("totalResponseSizeBytes").get<double>();
    if (j.contains("toolRequests"))
        d.tool_requests = j.at("toolRequests").get<std::vector<ToolRequestItem>>();
    if (j.contains("parentToolCallId"))
        d.parent_tool_call_id = j.at("parentToolCallId").get<std::string>();
}

struct AssistantMessageDeltaData
{
    std::string message_id;
    std::string delta_content;
    std::optional<double> total_response_size_bytes;
    std::optional<std::string> parent_tool_call_id;
};

inline void from_json(const json& j, AssistantMessageDeltaData& d)
{
    j.at("messageId").get_to(d.message_id);
    j.at("deltaContent").get_to(d.delta_content);
    if (j.contains("totalResponseSizeBytes"))
        d.total_response_size_bytes = j.at("totalResponseSizeBytes").get<double>();
    if (j.contains("parentToolCallId"))
        d.parent_tool_call_id = j.at("parentToolCallId").get<std::string>();
}

struct AssistantTurnEndData
{
    std::string turn_id;
};

inline void from_json(const json& j, AssistantTurnEndData& d)
{
    j.at("turnId").get_to(d.turn_id);
}

struct AssistantUsageData
{
    std::optional<std::string> model;
    std::optional<double> input_tokens;
    std::optional<double> output_tokens;
    std::optional<double> cache_read_tokens;
    std::optional<double> cache_write_tokens;
    std::optional<double> cost;
    std::optional<double> duration;
    std::optional<std::string> initiator;
    std::optional<std::string> api_call_id;
    std::optional<std::string> provider_call_id;
    std::optional<std::map<std::string, json>> quota_snapshots;
};

inline void from_json(const json& j, AssistantUsageData& d)
{
    if (j.contains("model"))
        d.model = j.at("model").get<std::string>();
    if (j.contains("inputTokens"))
        d.input_tokens = j.at("inputTokens").get<double>();
    if (j.contains("outputTokens"))
        d.output_tokens = j.at("outputTokens").get<double>();
    if (j.contains("cacheReadTokens"))
        d.cache_read_tokens = j.at("cacheReadTokens").get<double>();
    if (j.contains("cacheWriteTokens"))
        d.cache_write_tokens = j.at("cacheWriteTokens").get<double>();
    if (j.contains("cost"))
        d.cost = j.at("cost").get<double>();
    if (j.contains("duration"))
        d.duration = j.at("duration").get<double>();
    if (j.contains("initiator"))
        d.initiator = j.at("initiator").get<std::string>();
    if (j.contains("apiCallId"))
        d.api_call_id = j.at("apiCallId").get<std::string>();
    if (j.contains("providerCallId"))
        d.provider_call_id = j.at("providerCallId").get<std::string>();
    if (j.contains("quotaSnapshots"))
        d.quota_snapshots = j.at("quotaSnapshots").get<std::map<std::string, json>>();
}

struct AbortData
{
    std::string reason;
};

inline void from_json(const json& j, AbortData& d)
{
    j.at("reason").get_to(d.reason);
}

struct ToolUserRequestedData
{
    std::string tool_call_id;
    std::string tool_name;
    std::optional<json> arguments;
};

inline void from_json(const json& j, ToolUserRequestedData& d)
{
    j.at("toolCallId").get_to(d.tool_call_id);
    j.at("toolName").get_to(d.tool_name);
    if (j.contains("arguments"))
        d.arguments = j.at("arguments");
}

struct ToolExecutionStartData
{
    std::string tool_call_id;
    std::string tool_name;
    std::optional<json> arguments;
    std::optional<std::string> parent_tool_call_id;
};

inline void from_json(const json& j, ToolExecutionStartData& d)
{
    j.at("toolCallId").get_to(d.tool_call_id);
    j.at("toolName").get_to(d.tool_name);
    if (j.contains("arguments"))
        d.arguments = j.at("arguments");
    if (j.contains("parentToolCallId"))
        d.parent_tool_call_id = j.at("parentToolCallId").get<std::string>();
}

struct ToolExecutionPartialResultData
{
    std::string tool_call_id;
    std::string partial_output;
};

inline void from_json(const json& j, ToolExecutionPartialResultData& d)
{
    j.at("toolCallId").get_to(d.tool_call_id);
    j.at("partialOutput").get_to(d.partial_output);
}

struct ToolExecutionCompleteData
{
    std::string tool_call_id;
    bool success;
    std::optional<bool> is_user_requested;
    std::optional<ToolResultContent> result;
    std::optional<ToolExecutionError> error;
    std::optional<std::map<std::string, json>> tool_telemetry;
    std::optional<std::string> parent_tool_call_id;
};

inline void from_json(const json& j, ToolExecutionCompleteData& d)
{
    j.at("toolCallId").get_to(d.tool_call_id);
    j.at("success").get_to(d.success);
    if (j.contains("isUserRequested"))
        d.is_user_requested = j.at("isUserRequested").get<bool>();
    if (j.contains("result"))
        d.result = j.at("result").get<ToolResultContent>();
    if (j.contains("error"))
        d.error = j.at("error").get<ToolExecutionError>();
    if (j.contains("toolTelemetry"))
        d.tool_telemetry = j.at("toolTelemetry").get<std::map<std::string, json>>();
    if (j.contains("parentToolCallId"))
        d.parent_tool_call_id = j.at("parentToolCallId").get<std::string>();
}

struct CustomAgentStartedData
{
    std::string tool_call_id;
    std::string agent_name;
    std::string agent_display_name;
    std::string agent_description;
};

inline void from_json(const json& j, CustomAgentStartedData& d)
{
    j.at("toolCallId").get_to(d.tool_call_id);
    j.at("agentName").get_to(d.agent_name);
    j.at("agentDisplayName").get_to(d.agent_display_name);
    j.at("agentDescription").get_to(d.agent_description);
}

struct CustomAgentCompletedData
{
    std::string tool_call_id;
    std::string agent_name;
};

inline void from_json(const json& j, CustomAgentCompletedData& d)
{
    j.at("toolCallId").get_to(d.tool_call_id);
    j.at("agentName").get_to(d.agent_name);
}

struct CustomAgentFailedData
{
    std::string tool_call_id;
    std::string agent_name;
    std::string error;
};

inline void from_json(const json& j, CustomAgentFailedData& d)
{
    j.at("toolCallId").get_to(d.tool_call_id);
    j.at("agentName").get_to(d.agent_name);
    j.at("error").get_to(d.error);
}

struct CustomAgentSelectedData
{
    std::string agent_name;
    std::string agent_display_name;
    std::vector<std::string> tools;
};

inline void from_json(const json& j, CustomAgentSelectedData& d)
{
    j.at("agentName").get_to(d.agent_name);
    j.at("agentDisplayName").get_to(d.agent_display_name);
    j.at("tools").get_to(d.tools);
}

struct HookStartData
{
    std::string hook_invocation_id;
    std::string hook_type;
    std::optional<json> input;
};

inline void from_json(const json& j, HookStartData& d)
{
    j.at("hookInvocationId").get_to(d.hook_invocation_id);
    j.at("hookType").get_to(d.hook_type);
    if (j.contains("input"))
        d.input = j.at("input");
}

struct HookEndData
{
    std::string hook_invocation_id;
    std::string hook_type;
    std::optional<json> output;
    bool success;
    std::optional<HookError> error;
};

inline void from_json(const json& j, HookEndData& d)
{
    j.at("hookInvocationId").get_to(d.hook_invocation_id);
    j.at("hookType").get_to(d.hook_type);
    if (j.contains("output"))
        d.output = j.at("output");
    j.at("success").get_to(d.success);
    if (j.contains("error"))
        d.error = j.at("error").get<HookError>();
}

struct SystemMessageData
{
    std::string content;
    SystemMessageRole role;
    std::optional<std::string> name;
    std::optional<SystemMessageMetadata> metadata;
};

inline void from_json(const json& j, SystemMessageData& d)
{
    j.at("content").get_to(d.content);
    j.at("role").get_to(d.role);
    if (j.contains("name"))
        d.name = j.at("name").get<std::string>();
    if (j.contains("metadata"))
        d.metadata = j.at("metadata").get<SystemMessageMetadata>();
}

// =============================================================================
// Session Event Type (Discriminated Union)
// =============================================================================

/// All possible event types
enum class SessionEventType
{
    SessionStart,
    SessionResume,
    SessionError,
    SessionIdle,
    SessionInfo,
    SessionModelChange,
    SessionHandoff,
    SessionTruncation,
    UserMessage,
    PendingMessagesModified,
    AssistantTurnStart,
    AssistantIntent,
    AssistantReasoning,
    AssistantReasoningDelta,
    AssistantMessage,
    AssistantMessageDelta,
    AssistantTurnEnd,
    AssistantUsage,
    Abort,
    ToolUserRequested,
    ToolExecutionStart,
    ToolExecutionPartialResult,
    ToolExecutionComplete,
    CustomAgentStarted,
    CustomAgentCompleted,
    CustomAgentFailed,
    CustomAgentSelected,
    HookStart,
    HookEnd,
    SystemMessage,
    Unknown
};

/// Variant holding all possible event data types
using SessionEventData = std::variant<
    SessionStartData,
    SessionResumeData,
    SessionErrorData,
    SessionIdleData,
    SessionInfoData,
    SessionModelChangeData,
    SessionHandoffData,
    SessionTruncationData,
    UserMessageData,
    PendingMessagesModifiedData,
    AssistantTurnStartData,
    AssistantIntentData,
    AssistantReasoningData,
    AssistantReasoningDeltaData,
    AssistantMessageData,
    AssistantMessageDeltaData,
    AssistantTurnEndData,
    AssistantUsageData,
    AbortData,
    ToolUserRequestedData,
    ToolExecutionStartData,
    ToolExecutionPartialResultData,
    ToolExecutionCompleteData,
    CustomAgentStartedData,
    CustomAgentCompletedData,
    CustomAgentFailedData,
    CustomAgentSelectedData,
    HookStartData,
    HookEndData,
    SystemMessageData,
    json // Unknown event fallback
    >;

/// Base session event with common fields and typed data
struct SessionEvent
{
    std::string id;
    std::string timestamp; // ISO 8601
    std::optional<std::string> parent_id;
    std::optional<bool> ephemeral;
    SessionEventType type;
    std::string type_string; // Original type string for unknown events
    SessionEventData data;

    /// Check if this is a specific event type
    template <typename T>
    bool is() const
    {
        return std::holds_alternative<T>(data);
    }

    /// Get event data as specific type (throws if wrong type)
    template <typename T>
    const T& as() const
    {
        return std::get<T>(data);
    }

    /// Get event data as specific type (returns nullptr if wrong type)
    template <typename T>
    const T* try_as() const
    {
        return std::get_if<T>(&data);
    }
};

/// Parse session event from JSON
inline SessionEvent parse_session_event(const json& j)
{
    SessionEvent event;

    // Parse common fields
    event.id = j.at("id").get<std::string>();
    event.timestamp = j.at("timestamp").get<std::string>();
    if (j.contains("parentId") && !j.at("parentId").is_null())
        event.parent_id = j.at("parentId").get<std::string>();
    if (j.contains("ephemeral"))
        event.ephemeral = j.at("ephemeral").get<bool>();

    // Parse type and data
    event.type_string = j.at("type").get<std::string>();
    const auto& data_json = j.at("data");

    // Map type string to enum and parse data
    static const std::map<std::string, SessionEventType> type_map = {
        {"session.start", SessionEventType::SessionStart},
        {"session.resume", SessionEventType::SessionResume},
        {"session.error", SessionEventType::SessionError},
        {"session.idle", SessionEventType::SessionIdle},
        {"session.info", SessionEventType::SessionInfo},
        {"session.model_change", SessionEventType::SessionModelChange},
        {"session.handoff", SessionEventType::SessionHandoff},
        {"session.truncation", SessionEventType::SessionTruncation},
        {"user.message", SessionEventType::UserMessage},
        {"pending_messages.modified", SessionEventType::PendingMessagesModified},
        {"assistant.turn_start", SessionEventType::AssistantTurnStart},
        {"assistant.intent", SessionEventType::AssistantIntent},
        {"assistant.reasoning", SessionEventType::AssistantReasoning},
        {"assistant.reasoning_delta", SessionEventType::AssistantReasoningDelta},
        {"assistant.message", SessionEventType::AssistantMessage},
        {"assistant.message_delta", SessionEventType::AssistantMessageDelta},
        {"assistant.turn_end", SessionEventType::AssistantTurnEnd},
        {"assistant.usage", SessionEventType::AssistantUsage},
        {"abort", SessionEventType::Abort},
        {"tool.user_requested", SessionEventType::ToolUserRequested},
        {"tool.execution_start", SessionEventType::ToolExecutionStart},
        {"tool.execution_partial_result", SessionEventType::ToolExecutionPartialResult},
        {"tool.execution_complete", SessionEventType::ToolExecutionComplete},
        {"custom_agent.started", SessionEventType::CustomAgentStarted},
        {"custom_agent.completed", SessionEventType::CustomAgentCompleted},
        {"custom_agent.failed", SessionEventType::CustomAgentFailed},
        {"custom_agent.selected", SessionEventType::CustomAgentSelected},
        {"hook.start", SessionEventType::HookStart},
        {"hook.end", SessionEventType::HookEnd},
        {"system.message", SessionEventType::SystemMessage},
    };

    auto it = type_map.find(event.type_string);
    if (it != type_map.end())
    {
        event.type = it->second;

        // Parse data based on type
        switch (event.type)
        {
        case SessionEventType::SessionStart:
            event.data = data_json.get<SessionStartData>();
            break;
        case SessionEventType::SessionResume:
            event.data = data_json.get<SessionResumeData>();
            break;
        case SessionEventType::SessionError:
            event.data = data_json.get<SessionErrorData>();
            break;
        case SessionEventType::SessionIdle:
            event.data = data_json.get<SessionIdleData>();
            break;
        case SessionEventType::SessionInfo:
            event.data = data_json.get<SessionInfoData>();
            break;
        case SessionEventType::SessionModelChange:
            event.data = data_json.get<SessionModelChangeData>();
            break;
        case SessionEventType::SessionHandoff:
            event.data = data_json.get<SessionHandoffData>();
            break;
        case SessionEventType::SessionTruncation:
            event.data = data_json.get<SessionTruncationData>();
            break;
        case SessionEventType::UserMessage:
            event.data = data_json.get<UserMessageData>();
            break;
        case SessionEventType::PendingMessagesModified:
            event.data = data_json.get<PendingMessagesModifiedData>();
            break;
        case SessionEventType::AssistantTurnStart:
            event.data = data_json.get<AssistantTurnStartData>();
            break;
        case SessionEventType::AssistantIntent:
            event.data = data_json.get<AssistantIntentData>();
            break;
        case SessionEventType::AssistantReasoning:
            event.data = data_json.get<AssistantReasoningData>();
            break;
        case SessionEventType::AssistantReasoningDelta:
            event.data = data_json.get<AssistantReasoningDeltaData>();
            break;
        case SessionEventType::AssistantMessage:
            event.data = data_json.get<AssistantMessageData>();
            break;
        case SessionEventType::AssistantMessageDelta:
            event.data = data_json.get<AssistantMessageDeltaData>();
            break;
        case SessionEventType::AssistantTurnEnd:
            event.data = data_json.get<AssistantTurnEndData>();
            break;
        case SessionEventType::AssistantUsage:
            event.data = data_json.get<AssistantUsageData>();
            break;
        case SessionEventType::Abort:
            event.data = data_json.get<AbortData>();
            break;
        case SessionEventType::ToolUserRequested:
            event.data = data_json.get<ToolUserRequestedData>();
            break;
        case SessionEventType::ToolExecutionStart:
            event.data = data_json.get<ToolExecutionStartData>();
            break;
        case SessionEventType::ToolExecutionPartialResult:
            event.data = data_json.get<ToolExecutionPartialResultData>();
            break;
        case SessionEventType::ToolExecutionComplete:
            event.data = data_json.get<ToolExecutionCompleteData>();
            break;
        case SessionEventType::CustomAgentStarted:
            event.data = data_json.get<CustomAgentStartedData>();
            break;
        case SessionEventType::CustomAgentCompleted:
            event.data = data_json.get<CustomAgentCompletedData>();
            break;
        case SessionEventType::CustomAgentFailed:
            event.data = data_json.get<CustomAgentFailedData>();
            break;
        case SessionEventType::CustomAgentSelected:
            event.data = data_json.get<CustomAgentSelectedData>();
            break;
        case SessionEventType::HookStart:
            event.data = data_json.get<HookStartData>();
            break;
        case SessionEventType::HookEnd:
            event.data = data_json.get<HookEndData>();
            break;
        case SessionEventType::SystemMessage:
            event.data = data_json.get<SystemMessageData>();
            break;
        default:
            event.data = data_json; // Fallback to raw JSON
            break;
        }
    }
    else
    {
        // Unknown event type - store raw JSON
        event.type = SessionEventType::Unknown;
        event.data = data_json;
    }

    return event;
}

/// ADL hook for json
inline void from_json(const json& j, SessionEvent& event)
{
    event = parse_session_event(j);
}

} // namespace copilot
