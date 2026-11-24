/*
 * gateway/hosted_rooms_legacy_import.c -- real C11 port of the pure decisions of
 * upstream Hermes gateway/hosted_rooms_legacy_import.py.
 *
 * PoP: w20_hostedroom_b5adc0_source_path @ gateway/hosted_rooms_legacy_import.py:source_path
 * PoP: w20_hostedroom_b5adc0_settled @ gateway/hosted_rooms_legacy_import.py:settled
 * PoP: w20_hostedroom_b5adc0_select_expressions @ gateway/hosted_rooms_legacy_import.py:_select_expressions
 * PoP: w20_hostedroom_b5adc0_plan_tables @ gateway/hosted_rooms_legacy_import.py:_copy_rows
 * PoP: w20_hostedroom_b5adc0_table_included @ gateway/hosted_rooms_legacy_import.py:_copy_rows
 *
 * The connections, the savepoint and the marker write stay upstream; every
 * value here is a pure function of the schema and row metadata.
 */
#include "hermes_json.h"
#include "noc_w20_hostedroom_b5adc0.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HRLI_SOURCE_NAME "state.db"
#define HRLI_MARKER_TABLE "hosted_room_legacy_imports"

static const char *hrli_name_part(const char *path, size_t *start) {
    const char *p, *cut = NULL;
    if (!path) return "";
    for (p = path; *p; p++) {
        if (*p == '/' || *p == '\\') cut = p;
    }
    if (cut) {
        *start = (size_t)(cut - path) + 1;
        return cut + 1;
    }
    *start = 0;
    return path;
}

json_t *w20_hostedroom_b5adc0_source_path(const char *db_path) {
    size_t start = 0;
    const char *name = hrli_name_part(db_path, &start);
    json_t *out;
    if (strcmp(name, "shared-state.db")) return json_null();
    {
        size_t n = start;                      /* bytes before the name */
        size_t i;
        char *buf = (char *)malloc(n + strlen(HRLI_SOURCE_NAME) + 1);
        if (!buf) return json_null();
        memcpy(buf, db_path, n);
        strcpy(buf + n, HRLI_SOURCE_NAME);
        for (i = 0; i < n; i++) {              /* posix-normalized, as the oracle compares */
            if (buf[i] == '\\') buf[i] = '/';
        }
        out = json_string(buf);
        free(buf);
    }
    return out;
}

int w20_hostedroom_b5adc0_table_included(const char *name) {
    static const char *const SKIP[] = {"hosted_room_driver_leases",
                                       "hosted_room_router_claims", NULL};
    size_t n;
    int i;
    if (!name) return 0;
    for (i = 0; SKIP[i]; i++) {
        if (!strcmp(name, SKIP[i])) return 0;
    }
    if (!strcmp(name, HRLI_MARKER_TABLE)) return 0;
    n = strlen(name);
    if (n >= 5 && !strcmp(name + n - 5, "_next")) return 0;
    if (n >= 10 && !strcmp(name + n - 10, "_migrating")) return 0;
    return 1;
}

json_t *w20_hostedroom_b5adc0_settled(const char *db_path, int source_failed,
                                      int table_exists, int marker_row) {
    json_t *source = w20_hostedroom_b5adc0_source_path(db_path);
    int settled;
    if (source->type == JSON_NULL) {
        json_free(source);
        return json_bool(1);
    }
    json_free(source);
    if (source_failed) return json_bool(1);
    settled = table_exists && marker_row;
    return json_bool(settled ? 1 : 0);
}

/* ---------- per-table copy plan ---------- */

static int hrli_cmp_names(const void *a, const void *b) {
    const char *x = *(const char *const *)a;
    const char *y = *(const char *const *)b;
    int xp = strcmp(x, "hosted_rooms") != 0;
    int yp = strcmp(y, "hosted_rooms") != 0;
    if (xp != yp) return xp - yp;
    return strcmp(x, y);
}

json_t *w20_hostedroom_b5adc0_plan_tables(const json_t *names) {
    json_t *kept = json_array(), *out;
    const char **buf;
    size_t n = 0, i, cap;
    if (!names || names->type != JSON_ARRAY) return kept;
    cap = names->c.count + 1;
    buf = (const char **)calloc(cap, sizeof(char *));
    if (!buf) return kept;
    for (i = 0; i < names->c.count; i++) {
        json_t *v = names->c.items[i];
        if (!v || v->type != JSON_STRING) continue;
        if (!w20_hostedroom_b5adc0_table_included(v->str_val)) continue;
        buf[n++] = v->str_val;
    }
    qsort(buf, n, sizeof(buf[0]), hrli_cmp_names);
    out = json_array();
    for (i = 0; i < n; i++) json_append(out, json_string(buf[i]));
    free(buf);
    json_free(kept);
    return out;
}

/* ---------- _select_expressions ---------- */

static int hrli_in_array(const char *key, const json_t *arr) {
    size_t i;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (i = 0; i < arr->c.count; i++) {
        json_t *v = arr->c.items[i];
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, key)) return 1;
    }
    return 0;
}

json_t *w20_hostedroom_b5adc0_select_expressions(const char *name,
                                                 const json_t *source_cols,
                                                 const json_t *target_cols,
                                                 const json_t *defaults) {
    json_t *out = json_array();
    size_t i;
    for (i = 0; source_cols && source_cols->type == JSON_ARRAY && i < source_cols->c.count; i++) {
        json_t *c = source_cols->c.items[i];
        json_t *pair;
        if (!c || c->type != JSON_STRING) continue;
        if (!hrli_in_array(c->str_val, target_cols)) continue;
        pair = json_array();
        json_append(pair, json_string(c->str_val));
        json_append(pair, json_string(c->str_val));
        json_append(out, pair);
    }
    for (i = 0; defaults && defaults->type == JSON_ARRAY && i < defaults->c.count; i++) {
        json_t *row = defaults->c.items[i];
        json_t *table, *column, *value, *pair;
        if (!row || row->type != JSON_ARRAY || row->c.count < 3) continue;
        table = row->c.items[0];
        column = row->c.items[1];
        value = row->c.items[2];
        if (!table || table->type != JSON_STRING || strcmp(table->str_val, name ? name : "")) continue;
        if (!column || column->type != JSON_STRING) continue;
        if (!hrli_in_array(column->str_val, target_cols)) continue;
        if (hrli_in_array(column->str_val, source_cols)) continue;
        pair = json_array();
        json_append(pair, json_string(column->str_val));
        json_append(pair, value ? json_copy(value) : json_null());
        json_append(out, pair);
    }
    return out;
}
