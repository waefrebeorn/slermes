/*
 * _recovery.c - C11 home for upstream Hermes persistence/sqlite/admin/_recovery.py.
 *
 * Offline, non-destructive recovery of a damaged session database.  Only the
 * pure decision core is ported here; every sqlite3 handle, PRAGMA row, stat()
 * and disk-usage reading is a seam turned into a fixed fixture input by the
 * differential gates.  Ported symbols (PoP per function below):
 *
 *   gate w28_recovery_eb8799_p1 -- paths, statements, inventory
 *     _sidecar_path           -> w28_rec_p1_sidecar_path
 *     _format_bytes           -> w28_rec_p1_format_bytes
 *     _disk_space_preflight   -> w28_rec_p1_disk_space_preflight
 *     _quoted_columns         -> w28_rec_p1_quoted_columns
 *     _compatible_columns     -> w28_rec_p1_compatible_columns
 *     _table_inventory        -> w28_rec_p1_table_inventory
 *     _inspect_connection     -> w28_rec_p1_inspect_connection
 *     _append_skipped_range   -> w28_rec_p1_append_skipped_range
 *     _state_meta_precheck    -> w28_rec_p1_state_meta_precheck  (with _state_meta_result)
 *     _reconcile              -> w28_rec_p1_reconcile
 *
 *   gate w28_recovery_eb8799_p2 -- salvage geometry, verification classification, report
 *     _probe_populated_edge   -> w28_rec_p2_probe_populated_edge
 *     _salvage_rowid_bounds   -> w28_rec_p2_salvage_rowid_bounds
 *     _repair_out_of_window_timestamps -> w28_rec_p2_repair_plan
 *     _verify_structure       -> w28_rec_p2_verify_structure
 *     _verify_row_counts      -> w28_rec_p2_verify_row_counts
 *     _lost_and_found_plausibility_errors -> w28_rec_p2_lost_and_found_plausibility_errors
 *     _recovery_report        -> w28_rec_p2_recovery_report
 *
 * NOT ported (seams, not logic): copy_source_bundle/offline_file_access,
 * type=sqlite3 connections, tempfile.TemporaryDirectory, shutil.copy2/disk_usage,
 * StoreHolder/_fresh_destination, exclusive_maintenance, .recover subprocess.
 */
#define _POSIX_C_SOURCE 200809L

#include "noc_w28_recovery_eb8799_p1.h"
#include "noc_w28_recovery_eb8799_p2.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Mirrors of the upstream constants this lane's decisions are pinned to. */
#define REC_P1_SCHEMA_VERSION 33
#define REC_P1_FTS_STORAGE_VERSION 3
#define REC_P1_EPOCH_MIN 0.0
#define REC_P1_EPOCH_MAX 4200000000.0
#define REC_P1_EPOCH_LOW 1000000000.0
#define REC_P1_STUB_TITLE_PREFIX "[best-effort recovered"
#define REC_P1_MIN_SQLITE_ROWID (-9223372036854775807.0 - 1.0)
#define REC_P1_MAX_SQLITE_ROWID 9223372036854775807.0
#define REC_P1_MAX_SALVAGE_RANGE_QUERIES 10000

/* _INVENTORY_TABLES, in the upstream tuple order. */
static const char *const REC_P1_INVENTORY_TABLES[] = {
    "system_prompts", "sessions", "messages", "session_model_usage", "compression_locks",
    "gateway_routing", "async_delegations", "state_meta", "telegram_dm_topic_mode",
    "telegram_dm_topic_bindings", "delivery_obligations", "runtime_epoch", "session_admissions",
    "worker_executions", "worker_receipts", NULL,
};
#define REC_P1_INVENTORY_COUNT 15

/* _GENERATED_META_KEYS as a frozenset upstream; sorted() decides the emitted order. */
static const char *const REC_P1_GENERATED_META_KEYS[] = {
    "fts_storage_version", "fts_optimize_available", "fts_rebuild_high_water",
    "fts_rebuild_progress", "fts_cjk_stale", "fts_cjk_rebuild_high_water",
    "fts_cjk_rebuild_progress", "telegram_dm_topic_schema_version",
    "fts_tool_full_content_high_water", NULL,
};
#define REC_P1_GENERATED_META_COUNT 9

/* === small string builder (SQL text can outgrow a fixed buffer) === */
typedef struct {
    char *p;
    size_t len;
    size_t cap;
} rec_sb;

static void rec_sb_init(rec_sb *b) {
    b->cap = 128;
    b->len = 0;
    b->p = (char *)malloc(b->cap);
    if (!b->p) abort();
    b->p[0] = '\0';
}

static void rec_sb_need(rec_sb *b, size_t extra) {
    if (b->len + extra + 1 <= b->cap) return;
    while (b->len + extra + 1 > b->cap) b->cap *= 2;
    b->p = (char *)realloc(b->p, b->cap);
    if (!b->p) abort();
}

static void rec_sb_puts(rec_sb *b, const char *s) {
    size_t n = strlen(s);
    rec_sb_need(b, n);
    memcpy(b->p + b->len, s, n + 1);
    b->len += n;
}

static void rec_sb_printf(rec_sb *b, const char *fmt, ...) {
    va_list ap;
    va_list ap2;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    rec_sb_need(b, (size_t)n);
    va_start(ap2, fmt);
    vsnprintf(b->p + b->len, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    b->len += (size_t)n;
}

static char *rec_sb_take(rec_sb *b) {
    char *out = b->p;
    b->p = NULL;
    b->len = b->cap = 0;
    return out;
}

static int rec_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static int rec_array_has_str(const json_t *arr, const char *needle) {
    size_t i;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (i = 0; i < json_len(arr); i++) {
        const char *v = json_string_value(json_get(arr, i));
        if (v && strcmp(v, needle) == 0) return 1;
    }
    return 0;
}

/* === _sidecar_path ===
 * PoP: w28_rec_p1_sidecar_path @ persistence/sqlite/admin/_recovery.py:_sidecar_path */
json_t *w28_rec_p1_sidecar_path(const char *path, const char *suffix) {
    rec_sb b;
    const char *slash;
    size_t prefix;
    if (!path) return json_null();
    if (!suffix || !*suffix) return json_string(path);
    slash = strrchr(path, '/');
    prefix = slash ? (size_t)(slash - path) + 1 : 0;
    rec_sb_init(&b);
    {
        char *head = (char *)malloc(prefix + 1);
        memcpy(head, path, prefix);
        head[prefix] = '\0';
        rec_sb_puts(&b, head);
        free(head);
    }
    rec_sb_puts(&b, path + prefix);
    rec_sb_puts(&b, suffix);
    {
        char *out = rec_sb_take(&b);
        json_t *node = json_string(out);
        free(out);
        return node;
    }
}

/* === _format_bytes ===
 * PoP: w28_rec_p1_format_bytes @ persistence/sqlite/admin/_recovery.py:_format_bytes */
static void rec_p1_format_bytes(double value, char *buf, size_t cap) {
    static const char *const units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double amount = value;
    int i;
    for (i = 0; i < 5; i++) {
        if (amount < 1024.0 || i == 4) {
            snprintf(buf, cap, "%.1f %s", amount, units[i]);
            return;
        }
        amount /= 1024.0;
    }
    snprintf(buf, cap, "%.1f %s", amount, units[4]);
}

json_t *w28_rec_p1_format_bytes(double value) {
    char buf[64];
    rec_p1_format_bytes(value, buf, sizeof buf);
    return json_string(buf);
}

/* === _disk_space_preflight ===
 * PoP: w28_rec_p1_disk_space_preflight @ persistence/sqlite/admin/_recovery.py:_disk_space_preflight
 * req = {bundle_bytes, output_parent, work_free_bytes, output_free_bytes, same_filesystem,
 *        work_dir, output_dir} -> the preflight report, or {"error": <refusal>}. */
json_t *w28_rec_p1_disk_space_preflight(const json_t *req) {
    double bundle = req ? json_get_num(req, "bundle_bytes", 0.0) : 0.0;
    int has_output = req ? json_get_bool(req, "output_parent", false) : 0;
    double work_free = req ? json_get_num(req, "work_free_bytes", 0.0) : 0.0;
    double output_free = req ? json_get_num(req, "output_free_bytes", 0.0) : 0.0;
    int same_filesystem = req ? json_get_bool(req, "same_filesystem", true) : 1;
    const char *work_dir = req ? json_get_str(req, "work_dir", "work") : "work";
    const char *output_dir = req ? json_get_str(req, "output_dir", "out") : "out";
    double allowance = has_output ? bundle : 0.0;
    double headroom = (double)(long long)((bundle + allowance) * 0.05);
    json_t *report = json_object();

    if (headroom < 268435456.0) headroom = 268435456.0;
    json_set(report, "source_bundle_bytes", json_number(bundle));
    json_set(report, "estimated_output_bytes", json_number(allowance));
    json_set(report, "headroom_bytes", json_number(headroom));
    json_set(report, "work_dir", json_string(work_dir));
    json_set(report, "work_dir_free_bytes", json_number(work_free));

    if (!has_output || same_filesystem) {
        double required = bundle + allowance + headroom;
        json_set(report, "shared_filesystem", json_bool(true));
        json_set(report, "work_dir_required_bytes", json_number(required));
        if (work_free < required) {
            rec_sb b;
            char free_text[64];
            char required_text[64];
            char bundle_text[64];
            char allowance_text[64];
            char headroom_text[64];
            json_t *refusal = json_object();
            rec_p1_format_bytes(work_free, free_text, sizeof free_text);
            rec_p1_format_bytes(required, required_text, sizeof required_text);
            rec_p1_format_bytes(bundle, bundle_text, sizeof bundle_text);
            rec_p1_format_bytes(allowance, allowance_text, sizeof allowance_text);
            rec_p1_format_bytes(headroom, headroom_text, sizeof headroom_text);
            rec_sb_init(&b);
            rec_sb_printf(&b,
                          "Not enough free disk space for a safe recovery copy: %s available at "
                          "%s, %s required (%s source bundle + %s output allowance + %s headroom). "
                          "Use --work-dir or --output on a filesystem with more free space.",
                          free_text, work_dir, required_text, bundle_text, allowance_text,
                          headroom_text);
            {
                char *text = rec_sb_take(&b);
                json_set(refusal, "error", json_string(text));
                free(text);
            }
            json_free(report);
            return refusal;
        }
        return report;
    }

    {
        double work_required = bundle + headroom;
        double output_required = allowance + headroom;
        int shortage = 0;
        json_set(report, "shared_filesystem", json_bool(false));
        json_set(report, "work_dir_required_bytes", json_number(work_required));
        json_set(report, "output_dir", json_string(output_dir));
        json_set(report, "output_dir_free_bytes", json_number(output_free));
        json_set(report, "output_dir_required_bytes", json_number(output_required));
        if (work_free < work_required || output_free < output_required) {
            rec_sb b;
            char work_free_text[64];
            char work_required_text[64];
            char output_free_text[64];
            char output_required_text[64];
            rec_p1_format_bytes(work_free, work_free_text, sizeof work_free_text);
            rec_p1_format_bytes(work_required, work_required_text, sizeof work_required_text);
            rec_p1_format_bytes(output_free, output_free_text, sizeof output_free_text);
            rec_p1_format_bytes(output_required, output_required_text, sizeof output_required_text);
            rec_sb_init(&b);
            rec_sb_puts(&b, "Not enough free disk space for safe recovery: ");
            if (work_free < work_required) {
                rec_sb_printf(&b, "%s: %s available, %s required", work_dir, work_free_text,
                              work_required_text);
                shortage = 1;
            }
            if (output_free < output_required) {
                if (shortage) rec_sb_puts(&b, "; ");
                rec_sb_printf(&b, "%s: %s available, %s required", output_dir, output_free_text,
                              output_required_text);
                shortage = 1;
            }
            rec_sb_puts(&b, ". Choose work/output filesystems with more free space.");
            {
                json_t *refusal = json_object();
                char *text = rec_sb_take(&b);
                json_set(refusal, "error", json_string(text));
                free(text);
                json_free(report);
                return refusal;
            }
        }
        return report;
    }
}

/* === _quoted_columns ===
 * PoP: w28_rec_p1_quoted_columns @ persistence/sqlite/admin/_recovery.py:_quoted_columns */
json_t *w28_rec_p1_quoted_columns(const json_t *columns) {
    rec_sb quoted;
    rec_sb marks;
    size_t i;
    size_t n = (columns && columns->type == JSON_ARRAY) ? json_len(columns) : 0;
    json_t *out;
    rec_sb_init(&quoted);
    rec_sb_init(&marks);
    for (i = 0; i < n; i++) {
        const char *column = json_string_value(json_get(columns, i));
        if (i) {
            rec_sb_puts(&quoted, ", ");
            rec_sb_puts(&marks, ", ");
        }
        rec_sb_puts(&quoted, "\"");
        rec_sb_puts(&quoted, column ? column : "");
        rec_sb_puts(&quoted, "\"");
        rec_sb_puts(&marks, "?");
    }
    out = json_array();
    {
        char *q = rec_sb_take(&quoted);
        char *m = rec_sb_take(&marks);
        json_append(out, json_string(q));
        json_append(out, json_string(m));
        free(q);
        free(m);
    }
    return out;
}

/* === _compatible_columns ===
 * PoP: w28_rec_p1_compatible_columns @ persistence/sqlite/admin/_recovery.py:_compatible_columns */
json_t *w28_rec_p1_compatible_columns(const char *table, const json_t *source_columns,
                                      const json_t *destination_columns) {
    json_t *result = json_object();
    json_t *shared = json_array();
    size_t source_count = (source_columns && source_columns->type == JSON_ARRAY) ? json_len(source_columns) : 0;
    size_t i;
    (void)table;
    for (i = 0; i < ((destination_columns && destination_columns->type == JSON_ARRAY)
                         ? json_len(destination_columns) : 0);
         i++) {
        const char *column = json_string_value(json_get(destination_columns, i));
        if (column && rec_array_has_str(source_columns, column)) json_append(shared, json_string(column));
    }
    json_set(result, "columns", shared);
    if (!source_count) {
        json_set(result, "status", json_string("missing"));
        return result;
    }
    if (!json_len(shared)) {
        json_set(result, "status", json_string("failed"));
        json_set(result, "error", json_string("source and destination have no compatible columns"));
    }
    return result;
}

/* === _table_inventory ===
 * PoP: w28_rec_p1_table_inventory @ persistence/sqlite/admin/_recovery.py:_table_inventory */
json_t *w28_rec_p1_table_inventory(const json_t *req) {
    json_t *result = json_object();
    const json_t *columns = req ? json_obj_get(req, "columns") : NULL;
    const json_t *rows = req ? json_obj_get(req, "rows") : NULL;
    json_set(result, "available", json_bool(false));
    json_set(result, "columns", json_array());
    json_set(result, "rows", json_null());
    if (!columns || columns->type != JSON_ARRAY) {
        const char *error = req ? json_get_str(req, "columns_error", NULL) : NULL;
        json_set(result, "error",
                 json_string(error ? error : "database disk image is malformed"));
        return result;
    }
    json_set(result, "columns", json_copy(columns));
    if (!json_len(columns)) return result;
    json_set(result, "available", json_bool(true));
    if (!rows || rows->type != JSON_NUMBER) {
        const char *error = req ? json_get_str(req, "rows_error", NULL) : NULL;
        json_set(result, "error", json_string(error ? error : "database disk image is malformed"));
        return result;
    }
    json_set(result, "rows", json_number(json_number_value(rows)));
    return result;
}

/* === _inspect_connection ===
 * PoP: w28_rec_p1_inspect_connection @ persistence/sqlite/admin/_recovery.py:_inspect_connection */
json_t *w28_rec_p1_inspect_connection(const json_t *req) {
    json_t *report = json_object();
    json_t *tables = json_object();
    json_t *errors = json_array();
    json_t *warnings = json_array();
    const json_t *journal = req ? json_obj_get(req, "journal_mode") : NULL;
    const json_t *given_tables = req ? json_obj_get(req, "tables") : NULL;
    int i;

    json_set(report, "tables", tables);
    if (!journal || journal->type != JSON_STRING) {
        const char *error = req ? json_get_str(req, "journal_error", NULL) : NULL;
        char buf[512];
        json_set(report, "journal_mode", json_null());
        snprintf(buf, sizeof buf, "journal mode: %s",
                 error ? error : "database disk image is malformed");
        json_append(warnings, json_string(buf));
    } else {
        rec_sb b;
        const char *raw = json_string_value(journal);
        rec_sb_init(&b);
        for (; raw && *raw; raw++) {
            char c = *raw;
            char lower[2];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            lower[0] = c;
            lower[1] = '\0';
            rec_sb_puts(&b, lower);
        }
        {
            char *out = rec_sb_take(&b);
            json_set(report, "journal_mode", json_string(out));
            free(out);
        }
    }
    for (i = 0; i < REC_P1_INVENTORY_COUNT; i++) {
        const char *name = REC_P1_INVENTORY_TABLES[i];
        const json_t *found = given_tables ? json_obj_get(given_tables, name) : NULL;
        if (found && found->type == JSON_OBJECT) {
            json_set(tables, name, json_copy(found));
        } else {
            json_t *empty = json_object();
            json_set(empty, "available", json_bool(false));
            json_set(empty, "columns", json_array());
            json_set(empty, "rows", json_null());
            json_set(tables, name, empty);
        }
    }
    for (i = 0; i < 2; i++) {
        const char *required = i == 0 ? "sessions" : "messages";
        const json_t *entry = json_obj_get(tables, required);
        int available = entry ? json_get_bool(entry, "available", false) : 0;
        const json_t *rows = entry ? json_obj_get(entry, "rows") : NULL;
        if (!available || !rows || rows->type != JSON_NUMBER) {
            char buf[256];
            snprintf(buf, sizeof buf, "required table %s is not completely readable", required);
            json_append(errors, json_string(buf));
        }
    }
    json_set(report, "errors", errors);
    json_set(report, "warnings", warnings);
    json_set(report, "recoverable", json_bool(json_len(errors) == 0));
    return report;
}

/* === _append_skipped_range ===
 * PoP: w28_rec_p1_append_skipped_range @ persistence/sqlite/admin/_recovery.py:_append_skipped_range */
json_t *w28_rec_p1_append_skipped_range(const json_t *ranges, double low, double high,
                                        const char *error) {
    json_t *out = (ranges && ranges->type == JSON_ARRAY) ? json_copy(ranges) : json_array();
    size_t n = json_len(out);
    if (n) {
        json_t *last = json_get(out, n - 1);
        double last_high = json_get_num(last, "high", 0.0);
        const char *last_error = json_get_str(last, "error", "");
        if (last_high + 1.0 == low && strcmp(last_error ? last_error : "", error ? error : "") == 0) {
            json_set(last, "high", json_number(high));
            return out;
        }
    }
    {
        json_t *entry = json_object();
        json_set(entry, "low", json_number(low));
        json_set(entry, "high", json_number(high));
        json_set(entry, "error", json_string(error ? error : ""));
        json_append(out, entry);
    }
    return out;
}

/* PoP: w28_rec_p1_state_meta_result @ persistence/sqlite/admin/_recovery.py:_state_meta_result */
static json_t *rec_p1_state_meta_result(const json_t *req, const char *status, const char *error,
                                        int salvage) {
    json_t *result = json_object();
    json_t *excluded = json_array();
    const json_t *offset = req ? json_obj_get(req, "source_rows") : NULL;
    const char *const *keys = REC_P1_GENERATED_META_KEYS;
    const char *sorted[REC_P1_GENERATED_META_COUNT];
    int i;
    for (i = 0; i < REC_P1_GENERATED_META_COUNT; i++) sorted[i] = keys[i];
    qsort(sorted, REC_P1_GENERATED_META_COUNT, sizeof sorted[0], rec_cmp_str);
    json_set(result, "source_meta_rows",
             (offset && offset->type == JSON_NUMBER) ? json_number(json_number_value(offset))
                                                     : json_null());
    json_set(result, "copied_rows", json_number(0));
    {
        json_t *columns = json_array();
        json_append(columns, json_string("key"));
        json_append(columns, json_string("value"));
        json_set(result, "columns", columns);
    }
    for (i = 0; i < REC_P1_GENERATED_META_COUNT; i++) json_append(excluded, json_string(sorted[i]));
    json_set(result, "excluded_keys", excluded);
    if (salvage) json_set(result, "mode", json_string("rowid_range_salvage"));
    json_set(result, "status", json_string(status));
    if (error) json_set(result, "error", json_string(error));
    return result;
}

/* === _state_meta_precheck ===
 * PoP: w28_rec_p1_state_meta_precheck @ persistence/sqlite/admin/_recovery.py:_state_meta_precheck */
json_t *w28_rec_p1_state_meta_precheck(const json_t *req) {
    const json_t *source = req ? json_obj_get(req, "source_columns") : NULL;
    const json_t *destination = req ? json_obj_get(req, "destination_columns") : NULL;
    int salvage = req ? json_get_bool(req, "salvage", false) : 0;
    int source_ok = rec_array_has_str(source, "key") && rec_array_has_str(source, "value");
    int destination_ok =
        rec_array_has_str(destination, "key") && rec_array_has_str(destination, "value");
    if (!source_ok) {
        if (salvage && source && json_len(source)) {
            rec_sb found;
            size_t i;
            rec_sb_init(&found);
            for (i = 0; i < json_len(source); i++) {
                const char *column = json_string_value(json_get(source, i));
                if (i) rec_sb_puts(&found, ", ");
                rec_sb_puts(&found, column ? column : "");
            }
            if (!found.len) rec_sb_puts(&found, "none");
            {
                char *text = rec_sb_take(&found);
                rec_sb message;
                json_t *out;
                rec_sb_init(&message);
                rec_sb_puts(&message,
                            "source state_meta exists but is missing the key/value columns (found: ");
                rec_sb_puts(&message, text);
                rec_sb_puts(&message, ")");
                free(text);
                text = rec_sb_take(&message);
                out = rec_p1_state_meta_result(req, "failed", text, salvage);
                free(text);
                return out;
            }
        }
        return rec_p1_state_meta_result(req, "missing", NULL, salvage);
    }
    if (!destination_ok) {
        return rec_p1_state_meta_result(req, "failed",
                                        "destination state_meta schema is incomplete", salvage);
    }
    return json_null();
}

/* === _reconcile ===
 * PoP: w28_rec_p1_reconcile @ persistence/sqlite/admin/_recovery.py:_reconcile */
json_t *w28_rec_p1_reconcile(const char *table, const char *where, const char *mutation,
                             double count) {
    rec_sb b;
    json_t *out = json_object();
    json_t *statements = json_array();
    rec_sb_init(&b);
    rec_sb_printf(&b, "SELECT COUNT(*) FROM \"%s\" WHERE %s", table ? table : "",
                  where ? where : "");
    {
        char *text = rec_sb_take(&b);
        json_append(statements, json_string(text));
        free(text);
    }
    if (count) {
        rec_sb_init(&b);
        rec_sb_printf(&b, "%s WHERE %s", mutation ? mutation : "", where ? where : "");
        {
            char *text = rec_sb_take(&b);
            json_append(statements, json_string(text));
            free(text);
        }
    }
    json_set(out, "count", json_number(count));
    json_set(out, "statements", statements);
    return out;
}

/* ===================================================================== */
/* gate w28_recovery_eb8799_p2 -- salvage geometry + verification verdict */
/* ===================================================================== */

static int rec_p2_cmp_double(const void *a, const void *b) {
    double left = *(const double *)a;
    double right = *(const double *)b;
    if (left < right) return -1;
    if (left > right) return 1;
    return 0;
}

static double *rec_p2_sorted_rowids(const json_t *occupied, size_t *count) {
    double *values;
    size_t n;
    size_t i;
    *count = 0;
    if (!occupied || occupied->type != JSON_ARRAY || !json_len(occupied)) return NULL;
    n = json_len(occupied);
    values = (double *)malloc(n * sizeof *values);
    if (!values) abort();
    for (i = 0; i < n; i++) values[i] = json_number_value(json_get(occupied, i));
    qsort(values, n, sizeof values[0], rec_p2_cmp_double);
    *count = n;
    return values;
}

static int rec_p2_in_damage(const json_t *damage, double candidate) {
    size_t i;
    if (!damage || damage->type != JSON_ARRAY) return 0;
    for (i = 0; i < json_len(damage); i++) {
        const json_t *span = json_get(damage, i);
        double low;
        double high;
        if (!span || span->type != JSON_ARRAY || json_len(span) < 2) continue;
        low = json_number_value(json_get(span, 0));
        high = json_number_value(json_get(span, 1));
        if (candidate >= low && candidate <= high) return 1;
    }
    return 0;
}

typedef struct {
    const char *edge;
    double probes;
    int capped;
    double bound;
} rec_p2_probe;

/* Doubling-offset gallop from a readable anchor: a clean "no rows beyond" caps the
 * synthetic domain; damage or a hit widens further.  O(log range), never unbounded. */
static rec_p2_probe rec_p2_probe_edge(const double *occupied, size_t n, const json_t *damage,
                                      const char *edge, double anchor) {
    rec_p2_probe out;
    int ascending = edge && strcmp(edge, "high") == 0;
    double domain = ascending ? 9223372036854775807.0 : (-9223372036854775807.0 - 1.0);
    double position = anchor;
    double span = 1.0;
    long guard;

    out.edge = edge ? edge : "high";
    out.probes = 0.0;
    out.capped = 0;
    out.bound = domain;
    for (guard = 0; guard < 4096; guard++) {
        double candidate = ascending ? position + span : position - span;
        int found = 0;
        double next = 0.0;
        size_t i;
        if (ascending ? (candidate >= domain) : (candidate <= domain)) return out;
        out.probes += 1.0;
        if (rec_p2_in_damage(damage, candidate)) {
            span *= 2.0;
            continue;
        }
        if (ascending) {
            for (i = 0; i < n; i++) {
                if (occupied[i] > candidate) {
                    next = occupied[i];
                    found = 1;
                    break;
                }
            }
        } else {
            for (i = n; i > 0; i--) {
                if (occupied[i - 1] < candidate) {
                    next = occupied[i - 1];
                    found = 1;
                    break;
                }
            }
        }
        if (!found) {
            out.bound = candidate;
            out.capped = 1;
            return out;
        }
        position = next;
        span *= 2.0;
    }
    return out;
}

static json_t *rec_p2_probe_json(const rec_p2_probe *probe) {
    json_t *out = json_object();
    json_set(out, "edge", json_string(probe->edge ? probe->edge : "high"));
    json_set(out, "probes", json_number(probe->probes));
    json_set(out, "capped", json_bool(probe->capped != 0));
    json_set(out, "bound", json_number(probe->bound));
    return out;
}

/* === _probe_populated_edge ===
 * PoP: w28_rec_p2_probe_populated_edge @ persistence/sqlite/admin/_recovery.py:_probe_populated_edge */
json_t *w28_rec_p2_probe_populated_edge(const json_t *occupied, const json_t *damage,
                                        const char *edge, double anchor) {
    size_t n = 0;
    double *rowids = rec_p2_sorted_rowids(occupied, &n);
    rec_p2_probe probe = rec_p2_probe_edge(rowids, n, damage, edge, anchor);
    json_t *out = rec_p2_probe_json(&probe);
    free(rowids);
    return out;
}

/* === _salvage_rowid_bounds ===
 * PoP: w28_rec_p2_salvage_rowid_bounds @ persistence/sqlite/admin/_recovery.py:_salvage_rowid_bounds */
json_t *w28_rec_p2_salvage_rowid_bounds(const json_t *req) {
    const json_t *damage = req ? json_obj_get(req, "damage") : NULL;
    const char *error = req ? json_get_str(req, "error", "database disk image is malformed") : NULL;
    int low_ok = req ? json_get_bool(req, "low_ok", true) : 1;
    int high_ok = req ? json_get_bool(req, "high_ok", true) : 1;
    int aggregate_ok = req ? json_get_bool(req, "aggregate_ok", true) : 1;
    json_t *result = json_object();
    json_t *errors = json_array();
    json_t *fallback = json_array();
    json_t *aggregate_edges = NULL;
    json_t *edge_probes = NULL;
    size_t n = 0;
    double *rowids = rec_p2_sorted_rowids(req ? json_obj_get(req, "occupied") : NULL, &n);
    int have_low = 0;
    int have_high = 0;
    double low = 0.0;
    double high = 0.0;
    int fallback_low = 0;
    int fallback_high = 0;

    json_set(result, "errors", errors);
    json_set(result, "fallback_edges", fallback);

    if (low_ok) {
        if (n) {
            low = rowids[0];
            have_low = 1;
        }
    } else {
        char buf[512];
        snprintf(buf, sizeof buf, "low rowid: %s", error ? error : "");
        json_append(errors, json_string(buf));
    }
    if (high_ok) {
        if (n) {
            high = rowids[n - 1];
            have_high = 1;
        }
    } else {
        char buf[512];
        snprintf(buf, sizeof buf, "high rowid: %s", error ? error : "");
        json_append(errors, json_string(buf));
    }

    if (!have_low && !have_high) {
        json_set(result, json_len(errors) ? "unavailable" : "empty", json_bool(true));
        free(rowids);
        return result;
    }

    /* An ordered LIMIT 1 dies on a damaged edge leaf; the aggregate answers from any
     * covering index before the synthetic domain fallback is used. */
    if (!have_low || !have_high) {
        if (!aggregate_ok) {
            char buf[512];
            snprintf(buf, sizeof buf, "aggregate rowid bounds: %s", error ? error : "");
            json_append(errors, json_string(buf));
        } else if (n) {
            if (!have_low) {
                low = rowids[0];
                have_low = 1;
                if (!aggregate_edges) aggregate_edges = json_array();
                json_append(aggregate_edges, json_string("low"));
            }
            if (!have_high) {
                high = rowids[n - 1];
                have_high = 1;
                if (!aggregate_edges) aggregate_edges = json_array();
                json_append(aggregate_edges, json_string("high"));
            }
        }
    }
    if (aggregate_edges) json_set(result, "aggregate_edges", aggregate_edges);

    if (!have_low) {
        low = -9223372036854775807.0 - 1.0;
        fallback_low = 1;
        json_append(fallback, json_string("low"));
    }
    if (!have_high) {
        high = 9223372036854775807.0;
        fallback_high = 1;
        json_append(fallback, json_string("high"));
    }

    if (fallback_low || fallback_high) {
        edge_probes = json_array();
        if (fallback_high) {
            rec_p2_probe probe = rec_p2_probe_edge(rowids, n, damage, "high", low);
            json_append(edge_probes, rec_p2_probe_json(&probe));
            if (probe.capped) high = probe.bound;
        }
        if (fallback_low) {
            rec_p2_probe probe = rec_p2_probe_edge(rowids, n, damage, "low", high);
            json_append(edge_probes, rec_p2_probe_json(&probe));
            if (probe.capped) low = probe.bound;
        }
        json_set(result, "edge_probes", edge_probes);
    }
    json_set(result, "low", json_number(low));
    json_set(result, "high", json_number(high));
    free(rowids);
    return result;
}

/* === _repair_out_of_window_timestamps ===
 * PoP: w28_rec_p2_repair_plan @ persistence/sqlite/admin/_recovery.py:_repair_out_of_window_timestamps */
json_t *w28_rec_p2_repair_plan(const json_t *req) {
    static const char *const ok_window = "BETWEEN 0.0 AND 4200000000.0";
    static const char *const now = "CAST(strftime('%s', 'now') AS REAL)";
    const json_t *message_columns = req ? json_obj_get(req, "messages_columns") : NULL;
    const json_t *session_columns = req ? json_obj_get(req, "sessions_columns") : NULL;
    double repairs = req ? json_get_num(req, "repair_count", 0.0) : 0.0;
    json_t *out = json_object();
    json_t *statements = json_array();
    double repaired = 0.0;

    if (rec_array_has_str(message_columns, "timestamp")) {
        rec_sb b;
        rec_sb_init(&b);
        rec_sb_puts(&b, "SELECT COUNT(*) FROM \"messages\" WHERE NOT (timestamp ");
        rec_sb_puts(&b, ok_window);
        rec_sb_puts(&b, ")");
        {
            char *text = rec_sb_take(&b);
            json_append(statements, json_string(text));
            free(text);
        }
        if (repairs) {
            rec_sb_init(&b);
            rec_sb_puts(&b, "UPDATE messages SET timestamp = COALESCE(");
            rec_sb_puts(&b,
                        "(SELECT p.timestamp FROM messages p WHERE p.session_id = messages.session_id "
                        "AND p.id < messages.id AND p.timestamp ");
            rec_sb_puts(&b, ok_window);
            rec_sb_puts(&b, " ORDER BY p.id DESC LIMIT 1),(SELECT n.timestamp FROM messages n WHERE "
                            "n.session_id = messages.session_id AND n.id > messages.id AND n.timestamp ");
            rec_sb_puts(&b, ok_window);
            rec_sb_puts(&b, " ORDER BY n.id LIMIT 1),(SELECT s.started_at FROM sessions s WHERE "
                            "s.id = messages.session_id AND s.started_at ");
            rec_sb_puts(&b, ok_window);
            rec_sb_puts(&b, "), ");
            rec_sb_puts(&b, now);
            rec_sb_puts(&b, ") WHERE NOT (timestamp ");
            rec_sb_puts(&b, ok_window);
            rec_sb_puts(&b, ")");
            {
                char *text = rec_sb_take(&b);
                json_append(statements, json_string(text));
                free(text);
            }
            repaired += repairs;
        }
    }
    if (rec_array_has_str(session_columns, "started_at")) {
        rec_sb b;
        rec_sb_init(&b);
        rec_sb_puts(&b, "SELECT COUNT(*) FROM \"sessions\" WHERE NOT (started_at ");
        rec_sb_puts(&b, ok_window);
        rec_sb_puts(&b, ")");
        {
            char *text = rec_sb_take(&b);
            json_append(statements, json_string(text));
            free(text);
        }
        if (repairs) {
            rec_sb_init(&b);
            rec_sb_puts(&b, "UPDATE sessions SET started_at = COALESCE((SELECT MIN(m.timestamp) "
                            "FROM messages m WHERE m.session_id = sessions.id AND m.timestamp ");
            rec_sb_puts(&b, ok_window);
            rec_sb_puts(&b, "), ");
            rec_sb_puts(&b, now);
            rec_sb_puts(&b, ") WHERE NOT (started_at ");
            rec_sb_puts(&b, ok_window);
            rec_sb_puts(&b, ")");
            {
                char *text = rec_sb_take(&b);
                json_append(statements, json_string(text));
                free(text);
            }
            repaired += repairs;
        }
    }
    {
        static const char *const nullable[] = {"ended_at", "last_activity_at", NULL};
        int i;
        for (i = 0; nullable[i]; i++) {
            const char *column = nullable[i];
            rec_sb b;
            if (!rec_array_has_str(session_columns, column)) continue;
            rec_sb_init(&b);
            rec_sb_printf(&b, "SELECT COUNT(*) FROM \"sessions\" WHERE NOT (%s ", column);
            rec_sb_puts(&b, ok_window);
            rec_sb_puts(&b, ")");
            {
                char *text = rec_sb_take(&b);
                json_append(statements, json_string(text));
                free(text);
            }
            if (!repairs) continue;
            rec_sb_init(&b);
            rec_sb_printf(&b, "UPDATE sessions SET %s = NULL WHERE NOT (%s ", column, column);
            rec_sb_puts(&b, ok_window);
            rec_sb_puts(&b, ")");
            {
                char *text = rec_sb_take(&b);
                json_append(statements, json_string(text));
                free(text);
            }
            repaired += repairs;
        }
    }
    json_set(out, "repaired", json_number(repaired));
    json_set(out, "statements", statements);
    return out;
}

static void rec_p2_lower_copy(char *dst, size_t cap, const char *src) {
    size_t i = 0;
    if (!cap) return;
    for (; src && *src && i + 1 < cap; src++, i++) {
        char c = *src;
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        dst[i] = c;
    }
    dst[i] = '\0';
}

/* === _verify_structure ===
 * PoP: w28_rec_p2_verify_structure @ persistence/sqlite/admin/_recovery.py:_verify_structure */
json_t *w28_rec_p2_verify_structure(const json_t *req) {
    json_t *verification = json_object();
    json_t *errors = json_array();
    json_t *integrity = json_array();
    json_t *foreign_keys = json_array();
    json_t *fts_meta = json_object();
    json_t *pending = json_array();
    const json_t *integrity_rows = req ? json_obj_get(req, "integrity_rows") : NULL;
    const json_t *foreign_key_rows = req ? json_obj_get(req, "foreign_key_rows") : NULL;
    const json_t *schema_version = req ? json_obj_get(req, "schema_version") : NULL;
    const json_t *meta_in = req ? json_obj_get(req, "fts_meta") : NULL;
    const json_t *journal_in = req ? json_obj_get(req, "journal_mode") : NULL;
    char journal[64];
    size_t i;
    int integrity_ok;

    json_set(verification, "errors", errors);
    json_set(verification, "warnings", json_array());
    for (i = 0; i < ((integrity_rows && integrity_rows->type == JSON_ARRAY) ? json_len(integrity_rows) : 0); i++) {
        const char *value = json_string_value(json_get(integrity_rows, i));
        json_append(integrity, json_string(value ? value : ""));
    }
    json_set(verification, "integrity_check", integrity);
    {
        const char *first = json_len(integrity) ? json_string_value(json_get(integrity, 0)) : NULL;
        integrity_ok = json_len(integrity) == 1 && first != NULL && strcmp(first, "ok") == 0;
    }
    if (!integrity_ok) json_append(errors, json_string("PRAGMA integrity_check did not return exactly 'ok'"));
    for (i = 0; i < ((foreign_key_rows && foreign_key_rows->type == JSON_ARRAY) ? json_len(foreign_key_rows) : 0); i++)
        json_append(foreign_keys, json_copy(json_get(foreign_key_rows, i)));
    json_set(verification, "foreign_key_check", foreign_keys);
    if (json_len(foreign_keys)) json_append(errors, json_string("foreign key violations remain"));
    if (!journal_in || journal_in->type != JSON_STRING) {
        json_set(verification, "journal_mode", json_null());
    } else {
        rec_p2_lower_copy(journal, sizeof journal, json_string_value(journal_in));
        json_set(verification, "journal_mode", json_string(journal));
    }
    if (schema_version && schema_version->type == JSON_NUMBER) {
        char buf[128];
        json_set(verification, "schema_version", json_number(json_number_value(schema_version)));
        if (json_number_value(schema_version) != (double)REC_P1_SCHEMA_VERSION) {
            snprintf(buf, sizeof buf, "schema version is %.0f, expected %d",
                     json_number_value(schema_version), REC_P1_SCHEMA_VERSION);
            json_append(errors, json_string(buf));
        }
    } else {
        char buf[128];
        json_set(verification, "schema_version", json_null());
        snprintf(buf, sizeof buf, "schema version is None, expected %d", REC_P1_SCHEMA_VERSION);
        json_append(errors, json_string(buf));
    }
    if (meta_in && meta_in->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(meta_in); i++) {
            const char *key = json_object_get_key_at(meta_in, i);
            json_set(fts_meta, key, json_copy(json_object_get_at(meta_in, i)));
        }
    }
    json_set(verification, "fts_meta", fts_meta);
    {
        const json_t *storage = json_obj_get(fts_meta, "fts_storage_version");
        const char *value = storage ? json_string_value(storage) : NULL;
        if (!value || strcmp(value, "3") != 0)
            json_append(errors, json_string("fresh FTS storage version was not established"));
    }
    {
        const char *sorted[REC_P1_GENERATED_META_COUNT];
        int count = 0;
        int k;
        for (k = 0; k < REC_P1_GENERATED_META_COUNT; k++) {
            const char *key = REC_P1_GENERATED_META_KEYS[k];
            if (strncmp(key, "fts_", 4) != 0) continue;
            if (strcmp(key, "fts_storage_version") == 0) continue;
            if (!json_obj_get(fts_meta, key)) continue;
            sorted[count++] = key;
        }
        qsort(sorted, (size_t)count, sizeof sorted[0], rec_cmp_str);
        for (k = 0; k < count; k++) json_append(pending, json_string(sorted[k]));
    }
    json_set(verification, "pending_fts_keys", pending);
    if (json_len(pending))
        json_append(errors, json_string("derived FTS transition markers remain in the recovered database"));
    return verification;
}

static void rec_p2_flag(json_t *verification, const char *message, int soft) {
    json_t *bucket = json_obj_get(verification, soft ? "warnings" : "errors");
    if (!bucket) return;
    json_append(bucket, json_string(message));
    if (soft) json_set(verification, "loss_detected", json_bool(true));
}

/* === _verify_row_counts ===
 * PoP: w28_rec_p2_verify_row_counts @ persistence/sqlite/admin/_recovery.py:_verify_row_counts */
json_t *w28_rec_p2_verify_row_counts(const json_t *req) {
    const json_t *present = req ? json_obj_get(req, "present_tables") : NULL;
    const json_t *given_counts = req ? json_obj_get(req, "counts") : NULL;
    const json_t *expected_counts = req ? json_obj_get(req, "expected_counts") : NULL;
    const json_t *copy_report = req ? json_obj_get(req, "copy_report") : NULL;
    const json_t *cleanup_in = req ? json_obj_get(req, "orphan_cleanup") : NULL;
    int allow_partial = req ? json_get_bool(req, "allow_partial", false) : 0;
    json_t *verification = json_object();
    json_t *errors = json_array();
    json_t *warnings = json_array();
    json_t *counts = json_object();
    int i;

    json_set(verification, "errors", errors);
    json_set(verification, "warnings", warnings);
    json_set(verification, "loss_detected", json_bool(false));
    for (i = 0; i < REC_P1_INVENTORY_COUNT; i++) {
        const char *name = REC_P1_INVENTORY_TABLES[i];
        const json_t *value;
        if (!rec_array_has_str(present, name)) continue;
        value = given_counts ? json_obj_get(given_counts, name) : NULL;
        json_set(counts, name,
                 json_number(value && value->type == JSON_NUMBER ? json_number_value(value) : 0.0));
    }
    json_set(verification, "table_counts", counts);
    for (i = 0; i < 3; i++) {
        const char *table = i == 0 ? "sessions" : (i == 1 ? "messages" : "delivery_obligations");
        const json_t *expected = expected_counts ? json_obj_get(expected_counts, table) : NULL;
        const json_t *actual;
        char buf[512];
        if (!expected || expected->type == JSON_NULL) continue;
        actual = json_obj_get(counts, table);
        if (expected->type == JSON_NUMBER && actual && actual->type == JSON_NUMBER &&
            json_number_value(actual) == json_number_value(expected))
            continue;
        if (actual && actual->type == JSON_NUMBER) {
            snprintf(buf, sizeof buf, "%s count is %.0f, expected %.0f", table,
                     json_number_value(actual), json_number_value(expected));
        } else {
            snprintf(buf, sizeof buf, "%s count is None, expected %.0f", table,
                     json_number_value(expected));
        }
        rec_p2_flag(verification, buf, allow_partial);
    }
    {
        double rebuilt = cleanup_in ? json_get_num(cleanup_in, "sessions_reconstructed", 0.0) : 0.0;
        double retained = cleanup_in ? json_get_num(cleanup_in, "messages_retained", 0.0) : 0.0;
        double removed = cleanup_in ? json_get_num(cleanup_in, "messages_removed", 0.0) : 0.0;
        const json_t *session_count = json_obj_get(counts, "sessions");
        const json_t *message_count = json_obj_get(counts, "messages");
        int fully_reconstructed =
            rebuilt > 0 && session_count && session_count->type == JSON_NUMBER &&
            json_number_value(session_count) == rebuilt && message_count &&
            message_count->type == JSON_NUMBER && json_number_value(message_count) == retained &&
            removed == 0;
        size_t j;
        for (j = 0; copy_report && copy_report->type == JSON_OBJECT && j < json_object_size(copy_report); j++) {
            const char *table = json_object_get_key_at(copy_report, j);
            const json_t *entry = json_object_get_at(copy_report, j);
            const char *status = entry ? json_get_str(entry, "status", NULL) : NULL;
            int tolerable;
            char buf[256];
            if (!status) continue;
            if (strcmp(status, "failed") != 0 && strcmp(status, "partial") != 0) continue;
            tolerable = strcmp(status, "partial") == 0 ||
                        (strcmp(table, "sessions") != 0 && strcmp(table, "messages") != 0) ||
                        (strcmp(table, "sessions") == 0 && fully_reconstructed);
            snprintf(buf, sizeof buf, "%s copy status is %s", table, status);
            rec_p2_flag(verification, buf, allow_partial && tolerable);
        }
        if (cleanup_in && cleanup_in->type == JSON_OBJECT) {
            double orphans = json_get_num(cleanup_in, "total_removed_or_relinked", 0.0);
            char buf[640];
            if (orphans) {
                snprintf(buf, sizeof buf, "%.0f orphaned reference(s) were removed or relinked", orphans);
                rec_p2_flag(verification, buf, 1);
            }
            if (rebuilt) {
                snprintf(buf, sizeof buf,
                         "%.0f session(s) could not be salvaged and were reconstructed as "
                         "placeholders to retain %.0f message(s); their metadata (title, model, "
                         "timestamps, cost) is lost",
                         rebuilt, retained);
                rec_p2_flag(verification, buf, 1);
            }
        }
    }
    return verification;
}

/* === _lost_and_found_plausibility_errors ===
 * PoP: w28_rec_p2_lost_and_found_plausibility_errors @ persistence/sqlite/admin/_recovery.py:_lost_and_found_plausibility_errors */
json_t *w28_rec_p2_lost_and_found_plausibility_errors(const json_t *req) {
    static const char *const tables[] = {"sessions", "messages", NULL};
    static const char *const columns[] = {"started_at", "timestamp", NULL};
    json_t *errors = json_array();
    int i;
    for (i = 0; i < 2; i++) {
        const json_t *bucket = req ? json_obj_get(req, tables[i]) : NULL;
        double total = bucket ? json_get_num(bucket, "total", 0.0) : 0.0;
        double implausible = bucket ? json_get_num(bucket, "implausible", 0.0) : 0.0;
        char buf[640];
        if (!total) continue;
        if (implausible != total) continue;
        snprintf(buf, sizeof buf,
                 "%s.%s is implausible in all %.0f salvaged row(s) (NULL or before 2001-09): the "
                 "source's physical column order did not match the destination template, so cells "
                 "were mapped onto the wrong columns",
                 tables[i], columns[i], total);
        json_append(errors, json_string(buf));
    }
    return errors;
}

/* === _recovery_report ===
 * PoP: w28_rec_p2_recovery_report @ persistence/sqlite/admin/_recovery.py:_recovery_report */
json_t *w28_rec_p2_recovery_report(const json_t *req) {
    static const char *const runtime_tables[] = {"runtime_epoch", "session_admissions",
                                                 "worker_executions", "worker_receipts", NULL};
    static const char *const policy =
        "transcript-only; runtime ledger excluded; fresh epoch on activation";
    const json_t *inspection = req ? json_obj_get(req, "inspection") : NULL;
    json_t *verification = req ? json_copy(json_obj_get(req, "verification")) : json_object();
    const json_t *fields = req ? json_obj_get(req, "fields") : NULL;
    const char *on_source_change = req ? json_get_str(req, "on_source_change", "healthy") : "healthy";
    int unchanged = req ? json_get_bool(req, "source_unchanged", true) : 1;
    json_t *report = json_object();
    json_t *inner = json_object();
    json_t *runtime = json_object();
    json_t *excluded = json_object();
    int healthy;
    int complete;
    int loss;
    size_t i;

    if (!verification || verification->type != JSON_OBJECT) verification = json_object();
    if (!unchanged) {
        json_t *errors = json_obj_get(verification, "errors");
        if (errors) json_append(errors, json_string("the source database bundle changed during recovery"));
        json_set(verification, on_source_change ? on_source_change : "healthy", json_bool(false));
    }
    json_set(report, "operation", json_string("recover"));
    json_set(report, "source", json_string(req ? json_get_str(req, "source", "") : ""));
    json_set(report, "output", json_string(req ? json_get_str(req, "output", "") : ""));
    json_set(report, "source_bundle",
             json_copy(inspection ? json_obj_get(inspection, "source_bundle") : NULL));
    json_set(report, "source_fingerprint",
             json_copy(inspection ? json_obj_get(inspection, "source_fingerprint") : NULL));
    json_set(report, "source_unchanged", json_bool(unchanged != 0));
    json_set(report, "disk_space", json_copy(req ? json_obj_get(req, "disk_space") : NULL));
    {
        const json_t *journal = inspection ? json_obj_get(inspection, "journal_mode") : NULL;
        json_set(inner, "journal_mode", journal ? json_copy(journal) : json_null());
        json_set(inner, "tables", json_copy(inspection ? json_obj_get(inspection, "tables") : NULL));
        json_set(inner, "errors", json_copy(inspection ? json_obj_get(inspection, "errors") : NULL));
        json_set(inner, "warnings", json_copy(inspection ? json_obj_get(inspection, "warnings") : NULL));
        json_set(report, "inspection", inner);
    }
    if (fields && fields->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(fields); i++)
            json_set(report, json_object_get_key_at(fields, i),
                     json_copy(json_object_get_at(fields, i)));
    }
    healthy = json_get_bool(verification, "healthy", false);
    complete = (json_get_bool(verification, "complete", false) && unchanged) ? 1 : 0;
    loss = json_get_bool(verification, "loss_detected", false);
    json_set(report, "verification", verification);
    json_set(report, "complete", json_bool(complete));
    json_set(report, "partial", json_bool(loss));
    json_set(report, "verified", json_bool(healthy && unchanged));
    json_set(report, "installed", json_bool(false));
    for (i = 0; runtime_tables[i]; i++) {
        const json_t *tables = inspection ? json_obj_get(inspection, "tables") : NULL;
        const json_t *entry = tables ? json_obj_get(tables, runtime_tables[i]) : NULL;
        json_set(excluded, runtime_tables[i], entry ? json_copy(entry) : json_object());
    }
    json_set(runtime, "policy", json_string(policy));
    json_set(runtime, "excluded_tables", excluded);
    json_set(report, "runtime_state", runtime);
    return report;
}
