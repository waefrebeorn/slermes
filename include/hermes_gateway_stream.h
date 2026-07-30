/**
 * @file hermes_gateway_stream.h
 * @brief Stream event model for gateway dispatch.
 *
 * Port of Python gateway/stream_events.py (171 lines, 8 event classes).
 * Defines the typed event structs that the agent loop emits during
 * streaming generation. The gateway dispatcher routes each event
 * through the platform adapter's render hooks.
 *
 * Python equivalent: gateway/stream_events.py
 *   MessageChunk   → stream_event_chunk_t
 *   MessageStop    → stream_event_stop_t
 *   Commentary     → stream_event_commentary_t
 *   ToolCallChunk  → stream_event_tool_chunk_t
 *   ToolCallFinished → stream_event_tool_finished_t
 *   LongToolHint   → stream_event_long_tool_hint_t
 *   GatewayNotice  → stream_event_gateway_notice_t
 *   StreamEvent    → stream_event_t (union)
 *
 * @{
 */
#ifndef HERMES_GATEWAY_STREAM_H
#define HERMES_GATEWAY_STREAM_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Event type enum ────────────────────────────────────────────── */

typedef enum {
    STREAM_EVENT_MESSAGE_CHUNK,       /* incremental text delta */
    STREAM_EVENT_MESSAGE_STOP,        /* assistant message segment complete */
    STREAM_EVENT_COMMENTARY,          /* complete interim assistant message */
    STREAM_EVENT_TOOL_CALL_CHUNK,     /* tool invocation started/updated */
    STREAM_EVENT_TOOL_CALL_FINISHED,  /* tool invocation completed */
    STREAM_EVENT_LONG_TOOL_HINT,      /* onboarding nudge for long tool */
    STREAM_EVENT_GATEWAY_NOTICE,      /* gateway-originated control message */
} stream_event_type_t;

/* ── Event data structs ─────────────────────────────────────────── */

/* Port of Python stream_events.py:MessageChunk */
typedef struct {
    const char *text;  /* incremental content delta (not owned) */
} stream_event_chunk_t;

/* Port of Python stream_events.py:MessageStop */
typedef struct {
    bool final;  /* true only for terminal stop of entire turn */
} stream_event_stop_t;

/* Port of Python stream_events.py:Commentary */
typedef struct {
    const char *text;  /* complete interim message (not owned) */
} stream_event_commentary_t;

/* Port of Python stream_events.py:ToolCallChunk */
typedef struct {
    const char *tool_name;          /* function name being called */
    const char *preview;            /* short argument preview (may be NULL) */
    const char *args_json;          /* full args as JSON string (may be NULL) */
    int         index;              /* monotonic per-turn index */
} stream_event_tool_chunk_t;

/* Port of Python stream_events.py:ToolCallFinished */
typedef struct {
    const char *tool_name;          /* function name that completed */
    double      duration;           /* wall-clock seconds */
    bool        ok;                 /* true if tool returned without raising */
    int         index;              /* matches ToolCallChunk.index */
} stream_event_tool_finished_t;

/* Port of Python stream_events.py:LongToolHint */
typedef struct {
    const char *tool_name;          /* long-running tool name */
    double      duration;           /* elapsed seconds so far */
} stream_event_long_tool_hint_t;

/* Port of Python stream_events.py:GatewayNotice */
typedef struct {
    const char *kind;               /* stable switch-on string ("restart"/"online"/...) */
    const char *text;               /* human-readable default */
    const char *extra_json;         /* optional extra data as JSON (may be NULL) */
} stream_event_gateway_notice_t;

/* ── Union event type ───────────────────────────────────────────── */

/* Port of Python stream_events.py:StreamEvent (Union type) */
typedef struct {
    stream_event_type_t type;
    union {
        stream_event_chunk_t             chunk;
        stream_event_stop_t              stop;
        stream_event_commentary_t        commentary;
        stream_event_tool_chunk_t        tool_chunk;
        stream_event_tool_finished_t     tool_finished;
        stream_event_long_tool_hint_t    long_tool_hint;
        stream_event_gateway_notice_t    gateway_notice;
    } data;
} stream_event_t;

/* ── Dispatch callback signature ────────────────────────────────── */

/**
 * Callback invoked by stream_event_dispatch() for each event.
 * Platform adapters implement this to render events (e.g., native
 * Telegram drafts, edit-in-place, tool progress lines).
 *
 * Return: 0 on success, non-zero to abort streaming.
 */
typedef int (*stream_event_callback_t)(const stream_event_t *event, void *userdata);

/* ── Dispatch function ──────────────────────────────────────────── */

/* Port of Python stream_dispatch.py:GatewayEventDispatcher.dispatch() */
int stream_event_dispatch(const stream_event_t *event,
                           stream_event_callback_t callback,
                           void *userdata);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_GATEWAY_STREAM_H */
/** @} */
