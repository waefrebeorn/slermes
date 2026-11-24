/*
 * _hosted_rooms_legacy_import.c -- real C11 port of the pure decision and
 * column-mapping surface of upstream Hermes
 * persistence/sqlite/gateway/_hosted_rooms_legacy_import.py.
 *
 * PoP: w20_hostedroom_d4f060_source_path @ persistence/sqlite/gateway/_hosted_rooms_legacy_import.py:source_path
 * PoP: w20_hostedroom_d4f060_settled @ persistence/sqlite/gateway/_hosted_rooms_legacy_import.py:settled
 * PoP: w20_hostedroom_d4f060_select_expressions @ persistence/sqlite/gateway/_hosted_rooms_legacy_import.py:_select_expressions
 * PoP: w20_hostedroom_d4f060_table_skipped @ persistence/sqlite/gateway/_hosted_rooms_legacy_import.py:_copy_rows
 * PoP: w20_hostedroom_d4f060_ordered_tables @ persistence/sqlite/gateway/_hosted_rooms_legacy_import.py:_copy_rows
 *
 * What stays upstream: _copy_rows' actual INSERT/executemany traffic and
 * import_legacy_rooms' SAVEPOINT/ROLLBACK handling, both of which need a live
 * sqlite3 connection.  The decisions they make -- is this database subject to
 * the import at all, has it already settled, which columns map to which
 * expression, which tables are skipped and in what order they are copied --
 * are pure and are ported here.
 */
#include "noc_w20_hostedroom_d4f060.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *hr_d4f060_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* --- source_path --------------------------------------------------------- */

/* PurePosixPath(p).name: trailing slashes dropped, last '/' component. */
static size_t hr_d4f060_name_start(const char *path) {
    size_t n = strlen(path), end, i, start = 0;
    while (n > 1 && path[n - 1] == '/') n--;
    end = n;
    for (i = 0; i < end; i++) {
        if (path[i] == '/') start = i + 1;
    }
    return start;
}

char *w20_hostedroom_d4f060_source_path(const char *db_path) {
    static const char *const NAME = "shared-state.db";
    static const char *const SOURCE = "state.db";
    size_t start, end, dirlen, slen, namelen;
    char *out;

    if (!db_path) return NULL;
    start = hr_d4f060_name_start(db_path);
    end = strlen(db_path);
    while (end > 1 && db_path[end - 1] == '/') end--;
    namelen = end > start ? end - start : 0;
    if (namelen != strlen(NAME) || memcmp(db_path + start, NAME, namelen)) return NULL;
    dirlen = start;
    slen = strlen(SOURCE);
    out = (char *)malloc(dirlen + slen + 1);
    if (!out) return NULL;
    memcpy(out, db_path, dirlen);
    memcpy(out + dirlen, SOURCE, slen + 1);
    return out;
}

const char *w20_hostedroom_d4f060_marker_table(void) {
    return "hosted_room_legacy_imports";
}

int w20_hostedroom_d4f060_settled(const char *db_path, int in_failed_sources,
                                  int marker_table_exists, int marker_row_present) {
    char *source = w20_hostedroom_d4f060_source_path(db_path);
    int settled;
    if (!source) return 1;                       /* no predecessor: never applies */
    settled = in_failed_sources || (marker_table_exists && marker_row_present);
    free(source);
    return settled ? 1 : 0;
}

/* --- _select_expressions ------------------------------------------------- */

static int hr_d4f060_has(const json_t *arr, const char *value) {
    size_t i;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (i = 0; i < arr->c.count; i++) {
        json_t *it = arr->c.items[i];
        if (it && it->type == JSON_STRING && !strcmp(it->str_val, value)) return 1;
    }
    return 0;
}

static void hr_d4f060_push_pair(json_t *pairs, const char *column, const char *expr) {
    json_t *pair = json_array();
    json_append(pair, json_string(column ? column : ""));
    json_append(pair, json_string(expr ? expr : ""));
    json_append(pairs, pair);
}

json_t *w20_hostedroom_d4f060_select_expressions(const char *name,
                                                 const json_t *source_columns,
                                                 const json_t *target_columns,
                                                 const json_t *legacy_columns) {
    json_t *pairs = json_array();
    const char *table = name ? name : "";
    size_t i;

    if (!source_columns || source_columns->type != JSON_ARRAY) source_columns = NULL;

    /* Columns the source and the target share copy as themselves, in source order. */
    for (i = 0; source_columns && i < source_columns->c.count; i++) {
        json_t *col = source_columns->c.items[i];
        if (!col || col->type != JSON_STRING) continue;
        if (hr_d4f060_has(target_columns, col->str_val)) {
            hr_d4f060_push_pair(pairs, col->str_val, col->str_val);
        }
    }

    /* Target-only columns take the in-place migration's default, in _LEGACY_COLUMNS order. */
    for (i = 0; legacy_columns && legacy_columns->type == JSON_ARRAY &&
                i < legacy_columns->c.count; i++) {
        json_t *row = legacy_columns->c.items[i];
        json_t *col, *def;
        if (!row || row->type != JSON_ARRAY || row->c.count < 4) continue;
        col = row->c.items[1];
        def = row->c.items[3];
        if (!col || col->type != JSON_STRING) continue;
        if (def && def->type == JSON_NULL) continue;         /* default is None */
        if (!def) continue;
        if (strcmp(row->c.items[0] && row->c.items[0]->type == JSON_STRING
                   ? row->c.items[0]->str_val : "", table)) continue;
        if (!hr_d4f060_has(target_columns, col->str_val)) continue;
        if (hr_d4f060_has(source_columns, col->str_val)) continue;
        hr_d4f060_push_pair(pairs, col->str_val, def->str_val ? def->str_val : "");
    }
    return pairs;
}

/* --- table filter and copy order ---------------------------------------- */

int w20_hostedroom_d4f060_table_skipped(const char *name) {
    static const char *const SKIP[] = {"hosted_room_driver_leases",
                                       "hosted_room_router_claims", NULL};
    size_t len;
    int i;
    if (!name) return 0;
    for (i = 0; SKIP[i]; i++) {
        if (!strcmp(name, SKIP[i])) return 1;
    }
    if (!strcmp(name, w20_hostedroom_d4f060_marker_table())) return 1;
    len = strlen(name);
    if (len >= 5 && !strcmp(name + len - 5, "_next")) return 1;
    if (len >= 10 && !strcmp(name + len - 10, "_migrating")) return 1;
    return 0;
}

static int hr_d4f060_cmp_table(const void *a, const void *b) {
    const char *x = *(const char *const *)a;
    const char *y = *(const char *const *)b;
    int nx = strcmp(x, "hosted_rooms") != 0;
    int ny = strcmp(y, "hosted_rooms") != 0;
    if (nx != ny) return nx - ny;
    return strcmp(x, y);
}

json_t *w20_hostedroom_d4f060_ordered_tables(const json_t *names) {
    const char *items[128];
    char *owned[128];
    json_t *out = json_array();
    size_t i, n = 0;

    if (!names || names->type != JSON_ARRAY) return out;
    for (i = 0; i < names->c.count && n < 128; i++) {
        json_t *it = names->c.items[i];
        if (!it || it->type != JSON_STRING) continue;
        owned[n] = hr_d4f060_dup(it->str_val);
        if (!owned[n]) break;
        items[n] = owned[n];
        n++;
    }
    qsort(items, n, sizeof(items[0]), hr_d4f060_cmp_table);
    for (i = 0; i < n; i++) {
        json_append(out, json_string(items[i]));
        free(owned[i]);
    }
    return out;
}
