#include "hermes_logger.h"
#include "hermes_gateway_core.h"
#include "hermes_core_types.h"
#include <string.h>
#include <stdlib.h>

/*
 * stream_consumer.c — Port of Python gateway/stream_consumer.py
 *
 * Bridge functions for the GatewayStreamConsumer class.
 * The heavy streaming logic lives in server.c and stream_events.c.
 * This file implements the utility helpers that the Python
 * GatewayStreamConsumer._metadata_for_send() and
 * _notify_before_finalize() provide.
 *
 * These are used by the stream event dispatch to decorate outbound
 * message metadata and run pre-finalization hooks.
 *
 * PoP annotations referencing this module: 1
 */

/* Port of Python gateway/stream_consumer.py:GatewayStreamConsumer._metadata_for_send
 *
 * Return per-send metadata for stream-created messages.
 * Arguments:
 *   p1: metadata dict (json_node_t*) — base metadata from the consumer
 *   p2: int* — pointer to int 0 or 1  (final flag)
 *   p3: int* — pointer to int 0 or 1  (expect_edits flag)
 * Returns json_node_t* (void*) with metadata keys set, or NULL.
 */
void* cli_gateway_stream_consumer__metadata_for_send(void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p4; (void)p5;
    json_node_t *meta = (json_node_t*)p1;
    int final = p2 ? *(int*)p2 : 0;
    int expect_edits = p3 ? *(int*)p3 : 0;

    json_node_t *result;
    if (meta && json_node_is_object(meta)) {
        result = json_copy(meta);
    } else {
        result = json_new_object();
    }
    if (!result) return NULL;

    if (expect_edits) {
        json_object_set(result, "expect_edits", json_new_bool(1));
    }
    if (final) {
        json_object_set(result, "notify", json_new_bool(1));
    }

    /* Only return non-NULL if we actually added keys */
    if (!expect_edits && !final) {
        /* No meaningful keys added — return NULL like Python's `meta or None` */
        json_free(result);
        return NULL;
    }

    return result;
}

/* Port of Python gateway/stream_consumer.py:GatewayStreamConsumer._notify_before_finalize
 *
 * Run the pre-finalize callback exactly once.
 * Arguments:
 *   p1: void* — opaque callback context (userdata)
 *   p2: int* — pointer to int flag tracking whether already notified
 *   p3: callback function pointer (int (*)(void*))
 * Returns: int* — pointer to int 0 (success) or negative (error)
 *
 * NOTE: In Python this is async and swallows exceptions.
 * In C we call the callback synchronously from the same thread.
 */
void* cli_gateway_stream_consumer__notify_before_finalize(void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p4; (void)p5;
    int *already_notified = (int*)p2;
    int (*callback)(void*) = (int (*)(void*))p3;

    /* If already notified, skip */
    if (already_notified && *already_notified) {
        int *result = (int*)malloc(sizeof(int));
        if (result) *result = 0;
        return result;
    }

    /* Mark notified */
    if (already_notified) {
        *already_notified = 1;
    }

    /* Run callback (if provided) */
    int cb_result = 0;
    if (callback && p1) {
        cb_result = callback(p1);
    }

    int *result = (int*)malloc(sizeof(int));
    if (!result) return NULL;
    *result = cb_result;
    return result;
}