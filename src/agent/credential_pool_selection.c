/*
 * credential_pool_selection.c -- real C11 port of upstream Hermes
 *   agent/credential_pool_selection.py.
 *
 * PoP: w20_credential_af0c5f_serving @ agent/credential_pool_selection.py:_serving
 * PoP: w20_credential_af0c5f_strategy_pick @ agent/credential_pool_selection.py:_strategy_pick
 * PoP: w20_credential_af0c5f_record_pick @ agent/credential_pool_selection.py:_record_pick
 * PoP: w20_credential_af0c5f_select_unlocked @ agent/credential_pool_selection.py:_select_unlocked
 * PoP: w20_credential_af0c5f_peek @ agent/credential_pool_selection.py:peek
 *
 * The mixin's state (the pool's lock, its credential list, token refresh and
 * auth.json persistence) stays upstream; entries come in as data and every
 * decision is reproduced here.
 */
#include "hermes_json.h"
#include "noc_w20_credential_af0c5f.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CRED90_MAX_ENTRIES 32
#define CRED90_ID_CAP 128

typedef struct {
    char id[CRED90_ID_CAP];
    int has_id;
    double request_count;
    double priority;
} cred90_triple;

static char *cred90_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int cred90_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* --- normalize_route_base_url (hermes_cli/route_identity.py) --------------- */

static int cred90_is_space_byte(unsigned char c) { return c <= 0x20; }
static int cred90_is_scheme_byte(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
}
static int cred90_is_digit(unsigned char c) { return c >= '0' && c <= '9'; }

/* Canonicalize only proven-equivalent endpoint URL components: lower-cased
 * scheme/host, default ports elided, one trailing slash on a query-less path
 * dropped, fragment dropped.  Anything urlsplit would not call absolute comes
 * back verbatim. */
static char *cred90_norm(const char *base_url) {
    const char *raw = base_url ? base_url : "";
    size_t n = strlen(raw), i;
    const char *sep, *netloc, *netloc_end, *at, *host_start, *host_end;
    const char *port_start = NULL, *port_end = NULL;
    const char *path_start, *path_end, *query_start = NULL, *query_end = NULL;
    int had_query = 0, default_port = 0, has_userinfo = 0;
    char scheme[32], host[256], path[1024], query[1024], userinfo[256];
    char portbuf[16];
    size_t w;

    if (!raw[0]) return cred90_dup("");
    for (i = 0; i < n; i++) {
        if (cred90_is_space_byte((unsigned char)raw[i])) return cred90_dup(raw);
        if (raw[i] == '#' ) break;
        if (raw[i] == '?') { had_query = 1; break; }
    }

    /* scheme */
    sep = strstr(raw, "://");
    if (!sep || sep == raw) return cred90_dup(raw);
    if ((size_t)(sep - raw) >= sizeof(scheme)) return cred90_dup(raw);
    if (!((raw[0] >= 'a' && raw[0] <= 'z') || (raw[0] >= 'A' && raw[0] <= 'Z'))) {
        return cred90_dup(raw);
    }
    for (i = 0; raw + i < sep; i++) {
        if (!cred90_is_scheme_byte((unsigned char)raw[i])) return cred90_dup(raw);
    }
    for (i = 0; raw + i < sep; i++) {
        char c = raw[i];
        scheme[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    scheme[sep - raw] = '\0';
    if (!scheme[0]) return cred90_dup(raw);

    netloc = sep + 3;
    netloc_end = netloc;
    while (*netloc_end && *netloc_end != '/' && *netloc_end != '?' && *netloc_end != '#') netloc_end++;
    if (netloc_end == netloc) return cred90_dup(raw);        /* no hostname */

    at = NULL;
    for (i = 0; netloc + i < netloc_end; i++) {
        if (netloc[i] == '@') at = netloc + i;
    }
    host_start = at ? at + 1 : netloc;
    if (at) {
        size_t ulen = (size_t)(at - netloc);
        has_userinfo = 1;
        if (ulen >= sizeof(userinfo)) return cred90_dup(raw);
        memcpy(userinfo, netloc, ulen);
        userinfo[ulen] = '\0';
    }
    if (host_start >= netloc_end) return cred90_dup(raw);    /* empty hostname */

    if (*host_start == '[') {
        host_end = memchr(host_start, ']', (size_t)(netloc_end - host_start));
        if (!host_end) return cred90_dup(raw);               /* urlsplit would raise */
        host_end++;
        if (host_end < netloc_end) {
            if (*host_end != ':') return cred90_dup(raw);
            port_start = host_end + 1;
            port_end = netloc_end;
        }
    } else {
        const char *colon = NULL;
        for (i = 0; host_start + i < netloc_end; i++) {
            if (host_start[i] == ':') colon = host_start + i;
        }
        host_end = colon ? colon : netloc_end;
        if (colon) {
            port_start = colon + 1;
            port_end = netloc_end;
        }
    }

    if (port_start) {
        long value = 0;
        if (port_start == port_end) return cred90_dup(raw);  /* parsed.port raises */
        for (i = 0; port_start + i < port_end; i++) {
            if (!cred90_is_digit((unsigned char)port_start[i])) return cred90_dup(raw);
            value = value * 10 + (port_start[i] - '0');
            if (value > 65535) return cred90_dup(raw);
        }
        if ((!strcmp(scheme, "http") && value == 80) ||
            (!strcmp(scheme, "https") && value == 443)) {
            default_port = 1;
        } else {
            snprintf(portbuf, sizeof(portbuf), "%ld", value);
        }
    }

    /* host, lower-cased (a %zone keeps its case) */
    {
        size_t hlen = (size_t)(host_end - host_start);
        int in_zone = 0;
        if (hlen >= sizeof(host)) return cred90_dup(raw);
        for (i = 0; i < hlen; i++) {
            char c = host_start[i];
            if (c == '%') in_zone = 1;
            if (!in_zone && c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            host[i] = c;
        }
        host[hlen] = '\0';
        /* the brackets around an IPv6 literal are part of the emitted host */
        if (*host_start == '[') {
            host[hlen - 1] = '\0';
            snprintf(host + hlen - 1, sizeof(host) - (hlen - 1), "]");
        }
    }

    path_start = netloc_end;
    path_end = path_start;
    while (*path_end && *path_end != '?' && *path_end != '#') path_end++;
    if ((size_t)(path_end - path_start) >= sizeof(path)) return cred90_dup(raw);
    memcpy(path, path_start, (size_t)(path_end - path_start));
    path[path_end - path_start] = '\0';

    if (*path_end == '?') {
        query_start = path_end + 1;
        query_end = query_start;
        while (*query_end && *query_end != '#') query_end++;
        if ((size_t)(query_end - query_start) >= sizeof(query)) return cred90_dup(raw);
        memcpy(query, query_start, (size_t)(query_end - query_start));
        query[query_end - query_start] = '\0';
    } else {
        query[0] = '\0';
    }

    {
        size_t plen = strlen(path);
        if (plen > 0 && path[plen - 1] == '/' && !had_query) path[plen - 1] = '\0';
    }

    {
        char buf[8192];
        w = 0;
        w += (size_t)snprintf(buf + w, sizeof(buf) - w, "%s://", scheme);
        if (has_userinfo) w += (size_t)snprintf(buf + w, sizeof(buf) - w, "%s@", userinfo);
        w += (size_t)snprintf(buf + w, sizeof(buf) - w, "%s", host);
        if (port_start && !default_port) w += (size_t)snprintf(buf + w, sizeof(buf) - w, ":%s", portbuf);
        w += (size_t)snprintf(buf + w, sizeof(buf) - w, "%s", path);
        if (query[0]) snprintf(buf + w, sizeof(buf) - w, "?%s", query);
        else if (had_query) snprintf(buf + w, sizeof(buf) - w, "?");
        return cred90_dup(buf);
    }
}

/* --- entry helpers -------------------------------------------------------- */

static const char *cred90_entry_url(const json_t *entry, const char **which) {
    json_t *v;
    if (!entry || entry->type != JSON_OBJECT) return NULL;
    v = json_obj_get(entry, "runtime_base_url");
    if (v && v->type == JSON_STRING && v->str_val && v->str_val[0]) {
        if (which) *which = "runtime_base_url";
        return v->str_val;
    }
    v = json_obj_get(entry, "base_url");
    if (v && v->type == JSON_STRING && v->str_val && v->str_val[0]) {
        if (which) *which = "base_url";
        return v->str_val;
    }
    return NULL;
}

/* credential_pool_entry_serves_endpoint */
static int cred90_serves(const json_t *entry, const char *endpoint_url) {
    const char *entry_url;
    char *a, *b;
    int same;
    if (!endpoint_url || !endpoint_url[0]) return 1;
    entry_url = cred90_entry_url(entry, NULL);
    if (!entry_url) return 1;
    a = cred90_norm(entry_url);
    b = cred90_norm(endpoint_url);
    same = a && b && !strcmp(a, b);
    free(a);
    free(b);
    return same;
}

static const char *cred90_id_of(const json_t *entry) {
    return json_get_str(entry, "id", "");
}

static void cred90_read_triples(const json_t *entries, cred90_triple *out, int *count) {
    int n = 0;
    size_t i;
    if (entries && entries->type == JSON_ARRAY) {
        for (i = 0; i < entries->c.count && n < CRED90_MAX_ENTRIES; i++) {
            json_t *e = entries->c.items[i];
            const char *id = cred90_id_of(e);
            snprintf(out[n].id, sizeof(out[n].id), "%s", id ? id : "");
            out[n].has_id = 1;
            out[n].request_count = json_get_num(e, "request_count", 0.0);
            out[n].priority = json_get_num(e, "priority", 0.0);
            n++;
        }
    }
    *count = n;
}

static json_t *cred90_triple_json(const cred90_triple *t) {
    json_t *o = json_object();
    json_set(o, "id", json_string(t->id));
    json_set(o, "priority", json_number(t->priority));
    json_set(o, "request_count", json_number(t->request_count));
    return o;
}

/* Entries (copied) that serve `endpoint_url`; NULL endpoint keeps them all. */
static json_t *cred90_filter_entries(const json_t *entries, const char *endpoint_url) {
    json_t *out = json_array();
    size_t i;
    if (!entries || entries->type != JSON_ARRAY) return out;
    for (i = 0; i < entries->c.count; i++) {
        json_t *e = entries->c.items[i];
        if (!cred90_serves(e, endpoint_url)) continue;
        json_append(out, json_copy(e));
    }
    return out;
}

/* --- _serving ------------------------------------------------------------- */

json_t *w20_credential_af0c5f_serving(const json_t *entries, const char *endpoint_url) {
    json_t *served = cred90_filter_entries(entries, endpoint_url);
    json_t *out = json_array();
    size_t i;
    for (i = 0; i < served->c.count; i++) {
        json_append(out, json_string(cred90_id_of(served->c.items[i])));
    }
    json_free(served);
    return out;
}

/* --- _strategy_pick ------------------------------------------------------- */

static int cred90_pick_index(const char *strategy, const cred90_triple *rows, int n, int rng_index) {
    int i, best = 0;
    if (n <= 0) return -1;
    if (strategy && !strcmp(strategy, "random")) {
        int idx = rng_index % n;
        if (idx < 0) idx += n;
        return idx;
    }
    if (strategy && !strcmp(strategy, "least_used") && n > 1) {
        for (i = 1; i < n; i++) {
            if (rows[i].request_count < rows[best].request_count) best = i;
        }
        return best;
    }
    return 0;
}

json_t *w20_credential_af0c5f_strategy_pick(const char *strategy, const json_t *entries, int rng_index) {
    cred90_triple rows[CRED90_MAX_ENTRIES];
    int n = 0, idx;
    cred90_read_triples(entries, rows, &n);
    if (!strategy) strategy = "";
    idx = cred90_pick_index(strategy, rows, n, rng_index);
    if (idx < 0) return json_null();
    return cred90_triple_json(&rows[idx]);
}

/* --- _record_pick --------------------------------------------------------- */

json_t *w20_credential_af0c5f_record_pick(const char *strategy, const json_t *entries,
                                          const char *picked_id, int count) {
    cred90_triple rows[CRED90_MAX_ENTRIES], rotated[CRED90_MAX_ENTRIES], entry;
    int n = 0, picked = -1, i, k = 0, round_robin;
    json_t *out = json_object(), *list;

    if (!strategy) strategy = "";
    if (!picked_id) picked_id = "";
    cred90_read_triples(entries, rows, &n);
    for (i = 0; i < n; i++) {
        if (!strcmp(rows[i].id, picked_id)) { picked = i; break; }
    }
    if (picked < 0) {
        json_set(out, "entry", json_null());
        json_set(out, "entries", json_array());
        json_set(out, "persisted", json_bool(0));
        return out;
    }

    /* `entry = self._adopt(entry, ..., request_count=entry.request_count + 1)`:
     * a copy -- the pool's own list keeps the old count. */
    entry = rows[picked];
    if (count) entry.request_count += 1.0;

    round_robin = !strcmp(strategy, "round_robin") && n > 1;
    if (round_robin) {
        for (i = 0; i < n; i++) {
            if (i == picked) continue;
            rotated[k++] = rows[i];
        }
        rotated[k] = entry;
        rotated[k].priority = (double)(n - 1);
        k++;
        for (i = 0; i < n; i++) {
            rotated[i].priority = (double)i;
            rows[i] = rotated[i];
        }
        /* `_find(lambda candidate: candidate.id == entry.id) or entry` */
        picked = -1;
        for (i = 0; i < n; i++) {
            if (!strcmp(rows[i].id, entry.id)) picked = i;
        }
        if (picked < 0) picked = n - 1;
        entry = rows[picked];
    }

    json_set(out, "entry", cred90_triple_json(&entry));
    list = json_array();
    for (i = 0; i < n; i++) json_append(list, cred90_triple_json(&rows[i]));
    json_set(out, "entries", list);
    json_set(out, "persisted", json_bool(round_robin ? 1 : 0));
    return out;
}

/* --- _select_unlocked ----------------------------------------------------- */

json_t *w20_credential_af0c5f_select_unlocked(const json_t *available, const json_t *pending,
                                              const char *strategy, const char *endpoint_url,
                                              int count, int rng_index) {
    cred90_triple rows[CRED90_MAX_ENTRIES];
    json_t *served, *out = json_object(), *pending_out = json_array();
    int n = 0, idx;
    size_t i;

    served = cred90_filter_entries(available, endpoint_url);
    json_set(out, "entry", json_null());
    json_set(out, "log_no_entries", json_bool(0));
    json_set(out, "throttle_cleared", json_bool(0));
    json_set(out, "persisted", json_bool(0));

    if (pending && pending->type == JSON_ARRAY) {
        for (i = 0; i < pending->c.count; i++) {
            json_append(pending_out, json_copy(pending->c.items[i]));
        }
    }
    json_set(out, "pending", pending_out);

    cred90_read_triples(served, rows, &n);
    if (n == 0) {
        json_free(served);
        json_set(out, "log_no_entries", json_bool(1));       /* _current_id = None + log */
        return out;
    }
    json_set(out, "throttle_cleared", json_bool(1));          /* _last_no_entries_log_at = None */
    if (!strategy) strategy = "";
    idx = cred90_pick_index(strategy, rows, n, rng_index);
    if (idx < 0) return out;
    {
        json_t *record = w20_credential_af0c5f_record_pick(strategy, served, rows[idx].id, count);
        json_t *entry = json_obj_get(record, "entry");
        json_set(out, "entry", entry ? json_copy(entry) : json_null());
        json_set(out, "persisted", json_copy(json_obj_get(record, "persisted")));
        json_free(record);
    }
    json_free(served);
    return out;
}

/* --- peek ----------------------------------------------------------------- */

json_t *w20_credential_af0c5f_peek(const json_t *current, const json_t *available,
                                   const json_t *model, const char *endpoint_url) {
    json_t *served;
    cred90_triple rows[CRED90_MAX_ENTRIES];
    const char *current_id = NULL;
    int n = 0, i;

    if (current && current->type == JSON_OBJECT) current_id = cred90_id_of(current);

    /* current is not None and model is None and (endpoint is None or serves()) */
    if (current && current->type == JSON_OBJECT && !cred90_truthy(model) &&
        cred90_serves(current, endpoint_url)) {
        cred90_triple one;
        one.has_id = 1;
        snprintf(one.id, sizeof(one.id), "%s", current_id ? current_id : "");
        one.request_count = json_get_num(current, "request_count", 0.0);
        one.priority = json_get_num(current, "priority", 0.0);
        return cred90_triple_json(&one);
    }

    served = cred90_filter_entries(available, endpoint_url);
    cred90_read_triples(served, rows, &n);
    json_free(served);

    /* next((e for e in available if current and e.id == current.id), first) */
    if (current_id) {
        for (i = 0; i < n; i++) {
            if (!strcmp(rows[i].id, current_id)) return cred90_triple_json(&rows[i]);
        }
    }
    if (n == 0) return json_null();
    return cred90_triple_json(&rows[0]);
}
