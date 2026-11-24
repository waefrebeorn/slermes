/*
 * session_retry_admission.c - C11 port of upstream
 * gateway/session_retry_admission.py (gate w24_sessionret_6855aa).
 *
 * PoP: w24_sessionret_6855aa_take @ gateway/session_retry_admission.py:take
 * PoP: w24_sessionret_6855aa_rewrite @ gateway/session_retry_admission.py:RetryRewrite
 * PoP: w24_sessionret_6855aa_identity @ gateway/session_retry_admission.py:take.<is checks>
 *
 * Ported: when a bound ``/retry`` rewrite may be consumed.  ``take`` returns it
 * once -- only for the re-send itself, meaning the same SessionSource object on
 * the same store -- and null for everything else: no binding, a binding already
 * taken, an admission whose event carries another source (a drain task that
 * inherited the context), or an admission on another store.  The ContextVar, the
 * authority's storage handle, the admission write and the on_commit callback are
 * seams; object identity is carried by an identity token (equal tokens are the
 * same object, a null token is None on both sides).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_sessionret_6855aa.h"

static int sr_token_eq(const json_t *a, const json_t *b) {
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return 0;
}

static int sr_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len((json_t *)v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

int w24_sessionret_6855aa_identity(const json_t *a, const json_t *b) {
    return sr_token_eq(a, b);
}

json_t *w24_sessionret_6855aa_rewrite(const json_t *store, const json_t *source) {
    json_t *o = json_object();
    json_set(o, "store", store ? json_copy(store) : json_null());
    json_set(o, "source", source ? json_copy(source) : json_null());
    json_set(o, "taken", json_bool(false));
    json_set(o, "applied", json_bool(false));
    return o;
}

json_t *w24_sessionret_6855aa_take(const json_t *bound,
                                   const json_t *event_source,
                                   const json_t *store,
                                   int calls) {
    json_t *out = json_array();
    int taken = 0;
    int have = (bound && bound->type == JSON_OBJECT);
    const json_t *b_store = have ? json_obj_get(bound, "store") : NULL;
    const json_t *b_source = have ? json_obj_get(bound, "source") : NULL;
    const json_t *b_applied = have ? json_obj_get(bound, "applied") : NULL;
    int i;

    if (have) taken = sr_truthy(json_obj_get(bound, "taken"));
    if (calls < 0) calls = 0;
    if (calls > 8) calls = 8;                      /* bounded scripted loop */

    for (i = 0; i < calls; i++) {
        /* rewrite is None or rewrite.taken or another source or another store */
        if (!have || taken || !sr_token_eq(b_source, event_source) ||
            !sr_token_eq(b_store, store)) {
            json_append(out, json_null());
            continue;
        }
        taken = 1;                                 /* rewrite.taken = True */
        {
            json_t *row = json_object();
            json_set(row, "store", b_store ? json_copy(b_store) : json_null());
            json_set(row, "source", b_source ? json_copy(b_source) : json_null());
            json_set(row, "taken", json_bool(true));
            json_set(row, "applied",
                     b_applied ? json_copy(b_applied) : json_bool(false));
            json_append(out, row);
        }
    }
    return out;
}
