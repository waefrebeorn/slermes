/*
 * web_server_file_tickets.c — C11 port of upstream Hermes hermes_cli/web_server_file_tickets.py
 * (gate w26_webserverf_e4be3e).
 * STATUS: PORTED — behavior verified by tests/oracle_w26_webserverf_e4be3e.py.
 * PoP: w26_webserverf_e4be3e_scope @ hermes_cli/web_server_file_tickets.py:_scope
 * PoP: w26_webserverf_e4be3e_mint @ hermes_cli/web_server_file_tickets.py:mint
 * PoP: w26_webserverf_e4be3e_redeem @ hermes_cli/web_server_file_tickets.py:redeem
 * PoP: w26_webserverf_e4be3e_constants @ hermes_cli/web_server_file_tickets.py:<module constants>
 *
 * Seams: the threading lock, `secrets.token_urlsafe`, `time.monotonic` and the
 * browser-credential generation live outside the port.  They arrive as `token`,
 * `now`, `session_token` and a `current` generation map, so the ported decision
 * is the one the module actually makes: scope equality, expiry, use spending,
 * the store caps, and the generation revocation on sign-out.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_webserverf_e4be3e.h"

/* Module-level literal constants mirrored from the Python source. */
static const long long FT_DOWNLOAD_TTL_SECONDS = 60;
static const long long FT_DOWNLOAD_MAX_REDEMPTIONS = 3;
static const long long FT_STREAM_TTL_SECONDS = 60 * 60;
static const long long FT_MAX_TICKETS = 1024;

#define FT_ROUTE_DOWNLOAD "/api/fs/download"
#define FT_ROUTE_STREAM "/api/files/stream"

typedef struct {
    const char *k;
    const char *v;
} ft_pair;

static int ft_pair_cmp(const void *pa, const void *pb) {
    const ft_pair *a = (const ft_pair *)pa;
    const ft_pair *b = (const ft_pair *)pb;
    int c = strcmp(a->k, b->k);
    return c ? c : strcmp(a->v, b->v);
}

/* Python truthiness of a JSON value. */
static int ft_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static json_t *ft_err(const char *name) {
    json_t *o = json_object();
    json_set(o, "error", json_string(name));
    return o;
}

/* Collect [[k,v],...] entries and sort them the way tuple(sorted(query)) does. */
static ft_pair *ft_collect(const json_t *pairs, size_t *n) {
    ft_pair *arr;
    size_t i, cnt;
    *n = 0;
    if (!pairs || pairs->type != JSON_ARRAY) return NULL;
    cnt = json_len(pairs);
    if (!cnt) return NULL;
    arr = (ft_pair *)calloc(cnt, sizeof(ft_pair));
    if (!arr) return NULL;
    for (i = 0; i < cnt; i++) {
        const json_t *p = json_get(pairs, i);
        if (p && p->type == JSON_ARRAY && json_len(p) >= 2) {
            const json_t *k = json_get(p, 0);
            const json_t *v = json_get(p, 1);
            arr[*n].k = (k && k->type == JSON_STRING) ? k->str_val : "";
            arr[*n].v = (v && v->type == JSON_STRING) ? v->str_val : "";
            (*n)++;
        }
    }
    if (*n) qsort(arr, *n, sizeof(ft_pair), ft_pair_cmp);
    return arr;
}

static json_t *ft_query_json(const ft_pair *pairs, size_t n) {
    json_t *arr = json_array();
    size_t i;
    for (i = 0; i < n; i++) {
        json_t *p = json_array();
        json_append(p, json_string(pairs[i].k));
        json_append(p, json_string(pairs[i].v));
        json_append(arr, p);
    }
    return arr;
}

static json_t *ft_scope_obj(const char *route_path, const ft_pair *pairs, size_t n) {
    json_t *o = json_object();
    json_set(o, "route", json_string(route_path ? route_path : ""));
    json_set(o, "query", ft_query_json(pairs, n));
    return o;
}

json_t *w26_webserverf_e4be3e_scope(const char *route_path, const json_t *query) {
    size_t n = 0;
    ft_pair *pairs = ft_collect(query, &n);
    json_t *out = ft_scope_obj(route_path, pairs, n);
    free(pairs);
    return out;
}

/* The store as JSON: {token: {route, query, expires_at, uses_left, generation}}.
 * Python dict equality ignores order, so the emitted objects only need shape. */
static json_t *ft_store_of(const json_t *req) {
    const json_t *store = json_obj_get(req, "store");
    if (store && store->type == JSON_OBJECT) return json_copy(store);
    return json_object();
}

static double ft_num_or(const json_t *v, double def) {
    if (v && v->type == JSON_NUMBER) return v->num_val;
    if (v && v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

/* Stale tickets are deleted first, then the oldest keys make room. */
static void ft_prune(json_t *store, double now, long long max_tickets) {
    int guard = 0;
    for (;;) {
        size_t i, cnt = json_object_size(store);
        int dropped = 0;
        for (i = 0; i < cnt; i++) {
            const char *k = json_object_get_key_at(store, i);
            const json_t *e = json_object_get_at(store, i);
            const json_t *exp = e ? json_obj_get(e, "expires_at") : NULL;
            if (k && exp && ft_num_or(exp, 0.0) <= now) {
                json_obj_del(store, k);
                dropped = 1;
                break;
            }
        }
        if (!dropped) break;
        if (++guard > 100000) break;
    }
    while ((long long)json_object_size(store) >= max_tickets) {
        const char *k = json_object_get_key_at(store, 0);
        if (!k) break;
        json_obj_del(store, k);
    }
}

json_t *w26_webserverf_e4be3e_mint(const json_t *req) {
    const char *route = json_get_str(req, "route", "");
    const json_t *query = json_obj_get(req, "query");
    const char *token = json_get_str(req, "token", "");
    const json_t *sess = json_obj_get(req, "session_token");
    double now = ft_num_or(json_obj_get(req, "now"), 0.0);
    long long ttl_dl = (long long)ft_num_or(json_obj_get(req, "ttl_download"), (double)FT_DOWNLOAD_TTL_SECONDS);
    long long ttl_st = (long long)ft_num_or(json_obj_get(req, "ttl_stream"), (double)FT_STREAM_TTL_SECONDS);
    long long max_tickets = (long long)ft_num_or(json_obj_get(req, "max_tickets"), (double)FT_MAX_TICKETS);
    int download;
    const char *route_path;
    ft_pair *pairs = NULL;
    size_t n = 0, i;
    json_t *store, *entry, *out;

    if (!strcmp(route, "download")) {
        download = 1;
        route_path = FT_ROUTE_DOWNLOAD;
    } else if (!strcmp(route, "stream")) {
        download = 0;
        route_path = FT_ROUTE_STREAM;
    } else {
        return ft_err("KeyError");
    }

    if (query && query->type == JSON_OBJECT) {
        size_t cnt = json_object_size(query);
        if (cnt) {
            pairs = (ft_pair *)calloc(cnt, sizeof(ft_pair));
            if (!pairs) return json_null();
            for (i = 0; i < cnt; i++) {
                const char *k = json_object_get_key_at(query, i);
                const json_t *v = json_object_get_at(query, i);
                pairs[n].k = k ? k : "";
                pairs[n].v = (v && v->type == JSON_STRING) ? v->str_val : "";
                n++;
            }
            if (n) qsort(pairs, n, sizeof(ft_pair), ft_pair_cmp);
        }
    }

    store = ft_store_of(req);
    ft_prune(store, now, max_tickets);

    entry = ft_scope_obj(route_path, pairs, n);
    free(pairs);
    json_set(entry, "expires_at", json_number(now + (double)(download ? ttl_dl : ttl_st)));
    json_set(entry, "uses_left", download ? json_number((double)FT_DOWNLOAD_MAX_REDEMPTIONS) : json_null());
    json_set(entry, "generation", (sess && sess->type == JSON_STRING) ? json_string(sess->str_val)
                                                                     : json_null());
    json_set(store, token, entry);

    out = json_object();
    json_set(out, "ticket", json_string(token));
    json_set(out, "store", store);
    return out;
}

/* Compare an entry's scope with (route, sorted pairs). */
static int ft_scope_eq(const json_t *entry, const char *route_path, const ft_pair *pairs, size_t n) {
    const json_t *r = entry ? json_obj_get(entry, "route") : NULL;
    const json_t *q = entry ? json_obj_get(entry, "query") : NULL;
    size_t i;
    if (!r || r->type != JSON_STRING || strcmp(r->str_val ? r->str_val : "", route_path ? route_path : ""))
        return 0;
    if (!q || q->type != JSON_ARRAY || json_len(q) != n) return 0;
    for (i = 0; i < n; i++) {
        const json_t *p = json_get(q, i);
        const json_t *k, *v;
        if (!p || p->type != JSON_ARRAY || json_len(p) < 2) return 0;
        k = json_get(p, 0);
        v = json_get(p, 1);
        if (!k || k->type != JSON_STRING || strcmp(k->str_val ? k->str_val : "", pairs[i].k)) return 0;
        if (!v || v->type != JSON_STRING || strcmp(v->str_val ? v->str_val : "", pairs[i].v)) return 0;
    }
    return 1;
}

static json_t *ft_redeem_result(int ok, json_t *store) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(ok));
    json_set(o, "store", store);
    json_set(o, "authority", json_bool(ok));
    return o;
}

json_t *w26_webserverf_e4be3e_redeem(const json_t *req) {
    const char *method = json_get_str(req, "method", "GET");
    const char *path = json_get_str(req, "path", "");
    const json_t *query = json_obj_get(req, "query");
    const json_t *current = json_obj_get(req, "current");
    double now = ft_num_or(json_obj_get(req, "now"), 0.0);
    ft_pair *pairs = NULL;
    size_t n = 0, i, m = 0;
    char ticket[4096];
    json_t *store, *entry;
    const json_t *uses;
    const json_t *gen;

    ticket[0] = '\0';
    pairs = ft_collect(query, &n);
    /* First ?ticket= wins; everything else stays in the scope. */
    {
        ft_pair *filtered = NULL;
        size_t found = 0;
        if (n) filtered = (ft_pair *)calloc(n, sizeof(ft_pair));
        for (i = 0; i < n; i++) {
            if (!found && !strcmp(pairs[i].k, "ticket")) {
                snprintf(ticket, sizeof(ticket), "%s", pairs[i].v);
                found = 1;
                continue;
            }
            if (filtered) filtered[m++] = pairs[i];
        }
        free(pairs);
        pairs = filtered;
        if (!pairs) n = 0;
    }

    store = ft_store_of(req);
    if (!ticket[0] || (strcmp(method, "GET") && strcmp(method, "HEAD"))) {
        free(pairs);
        return ft_redeem_result(0, store);
    }
    entry = json_obj_get(store, ticket);
    if (!entry || entry->type != JSON_OBJECT || !ft_scope_eq(entry, path, pairs, m)) {
        free(pairs);
        return ft_redeem_result(0, store);
    }
    free(pairs);
    if (ft_num_or(json_obj_get(entry, "expires_at"), 0.0) <= now)
        return ft_redeem_result(0, store);

    gen = json_obj_get(entry, "generation");
    if (gen && gen->type == JSON_STRING) {
        const json_t *cur = current ? json_obj_get(current, gen->str_val) : NULL;
        if (!ft_truthy(cur)) {
            json_obj_del(store, ticket);
            return ft_redeem_result(0, store);
        }
    }

    uses = json_obj_get(entry, "uses_left");
    if (uses && uses->type == JSON_NUMBER) {
        long long left = (long long)uses->num_val;
        if (left > 1) {
            json_t *replacement = json_copy(entry);
            json_set(replacement, "uses_left", json_number((double)(left - 1)));
            json_set(store, ticket, replacement);
        } else {
            json_obj_del(store, ticket);
        }
    }
    return ft_redeem_result(1, store);
}

json_t *w26_webserverf_e4be3e_constants(void) {
    json_t *o = json_object(), *routes = json_object();
    json_set(o, "download_ttl_seconds", json_number((double)FT_DOWNLOAD_TTL_SECONDS));
    json_set(o, "download_max_redemptions", json_number((double)FT_DOWNLOAD_MAX_REDEMPTIONS));
    json_set(o, "stream_ttl_seconds", json_number((double)FT_STREAM_TTL_SECONDS));
    json_set(o, "max_tickets", json_number((double)FT_MAX_TICKETS));
    json_set(routes, "download", json_string(FT_ROUTE_DOWNLOAD));
    json_set(routes, "stream", json_string(FT_ROUTE_STREAM));
    json_set(o, "routes", routes);
    return o;
}
