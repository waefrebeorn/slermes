/*
 * agent_message_payload.c — Port of the payload-inspection + repair helpers
 * from agent/agent_runtime_helpers.py:
 *   _msg_has_payload            (is a message dict non-empty for the wire?)
 *   repair_empty_non_final_messages  (heal empty non-final turns in-memory)
 *
 * Both operate on json_t* message dicts (json_node_t == json_t), the same
 * representation used by agent/message_sanitization_pure.c.  Reuses libjson
 * (json_copy for the deep copy) + hermes_logger (hermes_log).
 */

#define _POSIX_C_SOURCE 200809L
#include "agent_runtime_pure.h"
#include <hermes_json.h>   /* json_t, json_obj_get, json_is_*, json_string, json_copy */
#include <hermes_logger.h>

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define INTERRUPTED_PLACEHOLDER "[response interrupted]"

/* ── helpers ──────────────────────────────────────────────────────────────── */

/* Python truthiness for a JSON-parsed value: `if val:`. */
static bool json_is_truthy(const json_t *v)
{
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:    return false;
        case JSON_BOOL:    return v->bool_val;
        case JSON_NUMBER:  return v->num_val != 0.0;
        case JSON_STRING:  return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:   return v->c.count > 0;
        case JSON_OBJECT:  return v->c.count > 0;
    }
    return false;
}

/* True if the string value is non-blank (Python content.strip()). */
static bool json_str_nonblank(const json_t *v)
{
    if (!v || v->type != JSON_STRING || !v->str_val) return false;
    for (const char *p = v->str_val; *p; p++)
        if (!isspace((unsigned char)*p)) return true;
    return false;
}

/* Python: if isinstance(content, list): loop over blocks */
/* Returns true if a content-list has a payload (typed non-blank or truthy block). */
static bool content_list_has_payload(const json_t *container)
{
    if (!container || container->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < container->c.count; i++) {
        json_t *block = container->c.items[i];
        if (!block) continue;
        if (block->type == JSON_OBJECT) {
            json_t *ttype = json_obj_get(block, "type");
            if (json_is_string(ttype) && ttype->str_val &&
                strcmp(ttype->str_val, "text") == 0) {
                json_t *text = json_obj_get(block, "text");
                if (json_str_nonblank(text)) return true;
                continue;  /* blank text block -> keep scanning */
            }
            return true;  /* any other typed block (image/tool_use/...) counts */
        }
        if (json_is_truthy(block))
            return true;
    }
    return false;
}

/* ── _msg_has_payload ────────────────────────────────────────────────────── */
/* PoP: _msg_has_payload @ agent/agent_runtime_helpers.py:_msg_has_payload */
bool msg_has_payload(const json_t *msg)
{
    if (!msg || msg->type != JSON_OBJECT) return false;

    json_t *content = json_obj_get(msg, "content");

    /* Python: if isinstance(content, str): if content.strip(): return True */
    if (content && content->type == JSON_STRING)
        return json_str_nonblank(content);

    /* Python: elif isinstance(content, list): for block in content: ... */
    if (content && content->type == JSON_ARRAY)
        return content_list_has_payload(content);

    /* Python: elif content not in (None, ""): return True
     * i.e. content is a dict / number / bool / any non-None, non-empty-string
     * value.  Note Python `not in (None, "")` is NOT a truthiness check:
     * False, {}, 0, [] all pass (a list would have been caught above, so
     * only dict/number/bool reach here). */
    if (content && content->type != JSON_NULL && content->type != JSON_STRING)
        return true;

    /* content is None or "" — fall through to structural checks */

    /* Python:
     *   if msg.get("tool_calls"): return True
     *   if isinstance(msg.get("reasoning_content"), str) and msg["reasoning_content"].strip(): return True
     *   if msg.get("reasoning") or msg.get("reasoning_details"): return True
     *   if msg.get("codex_message_items") or msg.get("codex_reasoning_items"): return True
     */
    if (json_is_truthy(json_obj_get(msg, "tool_calls")))
        return true;
    if (json_str_nonblank(json_obj_get(msg, "reasoning_content")))
        return true;
    if (json_is_truthy(json_obj_get(msg, "reasoning")) ||
        json_is_truthy(json_obj_get(msg, "reasoning_details")))
        return true;
    if (json_is_truthy(json_obj_get(msg, "codex_message_items")) ||
        json_is_truthy(json_obj_get(msg, "codex_reasoning_items")))
        return true;

    return false;
}

/* ── repair_empty_non_final_messages ─────────────────────────────────────── */
/* PoP: repair_empty_non_final_messages @ agent/agent_runtime_helpers.py:repair_empty_non_final_messages */
/*
 * Returns a request-local deep copy with empty non-final assistant/user
 * messages substituted by INTERRUPTED_PLACEHOLDER.  The original `messages`
 * array is never mutated.
 */
json_t *repair_empty_non_final_messages(const json_t *messages)
{
    if (!messages || messages->type != JSON_ARRAY || messages->c.count == 0)
        return json_copy(messages);  /* Python: if not messages: return messages */

    if (messages->c.count < 2)
        return json_copy(messages);  /* Python: len(messages) < 2 -> return messages */

    /* Deep-copy the array so stored history stays byte-stable. */
    json_t *repaired = json_copy(messages);
    if (!repaired) return NULL;

    size_t last = repaired->c.count - 1;
    int healed = 0;
    for (size_t idx = 0; idx < last; idx++) {
        json_t *msg = repaired->c.items[idx];
        if (!msg || msg->type != JSON_OBJECT) continue;

        /* Python: isinstance(msg, dict) and msg.get("role") in ("assistant","user")
         *          and not _msg_has_payload(msg) */
        json_t *role = json_obj_get(msg, "role");
        if (!json_is_string(role) || !role->str_val) continue;
        if (strcmp(role->str_val, "assistant") != 0 &&
            strcmp(role->str_val, "user") != 0)
            continue;
        if (msg_has_payload(msg)) continue;

        /* Substitute placeholder content (in-place on the deep copy).
         * Python: fixed = dict(msg); fixed["content"] = _INTERRUPTED_PLACEHOLDER */
        bool found = false;
        for (size_t k = 0; k < msg->c.count; k++) {
            if (msg->c.keys[k] && strcmp(msg->c.keys[k], "content") == 0) {
                json_free(msg->c.items[k]);
                msg->c.items[k] = json_string(INTERRUPTED_PLACEHOLDER);
                found = true;
                break;
            }
        }
        if (!found)
            json_set(msg, "content", json_string(INTERRUPTED_PLACEHOLDER));
        healed++;
    }

    if (healed)
        hermes_log(LOG_WARNING, "agent_runtime_helpers",
                   "Pre-call sanitizer: healed %d empty non-final message(s) by "
                   "substituting placeholder content — an empty-content turn was in "
                   "the transcript and would 400 the request ('messages must have "
                   "non-empty content' / INVALID_REQUEST_BODY). Self-recovering the "
                   "poisoned transcript in memory; no restart needed.", healed);

    return repaired;
}

/* ── msg_has_payload (Python name _msg_has_payload) accessor helpers ───── */
/* (kept minimal; the public fn is msg_has_payload per name-parity note below) */
