/*
 * port_prompt_caching_strip.c — Port of agent/prompt_caching.py's cache-control
 * stripping helpers.  These are pure json_t* mutations, no agent/loop deps:
 *   strip_anthropic_cache_control   — remove cache_control from messages + content parts
 *   strip_anthropic_tool_cache_control — return tools minus request-local markers
 *
 * Reuses: libjson (json_copy, json_obj_get/set/del, json_is_*).
 */

#define _POSIX_C_SOURCE 200809L
#include "port_prompt_caching_strip.h"
#include <hermes_json.h>   /* json_t, json_obj_get/set/del, json_is_* */

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

/* ── helpers ─────────────────────────────────────────────────────────────── */

/* True if `part` is a JSON dict. */
static bool is_dict(const json_t *v) { return json_is_object(v); }

/* set(part.keys() <= {"type","text"}) */
static bool dict_is_type_text_only(const json_t *part)
{
    if (!is_dict(part)) return false;
    for (size_t i = 0; i < part->c.count; i++) {
        const char *k = part->c.keys[i];
        if (!k) continue;
        if (strcmp(k, "type") != 0 && strcmp(k, "text") != 0)
            return false;
    }
    return true;
}

/* True if `part` is a dict carrying a "cache_control" key. */
static bool part_has_cache_control(const json_t *part)
{
    return is_dict(part) && json_obj_get(part, "cache_control") != NULL;
}

/* ── strip_anthropic_cache_control ────────────────────────────────────────── */
/* PoP: strip_anthropic_cache_control @ agent/prompt_caching.py:strip_anthropic_cache_control */
/*
 * Python: mutates api_messages in place (pop "cache_control" from each msg,
 * strip cache_control from content parts, and flatten decoration-shaped
 * content lists back to a plain string when the shape is exactly the kind
 * apply_anthropic_cache_control produces).  Returns the same list.
 *
 * C note: we mutate the deep-copied api_messages in place (matching Python's
 * in-place contract on the caller's wire copy).  The caller passes a json_t
 * array it owns; we free+replace "cache_control" values per dict.
 */
void pca_strip_anthropic_cache_control(json_t *api_messages)
{
    if (!api_messages || !json_is_array(api_messages)) return;

    for (size_t i = 0; i < api_messages->c.count; i++) {
        json_t *msg = api_messages->c.items[i];
        if (!is_dict(msg)) continue;

        /* msg.pop("cache_control", None) */
        json_obj_del(msg, "cache_control");

        json_t *content = json_obj_get(msg, "content");
        if (!content || !json_is_array(content)) continue;

        bool had_marker = false;
        for (size_t p = 0; p < content->c.count; p++) {
            if (part_has_cache_control(content->c.items[p])) {
                had_marker = true;
                /* {k:v for k,v in part.items() if k != "cache_control"} */
                json_t *part = content->c.items[p];
                /* Rebuild part dict minus cache_control */
                json_t *clean = json_object();
                for (size_t k = 0; k < part->c.count; k++) {
                    if (part->c.keys[k] &&
                        strcmp(part->c.keys[k], "cache_control") != 0)
                        json_set(clean, part->c.keys[k], json_copy(part->c.items[k]));
                }
                json_free(part);
                content->c.items[p] = clean;
            }
        }

        /* decoration_shape: list of {"type":"text","text":str} parts only,
         * with at most 1 part (or 2 for a system role). */
        bool decoration = content->c.count > 0;
        for (size_t p = 0; decoration && p < content->c.count; p++) {
            json_t *part = content->c.items[p];
            if (!is_dict(part)) { decoration = false; break; }
            json_t *ttype = json_obj_get(part, "type");
            json_t *text = json_obj_get(part, "text");
            if (!ttype || !json_is_string(ttype) || strcmp(ttype->str_val, "text") != 0)
                { decoration = false; break; }
            if (!text || !json_is_string(text))
                { decoration = false; break; }
            if (!dict_is_type_text_only(part))
                { decoration = false; break; }
        }
        if (decoration) {
            /* msg["content"] = "".join(part["text"] for part in content) */
            size_t cap = 1, len = 0;
            char *joined = malloc(cap);
            if (joined) {
                for (size_t p = 0; p < content->c.count; p++) {
                    json_t *text = json_obj_get(content->c.items[p], "text");
                    if (text && text->str_val) {
                        size_t tl = strlen(text->str_val);
                        if (len + tl + 1 > cap) {
                            cap = (len + tl + 1) * 2;
                            char *tmp = realloc(joined, cap);
                            if (!tmp) { free(joined); joined = NULL; break; }
                            joined = tmp;
                        }
                        memcpy(joined + len, text->str_val, tl);
                        len += tl;
                    }
                }
                if (joined) {
                    joined[len] = '\0';
                    /* json_set frees the old content array and sets new string. */
                    json_set(msg, "content", json_string(joined));
                    free(joined);
                }
            }
        }
    }
}

/* ── strip_anthropic_tool_cache_control ──────────────────────────────────── */
/* PoP: strip_anthropic_tool_cache_control @ agent/prompt_caching.py:strip_anthropic_tool_cache_control */
/*
 * Python: cleaned = deepcopy(tools or []); pop "cache_control" per tool dict;
 *  return cleaned.  C: returns a deep copy (caller frees with json_free).
 */
json_t *pca_strip_anthropic_tool_cache_control(const json_t *tools)
{
    /* Python: cleaned = copy.deepcopy(tools or []) */
    json_t *cleaned;
    if (!tools)
        cleaned = json_array();       /* tools or [] */
    else
        cleaned = json_copy(tools);
    if (!cleaned) return NULL;

    if (cleaned->type != JSON_ARRAY) {
        json_free(cleaned);
        return json_array();
    }
    for (size_t i = 0; i < cleaned->c.count; i++) {
        json_t *tool = cleaned->c.items[i];
        if (is_dict(tool))
            json_obj_del(tool, "cache_control");
    }
    return cleaned;
}
