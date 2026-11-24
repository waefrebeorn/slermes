/*
 * noc_w30_kanbanwatc_3847d4_p1.h - C surface for upstream
 * gateway/kanban_watchers_notifier.py (gate w30_kanbanwatc_3847d4_p1).
 *
 * Pure "what does one terminal event mean, and what does it say" core of the
 * kanban notifier: the kind tables, ``diagnostic_event``, the payload text
 * helpers (``_payload`` / ``_clip`` / ``_first_line`` / ``_safe_review_reason``),
 * the nine per-kind ping formatters behind ``format_event``, the per-batch
 * accumulator, and the ``_send_pings`` selection filter.
 *
 * Every I/O seam stays with the caller: the adapter ``send()``, the cursor
 * writes (``record_notify_ping`` / advance / rewind), the DB claim that
 * produced the events, the logger, the asyncio offload, and
 * ``agent.redact.redact_sensitive_text``.  Text that the redactor would touch
 * is handed in ALREADY REDACTED (``safe_review_reason`` takes ``redacted``);
 * format_event reads payload text as-is, so its ``changes_requested`` fixtures
 * must carry redaction-stable text.
 */
#ifndef NOC_W30_KANBANWATC_3847D4_P1_H
#define NOC_W30_KANBANWATC_3847D4_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"max_send_failures", "terminal_kinds", "wake_kinds", "thread_chat_types"} */
json_t *w30_kanbanwatc_3847d4_p1_constants(void);

/* {"event": {"kind": <str>, "payload": <obj|null>}} -> <bool>. */
json_t *w30_kanbanwatc_3847d4_p1_diagnostic_event(const json_t *req);

/* {"text": <str>, "limit": <int>} -> <str|null>. */
json_t *w30_kanbanwatc_3847d4_p1_first_line(const json_t *req);

/* {"event": {...}, "key": <str>, "tkey": <catalog key>, "limit": <int>} -> <str>. */
json_t *w30_kanbanwatc_3847d4_p1_clip(const json_t *req);

/* {"redacted": <str|null>, "limit": <int>} -> <str> (path-masked, whitespace
 * collapsed, ellipsis-truncated). */
json_t *w30_kanbanwatc_3847d4_p1_safe_review_reason(const json_t *req);

/* {"event": {...}, "sub": {...}, "task": {...}|null, "board": <str|null>} ->
 * {"msg": <str|null>, "handoff": <str|null>, "review_detail": <str|null>}
 * (null = that slot left untouched / a silent kind). */
json_t *w30_kanbanwatc_3847d4_p1_format_event(const json_t *req);

/* Same inputs plus "events": [...] ->
 * {"messages": [<str|null>...], "wake_handoff": <str>, "wake_review_detail": <str>}. */
json_t *w30_kanbanwatc_3847d4_p1_format_batch(const json_t *req);

/* {"events": [...], "sub": {...}, "task": {...}|null, "board": <str|null>,
 *  "is_push_adapter": <bool>} ->
 * {"ids": [...], "messages": [...], "wake_handoff": <str>, "wake_review_detail": <str>}
 * -- the events whose text ping would be sent (adapter send is the seam). */
json_t *w30_kanbanwatc_3847d4_p1_ping_plan(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
