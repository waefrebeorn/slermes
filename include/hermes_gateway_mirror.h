/**
 * @file hermes_gateway_mirror.h
 * @brief Session mirroring API (port of Python gateway/mirror.py).
 */
#ifndef HERMES_GATEWAY_MIRROR_H
#define HERMES_GATEWAY_MIRROR_H

#include "hermes_gateway_types.h"

/* ================================================================
 *  Session Mirroring (cross-platform delivery mirror)
 * ================================================================ */

/* Mirror a delivery to the target session's transcript.
 * Finds the gateway session for platform+chat_id, writes a mirror
 * entry to both JSONL and SQLite. Returns true on success.
 * All errors are caught — this is never fatal. */
bool mirror_to_session(const char *platform,
                        const char *chat_id,
                        const char *message_text,
                        const char *source_label,
                        const char *thread_id,
                        const char *user_id);

#endif /* HERMES_GATEWAY_MIRROR_H */