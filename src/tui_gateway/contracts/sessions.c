/*
 * sessions.c — C11 port of upstream tui_gateway/contracts/sessions.py
 * (gate w34_sessions_67685b).
 *
 * PoP: w34_sessions_67685b_catalog         @ tui_gateway/contracts/sessions.py:method(...) declarations
 * PoP: w34_sessions_67685b_models          @ tui_gateway/contracts/sessions.py:<model> declarations
 * PoP: w34_sessions_67685b_fields          @ tui_gateway/contracts/sessions.py:<model> field declarations
 * PoP: w34_sessions_67685b_required        @ tui_gateway/contracts/sessions.py:<model> required fields
 * PoP: w34_sessions_67685b_validate        @ tui_gateway/contracts/registry.py:validate_params
 *                                            (the "extra key → 4000" envelope, over this module's params)
 * PoP: w34_sessions_67685b_id_requirement  @ tui_gateway/contracts/sessions.py:SessionParams.session_id,
 *                                            SessionArchiveParams.session_key
 * PoP: w34_sessions_67685b_enum            @ tui_gateway/contracts/sessions.py:LiveSessionStatus,
 *                                            UndoIntent, InterruptStatus, CorrectionStatus
 * PoP: w34_sessions_67685b_accepts         @ tui_gateway/contracts/sessions.py:<params model> accepted keys
 *
 * What is ported here is the whole wire SHAPE of the session topic: the 33
 * method names and the params/result model each one rides, every declared
 * field of every model in the module (name, coarse annotation kind,
 * requiredness, default, default_factory, alias, min_length), the closed
 * enums, and the two value decisions the envelope makes before a handler runs:
 * which keys a params object may carry (an unknown key answers JSON-RPC 4000
 * with the key path, not a silent ignore) and which of them a model requires.
 *
 * Seams (deliberately left to the runtime):
 *   * pydantic itself — the model classes, their validators and the coercion /
 *     type errors they raise. registry.validate_params returns early on the
 *     extra-key error and leaves required/type errors to the handler, which
 *     owns its domain codes (4006 missing session_id, …); this port reproduces
 *     exactly that split, so shape_ok means "no unknown key, nothing required
 *     is missing" and NOT "pydantic would accept this object".
 *   * the gateway transport (JSON-RPC framing, 4000 error carriage).
 *   * the live session: the agent, the state.db rows, the transcript, the
 *     replay buffer. Nothing here reads or writes any of them.
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w34_sessions_67685b.h"

#define SESS_N(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
    const char *name;    /* wire key (pydantic field name) */
    const char *kind;    /* coarse annotation kind: str, int, opt(model), list(str), … */
    const char *alias;   /* serialisation alias, NULL when none */
    const char *def;     /* JSON literal of the default, NULL when required or a factory */
    int required;        /* field has no default at all */
    int factory;         /* default_factory: a container the model copies per instance */
    int min_length;      /* string constraint, -1 when unconstrained */
} sess_field;

typedef struct {
    const char *name;
    int extra_allow;     /* model_config extra == "allow" */
    int populate_by_name;/* the field name is accepted next to its alias */
    const sess_field *fields;
    size_t nfields;
} sess_model;

typedef struct {
    const char *method;
    const char *params;
    const char *result;
} sess_method;

typedef struct {
    const char *name;
    const char *const *values;
    size_t nvalues;
} sess_enum;

#define JSON_APPEND_STR(arr, s) json_append((arr), json_string((s)))

static int sess_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* ── fields: {"name", "kind", alias, default, required, factory, min_length} ── */
static const sess_field F_AutoContinue[] = {
    {"attempt", "int", NULL, NULL, 1, 0, -1},
    {"interrupted_at", "float", NULL, NULL, 1, 0, -1},
};
static const sess_field F_CompressionSummary[] = {
    {"noop", "bool", NULL, "false", 0, 0, -1},
    {"aborted", "bool", NULL, "false", 0, 0, -1},
    {"refused_would_grow", "opt(bool)", NULL, "null", 0, 0, -1},
    {"fallback_used", "opt(bool)", NULL, "null", 0, 0, -1},
    {"headline", "str", NULL, "\"\"", 0, 0, -1},
    {"token_line", "str", NULL, "\"\"", 0, 0, -1},
    {"note", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_ContextCategory[] = {
    {"color", "str", NULL, NULL, 1, 0, -1},
    {"id", "str", NULL, NULL, 1, 0, -1},
    {"label", "str", NULL, NULL, 1, 0, -1},
    {"tokens", "int", NULL, NULL, 1, 0, -1},
};
static const sess_field F_ContextFileSource[] = {
    {"label", "str", NULL, NULL, 1, 0, -1},
    {"path", "str", NULL, NULL, 1, 0, -1},
    {"chars", "int", NULL, NULL, 1, 0, -1},
    {"est_tokens", "int", NULL, NULL, 1, 0, -1},
    {"loaded", "bool", NULL, NULL, 1, 0, -1},
    {"status", "str", NULL, NULL, 1, 0, -1},
};
static const sess_field F_InflightTurn[] = {
    {"assistant", "str", NULL, "\"\"", 0, 0, -1},
    {"streaming", "bool", NULL, "false", 0, 0, -1},
    {"user", "str", NULL, "\"\"", 0, 0, -1},
    {"display_kind", "opt(str)", NULL, "null", 0, 0, -1},
    {"display_metadata", "opt(dict(str,json))", NULL, "null", 0, 0, -1},
    {"corrections", "opt(list(str))", NULL, "null", 0, 0, -1},
    {"correction_offsets", "opt(list(int))", NULL, "null", 0, 0, -1},
    {"error", "opt(str)", NULL, "null", 0, 0, -1},
    {"status", "opt(str)", NULL, "null", 0, 0, -1},
    {"recoverable", "opt(bool)", NULL, "null", 0, 0, -1},
    {"error_surface", "opt(dict(str,json))", NULL, "null", 0, 0, -1},
};
static const sess_field F_LiveSessionSnapshot[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"message_count", "int", NULL, NULL, 1, 0, -1},
    {"messages", "list(model)", NULL, NULL, 1, 0, -1},
    {"info", "model", NULL, NULL, 1, 0, -1},
    {"stored_session_id", "opt(str)", NULL, "null", 0, 0, -1},
    {"resumed", "opt(str)", NULL, "null", 0, 0, -1},
    {"session_key", "opt(str)", NULL, "null", 0, 0, -1},
    {"messages_omitted", "opt(bool)", NULL, "null", 0, 0, -1},
    {"hydrating", "opt(bool)", NULL, "null", 0, 0, -1},
    {"running", "opt(bool)", NULL, "null", 0, 0, -1},
    {"turn_started_at", "opt(float)", NULL, "null", 0, 0, -1},
    {"started_at", "opt(float)", NULL, "null", 0, 0, -1},
    {"status", "opt(str)", NULL, "null", 0, 0, -1},
    {"inflight", "opt(model)", NULL, "null", 0, 0, -1},
    {"queued", "opt(model)", NULL, "null", 0, 0, -1},
    {"pending_approval", "opt(model)", NULL, "null", 0, 0, -1},
    {"open_requests", "opt(list(model))", NULL, "null", 0, 0, -1},
    {"pending_connection", "opt(model)", NULL, "null", 0, 0, -1},
    {"todo_state", "opt(model)", NULL, "null", 0, 0, -1},
    {"auto_continue", "opt(model)", NULL, "null", 0, 0, -1},
};
static const sess_field F_LlmOneshotParams[] = {
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"template", "opt(str)", NULL, "null", 0, 0, -1},
    {"instructions", "opt(str)", NULL, "null", 0, 0, -1},
    {"input", "opt(str)", NULL, "null", 0, 0, -1},
    {"variables", "opt(dict(str,json))", NULL, "null", 0, 0, -1},
    {"task", "opt(str)", NULL, "null", 0, 0, -1},
    {"temperature", "opt(float)", NULL, "null", 0, 0, -1},
    {"max_tokens", "opt(int)", NULL, "null", 0, 0, -1},
    {"session_id", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_LlmOneshotResult[] = {
    {"text", "str", NULL, NULL, 1, 0, -1},
};
static const sess_field F_OpenRequestEntry[] = {
    {"id", "str", NULL, NULL, 1, 0, -1},
    {"method", "str", NULL, NULL, 1, 0, -1},
    {"params", "dict(str,json)", NULL, NULL, 1, 0, -1},
};
static const sess_field F_PromptCacheWindow[] = {
    {"guaranteed_until", "opt(float)", NULL, NULL, 1, 0, -1},
    {"estimated_until", "opt(float)", NULL, NULL, 1, 0, -1},
    {"tier", "opt(str)", NULL, NULL, 1, 0, -1},
    {"confidence", "str", NULL, NULL, 1, 0, -1},
    {"reason", "str", NULL, NULL, 1, 0, -1},
    {"refreshed_at", "opt(float)", NULL, "null", 0, 0, -1},
    {"coverage", "dict(str,float)", NULL, "{}", 0, 0, -1},
    {"warming_off", "bool", NULL, "false", 0, 0, -1},
    {"tier_pending", "bool", NULL, "false", 0, 0, -1},
    {"learned", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_QueuedPrompt[] = {
    {"user", "str", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SeedMessage[] = {
    {"role", "str", NULL, NULL, 1, 0, -1},
    {"content", "opt(str)", NULL, "null", 0, 0, -1},
    {"text", "opt(str)", NULL, "null", 0, 0, -1},
    {"display_kind", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionActivateParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"cols", "opt(int)", NULL, "null", 0, 0, -1},
    {"omit_messages", "bool", NULL, "false", 0, 0, -1},
};
static const sess_field F_SessionActivateResult[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"message_count", "int", NULL, NULL, 1, 0, -1},
    {"messages", "list(model)", NULL, NULL, 1, 0, -1},
    {"info", "model", NULL, NULL, 1, 0, -1},
    {"stored_session_id", "opt(str)", NULL, "null", 0, 0, -1},
    {"resumed", "opt(str)", NULL, "null", 0, 0, -1},
    {"session_key", "opt(str)", NULL, "null", 0, 0, -1},
    {"messages_omitted", "opt(bool)", NULL, "null", 0, 0, -1},
    {"hydrating", "opt(bool)", NULL, "null", 0, 0, -1},
    {"running", "opt(bool)", NULL, "null", 0, 0, -1},
    {"turn_started_at", "opt(float)", NULL, "null", 0, 0, -1},
    {"started_at", "opt(float)", NULL, "null", 0, 0, -1},
    {"status", "opt(str)", NULL, "null", 0, 0, -1},
    {"inflight", "opt(model)", NULL, "null", 0, 0, -1},
    {"queued", "opt(model)", NULL, "null", 0, 0, -1},
    {"pending_approval", "opt(model)", NULL, "null", 0, 0, -1},
    {"open_requests", "opt(list(model))", NULL, "null", 0, 0, -1},
    {"pending_connection", "opt(model)", NULL, "null", 0, 0, -1},
    {"todo_state", "opt(model)", NULL, "null", 0, 0, -1},
    {"auto_continue", "opt(model)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionActiveItem[] = {
    {"current", "bool", NULL, NULL, 1, 0, -1},
    {"id", "str", NULL, NULL, 1, 0, -1},
    {"last_active", "float", NULL, NULL, 1, 0, -1},
    {"message_count", "int", NULL, NULL, 1, 0, -1},
    {"model", "str", NULL, NULL, 1, 0, -1},
    {"preview", "str", NULL, NULL, 1, 0, -1},
    {"session_key", "str", NULL, NULL, 1, 0, -1},
    {"source", "opt(str)", NULL, "null", 0, 0, -1},
    {"started_at", "float", NULL, NULL, 1, 0, -1},
    {"status", "enum", NULL, NULL, 1, 0, -1},
    {"title", "str", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionActiveListParams[] = {
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"current_session_id", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionActiveListResult[] = {
    {"sessions", "list(model)", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionArchiveParams[] = {
    {"session_id", "opt(str)", NULL, "null", 0, 0, -1},
    {"session_key", "opt(str)", NULL, "null", 0, 0, -1},
    {"archived", "bool", NULL, "true", 0, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionArchiveResult[] = {
    {"archived", "bool", NULL, NULL, 1, 0, -1},
    {"session_key", "str", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionBranchParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"name", "opt(str)", NULL, "null", 0, 0, -1},
    {"count", "opt(int)", NULL, "null", 0, 0, -1},
    {"idempotency_key", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionBranchResult[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"stored_session_id", "str", NULL, NULL, 1, 0, -1},
    {"title", "str", NULL, NULL, 1, 0, -1},
    {"parent", "str", NULL, NULL, 1, 0, -1},
    {"message_count", "int", NULL, NULL, 1, 0, -1},
    {"messages", "list(model)", NULL, NULL, 1, 0, -1},
    {"info", "model", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionBranchStoredParams[] = {
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"parent_session_id", "str", NULL, NULL, 1, 0, 1},
    {"cols", "opt(int)", NULL, "null", 0, 0, -1},
    {"source", "opt(str)", NULL, "null", 0, 0, -1},
    {"cwd", "opt(str)", NULL, "null", 0, 0, -1},
    {"idempotency_key", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionBranchStoredResult[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"stored_session_id", "str", NULL, NULL, 1, 0, -1},
    {"message_count", "int", NULL, NULL, 1, 0, -1},
    {"messages_omitted", "bool", NULL, NULL, 1, 0, -1},
    {"info", "model", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionBranchWholeParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"name", "opt(str)", NULL, "null", 0, 0, -1},
    {"idempotency_key", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionBranchWholeResult[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"stored_session_id", "str", NULL, NULL, 1, 0, -1},
    {"title", "str", NULL, NULL, 1, 0, -1},
    {"parent", "str", NULL, NULL, 1, 0, -1},
    {"message_count", "int", NULL, NULL, 1, 0, -1},
    {"messages_omitted", "bool", NULL, NULL, 1, 0, -1},
    {"info", "model", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionCloseParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionCloseResult[] = {
    {"closed", "bool", NULL, NULL, 1, 0, -1},
    {"messages", "list(str)", NULL, NULL, 0, 1, -1},
};
static const sess_field F_SessionCompressParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"focus_topic", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionCompressResult[] = {
    {"status", "opt(str)", NULL, "null", 0, 0, -1},
    {"removed", "opt(int)", NULL, "null", 0, 0, -1},
    {"before_messages", "opt(int)", NULL, "null", 0, 0, -1},
    {"after_messages", "opt(int)", NULL, "null", 0, 0, -1},
    {"before_tokens", "opt(int)", NULL, "null", 0, 0, -1},
    {"after_tokens", "opt(int)", NULL, "null", 0, 0, -1},
    {"summary", "opt(model)", NULL, "null", 0, 0, -1},
    {"usage", "opt(model)", NULL, "null", 0, 0, -1},
    {"info", "opt(model)", NULL, "null", 0, 0, -1},
    {"messages", "opt(list(model))", NULL, "null", 0, 0, -1},
    {"compressed", "opt(bool)", NULL, "null", 0, 0, -1},
    {"lock_held", "opt(bool)", NULL, "null", 0, 0, -1},
    {"message", "opt(str)", NULL, "null", 0, 0, -1},
    {"turn_isolation", "opt(bool)", NULL, "null", 0, 0, -1},
    {"host_ack", "opt(dict(str,json))", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionContextBreakdownParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionContextBreakdownResult[] = {
    {"categories", "list(model)", NULL, NULL, 1, 0, -1},
    {"context_max", "int", NULL, NULL, 1, 0, -1},
    {"context_percent", "int", NULL, NULL, 1, 0, -1},
    {"context_used", "int", NULL, NULL, 1, 0, -1},
    {"estimated_total", "int", NULL, NULL, 1, 0, -1},
    {"context_estimated", "bool", NULL, NULL, 1, 0, -1},
    {"context_source", "str", NULL, NULL, 1, 0, -1},
    {"model", "str", NULL, NULL, 1, 0, -1},
    {"context_files", "list(model)", NULL, "[]", 0, 0, -1},
    {"prompt_cache", "opt(model)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionCorrectionParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"text", "str", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionCorrectionResult[] = {
    {"status", "enum", NULL, NULL, 1, 0, -1},
    {"text", "str", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionCreateParams[] = {
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"cols", "opt(int)", NULL, "null", 0, 0, -1},
    {"source", "opt(str)", NULL, "null", 0, 0, -1},
    {"cwd", "opt(str)", NULL, "null", 0, 0, -1},
    {"cwd_explicit", "opt(bool)", NULL, "null", 0, 0, -1},
    {"messages", "opt(list(model))", NULL, "null", 0, 0, -1},
    {"parent_session_id", "opt(str)", NULL, "null", 0, 0, -1},
    {"title", "opt(str)", NULL, "null", 0, 0, -1},
    {"model", "opt(str)", NULL, "null", 0, 0, -1},
    {"provider", "opt(str)", NULL, "null", 0, 0, -1},
    {"reasoning_effort", "opt(str)", NULL, "null", 0, 0, -1},
    {"fast", "opt(bool)", NULL, "null", 0, 0, -1},
    {"service_tier", "opt(str)", NULL, "null", 0, 0, -1},
    {"close_on_disconnect", "bool", NULL, "false", 0, 0, -1},
    {"hidden", "bool", NULL, "false", 0, 0, -1},
    {"room_plumbing", "bool", NULL, "false", 0, 0, -1},
    {"follow_profile_config", "bool", NULL, "false", 0, 0, -1},
    {"idempotency_key", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionCreateResult[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"stored_session_id", "str", NULL, NULL, 1, 0, -1},
    {"message_count", "int", NULL, NULL, 1, 0, -1},
    {"messages", "list(model)", NULL, NULL, 1, 0, -1},
    {"info", "model", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionCwdSetParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"cwd", "str", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionCwdSetResult[] = {
    {"model", "str", NULL, "\"\"", 0, 0, -1},
    {"provider", "str", NULL, "\"\"", 0, 0, -1},
    {"reasoning_effort", "str", NULL, "\"\"", 0, 0, -1},
    {"reasoning_effort_wire", "str", NULL, "\"\"", 0, 0, -1},
    {"service_tier", "str", NULL, "\"\"", 0, 0, -1},
    {"fast", "bool", NULL, "false", 0, 0, -1},
    {"yolo", "bool", NULL, "false", 0, 0, -1},
    {"approval_mode", "str", NULL, "\"manual\"", 0, 0, -1},
    {"tools", "dict(str,list(str))", NULL, NULL, 0, 1, -1},
    {"skills", "dict(str,list(str))", NULL, NULL, 0, 1, -1},
    {"cwd", "str", NULL, "\"\"", 0, 0, -1},
    {"branch", "opt(str)", NULL, "null", 0, 0, -1},
    {"project", "opt(model)", NULL, "null", 0, 0, -1},
    {"terminal_backend", "str", NULL, "\"\"", 0, 0, -1},
    {"personality", "str", NULL, "\"\"", 0, 0, -1},
    {"running", "bool", NULL, "false", 0, 0, -1},
    {"turn_started_at", "opt(float)", NULL, "null", 0, 0, -1},
    {"title", "str", NULL, "\"\"", 0, 0, -1},
    {"stored_session_id", "str", NULL, "\"\"", 0, 0, -1},
    {"desktop_contract", "opt(union(int,str))", NULL, "null", 0, 0, -1},
    {"version", "str", NULL, "\"\"", 0, 0, -1},
    {"release_date", "str", NULL, "\"\"", 0, 0, -1},
    {"update_behind", "opt(json)", NULL, "null", 0, 0, -1},
    {"update_command", "str", NULL, "\"\"", 0, 0, -1},
    {"usage", "opt(model)", NULL, "null", 0, 0, -1},
    {"profile_name", "opt(str)", NULL, "null", 0, 0, -1},
    {"mcp_servers", "list(model)", NULL, NULL, 0, 1, -1},
    {"system_prompt", "opt(str)", NULL, "null", 0, 0, -1},
    {"credential_warning", "opt(str)", NULL, "null", 0, 0, -1},
    {"lazy", "opt(bool)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionDeleteParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionDeleteResult[] = {
    {"deleted", "str", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionEventsSinceParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"last_seen", "opt(int)", NULL, "null", 0, 0, -1},
    {"replay_epoch", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionEventsSinceResult[] = {
    {"events", "list(dict(str,json))", NULL, NULL, 1, 0, -1},
    {"latest_seq", "int", NULL, NULL, 1, 0, -1},
    {"truncated", "bool", NULL, NULL, 1, 0, -1},
    {"count", "int", NULL, NULL, 1, 0, -1},
    {"epoch", "str", NULL, NULL, 1, 0, -1},
    {"open_requests", "list(model)", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionEventsStatsParams[] = {
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionEventsStatsResult[] = {
    {"sessions", "int", NULL, NULL, 1, 0, -1},
    {"events", "int", NULL, NULL, 1, 0, -1},
    {"bytes", "int", NULL, NULL, 1, 0, -1},
    {"max_per_session", "int", NULL, NULL, 1, 0, -1},
    {"max_bytes_per_session", "int", NULL, NULL, 1, 0, -1},
    {"max_bytes_process", "int", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionHistoryParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionHistoryResult[] = {
    {"count", "int", NULL, NULL, 1, 0, -1},
    {"messages", "list(model)", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionInterruptParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"expected_hosted_task_id", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionInterruptResult[] = {
    {"status", "enum", NULL, NULL, 1, 0, -1},
    {"interrupted", "opt(bool)", NULL, "null", 0, 0, -1},
    {"turn_isolation", "opt(bool)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionListParams[] = {
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"title", "opt(str)", NULL, "null", 0, 0, -1},
    {"limit", "opt(int)", NULL, "null", 0, 0, -1},
    {"include_hidden", "bool", NULL, "false", 0, 0, -1},
};
static const sess_field F_SessionListResult[] = {
    {"sessions", "list(model)", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionListRow[] = {
    {"id", "str", NULL, NULL, 1, 0, -1},
    {"resolved_id", "opt(str)", NULL, "null", 0, 0, -1},
    {"title", "str", NULL, "\"\"", 0, 0, -1},
    {"preview", "str", NULL, "\"\"", 0, 0, -1},
    {"started_at", "float", NULL, "0", 0, 0, -1},
    {"message_count", "int", NULL, "0", 0, 0, -1},
    {"live_message_count", "opt(int)", NULL, "null", 0, 0, -1},
    {"source", "str", NULL, "\"\"", 0, 0, -1},
    {"lineage_root_id", "opt(str)", "_lineage_root_id", "null", 0, 0, -1},
};
static const sess_field F_SessionMostRecentParams[] = {
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionMostRecentResult[] = {
    {"session_id", "opt(str)", NULL, NULL, 1, 0, -1},
    {"title", "opt(str)", NULL, "null", 0, 0, -1},
    {"started_at", "opt(float)", NULL, "null", 0, 0, -1},
    {"source", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionResumeParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"cols", "opt(int)", NULL, "null", 0, 0, -1},
    {"source", "opt(str)", NULL, "null", 0, 0, -1},
    {"lazy", "bool", NULL, "false", 0, 0, -1},
    {"defer_history", "bool", NULL, "false", 0, 0, -1},
    {"omit_messages", "bool", NULL, "false", 0, 0, -1},
    {"eager_build", "bool", NULL, "false", 0, 0, -1},
    {"close_on_disconnect", "bool", NULL, "false", 0, 0, -1},
    {"inline_images", "bool", NULL, "true", 0, 0, -1},
};
static const sess_field F_SessionResumeResult[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"message_count", "int", NULL, NULL, 1, 0, -1},
    {"messages", "list(model)", NULL, NULL, 1, 0, -1},
    {"info", "model", NULL, NULL, 1, 0, -1},
    {"stored_session_id", "opt(str)", NULL, "null", 0, 0, -1},
    {"resumed", "opt(str)", NULL, "null", 0, 0, -1},
    {"session_key", "opt(str)", NULL, "null", 0, 0, -1},
    {"messages_omitted", "opt(bool)", NULL, "null", 0, 0, -1},
    {"hydrating", "opt(bool)", NULL, "null", 0, 0, -1},
    {"running", "opt(bool)", NULL, "null", 0, 0, -1},
    {"turn_started_at", "opt(float)", NULL, "null", 0, 0, -1},
    {"started_at", "opt(float)", NULL, "null", 0, 0, -1},
    {"status", "opt(str)", NULL, "null", 0, 0, -1},
    {"inflight", "opt(model)", NULL, "null", 0, 0, -1},
    {"queued", "opt(model)", NULL, "null", 0, 0, -1},
    {"pending_approval", "opt(model)", NULL, "null", 0, 0, -1},
    {"open_requests", "opt(list(model))", NULL, "null", 0, 0, -1},
    {"pending_connection", "opt(model)", NULL, "null", 0, 0, -1},
    {"todo_state", "opt(model)", NULL, "null", 0, 0, -1},
    {"auto_continue", "opt(model)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionSaveParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionSaveResult[] = {
    {"file", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionSetHiddenParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"hidden", "bool", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionSetHiddenResult[] = {
    {"hidden", "bool", NULL, NULL, 1, 0, -1},
    {"session_key", "str", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionStatusParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionStatusResult[] = {
    {"output", "str", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionTitleParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"title", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionTitleResult[] = {
    {"title", "str", NULL, NULL, 1, 0, -1},
    {"session_key", "opt(str)", NULL, "null", 0, 0, -1},
    {"pending", "opt(bool)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionUndoParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"intent", "opt(enum)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionUndoResult[] = {
    {"removed", "int", NULL, NULL, 1, 0, -1},
    {"undo_hint", "opt(model)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionUsageParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionUsageResult[] = {
    {"model", "str", NULL, "\"\"", 0, 0, -1},
    {"input", "int", NULL, "0", 0, 0, -1},
    {"output", "int", NULL, "0", 0, 0, -1},
    {"reasoning", "int", NULL, "0", 0, 0, -1},
    {"prompt", "int", NULL, "0", 0, 0, -1},
    {"completion", "int", NULL, "0", 0, 0, -1},
    {"total", "int", NULL, "0", 0, 0, -1},
    {"calls", "int", NULL, "0", 0, 0, -1},
    {"compressions", "opt(int)", NULL, "null", 0, 0, -1},
    {"context_used", "opt(int)", NULL, "null", 0, 0, -1},
    {"context_max", "opt(int)", NULL, "null", 0, 0, -1},
    {"context_percent", "opt(int)", NULL, "null", 0, 0, -1},
    {"context_source", "opt(str)", NULL, "null", 0, 0, -1},
    {"context_estimated", "opt(bool)", NULL, "null", 0, 0, -1},
    {"cache_hit_pct", "opt(int)", NULL, "null", 0, 0, -1},
    {"cache_read", "opt(int)", NULL, "null", 0, 0, -1},
    {"cache_write", "opt(int)", NULL, "null", 0, 0, -1},
    {"avg_latency_s", "opt(float)", NULL, "null", 0, 0, -1},
    {"avg_tps", "opt(float)", NULL, "null", 0, 0, -1},
    {"active_subagents", "opt(int)", NULL, "null", 0, 0, -1},
    {"dev_credits_spent_micros", "opt(int)", NULL, "null", 0, 0, -1},
    {"cost_usd", "opt(float)", NULL, "null", 0, 0, -1},
    {"cost_status", "opt(str)", NULL, "null", 0, 0, -1},
    {"plan_gauge", "opt(model)", NULL, "null", 0, 0, -1},
    {"credits_lines", "opt(list(str))", NULL, "null", 0, 0, -1},
    {"credits_usage_window", "opt(bool)", NULL, "null", 0, 0, -1},
    {"nous_route", "opt(bool)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SessionWorkspaceMoveParams[] = {
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"session_key", "str", NULL, NULL, 1, 0, -1},
    {"cwd", "str", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SessionWorkspaceMoveResult[] = {
    {"cwd", "str", NULL, NULL, 1, 0, -1},
    {"branch", "opt(str)", NULL, "null", 0, 0, -1},
    {"git_repo_root", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SpawnTreeEntry[] = {
    {"path", "str", NULL, NULL, 1, 0, -1},
    {"session_id", "opt(str)", NULL, "null", 0, 0, -1},
    {"started_at", "opt(float)", NULL, "null", 0, 0, -1},
    {"finished_at", "opt(float)", NULL, "null", 0, 0, -1},
    {"label", "str", NULL, "\"\"", 0, 0, -1},
    {"count", "int", NULL, "0", 0, 0, -1},
};
static const sess_field F_SpawnTreeListParams[] = {
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"session_id", "opt(str)", NULL, "null", 0, 0, -1},
    {"cross_session", "bool", NULL, "false", 0, 0, -1},
    {"limit", "opt(int)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SpawnTreeListResult[] = {
    {"entries", "list(model)", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SpawnTreeLoadParams[] = {
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"path", "str", NULL, NULL, 1, 0, -1},
};
static const sess_field F_SpawnTreeLoadResult[] = {
    {"session_id", "opt(str)", NULL, "null", 0, 0, -1},
    {"started_at", "opt(float)", NULL, "null", 0, 0, -1},
    {"finished_at", "opt(float)", NULL, "null", 0, 0, -1},
    {"label", "opt(str)", NULL, "null", 0, 0, -1},
    {"subagents", "list(dict(str,json))", NULL, NULL, 0, 1, -1},
};
static const sess_field F_SpawnTreeSaveParams[] = {
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"subagents", "list(dict(str,json))", NULL, NULL, 1, 0, -1},
    {"session_id", "opt(str)", NULL, "null", 0, 0, -1},
    {"started_at", "opt(float)", NULL, "null", 0, 0, -1},
    {"finished_at", "opt(float)", NULL, "null", 0, 0, -1},
    {"label", "opt(str)", NULL, "null", 0, 0, -1},
};
static const sess_field F_SpawnTreeSaveResult[] = {
    {"path", "str", NULL, NULL, 1, 0, -1},
    {"session_id", "str", NULL, NULL, 1, 0, -1},
};
static const sess_field F_TerminalResizeParams[] = {
    {"session_id", "str", NULL, NULL, 1, 0, -1},
    {"profile", "opt(str)", NULL, "null", 0, 0, -1},
    {"cols", "opt(int)", NULL, "null", 0, 0, -1},
};
static const sess_field F_TerminalResizeResult[] = {
    {"cols", "int", NULL, NULL, 1, 0, -1},
};
static const sess_field F_TodoState[] = {
    {"todos", "list(dict(str,json))", NULL, NULL, 1, 0, -1},
    {"revision", "int", NULL, NULL, 1, 0, -1},
};

static const sess_model SESS_MODELS[] = {
    {"AutoContinue", 0, 0, F_AutoContinue, SESS_N(F_AutoContinue)},
    {"CompressionSummary", 1, 0, F_CompressionSummary, SESS_N(F_CompressionSummary)},
    {"ContextCategory", 0, 0, F_ContextCategory, SESS_N(F_ContextCategory)},
    {"ContextFileSource", 0, 0, F_ContextFileSource, SESS_N(F_ContextFileSource)},
    {"InflightTurn", 0, 0, F_InflightTurn, SESS_N(F_InflightTurn)},
    {"LiveSessionSnapshot", 0, 0, F_LiveSessionSnapshot, SESS_N(F_LiveSessionSnapshot)},
    {"LlmOneshotParams", 0, 1, F_LlmOneshotParams, SESS_N(F_LlmOneshotParams)},
    {"LlmOneshotResult", 0, 0, F_LlmOneshotResult, SESS_N(F_LlmOneshotResult)},
    {"OpenRequestEntry", 0, 0, F_OpenRequestEntry, SESS_N(F_OpenRequestEntry)},
    {"PromptCacheWindow", 0, 0, F_PromptCacheWindow, SESS_N(F_PromptCacheWindow)},
    {"QueuedPrompt", 0, 0, F_QueuedPrompt, SESS_N(F_QueuedPrompt)},
    {"SeedMessage", 1, 1, F_SeedMessage, SESS_N(F_SeedMessage)},
    {"SessionActivateParams", 0, 1, F_SessionActivateParams, SESS_N(F_SessionActivateParams)},
    {"SessionActivateResult", 0, 0, F_SessionActivateResult, SESS_N(F_SessionActivateResult)},
    {"SessionActiveItem", 0, 0, F_SessionActiveItem, SESS_N(F_SessionActiveItem)},
    {"SessionActiveListParams", 0, 1, F_SessionActiveListParams, SESS_N(F_SessionActiveListParams)},
    {"SessionActiveListResult", 0, 0, F_SessionActiveListResult, SESS_N(F_SessionActiveListResult)},
    {"SessionArchiveParams", 0, 1, F_SessionArchiveParams, SESS_N(F_SessionArchiveParams)},
    {"SessionArchiveResult", 0, 0, F_SessionArchiveResult, SESS_N(F_SessionArchiveResult)},
    {"SessionBranchParams", 0, 1, F_SessionBranchParams, SESS_N(F_SessionBranchParams)},
    {"SessionBranchResult", 0, 0, F_SessionBranchResult, SESS_N(F_SessionBranchResult)},
    {"SessionBranchStoredParams", 0, 1, F_SessionBranchStoredParams, SESS_N(F_SessionBranchStoredParams)},
    {"SessionBranchStoredResult", 0, 0, F_SessionBranchStoredResult, SESS_N(F_SessionBranchStoredResult)},
    {"SessionBranchWholeParams", 0, 1, F_SessionBranchWholeParams, SESS_N(F_SessionBranchWholeParams)},
    {"SessionBranchWholeResult", 0, 0, F_SessionBranchWholeResult, SESS_N(F_SessionBranchWholeResult)},
    {"SessionCloseParams", 0, 1, F_SessionCloseParams, SESS_N(F_SessionCloseParams)},
    {"SessionCloseResult", 0, 0, F_SessionCloseResult, SESS_N(F_SessionCloseResult)},
    {"SessionCompressParams", 0, 1, F_SessionCompressParams, SESS_N(F_SessionCompressParams)},
    {"SessionCompressResult", 1, 0, F_SessionCompressResult, SESS_N(F_SessionCompressResult)},
    {"SessionContextBreakdownParams", 0, 1, F_SessionContextBreakdownParams, SESS_N(F_SessionContextBreakdownParams)},
    {"SessionContextBreakdownResult", 0, 0, F_SessionContextBreakdownResult, SESS_N(F_SessionContextBreakdownResult)},
    {"SessionCorrectionParams", 0, 1, F_SessionCorrectionParams, SESS_N(F_SessionCorrectionParams)},
    {"SessionCorrectionResult", 0, 0, F_SessionCorrectionResult, SESS_N(F_SessionCorrectionResult)},
    {"SessionCreateParams", 0, 1, F_SessionCreateParams, SESS_N(F_SessionCreateParams)},
    {"SessionCreateResult", 0, 0, F_SessionCreateResult, SESS_N(F_SessionCreateResult)},
    {"SessionCwdSetParams", 0, 1, F_SessionCwdSetParams, SESS_N(F_SessionCwdSetParams)},
    {"SessionCwdSetResult", 1, 0, F_SessionCwdSetResult, SESS_N(F_SessionCwdSetResult)},
    {"SessionDeleteParams", 0, 1, F_SessionDeleteParams, SESS_N(F_SessionDeleteParams)},
    {"SessionDeleteResult", 0, 0, F_SessionDeleteResult, SESS_N(F_SessionDeleteResult)},
    {"SessionEventsSinceParams", 0, 1, F_SessionEventsSinceParams, SESS_N(F_SessionEventsSinceParams)},
    {"SessionEventsSinceResult", 0, 0, F_SessionEventsSinceResult, SESS_N(F_SessionEventsSinceResult)},
    {"SessionEventsStatsParams", 0, 1, F_SessionEventsStatsParams, SESS_N(F_SessionEventsStatsParams)},
    {"SessionEventsStatsResult", 0, 0, F_SessionEventsStatsResult, SESS_N(F_SessionEventsStatsResult)},
    {"SessionHistoryParams", 0, 1, F_SessionHistoryParams, SESS_N(F_SessionHistoryParams)},
    {"SessionHistoryResult", 0, 0, F_SessionHistoryResult, SESS_N(F_SessionHistoryResult)},
    {"SessionInterruptParams", 0, 1, F_SessionInterruptParams, SESS_N(F_SessionInterruptParams)},
    {"SessionInterruptResult", 0, 0, F_SessionInterruptResult, SESS_N(F_SessionInterruptResult)},
    {"SessionListParams", 0, 1, F_SessionListParams, SESS_N(F_SessionListParams)},
    {"SessionListResult", 0, 0, F_SessionListResult, SESS_N(F_SessionListResult)},
    {"SessionListRow", 0, 0, F_SessionListRow, SESS_N(F_SessionListRow)},
    {"SessionMostRecentParams", 0, 1, F_SessionMostRecentParams, SESS_N(F_SessionMostRecentParams)},
    {"SessionMostRecentResult", 0, 0, F_SessionMostRecentResult, SESS_N(F_SessionMostRecentResult)},
    {"SessionResumeParams", 0, 1, F_SessionResumeParams, SESS_N(F_SessionResumeParams)},
    {"SessionResumeResult", 0, 0, F_SessionResumeResult, SESS_N(F_SessionResumeResult)},
    {"SessionSaveParams", 0, 1, F_SessionSaveParams, SESS_N(F_SessionSaveParams)},
    {"SessionSaveResult", 1, 0, F_SessionSaveResult, SESS_N(F_SessionSaveResult)},
    {"SessionSetHiddenParams", 0, 1, F_SessionSetHiddenParams, SESS_N(F_SessionSetHiddenParams)},
    {"SessionSetHiddenResult", 0, 0, F_SessionSetHiddenResult, SESS_N(F_SessionSetHiddenResult)},
    {"SessionStatusParams", 0, 1, F_SessionStatusParams, SESS_N(F_SessionStatusParams)},
    {"SessionStatusResult", 0, 0, F_SessionStatusResult, SESS_N(F_SessionStatusResult)},
    {"SessionTitleParams", 0, 1, F_SessionTitleParams, SESS_N(F_SessionTitleParams)},
    {"SessionTitleResult", 0, 0, F_SessionTitleResult, SESS_N(F_SessionTitleResult)},
    {"SessionUndoParams", 0, 1, F_SessionUndoParams, SESS_N(F_SessionUndoParams)},
    {"SessionUndoResult", 0, 0, F_SessionUndoResult, SESS_N(F_SessionUndoResult)},
    {"SessionUsageParams", 0, 1, F_SessionUsageParams, SESS_N(F_SessionUsageParams)},
    {"SessionUsageResult", 1, 0, F_SessionUsageResult, SESS_N(F_SessionUsageResult)},
    {"SessionWorkspaceMoveParams", 0, 1, F_SessionWorkspaceMoveParams, SESS_N(F_SessionWorkspaceMoveParams)},
    {"SessionWorkspaceMoveResult", 0, 0, F_SessionWorkspaceMoveResult, SESS_N(F_SessionWorkspaceMoveResult)},
    {"SpawnTreeEntry", 1, 0, F_SpawnTreeEntry, SESS_N(F_SpawnTreeEntry)},
    {"SpawnTreeListParams", 0, 1, F_SpawnTreeListParams, SESS_N(F_SpawnTreeListParams)},
    {"SpawnTreeListResult", 0, 0, F_SpawnTreeListResult, SESS_N(F_SpawnTreeListResult)},
    {"SpawnTreeLoadParams", 0, 1, F_SpawnTreeLoadParams, SESS_N(F_SpawnTreeLoadParams)},
    {"SpawnTreeLoadResult", 1, 0, F_SpawnTreeLoadResult, SESS_N(F_SpawnTreeLoadResult)},
    {"SpawnTreeSaveParams", 0, 1, F_SpawnTreeSaveParams, SESS_N(F_SpawnTreeSaveParams)},
    {"SpawnTreeSaveResult", 0, 0, F_SpawnTreeSaveResult, SESS_N(F_SpawnTreeSaveResult)},
    {"TerminalResizeParams", 0, 1, F_TerminalResizeParams, SESS_N(F_TerminalResizeParams)},
    {"TerminalResizeResult", 0, 0, F_TerminalResizeResult, SESS_N(F_TerminalResizeResult)},
    {"TodoState", 0, 0, F_TodoState, SESS_N(F_TodoState)},
};

static const sess_method SESS_METHODS[] = {
    {"llm.oneshot", "LlmOneshotParams", "LlmOneshotResult"},
    {"session.activate", "SessionActivateParams", "SessionActivateResult"},
    {"session.active_list", "SessionActiveListParams", "SessionActiveListResult"},
    {"session.archive", "SessionArchiveParams", "SessionArchiveResult"},
    {"session.branch", "SessionBranchParams", "SessionBranchResult"},
    {"session.branch_stored", "SessionBranchStoredParams", "SessionBranchStoredResult"},
    {"session.branch_whole", "SessionBranchWholeParams", "SessionBranchWholeResult"},
    {"session.close", "SessionCloseParams", "SessionCloseResult"},
    {"session.compress", "SessionCompressParams", "SessionCompressResult"},
    {"session.context_breakdown", "SessionContextBreakdownParams", "SessionContextBreakdownResult"},
    {"session.create", "SessionCreateParams", "SessionCreateResult"},
    {"session.cwd.set", "SessionCwdSetParams", "SessionCwdSetResult"},
    {"session.delete", "SessionDeleteParams", "SessionDeleteResult"},
    {"session.events.since", "SessionEventsSinceParams", "SessionEventsSinceResult"},
    {"session.events.stats", "SessionEventsStatsParams", "SessionEventsStatsResult"},
    {"session.history", "SessionHistoryParams", "SessionHistoryResult"},
    {"session.interrupt", "SessionInterruptParams", "SessionInterruptResult"},
    {"session.list", "SessionListParams", "SessionListResult"},
    {"session.most_recent", "SessionMostRecentParams", "SessionMostRecentResult"},
    {"session.redirect", "SessionCorrectionParams", "SessionCorrectionResult"},
    {"session.resume", "SessionResumeParams", "SessionResumeResult"},
    {"session.save", "SessionSaveParams", "SessionSaveResult"},
    {"session.set_hidden", "SessionSetHiddenParams", "SessionSetHiddenResult"},
    {"session.status", "SessionStatusParams", "SessionStatusResult"},
    {"session.steer", "SessionCorrectionParams", "SessionCorrectionResult"},
    {"session.title", "SessionTitleParams", "SessionTitleResult"},
    {"session.undo", "SessionUndoParams", "SessionUndoResult"},
    {"session.usage", "SessionUsageParams", "SessionUsageResult"},
    {"session.workspace.move", "SessionWorkspaceMoveParams", "SessionWorkspaceMoveResult"},
    {"spawn_tree.list", "SpawnTreeListParams", "SpawnTreeListResult"},
    {"spawn_tree.load", "SpawnTreeLoadParams", "SpawnTreeLoadResult"},
    {"spawn_tree.save", "SpawnTreeSaveParams", "SpawnTreeSaveResult"},
    {"terminal.resize", "TerminalResizeParams", "TerminalResizeResult"},
};

static const char *const E_CorrectionStatus[] = {
    "queued", "redirected", "rejected",
};

static const char *const E_InterruptStatus[] = {
    "interrupted", "not_interrupted",
};

static const char *const E_LiveSessionStatus[] = {
    "idle", "resuming", "starting", "streaming", "waiting", "working",
};

static const char *const E_UndoIntent[] = {
    "retry", "undo",
};

static const sess_enum SESS_ENUMS[] = {
    {"CorrectionStatus", E_CorrectionStatus, SESS_N(E_CorrectionStatus)},
    {"InterruptStatus", E_InterruptStatus, SESS_N(E_InterruptStatus)},
    {"LiveSessionStatus", E_LiveSessionStatus, SESS_N(E_LiveSessionStatus)},
    {"UndoIntent", E_UndoIntent, SESS_N(E_UndoIntent)},
};

/* ── lookups (bounded, table only) ─────────────────────────────────────────── */

static const sess_model *sess_model_find(const char *name) {
    size_t i;
    if (!name) return NULL;
    for (i = 0; i < SESS_N(SESS_MODELS); i++)
        if (SESS_MODELS[i].name && strcmp(SESS_MODELS[i].name, name) == 0) return &SESS_MODELS[i];
    return NULL;
}

static const sess_method *sess_method_find(const char *name) {
    size_t i;
    if (!name) return NULL;
    for (i = 0; i < SESS_N(SESS_METHODS); i++)
        if (SESS_METHODS[i].method && strcmp(SESS_METHODS[i].method, name) == 0) return &SESS_METHODS[i];
    return NULL;
}

static const sess_enum *sess_enum_find(const char *name) {
    size_t i;
    if (!name) return NULL;
    for (i = 0; i < SESS_N(SESS_ENUMS); i++)
        if (SESS_ENUMS[i].name && strcmp(SESS_ENUMS[i].name, name) == 0) return &SESS_ENUMS[i];
    return NULL;
}

/* The params model a method validates against, or NULL. */
static const sess_model *sess_params_model(const char *method) {
    const sess_method *m = sess_method_find(method);
    return m ? sess_model_find(m->params) : NULL;
}

/* Is `key` the accepted spelling of this field?  A field's own name is accepted
 * unless the model has an alias and does not populate by name. */
static int sess_field_accepts(const sess_model *m, const sess_field *f, const char *key) {
    if (!f->name || !key) return 0;
    if (strcmp(key, f->name) == 0 && (!f->alias || m->populate_by_name == 1)) return 1;
    return f->alias != NULL && strcmp(key, f->alias) == 0;
}

static const sess_field *sess_field_find(const sess_model *m, const char *key) {
    size_t i;
    for (i = 0; i < m->nfields; i++)
        if (sess_field_accepts(m, &m->fields[i], key)) return &m->fields[i];
    return NULL;
}

/* The accepted input spellings of a model, sorted (both spellings when the
 * model populates by name next to an alias). */
static json_t *sess_keys(const sess_model *m) {
    json_t *arr = json_array();
    char **keys;
    size_t i, n = 0, cap = m->nfields ? m->nfields * 2 : 1;

    if (!arr) return json_null();
    keys = (char **)calloc(cap, sizeof(char *));
    if (!keys) return arr;
    for (i = 0; i < m->nfields; i++) {
        const sess_field *f = &m->fields[i];
        if (f->name && (!f->alias || m->populate_by_name == 1) && n < cap) keys[n++] = (char *)f->name;
        if (f->alias && n < cap) keys[n++] = (char *)f->alias;
    }
    qsort(keys, n, sizeof(char *), sess_cmp_str);
    for (i = 0; i < n; i++) JSON_APPEND_STR(arr, keys[i]);
    free(keys);
    return arr;
}

/* ── catalog / models ──────────────────────────────────────────────────────── */

json_t *w34_sessions_67685b_catalog(void) {
    json_t *arr = json_array();
    size_t i;
    for (i = 0; i < SESS_N(SESS_METHODS); i++) {
        json_t *row = json_object();
        json_set(row, "method", json_string(SESS_METHODS[i].method));
        json_set(row, "params", json_string(SESS_METHODS[i].params));
        json_set(row, "result", json_string(SESS_METHODS[i].result));
        json_append(arr, row);
    }
    return arr;
}

json_t *w34_sessions_67685b_models(void) {
    json_t *arr = json_array();
    size_t i;
    for (i = 0; i < SESS_N(SESS_MODELS); i++) JSON_APPEND_STR(arr, SESS_MODELS[i].name);
    return arr;
}

/* ── one model's wire shape ────────────────────────────────────────────────── */

json_t *w34_sessions_67685b_fields(const char *model) {
    const sess_model *m = sess_model_find(model);
    json_t *out, *fields;
    size_t i;

    if (!m) return json_null();
    out = json_object();
    fields = json_array();
    json_set(out, "model", json_string(m->name));
    json_set(out, "extra", json_string(m->extra_allow ? "allow" : "forbid"));
    json_set(out, "populate_by_name", json_bool(m->populate_by_name != 0));
    json_set(out, "keys", sess_keys(m));
    for (i = 0; i < m->nfields; i++) {
        const sess_field *f = &m->fields[i];
        json_t *row = json_object();
        json_set(row, "name", json_string(f->name));
        json_set(row, "kind", json_string(f->kind));
        json_set(row, "required", json_bool(f->required != 0));
        json_set(row, "factory", json_bool(f->factory != 0));
        json_set(row, "alias", f->alias ? json_string(f->alias) : json_null());
        json_set(row, "min_length", f->min_length >= 0 ? json_number(f->min_length) : json_null());
        /* A value default is carried as its JSON literal, parsed back so the
         * caller sees a value and not a string ("false", not "\"false\""). */
        if (f->def) {
            json_t *d = json_parse(f->def, NULL);
            json_set(row, "default", d ? d : json_null());
        }
        json_append(fields, row);
    }
    json_set(out, "fields", fields);
    return out;
}

json_t *w34_sessions_67685b_required(const char *model) {
    const sess_model *m = sess_model_find(model);
    json_t *arr, *names;
    char **buf;
    size_t i, n = 0;

    if (!m) return json_null();
    arr = json_array();
    buf = (char **)calloc(m->nfields ? m->nfields : 1, sizeof(char *));
    if (!buf) return arr;
    for (i = 0; i < m->nfields; i++)
        if (m->fields[i].required) buf[n++] = (char *)m->fields[i].name;
    qsort(buf, n, sizeof(char *), sess_cmp_str);
    names = arr;
    for (i = 0; i < n; i++) JSON_APPEND_STR(names, buf[i]);
    free(buf);
    return arr;
}

/* ── the params envelope ───────────────────────────────────────────────────── */

/* The one text the gateway sends for a params object carrying a key the
 * contract does not declare (registry.validate_params). */
static json_t *sess_extra_error(const char *method, const char *key) {
    static const char *const TAIL =
        ": Extra inputs are not permitted \xe2\x80\x94 the client and the Hermes backend are out of "
        "sync (different versions); run `hermes update` and restart both";
    size_t need = strlen("invalid params for ") + strlen(method) + strlen(key) + 2 + strlen(TAIL) + 1;
    char *buf = (char *)malloc(need);
    json_t *out;
    if (!buf) return json_null();
    snprintf(buf, need, "invalid params for %s: %s%s", method, key, TAIL);
    out = json_string(buf);
    free(buf);
    return out;
}

json_t *w34_sessions_67685b_validate(const char *method, const json_t *params) {
    const sess_model *m = sess_params_model(method);
    json_t *out = json_object(), *unknown = json_array(), *missing = json_array();
    unsigned char *seen;
    size_t i;

    json_set(out, "found", json_bool(m != NULL));
    json_set(out, "unknown_keys", unknown);
    json_set(out, "missing_keys", missing);
    json_set(out, "error", json_null());
    json_set(out, "shape_ok", json_bool(0));
    if (!m) return out;

    seen = (unsigned char *)calloc(m->nfields ? m->nfields : 1, 1);
    if (!seen) return out;

    if (params && params->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(params); i++) {
            const char *k = json_object_get_key_at(params, i);
            const sess_field *f = sess_field_find(m, k);
            if (!f) JSON_APPEND_STR(unknown, k ? k : "");
            else seen[(size_t)(f - m->fields)] = 1;
        }
    }
    for (i = 0; i < m->nfields; i++)
        if (m->fields[i].required && !seen[i]) JSON_APPEND_STR(missing, m->fields[i].name);

    if (json_len(unknown) > 0) {
        json_t *first = json_get(unknown, 0);
        json_set(out, "error", sess_extra_error(method, first && first->str_val ? first->str_val : ""));
    }
    json_set(out, "shape_ok",
             json_bool(json_len(unknown) == 0 && json_len(missing) == 0));
    free(seen);
    return out;
}

/* ── session identity / closed sets ────────────────────────────────────────── */

json_t *w34_sessions_67685b_id_requirement(const char *method) {
    const sess_model *m = sess_params_model(method);
    const sess_field *sid, *key;
    json_t *out;

    if (!m) return json_null();
    sid = NULL;
    key = NULL;
    for (size_t i = 0; i < m->nfields; i++) {
        if (m->fields[i].name && strcmp(m->fields[i].name, "session_id") == 0) sid = &m->fields[i];
        if (m->fields[i].name && strcmp(m->fields[i].name, "session_key") == 0) key = &m->fields[i];
    }
    out = json_object();
    json_set(out, "model", json_string(m->name));
    json_set(out, "has_session_id", json_bool(sid != NULL));
    json_set(out, "session_id_required", json_bool(sid && sid->required));
    json_set(out, "has_session_key", json_bool(key != NULL));
    if (sid && sid->required) json_set(out, "requirement", json_string("session_id"));
    else if (sid && key) json_set(out, "requirement", json_string("either"));
    else if (sid) json_set(out, "requirement", json_string("optional"));
    else json_set(out, "requirement", json_string("none"));
    return out;
}

json_t *w34_sessions_67685b_enum(const char *name) {
    const sess_enum *e = sess_enum_find(name);
    json_t *arr;
    size_t i;
    if (!e) return json_null();
    arr = json_array();
    for (i = 0; i < e->nvalues; i++) JSON_APPEND_STR(arr, e->values[i]);
    return arr;
}

json_t *w34_sessions_67685b_accepts(const char *method, const char *key) {
    const sess_model *m = sess_params_model(method);
    if (!m) return json_null();
    return json_bool(sess_field_find(m, key) != NULL);
}
