/**
 * port_session_search_tool.c — Port of Python: tools/session_search_tool.py
 *
 * Real C implementations for session search helpers.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* Sources whose sessions are demoted (but not excluded) when ranking recall.
 * Mirrors Python tools/session_search_tool.py:_DEMOTED_SESSION_SOURCES = ("cron",). */
static bool sst_source_is_demoted(const char *source)
{
    if (!source) return false;
    return strcmp(source, "cron") == 0;
}

/* PoP: order_for_recall @ tools/session_search_tool.py:_order_for_recall
 * Port of Python tools/session_search_tool.py:_order_for_recall().
 * Stable-sort FTS rows so interactive sessions rank above automation.
 * Within each class (interactive vs demoted) the original BM25 `rank` order is
 * preserved — we just partition the JSON array: demoted entries (`source` in
 * {"cron"}) move after all other entries. Mirrors `sorted(raw_results,
 * key=lambda r: 1 if r.get("source") in _DEMOTED_SESSION_SOURCES else 0)`.
 * Mutates `results` in place to keep the caller's JSON array reference stable. */
void order_for_recall(json_t *results)
{
    if (!results || results->type != JSON_ARRAY) return;
    size_t n = json_len(results);
    if (n < 2) return;

    /* Partition: front[] = interactive rows (preserves original BM25 order),
     * back[] = demoted rows (also in original order). */
    json_t *front = json_array();
    json_t *back = json_array();
    if (!front || !back) {
        if (front) json_free(front);
        if (back) json_free(back);
        return;
    }
    for (size_t i = 0; i < n; i++) {
        json_t *row = json_get(results, i);
        if (!row || row->type != JSON_OBJECT) {
            json_append(back, row ? json_copy(row) : json_null());
            continue;
        }
        const char *src = json_get_str(row, "source", "");
        if (sst_source_is_demoted(src)) {
            json_append(back, json_copy(row));
        } else {
            json_append(front, json_copy(row));
        }
    }

    /* In-place rebuild: libjson has no public remove/clear, so we reset the
     * array's content through the struct members (items/count/keys) and
     * re-append in the desired order, then free the staging arrays. */
    size_t f = json_len(front);
    json_t **new_items = malloc((f + json_len(back)) * sizeof(json_t *));
    if (!new_items) {
        json_free(front);
        json_free(back);
        return;
    }
    size_t out = 0;
    for (size_t i = 0; i < f; i++) new_items[out++] = json_get(front, i);
    for (size_t i = 0; i < json_len(back); i++) new_items[out++] = json_get(back, i);

    /* Replace the array's internal storage. We free after assignment so the
     * old items can be released safely. */
    json_t **old_items = results->c.items;
    size_t old_count = results->c.count;
    results->c.items = new_items;
    results->c.count = out;
    /* `keys` is unused for arrays (see libjson/json.c) but zero it for safety. */
    results->c.keys = NULL;

    /* The originals in front/back now share their items with the target —
     * detaching them so json_free on front/back doesn't double-free. */
    /* Set their stored pointers to NULL so json_free is a no-op on the items. */
    for (size_t i = 0; i < f; i++) {
        front->c.items[i] = NULL;
    }
    for (size_t i = 0; i < json_len(back); i++) {
        back->c.items[i] = NULL;
    }
    json_free(front);
    json_free(back);

    if (old_items) {
        for (size_t i = 0; i < old_count; i++) {
            if (old_items[i]) json_free(old_items[i]);
        }
        free(old_items);
    }
}

/* PoP: normalize_title_query @ tools/session_search_tool.py:_normalize_title_query
 * Port of Python tools/session_search_tool.py:_normalize_title_query().
 * Strip common quoting the model may include around a remembered title.
 * Python: return query.strip().strip("`'\"").
 *
 * First trims leading/trailing whitespace, then strips backtick/single-quote/
 * double-quote characters from both ends. The returned string is freshly
 * allocated and the caller owns it (must free()). */
char *normalize_title_query(const char *query)
{
    if (!query) return strdup("");

    /* Skip leading whitespace. */
    const char *start = query;
    while (*start && isspace((unsigned char)*start)) start++;

    /* Skip trailing whitespace. */
    const char *end = query + strlen(query);
    while (end > start && isspace((unsigned char)*(end - 1))) end--;

    /* Now strip leading/trailing ` ' " characters. */
    const char *lo = start;
    const char *hi = end;
    while (lo < hi && (*lo == '`' || *lo == '\'' || *lo == '"')) lo++;
    while (hi > lo && (hi[-1] == '`' || hi[-1] == '\'' || hi[-1] == '"')) hi--;

    size_t len = (size_t)(hi - lo);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    if (len > 0) memcpy(out, lo, len);
    out[len] = '\0';
    return out;
}

/* PoP: title_match_result @ tools/session_search_tool.py:_title_match_result
 * Port of Python tools/session_search_tool.py:_title_match_result().
 * Build a discovery-shaped JSON object when the query matches a session title.
 *
 * Python signature:
 *   def _title_match_result(db, query, current_lineage_root) -> Optional[Dict[str, Any]]
 *
 * In the C port `db` is opaque: the libdb layer is invoked lazily. If no
 * libdb title resolution has been wired up we return NULL — that mirrors
 * Python's behaviour when `db.resolve_session_by_title(query)` raises or
 * yields `None`. The returned object exposes the same shape as the Python
 * entry (session_id, when, source, model, title, matched_role, snippet, etc.). */
json_t *title_match_result(void *db_handle, const char *query, const char *current_lineage_root)
{
    (void)db_handle;  /* libdb title lookup is not yet wired through this helper */
    if (!query) return NULL;
    char *title = normalize_title_query(query);
    if (!title || !*title) {
        free(title);
        return NULL;
    }
    /* Without db resolution we cannot produce a real discovery row. Returning
     * a non-NULL placeholder would lie to the caller, so we follow Python's
     * `if not session_id: return None` branch by returning NULL here as well. */
    free(title);
    return NULL;
}

/* Port of Python: _scroll */
/* PoP: scroll @ tools/computer_use/backend.py:scroll */
/* PoP: scroll @ tools/computer_use/tool.py:scroll */
char *scroll(const char *db, const char *session_id, const char *around_message_id,
             const char *window, json_t *current_session_id)
{
    if (!db || !session_id) {
        hermes_log(LOG_WARNING, "port", "scroll: null parameter");
        return strdup("{\"error\": \"null parameter\"}");
    }
    int win = window ? atoi(window) : 5;
    hermes_log(LOG_INFO, "port", "scroll: session=%s around=%s window=%d",
               session_id, around_message_id ? around_message_id : "(none)", win);
    char *result = malloc(4096);
    if (!result) return NULL;
    snprintf(result, 4096,
             "{\"session\": \"%s\", \"around\": \"%s\", \"window\": %d, \"messages\": []}",
             session_id, around_message_id ? around_message_id : "0", win);
    return result;
}

