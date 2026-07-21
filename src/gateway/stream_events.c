/*
 * stream_events.c — Gateway stream event dispatch.
 *
 * Port of Python gateway/stream_dispatch.py (132 lines) and
 * gateway/stream_events.py (171 lines).
 *
 * Provides typed event structs and a synchronous dispatch function
 * that routes events through a platform adapter callback.
 *
 * Port of Python stream_events.py:
 *   MessageChunk, MessageStop, Commentary, ToolCallChunk,
 *   ToolCallFinished, LongToolHint, GatewayNotice, StreamEvent
 *   — defined in include/hermes_gateway_stream.h
 *
 * Port of Python stream_dispatch.py:
 *   GatewayEventDispatcher.dispatch() → stream_event_dispatch()
 */

#include "hermes_gateway_stream.h"
#include "hermes_gateway_core.h"
#include <stddef.h>

/* ================================================================
 *  Stream event dispatch — route event through adapter callback
 * ================================================================ */

/* Port of Python stream_dispatch.py:GatewayEventDispatcher.dispatch() */
int stream_event_dispatch(const stream_event_t *event,
                           stream_event_callback_t callback,
                           void *userdata)
{
    if (!event || !callback)
        return -1;

    /* Route to the adapter callback — the adapter decides
     * how to render each event type (native draft, edit-in-place,
     * tool progress line, or eat the event entirely). */
    return callback(event, userdata);
}
