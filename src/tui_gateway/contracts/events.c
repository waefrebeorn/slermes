/*
 * events.c - C11 port of upstream tui_gateway/contracts/events.py
 * (gate w32_events_3805ad).
 *
 * PoP: w32_events_3805ad_catalog          @ tui_gateway/contracts/events.py:event(...) call sites
 * PoP: w32_events_3805ad_schema_all       @ tui_gateway/contracts/events.py:<Payload> model bodies
 * PoP: w32_events_3805ad_schema           @ tui_gateway/contracts/events.py:<Payload> model bodies
 * PoP: w32_events_3805ad_problems         @ tui_gateway/contracts/registry.py:check_payload
 * PoP: w32_events_3805ad_enums            @ tui_gateway/contracts/events.py:WireEnum subclasses
 * PoP: w32_events_3805ad_select           @ tui_gateway/contracts/events.py:event(...) declarations
 * PoP: w32_events_3805ad_frames           @ tui_gateway/contracts/registry.py:check_payload
 *
 * The gateway's sockets, the emitters and pydantic's validators are seams: what
 * is ported is the value they are all written against -- the declared event
 * catalog, each payload's closed/open field schema, the wire enums, and the
 * structural verdict the registry derives from that schema (a missing required
 * field, an undeclared key on a closed payload, a payload on a payloadless
 * frame).  Fields keep declaration order; inherited fields come before the
 * subclass's own.  An annotation is carried verbatim for the client's sake, not
 * enforced: value/type checking stays with pydantic on the Python side.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_events_3805ad.h"

typedef struct {
    const char *name;
    const char *ann;        /* the declared annotation, verbatim */
    int required;           /* no default -> the emitter must set it */
} ev_field;

typedef struct {
    const char *name;
    const char *model;      /* payload model name, NULL when the frame carries none */
    int is_open;            /* extra="allow": the closed set is owned elsewhere */
    const char *doc;
    const ev_field *fields;
    size_t nfields;
} ev_contract;

typedef struct {
    const char *name;
    const char *const *values;
    size_t nvalues;
} ev_enum;
static const ev_field ev_f0[] = {
    {"skin", "SkinPayload", 1},
    {"change_events", "bool", 1},
    {"replay_epoch", "str", 1},
    {"heartbeat", "bool | None", 0},
    {"session_authority", "bool | None", 0},
};
static const ev_field ev_f1[] = {
    {"name", "str", 0},
    {"description", "str", 0},
    {"colors", "dict[str, str]", 0},
    {"light_colors", "dict[str, str]", 0},
    {"dark_colors", "dict[str, str]", 0},
    {"branding", "dict[str, str]", 0},
    {"banner_logo", "str", 0},
    {"banner_hero", "str", 0},
    {"tool_prefix", "str", 0},
    {"help_header", "str", 0},
};
static const ev_field ev_f2[] = {
    {"run_id", "str", 1},
    {"stored_session_id", "str", 1},
    {"outcome", "OnboardingOutcome", 1},
};
static const ev_field ev_f3[] = {
    {"provider_configured", "bool", 1},
    {"inference_provider", "str", 1},
    {"free_tier_account", "bool", 1},
    {"free_tier_route", "bool", 1},
    {"has_identity", "bool", 1},
    {"other_providers", "bool", 1},
    {"error", "str", 0},
    {"error_code", "str | None", 0},
    {"retryable", "bool | None", 0},
    {"retry_after", "int | None", 0},
    {"finished_at", "float", 1},
};
static const ev_field ev_f4[] = {
    {"type", "Literal['browser']", 1},
    {"url", "str", 1},
    {"required", "bool", 1},
    {"expires_in", "int", 1},
    {"message", "str", 1},
    {"attempt", "int", 0},
};
static const ev_field ev_f5[] = {
    {"message", "str", 1},
};
static const ev_field ev_f6[] = {
    {"message", "str", 1},
};
static const ev_field ev_f7[] = {
    {"display_kind", "str | None", 0},
    {"display_text", "str | None", 0},
    {"notification_category", "str | None", 0},
    {"text", "str | None", 0},
    {"turn_id", "str | None", 0},
};
static const ev_field ev_f8[] = {
    {"text", "str | None", 1},
    {"rendered", "str | None", 0},
    {"verbose", "bool | None", 0},
    {"turn_id", "str | None", 0},
    {"segment", "int | None", 0},
    {"offset", "int | None", 0},
};
static const ev_field ev_f9[] = {
    {"text", "str", 1},
    {"rendered", "str | None", 0},
    {"verbose", "bool | None", 0},
};
static const ev_field ev_f10[] = {
    {"text", "str", 1},
    {"rendered", "str | None", 0},
    {"verbose", "bool | None", 0},
};
static const ev_field ev_f11[] = {
    {"text", "str", 1},
    {"rendered", "str | None", 0},
    {"verbose", "bool | None", 0},
};
static const ev_field ev_f12[] = {
    {"text", "str", 1},
    {"already_streamed", "bool", 1},
    {"turn_id", "str | None", 0},
    {"segment", "int | None", 0},
};
static const ev_field ev_f13[] = {
    {"text", "str | JsonValue", 0},
    {"usage", "Usage | None", 0},
    {"status", "TurnStatus | None", 0},
    {"reasoning", "str | None", 0},
    {"warning", "str | None", 0},
    {"response_previewed", "bool | None", 0},
    {"response_reused", "bool | None", 0},
    {"response_transformed", "bool | None", 0},
    {"billing", "BillingBlock | None", 0},
    {"failure_reason", "str | None", 0},
    {"rendered", "str | None", 0},
    {"error", "str | None", 0},
    {"recoverable", "bool | None", 0},
    {"error_surface", "ErrorSurface | None", 0},
    {"partial", "bool | None", 0},
    {"persisted_turn", "PersistedTurn | None", 0},
};
static const ev_field ev_f14[] = {
    {"kind", "str", 1},
    {"text", "str", 1},
};
static const ev_field ev_f15[] = {
    {"usage", "Usage | None", 0},
    {"input_tokens", "int | None", 0},
    {"output_tokens", "int | None", 0},
    {"cache_read_tokens", "int | None", 0},
    {"cache_write_tokens", "int | None", 0},
    {"prompt_tokens", "int | None", 0},
    {"completion_tokens", "int | None", 0},
    {"total_tokens", "int | None", 0},
    {"reasoning_tokens", "int | None", 0},
    {"api_calls", "int | None", 0},
    {"last_prompt_tokens", "int | None", 0},
    {"context_tokens", "int | None", 0},
    {"context_length", "int | None", 0},
    {"config_context_length", "int | None", 0},
    {"context_source", "str | None", 0},
    {"compression_count", "int | None", 0},
    {"avg_latency_s", "float | None", 0},
    {"tokens_per_s", "float | None", 0},
    {"last_turn_tokens", "int | None", 0},
    {"rate_limit", "dict[str, JsonValue] | None", 0},
    {"active_subagents", "int | None", 0},
    {"model", "str | None", 0},
    {"provider", "str | None", 0},
    {"fallback_model", "str | None", 0},
    {"plan_gauge", "PlanGauge | None", 0},
};
static const ev_field ev_f16[] = {
    {"session_id", "str", 1},
    {"title", "str", 1},
};
static const ev_field ev_f17[] = {
    {"kind", "str", 1},
};
static const ev_field ev_f18[] = {
    {"text", "str", 1},
};
static const ev_field ev_f19[] = {
    {"text", "str", 1},
};
static const ev_field ev_f20[] = {
    {"tool_id", "str", 1},
    {"name", "str", 1},
    {"context", "str | None", 0},
    {"args", "dict[str, JsonValue] | None", 0},
    {"args_text", "str | None", 0},
    {"preview", "str | None", 0},
    {"labels", "list[ToolLabel] | None", 0},
};
static const ev_field ev_f21[] = {
    {"tool_id", "str", 1},
    {"name", "str", 1},
    {"args", "dict[str, JsonValue] | None", 0},
    {"duration_s", "float | None", 0},
    {"result", "JsonValue", 0},
    {"summary", "str | None", 0},
    {"result_text", "str | None", 0},
    {"inline_diff", "str | None", 0},
    {"todos", "list[JsonValue] | None", 0},
    {"revision", "int | None", 0},
    {"labels", "list[ToolLabel] | None", 0},
    {"image_path", "str | None", 0},
};
static const ev_field ev_f22[] = {
    {"name", "str", 1},
};
static const ev_field ev_f23[] = {
    {"tool_id", "str", 1},
    {"name", "str", 1},
    {"risk", "str", 1},
    {"findings", "list[str]", 1},
    {"redacted", "bool", 1},
};
static const ev_field ev_f24[] = {
    {"tool_id", "str", 1},
    {"name", "str", 1},
    {"progress", "float", 1},
    {"total", "float | None", 0},
    {"percent", "int | None", 0},
    {"message", "str | None", 0},
};
static const ev_field ev_f25[] = {
    {"todos", "list[JsonValue]", 1},
    {"revision", "int", 1},
};
static const ev_field ev_f26[] = {
    {"text", "str", 1},
    {"level", "str", 1},
    {"kind", "str", 1},
    {"ttl_ms", "int | None", 0},
    {"key", "str | None", 0},
    {"id", "str | None", 0},
    {"detail", "str | None", 0},
};
static const ev_field ev_f27[] = {
    {"key", "str", 1},
};
static const ev_field ev_f28[] = {
    {"selector", "str", 1},
    {"text", "str", 1},
    {"title", "str | None", 0},
    {"side", "str | None", 0},
};
static const ev_field ev_f29[] = {
    {"model", "str", 0},
    {"provider", "str", 0},
    {"reasoning_effort", "str", 0},
    {"reasoning_effort_wire", "str", 0},
    {"service_tier", "str", 0},
    {"fast", "bool", 0},
    {"yolo", "bool", 0},
    {"approval_mode", "str", 0},
    {"tools", "dict[str, list[str]]", 0},
    {"skills", "dict[str, list[str]]", 0},
    {"cwd", "str", 0},
    {"branch", "str | None", 0},
    {"project", "ProjectRef | None", 0},
    {"terminal_backend", "str", 0},
    {"personality", "str", 0},
    {"running", "bool", 0},
    {"turn_started_at", "float | None", 0},
    {"title", "str", 0},
    {"stored_session_id", "str", 0},
    {"desktop_contract", "int | str | None", 0},
    {"version", "str", 0},
    {"release_date", "str", 0},
    {"update_behind", "JsonValue | None", 0},
    {"update_command", "str", 0},
    {"usage", "Usage | None", 0},
    {"profile_name", "str | None", 0},
    {"mcp_servers", "list[McpServerStatus]", 0},
    {"system_prompt", "str | None", 0},
    {"credential_warning", "str | None", 0},
    {"lazy", "bool | None", 0},
};
static const ev_field ev_f30[] = {
    {"phase", "str", 1},
    {"status", "ResumePhaseStatus", 1},
    {"message_count", "int | None", 0},
    {"message", "str | None", 0},
};
static const ev_field ev_f31[] = {
    {"session_id", "str", 1},
    {"stored_session_id", "str", 1},
    {"reason", "str", 1},
};
static const ev_field ev_f32[] = {
    {"session_id", "str", 1},
    {"stored_session_id", "str", 1},
    {"reason", "str", 1},
    {"cancelled_count", "int", 1},
    {"request_ids", "list[str]", 1},
};
static const ev_field ev_f33[] = {
    {"control", "SessionControlSnapshot", 1},
};
static const ev_field ev_f34[] = {
    {"verification_url", "str", 1},
    {"user_code", "str", 1},
};
static const ev_field ev_f35[] = {
    {"task_id", "str", 1},
    {"text", "str", 1},
    {"question", "str | None", 0},
};
static const ev_field ev_f36[] = {
    {"task_id", "str", 1},
    {"text", "str", 1},
    {"question", "str | None", 0},
};
static const ev_field ev_f37[] = {
    {"task_id", "str", 1},
    {"text", "str", 1},
    {"question", "str | None", 0},
};
static const ev_field ev_f38[] = {
    {"task_id", "str", 1},
    {"text", "str", 1},
    {"level", "str | None", 0},
};
static const ev_field ev_f39[] = {
    {"goal", "str", 1},
    {"task_count", "int", 1},
    {"task_index", "int", 1},
    {"subagent_id", "str | None", 0},
    {"parent_id", "str | None", 0},
    {"child_session_id", "str | None", 0},
    {"delegation_id", "str | None", 0},
    {"depth", "int | None", 0},
    {"model", "str | None", 0},
    {"tool_count", "int | None", 0},
    {"toolsets", "list[str] | None", 0},
    {"input_tokens", "int | None", 0},
    {"output_tokens", "int | None", 0},
    {"reasoning_tokens", "int | None", 0},
    {"api_calls", "int | None", 0},
    {"files_read", "list[str] | None", 0},
    {"files_written", "list[str] | None", 0},
    {"output_tail", "list[SubagentOutputTailEntry] | None", 0},
    {"tool_name", "str | None", 0},
    {"text", "str | None", 0},
    {"status", "SubagentStatus | None", 0},
    {"summary", "str | None", 0},
    {"duration_seconds", "float | None", 0},
    {"tool_preview", "str | None", 0},
};
static const ev_field ev_f40[] = {
    {"goal", "str", 1},
    {"task_count", "int", 1},
    {"task_index", "int", 1},
    {"subagent_id", "str | None", 0},
    {"parent_id", "str | None", 0},
    {"child_session_id", "str | None", 0},
    {"delegation_id", "str | None", 0},
    {"depth", "int | None", 0},
    {"model", "str | None", 0},
    {"tool_count", "int | None", 0},
    {"toolsets", "list[str] | None", 0},
    {"input_tokens", "int | None", 0},
    {"output_tokens", "int | None", 0},
    {"reasoning_tokens", "int | None", 0},
    {"api_calls", "int | None", 0},
    {"files_read", "list[str] | None", 0},
    {"files_written", "list[str] | None", 0},
    {"output_tail", "list[SubagentOutputTailEntry] | None", 0},
    {"tool_name", "str | None", 0},
    {"text", "str | None", 0},
    {"status", "SubagentStatus | None", 0},
    {"summary", "str | None", 0},
    {"duration_seconds", "float | None", 0},
    {"tool_preview", "str | None", 0},
};
static const ev_field ev_f41[] = {
    {"goal", "str", 1},
    {"task_count", "int", 1},
    {"task_index", "int", 1},
    {"subagent_id", "str | None", 0},
    {"parent_id", "str | None", 0},
    {"child_session_id", "str | None", 0},
    {"delegation_id", "str | None", 0},
    {"depth", "int | None", 0},
    {"model", "str | None", 0},
    {"tool_count", "int | None", 0},
    {"toolsets", "list[str] | None", 0},
    {"input_tokens", "int | None", 0},
    {"output_tokens", "int | None", 0},
    {"reasoning_tokens", "int | None", 0},
    {"api_calls", "int | None", 0},
    {"files_read", "list[str] | None", 0},
    {"files_written", "list[str] | None", 0},
    {"output_tail", "list[SubagentOutputTailEntry] | None", 0},
    {"tool_name", "str | None", 0},
    {"text", "str | None", 0},
    {"status", "SubagentStatus | None", 0},
    {"summary", "str | None", 0},
    {"duration_seconds", "float | None", 0},
    {"tool_preview", "str | None", 0},
};
static const ev_field ev_f42[] = {
    {"goal", "str", 1},
    {"task_count", "int", 1},
    {"task_index", "int", 1},
    {"subagent_id", "str | None", 0},
    {"parent_id", "str | None", 0},
    {"child_session_id", "str | None", 0},
    {"delegation_id", "str | None", 0},
    {"depth", "int | None", 0},
    {"model", "str | None", 0},
    {"tool_count", "int | None", 0},
    {"toolsets", "list[str] | None", 0},
    {"input_tokens", "int | None", 0},
    {"output_tokens", "int | None", 0},
    {"reasoning_tokens", "int | None", 0},
    {"api_calls", "int | None", 0},
    {"files_read", "list[str] | None", 0},
    {"files_written", "list[str] | None", 0},
    {"output_tail", "list[SubagentOutputTailEntry] | None", 0},
    {"tool_name", "str | None", 0},
    {"text", "str | None", 0},
    {"status", "SubagentStatus | None", 0},
    {"summary", "str | None", 0},
    {"duration_seconds", "float | None", 0},
    {"tool_preview", "str | None", 0},
};
static const ev_field ev_f43[] = {
    {"goal", "str", 1},
    {"task_count", "int", 1},
    {"task_index", "int", 1},
    {"subagent_id", "str | None", 0},
    {"parent_id", "str | None", 0},
    {"child_session_id", "str | None", 0},
    {"delegation_id", "str | None", 0},
    {"depth", "int | None", 0},
    {"model", "str | None", 0},
    {"tool_count", "int | None", 0},
    {"toolsets", "list[str] | None", 0},
    {"input_tokens", "int | None", 0},
    {"output_tokens", "int | None", 0},
    {"reasoning_tokens", "int | None", 0},
    {"api_calls", "int | None", 0},
    {"files_read", "list[str] | None", 0},
    {"files_written", "list[str] | None", 0},
    {"output_tail", "list[SubagentOutputTailEntry] | None", 0},
    {"tool_name", "str | None", 0},
    {"text", "str | None", 0},
    {"status", "SubagentStatus | None", 0},
    {"summary", "str | None", 0},
    {"duration_seconds", "float | None", 0},
    {"tool_preview", "str | None", 0},
};
static const ev_field ev_f44[] = {
    {"goal", "str", 1},
    {"task_count", "int", 1},
    {"task_index", "int", 1},
    {"subagent_id", "str | None", 0},
    {"parent_id", "str | None", 0},
    {"child_session_id", "str | None", 0},
    {"delegation_id", "str | None", 0},
    {"depth", "int | None", 0},
    {"model", "str | None", 0},
    {"tool_count", "int | None", 0},
    {"toolsets", "list[str] | None", 0},
    {"input_tokens", "int | None", 0},
    {"output_tokens", "int | None", 0},
    {"reasoning_tokens", "int | None", 0},
    {"api_calls", "int | None", 0},
    {"files_read", "list[str] | None", 0},
    {"files_written", "list[str] | None", 0},
    {"output_tail", "list[SubagentOutputTailEntry] | None", 0},
    {"tool_name", "str | None", 0},
    {"text", "str | None", 0},
    {"status", "SubagentStatus | None", 0},
    {"summary", "str | None", 0},
    {"duration_seconds", "float | None", 0},
    {"tool_preview", "str | None", 0},
};
static const ev_field ev_f45[] = {
    {"label", "str", 1},
    {"text", "str", 1},
    {"index", "int | None", 0},
    {"count", "int | None", 0},
};
static const ev_field ev_f46[] = {
    {"aggregator", "str", 1},
};
static const ev_field ev_f47[] = {
    {"label", "str", 1},
    {"refs_done", "int", 1},
    {"refs_total", "int", 1},
};
static const ev_field ev_f48[] = {
    {"phase", "str", 1},
    {"refs_done", "int | None", 0},
    {"refs_total", "int | None", 0},
    {"aggregator", "str | None", 0},
};
static const ev_field ev_f49[] = {
    {"url", "str", 1},
    {"label", "str", 0},
};
static const ev_field ev_f50[] = {
    {"url", "str", 0},
};
static const ev_field ev_f51[] = {
    {"pane", "str", 1},
};
static const ev_field ev_f52[] = {
    {"row_id", "int", 1},
    {"reactions", "list[MessageReaction]", 1},
    {"role", "str", 1},
};
static const ev_field ev_f53[] = {
    {"process_id", "str", 1},
    {"chunk", "str", 1},
};
static const ev_field ev_f54[] = {
    {"process_id", "str", 1},
};
static const ev_field ev_f55[] = {
    {"message", "str", 1},
    {"level", "str", 1},
};
static const ev_field ev_f56[] = { { NULL, NULL, 0 } };
static const ev_field ev_f57[] = {
    {"state", "str", 1},
};
static const ev_field ev_f58[] = {
    {"text", "str", 1},
};
static const ev_field ev_f59[] = {
    {"text", "str | None", 0},
    {"stop_phrase", "bool | None", 0},
    {"typed", "bool | None", 0},
    {"no_speech_limit", "bool | None", 0},
};
static const ev_field ev_f60[] = {
    {"phrase", "str", 1},
    {"profile", "str | None", 0},
    {"start_new_session", "bool", 1},
};
static const ev_field ev_f61[] = {
    {"enabled", "bool", 1},
    {"slug", "str | None", 0},
    {"displayName", "str | None", 0},
    {"scale", "float | None", 0},
    {"spritesheetRevision", "str | None", 0},
};
static const ev_field ev_f62[] = {
    {"token", "str", 1},
    {"count", "int", 1},
    {"index", "int | None", 0},
    {"dataUri", "str | None", 0},
};
static const ev_field ev_f63[] = {
    {"event", "str", 1},
    {"detail", "str | None", 0},
    {"state", "str | None", 0},
    {"done", "str | None", 0},
    {"total", "str | None", 0},
};
static const ev_field ev_f64[] = { { NULL, NULL, 0 } };
static const ev_field ev_f65[] = { { NULL, NULL, 0 } };
static const ev_field ev_f66[] = { { NULL, NULL, 0 } };
static const ev_field ev_f67[] = { { NULL, NULL, 0 } };
static const ev_field ev_f68[] = { { NULL, NULL, 0 } };
static const ev_field ev_f69[] = { { NULL, NULL, 0 } };
static const ev_field ev_f70[] = { { NULL, NULL, 0 } };
static const ev_field ev_f71[] = {
    {"room_ids", "list[str] | None", 0},
    {"broadcast_id", "str | None", 0},
};

static const ev_contract ev_catalog[] = {
    {"gateway.ready", "GatewayReadyPayload", 0,
     "First frame of a connection: the resolved skin, the change-event capability and the "
     "replay epoch.",
     ev_f0, 5},
    {"skin.changed", "SkinPayload", 1,
     "The active skin moved (name switch or live colour edit); repaint from this palette.",
     ev_f1, 10},
    {"onboarding.ended", "OnboardingEndedPayload", 0,
     "A first-run onboarding run ended; sent to every connected window.",
     ev_f2, 3},
    {"setup.ready", "SetupReadyPayload", 1,
     "The free-tier bootstrap finished (broadcast); the desktop's setup gate reads the "
     "record.",
     ev_f3, 11},
    {"free_tier.challenge", "FreeTierChallengePayload", 1,
     "The account service wants a browser challenge cleared before the free-tier token "
     "exchange (broadcast); the desktop loads ``url`` in a hidden window.",
     ev_f4, 6},
    {"error", "ErrorPayload", 0,
     "A session-level failure outside a turn (agent init, model switch, compression, "
     "resume).",
     ev_f5, 1},
    {"notice", "NoticePayload", 0,
     "Informational one-liner for the session (capabilities refreshed).",
     ev_f6, 1},
    {"message.start", "MessageStartPayload", 0,
     "A turn began streaming; an automation turn carries its async-result card.",
     ev_f7, 5},
    {"message.delta", "MessageDeltaPayload", 0,
     "One streamed chunk of the assistant reply (null text: segment boundary).",
     ev_f8, 6},
    {"reasoning.delta", "StreamDeltaPayload", 0,
     "One streamed chunk of the model's reasoning.",
     ev_f9, 3},
    {"reasoning.available", "StreamDeltaPayload", 0,
     "A completed reasoning block (non-streaming providers).",
     ev_f10, 3},
    {"thinking.delta", "StreamDeltaPayload", 0,
     "Legacy thinking-text chunk (thinking_callback).",
     ev_f11, 3},
    {"message.interim", "MessageInterimPayload", 0,
     "Interim assistant commentary (text beside tool calls) sealed as its own segment.",
     ev_f12, 4},
    {"message.complete", "MessageCompletePayload", 0,
     "The turn ended: final text, usage and outcome.",
     ev_f13, 16},
    {"status.update", "StatusUpdatePayload", 0,
     "Transient status line (kind: status, lifecycle, compacting, goal, loop, heartbeat, "
     "process, \342\200\246).",
     ev_f14, 2},
    {"session.usage", "SessionUsagePayload", 0,
     "Mid-turn usage tick; message.complete carries the authoritative final usage.",
     ev_f15, 25},
    {"session.title", "SessionTitlePayload", 0,
     "Auto-titling renamed the session (``session_id`` is the stored key).",
     ev_f16, 2},
    {"reaction", "ReactionPayload", 0,
     "Affection reaction detected in the user's message (hearts etc.).",
     ev_f17, 1},
    {"review.summary", "ReviewSummaryPayload", 0,
     "Background review of the last turn finished.",
     ev_f18, 1},
    {"agent.print", "AgentPrintPayload", 0,
     "A status line the agent printed during a local viewer's turn.",
     ev_f19, 1},
    {"tool.start", "ToolStartPayload", 0,
     "A tool call began (stable id + full args).",
     ev_f20, 7},
    {"tool.complete", "ToolCompletePayload", 0,
     "A tool call finished: parsed result, summary, optional diff / todo snapshot.",
     ev_f21, 12},
    {"tool.generating", "ToolGeneratingPayload", 0,
     "The model is emitting a tool call's arguments.",
     ev_f22, 1},
    {"tool.output_risk", "ToolOutputRiskPayload", 0,
     "Tool output was classified as risky (prompt-injection / secret findings).",
     ev_f23, 5},
    {"tool.progress", "ToolProgressPayload", 0,
     "Live progress of a running tool call (MCP notifications/progress).",
     ev_f24, 6},
    {"todo.updated", "TodoUpdatedPayload", 0,
     "Full todo snapshot after a todo tool ran.",
     ev_f25, 2},
    {"notification.show", "NotificationShowPayload", 0,
     "Show / replace a keyed out-of-band notice (toast or status bar).",
     ev_f26, 7},
    {"notification.clear", "NotificationClearPayload", 0,
     "Withdraw the notice with this key.",
     ev_f27, 1},
    {"tip.show", "TipShowPayload", 0,
     "Point at a desktop element with a one-line tip bubble.",
     ev_f28, 4},
    {"session.info", "SessionLiveInfo", 1,
     "Live session settings snapshot (``server._session_info``); also the ``info`` of "
     "create/resume/activate.",
     ev_f29, 30},
    {"session.resume_progress", "SessionResumeProgressPayload", 0,
     "Deferred resume hydration progress.",
     ev_f30, 4},
    {"session.reclaimed", "SessionReclaimedPayload", 0,
     "The backend reclaimed a live session out from under its clients.",
     ev_f31, 3},
    {"approval.cancelled", "ApprovalCancelledPayload", 0,
     "Pending gateway approvals were dropped by interrupt/reap/teardown; the wait resolved "
     "as deny (not a user refusal).",
     ev_f32, 5},
    {"session.control.update", "SessionControlUpdatePayload", 0,
     "Persisted goal / loop / heartbeat state changed.",
     ev_f33, 1},
    {"billing.step_up.verification", "BillingStepUpVerificationPayload", 0,
     "Device-flow URL + code for the billing scope step-up; the client opens the browser.",
     ev_f34, 2},
    {"background.complete", "SideAgentCompletePayload", 0,
     "A /background side agent finished.",
     ev_f35, 3},
    {"btw.complete", "SideAgentCompletePayload", 0,
     "A /btw side question was answered.",
     ev_f36, 3},
    {"preview.restart.complete", "SideAgentCompletePayload", 0,
     "The hidden preview-restart agent finished.",
     ev_f37, 3},
    {"preview.restart.progress", "PreviewRestartProgressPayload", 0,
     "Progress line from the preview-restart agent.",
     ev_f38, 3},
    {"subagent.spawn_requested", "SubagentEventPayload", 0,
     "delegate_task accepted a child goal (before the child starts).",
     ev_f39, 24},
    {"subagent.start", "SubagentEventPayload", 0,
     "A delegated child started running.",
     ev_f40, 24},
    {"subagent.progress", "SubagentEventPayload", 0,
     "Batched tool-name progress from a child.",
     ev_f41, 24},
    {"subagent.thinking", "SubagentEventPayload", 0,
     "A child's reasoning chunk.",
     ev_f42, 24},
    {"subagent.tool", "SubagentEventPayload", 0,
     "A child called a tool.",
     ev_f43, 24},
    {"subagent.complete", "SubagentEventPayload", 0,
     "A child finished (status + observability rollup).",
     ev_f44, 24},
    {"moa.reference", "MoaReferencePayload", 0,
     "One MoA reference model's output.",
     ev_f45, 4},
    {"moa.aggregating", "MoaAggregatingPayload", 0,
     "The MoA aggregator started.",
     ev_f46, 1},
    {"moa.progress", "MoaProgressPayload", 0,
     "MoA reference fan-out progress (n/total).",
     ev_f47, 3},
    {"moa.phase", "MoaPhasePayload", 0,
     "MoA phase transition (currently only ``aggregator``).",
     ev_f48, 4},
    {"preview.open", "PreviewOpenPayload", 0,
     "Open a URL / file in the desktop preview pane.",
     ev_f49, 2},
    {"preview.close", "PreviewClosePayload", 0,
     "Close the preview pane or one tab.",
     ev_f50, 1},
    {"pane.reveal", "PaneRevealPayload", 1,
     "Focus / reveal a named desktop pane.",
     ev_f51, 1},
    {"message.reaction", "MessageReactionPayload", 0,
     "The agent reacted to a message; paint it live.",
     ev_f52, 3},
    {"agent.terminal.output", "TerminalOutputPayload", 0,
     "Output chunk from an agent-owned background process.",
     ev_f53, 2},
    {"terminal.close", "TerminalClosePayload", 0,
     "An agent-owned background process closed.",
     ev_f54, 1},
    {"browser.progress", "BrowserProgressPayload", 0,
     "Browser (CDP) connect / install progress line.",
     ev_f55, 2},
    {"voice.interrupted", NULL, 0,
     "Barge-in: the spoken interjection interrupted the turn; no payload.",
     ev_f56, 0},
    {"voice.status", "VoiceStatusPayload", 0,
     "Voice recorder state changed.",
     ev_f57, 1},
    {"voice.partial", "VoicePartialPayload", 0,
     "Live STT text so far while the user is still speaking.",
     ev_f58, 1},
    {"voice.transcript", "VoiceTranscriptPayload", 0,
     "A voice capture produced text (or a stop phrase / silence limit).",
     ev_f59, 4},
    {"wake.detected", "WakeDetectedPayload", 0,
     "A wake phrase fired.",
     ev_f60, 3},
    {"pet.changed", "PetChangedPayload", 1,
     "The active pet / its spritesheet changed (watcher).",
     ev_f61, 5},
    {"pet.generate.progress", "PetGenerateProgressPayload", 1,
     "Pet base-draft generation progress.",
     ev_f62, 4},
    {"pet.hatch.progress", "PetHatchProgressPayload", 1,
     "Pet hatch (row drawing) progress.",
     ev_f63, 5},
    {"cron.changed", "ChangeSignalPayload", 1,
     "cron/jobs.json moved; refetch the cron list.",
     ev_f64, 0},
    {"sessions.changed", "ChangeSignalPayload", 1,
     "state.db moved; refetch the session list.",
     ev_f65, 0},
    {"platforms.changed", "ChangeSignalPayload", 1,
     "gateway_state.json moved; refetch platform status.",
     ev_f66, 0},
    {"projects.changed", "ChangeSignalPayload", 1,
     "projects.db moved; refetch the project list + tree.",
     ev_f67, 0},
    {"pairing.changed", "ChangeSignalPayload", 1,
     "Pairing state moved; refetch pairing.",
     ev_f68, 0},
    {"skills.changed", "ChangeSignalPayload", 1,
     "A skill root moved (installed/removed elsewhere); refetch commands.catalog from the "
     "gateway.",
     ev_f69, 0},
    {"bot_relay.outbox.pending", "ChangeSignalPayload", 1,
     "A bot-relay outbox envelope is queued; drain it.",
     ev_f70, 0},
    {"groups.changed", "GroupsChangedPayload", 1,
     "A Group Chat log, task or live status moved; an open room re-reads its log delta and "
     "groups.state.",
     ev_f71, 2},
};

static const size_t ev_catalog_len = sizeof(ev_catalog) / sizeof(ev_catalog[0]);

static const char *const ev_e0_values[] = {
    "done",
    "skipped",
    "replaced",
    "deleted",
};

static const char *const ev_e1_values[] = {
    "complete",
    "error",
    "interrupted",
};

static const char *const ev_e2_values[] = {
    "loading",
    "complete",
    "failed",
};

static const ev_enum ev_enums[] = {
    {"OnboardingOutcome", ev_e0_values, 4},
    {"TurnStatus", ev_e1_values, 3},
    {"ResumePhaseStatus", ev_e2_values, 3},
};

static const size_t ev_enums_len = sizeof(ev_enums) / sizeof(ev_enums[0]);

/* Python truthiness of a JSON value. */
static int ev_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static const ev_contract *ev_find(const char *name) {
    size_t i;
    if (!name) return NULL;
    for (i = 0; i < ev_catalog_len; i++)
        if (strcmp(ev_catalog[i].name, name) == 0) return &ev_catalog[i];
    return NULL;
}

static int ev_field_at(const ev_contract *c, const char *key) {
    size_t i;
    for (i = 0; i < c->nfields; i++)
        if (strcmp(c->fields[i].name, key) == 0) return (int)i;
    return -1;
}

static json_t *ev_model_name(const ev_contract *c) {
    return c->model ? json_string(c->model) : json_null();
}

static json_t *ev_schema_entry(const ev_contract *c) {
    json_t *o = json_object();
    json_t *req = json_array(), *opt = json_array(), *types = json_object();
    size_t i;
    json_set(o, "name", json_string(c->name));
    json_set(o, "payload", ev_model_name(c));
    json_set(o, "open", json_bool(c->is_open ? true : false));
    for (i = 0; i < c->nfields; i++) {
        json_append(c->fields[i].required ? req : opt, json_string(c->fields[i].name));
        json_set(types, c->fields[i].name, json_string(c->fields[i].ann));
    }
    json_set(o, "required", req);
    json_set(o, "optional", opt);
    json_set(o, "types", types);
    return o;
}

json_t *w32_events_3805ad_catalog(void) {
    json_t *out = json_array();
    size_t i;
    for (i = 0; i < ev_catalog_len; i++) {
        const ev_contract *c = &ev_catalog[i];
        json_t *row = json_object();
        json_set(row, "name", json_string(c->name));
        json_set(row, "payload", ev_model_name(c));
        json_set(row, "open", json_bool(c->is_open ? true : false));
        json_set(row, "doc", json_string(c->doc));
        json_append(out, row);
    }
    return out;
}

json_t *w32_events_3805ad_schema_all(void) {
    json_t *out = json_array();
    size_t i;
    for (i = 0; i < ev_catalog_len; i++) json_append(out, ev_schema_entry(&ev_catalog[i]));
    return out;
}

json_t *w32_events_3805ad_schema(const json_t *event) {
    const ev_contract *c;
    if (!event || event->type != JSON_STRING) return json_null();
    c = ev_find(event->str_val);
    return c ? ev_schema_entry(c) : json_null();
}

json_t *w32_events_3805ad_problems(const json_t *event, const json_t *payload) {
    const ev_contract *c;
    json_t *out;
    size_t i, n;

    if (!event || event->type != JSON_STRING) return json_null();
    c = ev_find(event->str_val);
    if (!c) return json_null();
    out = json_array();

    /* A payloadless frame (payload: None) must not carry a truthy payload. */
    if (!c->model) {
        if (ev_truthy(payload)) json_append(out, json_string("payload:forbidden"));
        return out;
    }
    if (!payload || payload->type == JSON_NULL) {
        /* Absent and explicit null both arrive as an empty payload. */
    } else if (payload->type != JSON_OBJECT) {
        json_append(out, json_string("payload:dict_type"));
        return out;
    }
    for (i = 0; i < c->nfields; i++) {
        const char *f = c->fields[i].name;
        if (!c->fields[i].required) continue;
        if (!payload || payload->type != JSON_OBJECT || !json_obj_get(payload, f)) {
            char buf[256];
            snprintf(buf, sizeof buf, "%s:missing", f);
            json_append(out, json_string(buf));
        }
    }
    if (c->is_open || !payload || payload->type != JSON_OBJECT) return out;
    n = json_object_size(payload);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(payload, i);
        if (!k || ev_field_at(c, k) >= 0) continue;
        {
            char buf[256];
            snprintf(buf, sizeof buf, "%s:extra_forbidden", k);
            json_append(out, json_string(buf));
        }
    }
    return out;
}

json_t *w32_events_3805ad_enums(void) {
    json_t *out = json_object();
    size_t i, j;
    for (i = 0; i < ev_enums_len; i++) {
        json_t *vals = json_array();
        for (j = 0; j < ev_enums[i].nvalues; j++)
            json_append(vals, json_string(ev_enums[i].values[j]));
        json_set(out, ev_enums[i].name, vals);
    }
    return out;
}

json_t *w32_events_3805ad_select(const json_t *filter) {
    json_t *out = json_array();
    const json_t *f_open, *f_payload, *f_doc;
    size_t i;

    f_open = json_obj_get(filter, "open");
    f_payload = json_obj_get(filter, "payload");
    f_doc = json_obj_get(filter, "doc_has");
    if (f_open && f_open->type == JSON_NULL) f_open = NULL;
    if (f_payload && f_payload->type == JSON_NULL) f_payload = NULL;
    if (f_doc && f_doc->type == JSON_NULL) f_doc = NULL;

    for (i = 0; i < ev_catalog_len; i++) {
        const ev_contract *c = &ev_catalog[i];
        if (f_open && ev_truthy(f_open) != c->is_open) continue;
        if (f_payload) {
            if (f_payload->type != JSON_STRING || !c->model) continue;
            if (strcmp(f_payload->str_val, c->model) != 0) continue;
        }
        if (f_doc) {
            if (f_doc->type != JSON_STRING || !strstr(c->doc, f_doc->str_val)) continue;
        }
        json_append(out, json_string(c->name));
    }
    return out;
}

json_t *w32_events_3805ad_frames(const json_t *frames) {
    json_t *out = json_array();
    size_t i, n;
    if (!frames || frames->type != JSON_ARRAY) return out;
    n = json_len(frames);
    for (i = 0; i < n; i++) {
        const json_t *fr = json_get(frames, i);
        json_t *row = json_object();
        const json_t *event = NULL, *payload = NULL;
        if (fr && fr->type == JSON_OBJECT) {
            event = json_obj_get(fr, "event");
            payload = json_obj_get(fr, "payload");
        }
        json_set(row, "index", json_number((double)i));
        json_set(row, "problems", w32_events_3805ad_problems(event, payload));
        json_append(out, row);
    }
    return out;
}
