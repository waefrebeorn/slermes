/*
 * session_stored_reads.c - C home for upstream Hermes gateway/session_stored_reads.py.
 * PoP: w16_sessionsto_8c91af_take @ gateway/session_stored_reads.py:_take
 * PoP: w16_sessionsto_8c91af_cut @ gateway/session_stored_reads.py:_cut
 * PoP: w16_sessionsto_8c91af_sent @ gateway/session_stored_reads.py:_sent
 *
 * Faithful ports of the pure page-boundary helpers. MAX_PAGE_ROWS in the live
 * module is 500; store/IO paging (read_page, _window, ...) stays in Python.
 */
#include "hermes_json.h"
#include "noc_w16_sessionsto_8c91af.h"

static const char *role_at(const json_t *roles, long long i) {
    const json_t *r = json_get(roles, (size_t)i);
    return (r && r->type == JSON_STRING && r->str_val) ? r->str_val : "";
}

struct json_t *w16_sessionsto_8c91af_take(const struct json_t *roles, int latest, long long limit) {
    long long n = (long long)json_len(roles);
    long long taken = limit < n ? limit : n;
    while (taken < n) {
        /* Python: roles[taken - 1 if latest else taken]; t==0/latest is roles[-1]. */
        long long idx = latest ? (taken == 0 ? n - 1 : taken - 1) : taken;
        if (idx < 0 || idx >= n || strcmp(role_at(roles, idx), "tool") != 0) break;
        taken++;
    }
    return json_number((double)taken);
}

struct json_t *w16_sessionsto_8c91af_cut(const struct json_t *roles, int latest, long long taken) {
    long long n = (long long)json_len(roles);
    long long keep = taken / 2;
    if (keep < 1) keep = 1;
    while (keep > 1) {
        long long idx = latest ? keep - 1 : keep;
        if (idx >= n || strcmp(role_at(roles, idx), "tool") != 0) break;
        keep--;
    }
    return json_number((double)keep);
}

struct json_t *w16_sessionsto_8c91af_sent(const struct json_t *handoffs, int latest,
                                          const struct json_t *item) {
    long long index = (long long)json_number_value(json_get(item, 0));
    long long slot = (long long)json_number_value(json_get(item, 1));
    if (latest) return slot ? json_number((double)slot) : json_null();
    long long hlen = (long long)json_len(json_get(handoffs, (size_t)index));
    return slot == hlen ? json_null() : json_number((double)(slot + 1));
}
