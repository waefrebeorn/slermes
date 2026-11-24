/*
 * search.c -- real C11 port of the caller-side search mapping of upstream Hermes
 *   persistence/sqlite/conversation/search.py.
 *
 * PoP: w20_search_de6deb_sort_arg @ persistence/sqlite/conversation/search.py:_SORT
 * PoP: w20_search_de6deb_optional_list @ persistence/sqlite/conversation/search.py:_optional_list
 * PoP: w20_search_de6deb_hit @ persistence/sqlite/conversation/search.py:_hit
 * PoP: w20_search_de6deb_store_call @ persistence/sqlite/conversation/search.py:SQLiteSessionSearch.search_messages
 *
 * What the module owns is here: the SearchSort -> store-argument table, the
 * optional-list coercion, the match -> SearchHit projection and the exact
 * keyword set forwarded to the store. The store handle, the holder, the
 * refusals context manager and search_messages() itself are backend seams and
 * stay upstream; the fixture scripts their answers.
 */
#include "hermes_json.h"
#include "noc_w20_search_de6deb.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *srch20_raised(const char *type, const char *message) {
    json_t *out = json_object();
    json_set(out, "status", json_string("raised"));
    json_set(out, "type", json_string(type ? type : "Exception"));
    json_set(out, "message", json_string(message ? message : ""));
    return out;
}

/* --- _SORT ---------------------------------------------------------------- */

json_t *w20_search_de6deb_sort_arg(const char *sort_name) {
    json_t *out = json_object();
    if (sort_name && (!strcmp(sort_name, "relevance"))) {
        json_set(out, "value", json_null()); /* SearchSort.RELEVANCE -> None */
    } else if (sort_name && !strcmp(sort_name, "newest")) {
        json_set(out, "value", json_string("newest"));
    } else if (sort_name && !strcmp(sort_name, "oldest")) {
        json_set(out, "value", json_string("oldest"));
    } else {
        json_set(out, "error", json_string("KeyError"));
    }
    return out;
}

/* --- _optional_list ------------------------------------------------------- */

json_t *w20_search_de6deb_optional_list(const json_t *values) {
    size_t i;
    if (!values || values->type == JSON_NULL) return json_null();
    if (values->type == JSON_ARRAY) {
        json_t *arr = json_array();
        for (i = 0; i < values->c.count; i++) json_append(arr, json_copy(values->c.items[i]));
        return arr;
    }
    if (values->type == JSON_STRING) {
        json_t *arr = json_array(); /* list("ab") == ['a', 'b'] */
        const char *s = values->str_val ? values->str_val : "";
        for (i = 0; s[i]; i++) {
            char ch[2];
            ch[0] = s[i];
            ch[1] = '\0';
            json_append(arr, json_string(ch));
        }
        return arr;
    }
    if (values->type == JSON_OBJECT) {
        json_t *arr = json_array(); /* list(dict) == its keys, in insertion order */
        for (i = 0; i < values->c.count; i++) json_append(arr, json_string(values->c.keys[i]));
        return arr;
    }
    {
        char msg[64];
        const char *tname = "object";
        if (values->type == JSON_NUMBER)
            tname = (floor(values->num_val) == values->num_val) ? "int" : "float";
        else if (values->type == JSON_BOOL) tname = "bool";
        snprintf(msg, sizeof(msg), "'%s' object is not iterable", tname);
        return srch20_raised("TypeError", msg);
    }
}

/* --- _hit ----------------------------------------------------------------- */

/* int(value) for the id field; NULL + message on ValueError. */
static int srch20_to_int(const json_t *v, long long *out, char *err, size_t err_cap) {
    if (v->type == JSON_NUMBER) {
        *out = (long long)v->num_val; /* int(float) truncates toward zero */
        return 1;
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        char *end = NULL;
        long long parsed;
        errno = 0;
        parsed = strtoll(s, &end, 10);
        if (s[0] != '\0' && end && *end == '\0' && errno == 0) {
            *out = parsed;
            return 1;
        }
        snprintf(err, err_cap, "invalid literal for int() with base 10: '%s'", s);
        return 0;
    }
    snprintf(err, err_cap, "int() argument must be a string or a real number");
    return 0;
}

/* tuple(SearchContext(row["role"], row["content"]) for row in match.get("context") or ()) */
static json_t *srch20_context(const json_t *match) {
    json_t *ctx = json_obj_get(match, "context");
    json_t *arr = json_array();
    size_t i;
    if (!ctx || ctx->type == JSON_NULL) return arr; /* `... or ()` */
    if (ctx->type != JSON_ARRAY) return arr;
    for (i = 0; i < ctx->c.count; i++) {
        json_t *row = ctx->c.items[i];
        json_t *entry, *role, *content;
        if (!row || row->type != JSON_OBJECT) {
            json_free(arr);
            if (row && row->type == JSON_STRING)
                return srch20_raised("TypeError", "string indices must be integers, not 'str'");
            return srch20_raised("TypeError", "'NoneType' object is not subscriptable");
        }
        role = json_obj_get(row, "role");       /* row["role"], not .get */
        if (!role) {
            json_free(arr);
            return srch20_raised("KeyError", "'role'");
        }
        content = json_obj_get(row, "content"); /* row["content"] */
        if (!content) {
            json_free(arr);
            return srch20_raised("KeyError", "'content'");
        }
        entry = json_object();
        json_set(entry, "role", json_copy(role));
        json_set(entry, "content", json_copy(content));
        json_append(arr, entry);
    }
    return arr;
}

json_t *w20_search_de6deb_hit(const json_t *match) {
    static const char *const PLAIN_FIELDS[] = {
        "session_id", "role", "snippet", "timestamp", "tool_name",
        "source", "model", "session_started", NULL
    };
    json_t *out, *id;
    int i;
    char err[512];

    if (!match || match->type != JSON_OBJECT) {
        return srch20_raised("AttributeError", "'NoneType' object has no attribute 'get'");
    }
    out = json_object();
    id = json_obj_get(match, "id");
    if (!id || id->type == JSON_NULL) {
        json_set(out, "message_id", json_null());
    } else {
        long long v;
        if (!srch20_to_int(id, &v, err, sizeof(err))) return srch20_raised("ValueError", err);
        json_set(out, "message_id", json_number((double)v));
    }
    for (i = 0; PLAIN_FIELDS[i]; i++) {
        json_t *v = json_obj_get(match, PLAIN_FIELDS[i]);
        json_set(out, PLAIN_FIELDS[i], v ? json_copy(v) : json_null());
    }
    {
        json_t *ctx = srch20_context(match);
        if (ctx && ctx->type == JSON_OBJECT && json_obj_get(ctx, "status")) {
            json_free(out);
            return ctx; /* the projection raised: KeyError/TypeError escapes _hit */
        }
        json_set(out, "context", ctx);
    }
    return out;
}

/* --- search_messages: the keyword set forwarded to the store -------------- */

json_t *w20_search_de6deb_store_call(const json_t *params) {
    static const char *const FILTERS[] = {"source_filter", "exclude_sources", "role_filter", NULL};
    json_t *value, *hits, *out, *sort;
    const char *sort_name = json_get_str(params, "sort", "relevance");
    int i;

    sort = w20_search_de6deb_sort_arg(sort_name);
    if (json_obj_get(sort, "error")) {
        char msg[256];
        snprintf(msg, sizeof(msg), "'%s'", sort_name);
        json_free(sort);
        return srch20_raised("KeyError", msg);
    }

    value = json_object();
    json_set(value, "query", json_string(json_get_str(params, "query", "")));
    for (i = 0; FILTERS[i]; i++) {
        json_set(value, FILTERS[i],
                 w20_search_de6deb_optional_list(json_obj_get(params, FILTERS[i])));
    }
    json_set(value, "limit", json_copy(json_obj_get(params, "limit")
                                           ? json_obj_get(params, "limit") : json_number(20.0)));
    json_set(value, "offset", json_copy(json_obj_get(params, "offset")
                                            ? json_obj_get(params, "offset") : json_number(0.0)));
    json_set(value, "sort", json_copy(json_obj_get(sort, "value")));
    json_set(value, "include_inactive",
             json_bool(json_get_bool(params, "include_inactive", 0) ? 1 : 0));
    {
        json_t *fields = json_obj_get(params, "fields");
        json_set(value, "fields", fields ? json_copy(fields) : json_null());
    }
    {
        json_t *after = json_obj_get(params, "after_ts");
        json_t *before = json_obj_get(params, "before_ts");
        json_set(value, "after_ts", after ? json_copy(after) : json_null());
        json_set(value, "before_ts", before ? json_copy(before) : json_null());
    }

    hits = json_array();
    {
        json_t *matches = json_obj_get(params, "matches");
        size_t n = (matches && matches->type == JSON_ARRAY) ? matches->c.count : 0;
        size_t j;
        for (j = 0; j < n; j++) {
            json_t *hit = w20_search_de6deb_hit(matches->c.items[j]);
            if (json_obj_get(hit, "type") && json_obj_get(hit, "status")) {
                json_free(value);
                json_free(hits);
                json_free(sort);
                return hit; /* the projection raised: same exception escapes the method */
            }
            json_append(hits, hit);
        }
    }
    json_free(sort);
    out = json_object();
    json_set(out, "status", json_string("ok"));
    {
        json_t *ok = json_object();
        json_set(ok, "kwargs", value);
        json_set(ok, "hits", hits);
        json_set(out, "value", ok);
    }
    return out;
}
