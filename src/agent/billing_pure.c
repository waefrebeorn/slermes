/*
 * billing_pure.c — Pure billing parser ported from agent/billing_view.py.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "libjson/json.h"
#include "hermes_json.h"

/* PoP: _parse_payment_method @ agent/billing_view.py:_parse_payment_method */
char *ts_parse_payment_method(const char *raw_json)
{
    char *err = NULL;
    json_t *raw = raw_json ? json_parse(raw_json, &err) : NULL;
    if (err) { free(err); }
    if (!raw || raw->type != JSON_OBJECT) {
        if (raw) json_free(raw);
        return strdup("{}");
    }
    json_t *kindj = json_obj_get(raw, "kind");
    const char *kind = NULL;
    if (kindj && kindj->type == JSON_STRING)
        kind = kindj->str_val;
    if (!kind || !*kind) {
        json_free(raw);
        return strdup("{}");
    }

    /* _optional_string helper — COPY values before freeing the tree */
    char *rv = NULL, *brd = NULL, *l4 = NULL, *wl = NULL, *em = NULL, *kind_c = strdup(kind);
    json_t *j;
    j = json_obj_get(raw, "resolvedVia");
    rv = (j && j->type == JSON_STRING && j->str_val) ? strdup(j->str_val) : NULL;
    j = json_obj_get(raw, "brand");
    brd = (j && j->type == JSON_STRING && j->str_val) ? strdup(j->str_val) : NULL;
    j = json_obj_get(raw, "last4");
    l4 = (j && j->type == JSON_STRING && j->str_val) ? strdup(j->str_val) : NULL;
    j = json_obj_get(raw, "wallet");
    wl = (j && j->type == JSON_STRING && j->str_val) ? strdup(j->str_val) : NULL;
    j = json_obj_get(raw, "email");
    em = (j && j->type == JSON_STRING && j->str_val) ? strdup(j->str_val) : NULL;

    json_free(raw);

    json_t *out = json_object();
    json_set(out, "kind", json_string(kind_c));
    json_set(out, "resolved_via", rv ? json_string(rv) : json_null());

    if (strcmp(kind_c, "card") == 0 && brd && l4) {
        json_set(out, "brand", json_string(brd));
        json_set(out, "last4", json_string(l4));
        json_set(out, "wallet", wl ? json_string(wl) : json_null());
        json_set(out, "email", json_null());
        json_set(out, "raw_kind", json_null());
    } else if (strcmp(kind_c, "link") == 0) {
        json_set(out, "brand", json_null());
        json_set(out, "last4", json_null());
        json_set(out, "wallet", json_null());
        json_set(out, "email", em ? json_string(em) : json_null());
        json_set(out, "raw_kind", json_null());
    } else {
        json_set(out, "brand", json_null());
        json_set(out, "last4", json_null());
        json_set(out, "wallet", json_null());
        json_set(out, "email", json_null());
        json_set(out, "kind", json_string("unknown"));
        json_set(out, "raw_kind", json_string(kind_c));
    }

    char *s = json_serialize(out);
    json_free(out);
    free(kind_c);
    free(rv); free(brd); free(l4); free(wl); free(em);
    return s;
}
