/*
 * port_conversation_compression.c -- see port_conversation_compression.h.
 */

#include "port_conversation_compression.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define CC_STATUS_MARKER "Compacting context"
#define CC_DONE_STATUS "✓ Context compaction complete — continuing turn..."
#define CC_DB_PERSISTED_MARKER "_db_persisted"

/* ===========================================================================
 *  _strip_marker_for_comparison  (conversation_compression.py:83)
 *
 *      if not isinstance(msgs, list): return msgs
 *      return [{k: v for k, v in m.items() if k != MARKER}
 *              if isinstance(m, dict) else m for m in msgs]
 * =========================================================================== */

/* PoP: cc_strip_marker_for_comparison @ agent/conversation_compression.py:_strip_marker_for_comparison */
json_t *cc_strip_marker_for_comparison(const json_t *msgs)
{
    if (!msgs || msgs->type != JSON_ARRAY)
        return msgs ? json_copy(msgs) : json_array();

    json_t *out = json_array();
    if (!out)
        return NULL;
    for (size_t i = 0; i < msgs->c.count; i++) {
        const json_t *m = msgs->c.items[i];
        if (!m || m->type != JSON_OBJECT) {
            json_append(out, json_copy(m));
            continue;
        }
        json_t *copy = json_object();
        if (!copy) {
            json_free(out);
            return NULL;
        }
        for (size_t j = 0; j < m->c.count; j++) {
            const char *k = m->c.keys[j];
            if (k && strcmp(k, CC_DB_PERSISTED_MARKER) == 0)
                continue;
            json_set(copy, k ? k : "", json_copy(m->c.items[j]));
        }
        json_append(out, copy);
    }
    return out;
}

/* ===========================================================================
 *  is_compaction_progress_status  (conversation_compression.py:149)
 *
 *      body = text.strip() if isinstance(text, str) else ""
 *      if not body: return False
 *      if MARKER in body: return True
 *      if body == DONE_STATUS: return False
 *      lowered = body.lower()
 *      if "compaction complete" in lowered or \
 *         "compression is currently blocked" in lowered: return False
 *      return "compact" in lowered or "compress" in lowered or \
 *             "context reduced to" in lowered
 * =========================================================================== */

static void trim_ws(const char **p, size_t *n)
{
    while (*n > 0 && ((*p)[0] == ' ' || (*p)[0] == '\t' || (*p)[0] == '\n'
                      || (*p)[0] == '\r' || (*p)[0] == '\v' || (*p)[0] == '\f')) {
        (*p)++;
        (*n)--;
    }
    while (*n > 0 && ((*p)[*n - 1] == ' ' || (*p)[*n - 1] == '\t'
                      || (*p)[*n - 1] == '\n' || (*p)[*n - 1] == '\r'
                      || (*p)[*n - 1] == '\v' || (*p)[*n - 1] == '\f')) {
        (*n)--;
    }
}

/* case-insensitive substring search over a length-delimited haystack */
static int ci_contains(const char *hay, size_t hn, const char *needle)
{
    size_t nn = strlen(needle);
    if (nn == 0)
        return 1;
    if (hn < nn)
        return 0;
    for (size_t i = 0; i + nn <= hn; i++) {
        size_t j = 0;
        while (j < nn
               && tolower((unsigned char)hay[i + j]) == tolower((unsigned char)needle[j]))
            j++;
        if (j == nn)
            return 1;
    }
    return 0;
}

/* PoP: cc_is_compaction_progress_status @ agent/conversation_compression.py:is_compaction_progress_status */
int cc_is_compaction_progress_status(const char *text)
{
    if (!text)
        return 0;
    const char *body = text;
    size_t n = strlen(text);
    trim_ws(&body, &n);
    if (n == 0)
        return 0;

    if (ci_contains(body, n, CC_STATUS_MARKER))
        return 1;

    /* body == DONE_STATUS (exact, after the strip) */
    if (n == strlen(CC_DONE_STATUS)
        && memcmp(body, CC_DONE_STATUS, n) == 0)
        return 0;

    /* lowercase copy for the case-insensitive checks */
    char lower[1024];
    size_t cn = n < sizeof(lower) - 1 ? n : sizeof(lower) - 1;
    for (size_t i = 0; i < cn; i++)
        lower[i] = (char)tolower((unsigned char)body[i]);
    lower[cn] = '\0';

    if (ci_contains(lower, cn, "compaction complete")
        || ci_contains(lower, cn, "compression is currently blocked"))
        return 0;

    return ci_contains(lower, cn, "compact")
           || ci_contains(lower, cn, "compress")
           || ci_contains(lower, cn, "context reduced to");
}
