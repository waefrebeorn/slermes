/*
 * display.c - C11 port of upstream tools/connectors/display.py
 * (gate w24_display_330acd).
 *
 * PoP: w24_display_330acd_connected_slugs @ tools/connectors/display.py:connected_slugs
 * PoP: w24_display_330acd_apply_answer @ tools/connectors/display.py:apply_answer
 * PoP: w24_display_330acd_result @ tools/connectors/display.py:_result
 * PoP: w24_display_330acd_constants @ tools/connectors/display.py:OFF_DESKTOP_NOTE,SettleReason
 *
 * What is ported is the card's decision core: the account set read off the
 * client's connector list (lower-cased, only the rows the backend reports as
 * connected), what a word typed in the composer does to the operation (store
 * it, settle it by "continue"), and the result the card freezes -- including
 * the "a user who connected an app and walked away still gets it reported"
 * timed_out flag -- plus the off-desktop note.
 *
 * Seams, deliberately not ported: the tool thread (open_and_drive), the
 * connection callback, the card renderer, the deadline clock and the client's
 * list_connectors() call.  Those become the arguments: the connector rows, the
 * before-set, the settle reason.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_display_330acd.h"

/* OFF_DESKTOP_NOTE */
static const char *const DISPLAY_OFF_DESKTOP_NOTE =
    "The connector card shows only in the Hermes desktop app. Tell the user to "
    "connect apps from the desktop app or the portal.";

/* SettleReason */
static const char *const DISPLAY_SETTLE_REASONS[W24_DISPLAY_330ACD_SETTLE_REASONS] = {
    "all_resolved", "continue", "deadline", "interrupt",
};

/* OPERATION_DEADLINE_SECONDS: as long as a connect link lives (10 minutes). */
static const double DISPLAY_DEADLINE_SECONDS = 600.0;

static int dv_truthy(const json_t *v) {
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

/* str() of the scalar shapes a connector name holds; NULL for a container. */
static char *dv_pystr(const json_t *v) {
    char *buf;
    double d;

    if (!v || v->type == JSON_NULL) {
        buf = (char *)malloc(5);
        if (buf) strcpy(buf, "None");
        return buf;
    }
    if (v->type == JSON_STRING) {
        size_t n = strlen(v->str_val ? v->str_val : "");
        buf = (char *)malloc(n + 1);
        if (buf) memcpy(buf, v->str_val ? v->str_val : "", n + 1);
        return buf;
    }
    if (v->type == JSON_BOOL) {
        const char *t = v->bool_val ? "True" : "False";
        buf = (char *)malloc(strlen(t) + 1);
        if (buf) strcpy(buf, t);
        return buf;
    }
    if (v->type == JSON_NUMBER) {
        buf = (char *)malloc(40);
        if (!buf) return NULL;
        d = v->num_val;
        if (d == (double)(long long)d && d > -1e17 && d < 1e17) snprintf(buf, 40, "%.0f", d);
        else snprintf(buf, 40, "%.15g", d);
        return buf;
    }
    return NULL;
}

static void dv_lower(char *s) {
    for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

/* str.strip() over ASCII whitespace, in place. */
static void dv_strip(char *s) {
    size_t n;
    char *p = s;

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    n = strlen(s);
    while (n > 0) {
        char c = s[n - 1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') s[--n] = '\0';
        else break;
    }
}

static int dv_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

typedef struct { char **v; size_t n, cap; } dv_set;

static int dv_set_add(dv_set *s, char *owned) {
    size_t i;
    for (i = 0; i < s->n; i++) {
        if (strcmp(s->v[i], owned) == 0) { free(owned); return 1; }
    }
    if (s->n == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 8;
        char **nv = (char **)realloc(s->v, cap * sizeof(char *));
        if (!nv) { free(owned); return 0; }
        s->v = nv; s->cap = cap;
    }
    s->v[s->n++] = owned;
    return 1;
}

static int dv_set_has(const dv_set *s, const char *name) {
    size_t i;
    for (i = 0; i < s->n; i++) if (strcmp(s->v[i], name) == 0) return 1;
    return 0;
}

static void dv_set_free(dv_set *s) {
    size_t i;
    for (i = 0; i < s->n; i++) free(s->v[i]);
    free(s->v);
    s->v = NULL; s->n = s->cap = 0;
}

/* {str(i.get("connector", "")).lower() for i in client.list_connectors()
 *  if isinstance(i, dict) and i.get("connected")} */
static int dv_connected(const json_t *items, dv_set *out) {
    size_t i, n = json_len(items);

    for (i = 0; i < n; i++) {
        const json_t *row = json_get(items, i);
        const json_t *conn;
        char *name;
        if (!row || row->type != JSON_OBJECT) continue;
        if (!dv_truthy(json_obj_get(row, "connected"))) continue;
        conn = json_obj_get(row, "connector");
        name = conn ? dv_pystr(conn) : NULL;
        if (!name) {                 /* .get("connector", "") */
            name = (char *)malloc(1);
            if (!name) return 0;
            name[0] = '\0';
        }
        dv_lower(name);
        if (!dv_set_add(out, name)) return 0;
    }
    return 1;
}

static json_t *dv_sorted_array(const dv_set *s) {
    json_t *arr = json_array();
    char **copy;
    size_t i;

    if (s->n == 0) return arr;
    copy = (char **)malloc(s->n * sizeof(char *));
    if (!copy) return arr;
    for (i = 0; i < s->n; i++) copy[i] = s->v[i];
    qsort(copy, s->n, sizeof(char *), dv_cmp);
    for (i = 0; i < s->n; i++) json_append(arr, json_string(copy[i]));
    free(copy);
    return arr;
}

json_t *w24_display_330acd_connected_slugs(const json_t *items) {
    dv_set set = { NULL, 0, 0 };
    json_t *out;

    if (!dv_connected(items, &set)) {
        dv_set_free(&set);
        return json_array();
    }
    out = dv_sorted_array(&set);
    dv_set_free(&set);
    return out;
}

json_t *w24_display_330acd_apply_answer(const json_t *raw) {
    json_t *answer, *out;
    const json_t *said_v, *by;
    char *said = NULL;
    int settled;

    if (!raw || raw->type != JSON_STRING) return json_null();        /* json.loads() seam */
    answer = json_parse(raw->str_val ? raw->str_val : "", NULL);
    if (!answer) return json_null();                                 /* JSONDecodeError seam */
    if (answer->type != JSON_OBJECT) { json_free(answer); return json_null(); }

    said_v = json_obj_get(answer, "said");
    if (dv_truthy(said_v)) {
        said = dv_pystr(said_v);
        if (!said) { json_free(answer); return json_null(); }        /* str() seam */
        dv_strip(said);
    }
    by = json_obj_get(answer, "settled_by");
    /* if said or answer.get("settled_by") == SettleReason.continue_.value */
    settled = (said && said[0]) || (by && by->type == JSON_STRING && by->str_val &&
                                    strcmp(by->str_val, "continue") == 0);
    out = json_object();
    json_set(out, "said", (said && said[0]) ? json_string(said) : json_null());
    json_set(out, "settled", json_bool(settled));
    json_set(out, "settled_by", settled ? json_string("continue") : json_null());
    free(said);
    json_free(answer);
    return out;
}

json_t *w24_display_330acd_result(const json_t *items, const json_t *before,
                                  const json_t *suggested, const json_t *said,
                                  const json_t *settled_by) {
    dv_set after = { NULL, 0, 0 }, prior = { NULL, 0, 0 }, fresh = { NULL, 0, 0 };
    json_t *out;
    size_t i, n, j;

    if (!dv_connected(items, &after)) {
        dv_set_free(&after);
        return json_object();
    }
    /* before: the set captured when the card opened */
    n = json_len(before);
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(before, i);
        char *name;
        if (!v || v->type != JSON_STRING || !v->str_val) continue;
        name = dv_pystr(v);
        if (!name) continue;
        dv_set_add(&prior, name);
    }
    /* newly_connected = sorted(after - before) */
    for (j = 0; j < after.n; j++) {
        char *copy;
        if (dv_set_has(&prior, after.v[j])) continue;
        copy = (char *)malloc(strlen(after.v[j]) + 1);
        if (!copy) continue;
        strcpy(copy, after.v[j]);
        dv_set_add(&fresh, copy);
    }

    out = json_object();
    json_set(out, "connected", dv_sorted_array(&after));
    json_set(out, "newly_connected", dv_sorted_array(&fresh));
    json_set(out, "suggested", suggested && suggested->type != JSON_NULL ? json_copy(suggested) : json_array());
    if (dv_truthy(said)) json_set(out, "said", json_copy(said));
    /* A user who connected an app and walked away still gets it reported. */
    if (settled_by && settled_by->type == JSON_STRING && settled_by->str_val &&
        strcmp(settled_by->str_val, "deadline") == 0)
        json_set(out, "timed_out", json_bool(1));

    dv_set_free(&after);
    dv_set_free(&prior);
    dv_set_free(&fresh);
    return out;
}

json_t *w24_display_330acd_constants(void) {
    json_t *o = json_object();
    json_t *reasons = json_array();
    size_t i;

    for (i = 0; i < W24_DISPLAY_330ACD_SETTLE_REASONS; i++)
        json_append(reasons, json_string(DISPLAY_SETTLE_REASONS[i]));
    json_set(o, "off_desktop_note", json_string(DISPLAY_OFF_DESKTOP_NOTE));
    json_set(o, "deadline_seconds", json_number(DISPLAY_DEADLINE_SECONDS));
    json_set(o, "settle_reasons", reasons);
    return o;
}
