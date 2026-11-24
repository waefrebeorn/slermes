/*
 * _hosted_work.c - C11 port of upstream persistence/sqlite/gateway/_hosted_work.py
 * (gate w34_hostedwork_56ec19).
 *
 * PoP: w34_hostedwork_56ec19_retained_fields    @ persistence/sqlite/gateway/_hosted_work.py:row_size_sql
 *                                                  -> session_rules/hosted_rooms.py:retained_fields
 * PoP: w34_hostedwork_56ec19_row_size_sql       @ persistence/sqlite/gateway/_hosted_work.py:row_size_sql
 * PoP: w34_hostedwork_56ec19_usage_sql          @ persistence/sqlite/gateway/_hosted_work.py:usage_sql
 * PoP: w34_hostedwork_56ec19_scope_disposition  @ persistence/sqlite/gateway/_hosted_work.py:scope_disposition
 * PoP: w34_hostedwork_56ec19_guards_sql         @ persistence/sqlite/gateway/_hosted_work.py:_guards
 * PoP: w34_hostedwork_56ec19_invalid_guards_sql @ persistence/sqlite/gateway/_hosted_work.py:_invalid_guards
 * PoP: w34_hostedwork_56ec19_save               @ persistence/sqlite/gateway/_hosted_work.py:save_locked
 * PoP: w34_hostedwork_56ec19_budget_refused     @ persistence/sqlite/gateway/_hosted_work.py:_budget
 * PoP: w34_hostedwork_56ec19_read_history       @ persistence/sqlite/gateway/_hosted_work.py:read_history_locked
 * PoP: w34_hostedwork_56ec19_prefix_check       @ persistence/sqlite/gateway/_hosted_work.py:source_prefix_locked
 * PoP: w34_hostedwork_56ec19_capture_tasks      @ persistence/sqlite/gateway/_hosted_work.py:_capture_tasks
 * PoP: w34_hostedwork_56ec19_constants          @ persistence/sqlite/gateway/_hosted_work.py:<module>
 *
 * Seams (not ported, deliberately left to the caller):
 *   - sqlite3.Connection and its savepoints: initialize, _initialize_locked,
 *     validate_stored_locked, capture_locked, capture_transition_locked;
 *   - canonical JSON / SHA-256 / record validation from
 *     session_rules.hosted_rooms: encode, digest, roster_digest, validate,
 *     validate_stored, validate_history, validate_provenance;
 *   - normalize_task_payload from session_rules.hosted_tasks (the payload the
 *     caller normalized is an input here);
 *   - table_exists from ._hosted_rooms_common (a flag here).
 *
 * The statement builders below reproduce the upstream f-strings byte for byte,
 * including their embedded newlines and indentation, because those bytes are
 * the schema the store installs.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w34_hostedwork_56ec19.h"

const char *const W34_HOSTEDWORK_56EC19_SOURCE_TABLE = "hosted_room_work_records_source";
const char *const W34_HOSTEDWORK_56EC19_TARGET_TABLE = "hosted_room_work_records_target";
const char *const W34_HOSTEDWORK_56EC19_PENDING_TABLE = "hosted_room_work_records_pending";
const char *const W34_HOSTEDWORK_56EC19_INVALID_TABLE = "hosted_room_work_records_invalid";

/* ── growable string buffer ──────────────────────────────────────────────── */

typedef struct {
    char *p;
    size_t n;
    size_t cap;
} hw_sb;

static void hw_sb_init(hw_sb *b) {
    b->cap = 256;
    b->n = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}

static void hw_sb_add(hw_sb *b, const char *s) {
    size_t l;
    if (!b->p || !s) return;
    l = strlen(s);
    if (b->n + l + 1 > b->cap) {
        char *q;
        while (b->n + l + 1 > b->cap) b->cap *= 2;
        q = (char *)realloc(b->p, b->cap);
        if (!q) { free(b->p); b->p = NULL; b->n = 0; return; }
        b->p = q;
    }
    memcpy(b->p + b->n, s, l + 1);
    b->n += l;
}

static void hw_sb_reset(hw_sb *b) {
    b->n = 0;
    if (b->p) b->p[0] = '\0';
}

static void hw_sb_addf(hw_sb *b, const char *fmt, ...) {
    va_list ap, ap2;
    int need;
    char *tmp;
    if (!b->p) return;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need < 0) { va_end(ap2); return; }
    tmp = (char *)malloc((size_t)need + 1);
    if (!tmp) { va_end(ap2); return; }
    vsnprintf(tmp, (size_t)need + 1, fmt, ap2);
    va_end(ap2);
    hw_sb_add(b, tmp);
    free(tmp);
}

static char *hw_sb_take(hw_sb *b) {
    char *p = b->p;
    b->p = NULL;
    b->n = b->cap = 0;
    return p;
}

static char *hw_strdup(const char *s) {
    size_t l;
    char *p;
    if (!s) return NULL;
    l = strlen(s);
    p = (char *)malloc(l + 1);
    if (p) memcpy(p, s, l + 1);
    return p;
}

/* ── small JSON helpers ──────────────────────────────────────────────────── */

static int hw_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int hw_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }

/* Python int(): a JSON number with an integral value (bools are not numbers). */
static int hw_int(const json_t *v, double *out) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (d != (double)(long long)d) return 0;
    *out = d;
    return 1;
}

static int hw_str_eq(const json_t *v, const char *s) {
    if (!v || v->type != JSON_STRING || !v->str_val) return 0;
    return strcmp(v->str_val, s) == 0;
}

static int hw_is_pending(const char *t) {
    return t && strcmp(t, W34_HOSTEDWORK_56EC19_PENDING_TABLE) == 0;
}

static int hw_is_invalid(const char *t) {
    return t && strcmp(t, W34_HOSTEDWORK_56EC19_INVALID_TABLE) == 0;
}

static json_t *hw_error(const char *msg) {
    json_t *o = json_object();
    json_set(o, "error", json_string(msg ? msg : ""));
    return o;
}

/* ── retained_fields ─────────────────────────────────────────────────────── */

/* PoP: w34_hostedwork_56ec19_retained_fields @ session_rules/hosted_rooms.py:retained_fields
 * The chargeable columns of one work-record table, in row_size_sql's order. */
static size_t hw_retained_fields(const char *table, const char *out[12]) {
    static const char *const base[4] = { "room_id", "revision", "digest", "record_json" };
    size_t n = 0;
    size_t i;
    for (i = 0; i < 4; i++) out[n++] = base[i];
    if (hw_is_invalid(table)) {
        out[n++] = "source_table";
    } else {
        out[n++] = "producer_gateway_id";
        out[n++] = "producer_epoch";
    }
    if (hw_is_pending(table) || hw_is_invalid(table)) {
        out[n++] = "target_install_id";
        out[n++] = "route_generation";
        out[n++] = "status";
    }
    return n;
}

json_t *w34_hostedwork_56ec19_retained_fields(const char *table) {
    const char *fields[12];
    size_t n = hw_retained_fields(table, fields);
    size_t i;
    json_t *out = json_array();
    for (i = 0; i < n; i++) json_append(out, json_string(fields[i]));
    return out;
}

/* ── row_size_sql / usage_sql ────────────────────────────────────────────── */

/* PoP: w34_hostedwork_56ec19_row_size_sql @ persistence/sqlite/gateway/_hosted_work.py:row_size_sql
 * "20+" plus a byte charge per retained column; the 20 reserved bytes leave
 * room for the longest bounded disposition. */
static char *hw_row_size_sql(const char *table, const char *prefix) {
    const char *fields[12];
    size_t n = hw_retained_fields(table, fields);
    size_t i;
    hw_sb b;
    hw_sb_init(&b);
    hw_sb_add(&b, "20+");
    for (i = 0; i < n; i++) {
        if (i) hw_sb_add(&b, "+");
        hw_sb_add(&b, "COALESCE(length(CAST(");
        hw_sb_add(&b, prefix ? prefix : "");
        hw_sb_add(&b, fields[i]);
        hw_sb_add(&b, " AS BLOB)),0)");
    }
    return hw_sb_take(&b);
}

char *w34_hostedwork_56ec19_row_size_sql(const char *table, const char *prefix) {
    return hw_row_size_sql(table, prefix);
}

static char *hw_usage_total(const char *const *tables, size_t n) {
    hw_sb b;
    size_t i;
    hw_sb_init(&b);
    for (i = 0; i < n; i++) {
        char *size;
        if (i) hw_sb_add(&b, " + ");
        size = hw_row_size_sql(tables[i], "");
        hw_sb_add(&b, "(SELECT COALESCE(SUM(");
        hw_sb_add(&b, size ? size : "");
        free(size);
        hw_sb_addf(&b, "),0) FROM %s)", tables[i]);
    }
    return hw_sb_take(&b);
}

static char *hw_usage_count(const char *const *tables, size_t n) {
    hw_sb b;
    size_t i;
    hw_sb_init(&b);
    for (i = 0; i < n; i++) {
        if (i) hw_sb_add(&b, " + ");
        hw_sb_addf(&b, "(SELECT COUNT(*) FROM %s)", tables[i]);
    }
    return hw_sb_take(&b);
}

static const char **hw_table_names(const json_t *tables, size_t *out_n) {
    size_t n = hw_is_arr(tables) ? json_len(tables) : 0;
    size_t i;
    const char **names = (const char **)calloc(n + 1, sizeof(const char *));
    *out_n = 0;
    if (!names) return NULL;
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(tables, i);
        if (v && v->type == JSON_STRING) names[(*out_n)++] = v->str_val ? v->str_val : "";
    }
    return names;
}

/* PoP: w34_hostedwork_56ec19_usage_sql @ persistence/sqlite/gateway/_hosted_work.py:usage_sql
 * The two scalar subqueries the budget guard and _budget read. */
json_t *w34_hostedwork_56ec19_usage_sql(const json_t *tables) {
    size_t n = 0;
    const char **names = hw_table_names(tables, &n);
    char *total = hw_usage_total(names, n);
    char *count = hw_usage_count(names, n);
    json_t *out = json_array();
    json_append(out, json_string(total ? total : ""));
    json_append(out, json_string(count ? count : ""));
    free(total);
    free(count);
    free(names);
    return out;
}

/* ── scope_disposition ───────────────────────────────────────────────────── */

/* PoP: w34_hostedwork_56ec19_scope_disposition @ persistence/sqlite/gateway/_hosted_work.py:scope_disposition
 * A retained row stays 'current' only while its producer is still the room's
 * authority; an unacked delivery row becomes 'superseded_authority'. */
char *w34_hostedwork_56ec19_scope_disposition(const char *table, const json_t *row, const json_t *current) {
    const char *disposition;
    const char *gateway;
    double epoch = 0.0;
    int have_epoch;
    int same = 0;

    if (!hw_is_obj(row)) return NULL;
    disposition = json_get_str(row, "disposition", NULL);
    if (!disposition) return NULL;
    if (strcmp(disposition, "current") != 0) return hw_strdup(disposition);

    gateway = json_get_str(row, "producer_gateway_id", NULL);
    have_epoch = hw_int(json_obj_get(row, "producer_epoch"), &epoch);
    if (current && current->type != JSON_NULL) {
        if (hw_is_obj(current)) {
            const char *cg = json_get_str(current, "gateway_id", NULL);
            double ce = 0.0;
            if (!hw_int(json_obj_get(current, "epoch"), &ce)) return NULL;
            same = cg && gateway && have_epoch && ce == epoch && strcmp(cg, gateway) == 0;
        } else if (hw_is_arr(current) && json_len(current) >= 2) {
            const json_t *cg = json_get(current, 0);
            double ce = 0.0;
            if (!hw_int(json_get(current, 1), &ce)) return NULL;
            same = cg && cg->type == JSON_STRING && cg->str_val && gateway && have_epoch
                   && ce == epoch && strcmp(cg->str_val, gateway) == 0;
        }
    } else {
        /* No authority row: the projection has nothing to disagree with. */
        return hw_strdup("current");
    }
    if (same) return hw_strdup("current");
    if (hw_is_pending(table) && !hw_str_eq(json_obj_get(row, "status"), "acked"))
        return hw_strdup("superseded_authority");
    return hw_strdup("historical");
}

/* ── guard DDL ───────────────────────────────────────────────────────────── */

/* PoP: w34_hostedwork_56ec19_guards_sql @ persistence/sqlite/gateway/_hosted_work.py:_guards
 * The budget, immutability, replace and authority-transition triggers, in the
 * exact order and spelling the initializer installs them. */
json_t *w34_hostedwork_56ec19_guards_sql(const json_t *tables) {
    static const char *const ops[2] = { "INSERT", "UPDATE" };
    size_t n = 0, total_n = 0;
    const char **names = hw_table_names(tables, &n);
    const char **all;
    char *total = NULL, *count = NULL;
    json_t *out = json_array();
    size_t i;

    if (!names) return out;
    all = (const char **)calloc(n + 2, sizeof(const char *));
    if (!all) { free(names); return out; }
    for (i = 0; i < n; i++) all[total_n++] = names[i];
    all[total_n++] = W34_HOSTEDWORK_56EC19_INVALID_TABLE;
    total = hw_usage_total(all, total_n);
    count = hw_usage_count(all, total_n);

    for (i = 0; i < n; i++) {
        const char *table = names[i];
        int pending = hw_is_pending(table);
        size_t oi;
        for (oi = 0; oi < 2; oi++) {
            const char *op = ops[oi];
            const char *op_lower = oi == 0 ? "insert" : "update";
            hw_sb key, stmt;
            char *new_size = hw_row_size_sql(table, "NEW.");
            char *plain_size = hw_row_size_sql(table, "");
            char *old_size;
            char *old_count;
            char keybuf[600];

            hw_sb_init(&key);
            hw_sb_add(&key, "room_id=NEW.room_id AND producer_gateway_id=NEW.producer_gateway_id"
                            " AND producer_epoch=NEW.producer_epoch");
            if (pending) hw_sb_add(&key, " AND target_install_id=NEW.target_install_id");
            snprintf(keybuf, sizeof(keybuf), "%s", key.p ? key.p : "");
            hw_sb_take(&key);

            if (oi == 0) {
                hw_sb os, oc;
                hw_sb_init(&os);
                hw_sb_add(&os, "COALESCE((SELECT ");
                hw_sb_add(&os, plain_size ? plain_size : "");
                hw_sb_addf(&os, " FROM %s WHERE %s)", table, keybuf);
                hw_sb_add(&os, ",0)");
                old_size = hw_sb_take(&os);
                hw_sb_init(&oc);
                hw_sb_addf(&oc, "(SELECT COUNT(*) FROM %s WHERE %s)", table, keybuf);
                old_count = hw_sb_take(&oc);
            } else {
                old_size = hw_row_size_sql(table, "OLD.");
                old_count = hw_strdup("1");
            }

            hw_sb_init(&stmt);
            hw_sb_addf(&stmt, "DROP TRIGGER IF EXISTS trg_%s_budget_v2_%s", table, op_lower);
            json_append(out, json_string(stmt.p ? stmt.p : ""));
            hw_sb_reset(&stmt);
            hw_sb_addf(&stmt, "DROP TRIGGER IF EXISTS trg_%s_budget_v3_%s", table, op_lower);
            json_append(out, json_string(stmt.p ? stmt.p : ""));
            hw_sb_reset(&stmt);
            hw_sb_addf(&stmt, "DROP TRIGGER IF EXISTS trg_%s_budget_v4_%s", table, op_lower);
            json_append(out, json_string(stmt.p ? stmt.p : ""));
            hw_sb_reset(&stmt);
            hw_sb_addf(&stmt, "CREATE TRIGGER trg_%s_budget_v4_%s\n", table, op_lower);
            hw_sb_addf(&stmt, "                BEFORE %s ON %s WHEN (\n", op, table);
            hw_sb_addf(&stmt, "                    ((%s)>(%s) AND (%s)-(%s)+(%s)>%d)\n",
                       new_size ? new_size : "", old_size ? old_size : "",
                       total ? total : "", old_size ? old_size : "", new_size ? new_size : "",
                       W34_HOSTEDWORK_56EC19_MAX_STORE_BYTES);
            hw_sb_addf(&stmt, "                    OR (%s=0 AND (%s>%d OR %s+1>%d)))\n",
                       old_count ? old_count : "", total ? total : "",
                       W34_HOSTEDWORK_56EC19_MAX_STORE_BYTES,
                       count ? count : "", W34_HOSTEDWORK_56EC19_MAX_STORE_ROWS);
            hw_sb_add(&stmt, "                BEGIN SELECT RAISE(ABORT, 'work record storage is full'); END");
            json_append(out, json_string(stmt.p ? stmt.p : ""));
            hw_sb_take(&stmt);

            free(new_size);
            free(plain_size);
            free(old_size);
            free(old_count);
        }

        {
            hw_sb stmt;
            hw_sb_init(&stmt);
            hw_sb_addf(&stmt, "CREATE TRIGGER IF NOT EXISTS trg_%s_immutable_v2\n", table);
            hw_sb_addf(&stmt, "            BEFORE UPDATE ON %s WHEN\n", table);
            hw_sb_add(&stmt, "                NEW.room_id!=OLD.room_id OR NEW.producer_gateway_id!=OLD.producer_gateway_id\n");
            hw_sb_add(&stmt, "                OR NEW.producer_epoch!=OLD.producer_epoch\n");
            hw_sb_add(&stmt, "                OR (OLD.disposition!='current' AND (NEW.record_json!=OLD.record_json OR NEW.revision!=OLD.revision\n");
            hw_sb_add(&stmt, "                    OR NEW.digest!=OLD.digest OR NEW.disposition!=OLD.disposition))\n");
            hw_sb_add(&stmt, "            BEGIN SELECT RAISE(ABORT, 'historical work record is immutable'); END");
            json_append(out, json_string(stmt.p ? stmt.p : ""));
            hw_sb_take(&stmt);
        }
        {
            hw_sb stmt;
            hw_sb_init(&stmt);
            hw_sb_addf(&stmt, "CREATE TRIGGER IF NOT EXISTS trg_%s_delete_v2 BEFORE DELETE ON %s\n", table, table);
            hw_sb_add(&stmt, "            WHEN OLD.disposition!='current' AND EXISTS (SELECT 1 FROM hosted_rooms\n");
            hw_sb_add(&stmt, "                WHERE room_id=OLD.room_id AND disbanded_at IS NULL)\n");
            hw_sb_add(&stmt, "            BEGIN SELECT RAISE(ABORT, 'historical work record is immutable'); END");
            json_append(out, json_string(stmt.p ? stmt.p : ""));
            hw_sb_take(&stmt);
        }
        {
            hw_sb stmt;
            hw_sb_init(&stmt);
            hw_sb_addf(&stmt, "CREATE TRIGGER IF NOT EXISTS trg_%s_replace_v2\n", table);
            hw_sb_addf(&stmt, "            BEFORE INSERT ON %s WHEN EXISTS (SELECT 1 FROM %s p WHERE p.room_id=NEW.room_id\n",
                       table, table);
            hw_sb_add(&stmt, "                AND p.producer_gateway_id=NEW.producer_gateway_id AND p.producer_epoch=NEW.producer_epoch\n");
            hw_sb_add(&stmt, "                ");
            if (pending) hw_sb_add(&stmt, " AND p.target_install_id=NEW.target_install_id");
            hw_sb_add(&stmt, " AND p.disposition!='current')\n");
            hw_sb_add(&stmt, "            BEGIN SELECT RAISE(ABORT, 'historical work record is immutable'); END");
            json_append(out, json_string(stmt.p ? stmt.p : ""));
            hw_sb_take(&stmt);
        }
    }

    {
        hw_sb stmt;
        hw_sb_init(&stmt);
        hw_sb_add(&stmt, "CREATE TRIGGER IF NOT EXISTS trg_work_pending_outcome_v2 BEFORE UPDATE ON ");
        hw_sb_add(&stmt, W34_HOSTEDWORK_56EC19_PENDING_TABLE);
        hw_sb_add(&stmt, "\n        WHEN OLD.disposition!='current' AND (NEW.status!=OLD.status OR NEW.route_generation!=OLD.route_generation\n");
        hw_sb_add(&stmt, "            OR NEW.target_install_id!=OLD.target_install_id)\n");
        hw_sb_add(&stmt, "        BEGIN SELECT RAISE(ABORT, 'historical delivery outcome is immutable'); END");
        json_append(out, json_string(stmt.p ? stmt.p : ""));
        hw_sb_take(&stmt);
    }
    {
        hw_sb stmt;
        hw_sb_init(&stmt);
        hw_sb_add(&stmt, "CREATE TRIGGER IF NOT EXISTS trg_work_authority_transition_v2\n");
        hw_sb_add(&stmt, "        AFTER UPDATE OF authority_gateway_id,authority_epoch ON hosted_rooms BEGIN\n");
        hw_sb_addf(&stmt, "        UPDATE %s SET disposition='historical' WHERE room_id=NEW.room_id AND disposition='current'\n",
                   W34_HOSTEDWORK_56EC19_SOURCE_TABLE);
        hw_sb_add(&stmt, "            AND (producer_gateway_id!=NEW.authority_gateway_id OR producer_epoch!=NEW.authority_epoch);\n");
        hw_sb_addf(&stmt, "        UPDATE %s SET disposition=CASE WHEN status='acked' THEN 'historical' ELSE 'superseded_authority' END\n",
                   W34_HOSTEDWORK_56EC19_PENDING_TABLE);
        hw_sb_add(&stmt, "            WHERE room_id=NEW.room_id AND disposition='current'\n");
        hw_sb_add(&stmt, "            AND (producer_gateway_id!=NEW.authority_gateway_id OR producer_epoch!=NEW.authority_epoch);\n");
        hw_sb_add(&stmt, "        END");
        json_append(out, json_string(stmt.p ? stmt.p : ""));
        hw_sb_take(&stmt);
    }

    free(total);
    free(count);
    free(all);
    free(names);
    return out;
}

/* PoP: w34_hostedwork_56ec19_invalid_guards_sql @ persistence/sqlite/gateway/_hosted_work.py:_invalid_guards
 * Opaque orphan evidence has no first-writer owner: every write is refused. */
json_t *w34_hostedwork_56ec19_invalid_guards_sql(void) {
    static const char *const ops[3][2] = {
        { "insert", "INSERT" }, { "update", "UPDATE" }, { "delete", "DELETE" },
    };
    json_t *out = json_array();
    size_t i;
    for (i = 0; i < 3; i++) {
        hw_sb stmt;
        hw_sb_init(&stmt);
        hw_sb_addf(&stmt, "CREATE TRIGGER IF NOT EXISTS trg_work_invalid_%s\n", ops[i][0]);
        hw_sb_addf(&stmt, "            BEFORE %s ON %s\n", ops[i][1], W34_HOSTEDWORK_56EC19_INVALID_TABLE);
        hw_sb_add(&stmt, "            BEGIN SELECT RAISE(ABORT, 'invalid work evidence is immutable'); END");
        json_append(out, json_string(stmt.p ? stmt.p : ""));
        hw_sb_take(&stmt);
    }
    return out;
}

/* ── save_locked ─────────────────────────────────────────────────────────── */

/* PoP: w34_hostedwork_56ec19_save @ persistence/sqlite/gateway/_hosted_work.py:save_locked
 * The upsert of the current record of one scope; a delivery row starts
 * 'pending'.  record_json is the caller's encode() - canonical JSON is a seam. */
json_t *w34_hostedwork_56ec19_save(const char *table, const json_t *record, const char *record_json,
                                   const char *target_install_id, const char *route_generation) {
    static const char *const base[7] = {
        "room_id", "producer_gateway_id", "producer_epoch", "revision", "digest", "record_json", "disposition",
    };
    const char *fields[10];
    const char *keys[4] = { "room_id", "producer_gateway_id", "producer_epoch", NULL };
    size_t nf = 7, nkeys = 3, i;
    const json_t *authority, *digest_v;
    const char *room_id, *gateway;
    double epoch = 0.0, revision = 0.0;
    hw_sb sql;
    json_t *out, *vals;
    int pending = target_install_id != NULL;

    if (!hw_is_obj(record)) return NULL;
    authority = json_obj_get(record, "authority");
    room_id = json_get_str(record, "room_id", NULL);
    gateway = hw_is_obj(authority) ? json_get_str(authority, "gateway_id", NULL) : NULL;
    digest_v = json_obj_get(record, "digest");
    if (!room_id || !gateway || !digest_v) return NULL;
    if (!hw_int(json_obj_get(authority, "epoch"), &epoch)) return NULL;
    if (!hw_int(json_obj_get(record, "revision"), &revision)) return NULL;

    for (i = 0; i < 7; i++) fields[i] = base[i];
    if (pending) {
        fields[nf++] = "target_install_id";
        fields[nf++] = "route_generation";
        fields[nf++] = "status";
        keys[3] = "target_install_id";
        nkeys = 4;
    }

    hw_sb_init(&sql);
    hw_sb_addf(&sql, "INSERT INTO %s (", table ? table : "");
    for (i = 0; i < nf; i++) {
        if (i) hw_sb_add(&sql, ",");
        hw_sb_add(&sql, fields[i]);
    }
    hw_sb_add(&sql, ") VALUES (");
    for (i = 0; i < nf; i++) {
        if (i) hw_sb_add(&sql, ",");
        hw_sb_add(&sql, "?");
    }
    hw_sb_add(&sql, ") ON CONFLICT(");
    for (i = 0; i < nkeys; i++) {
        if (i) hw_sb_add(&sql, ",");
        hw_sb_add(&sql, keys[i]);
    }
    hw_sb_add(&sql, ") DO UPDATE SET ");
    {
        int first = 1;
        for (i = 0; i < nf; i++) {
            size_t k;
            int keyed = 0;
            for (k = 0; k < nkeys; k++)
                if (strcmp(fields[i], keys[k]) == 0) keyed = 1;
            if (keyed) continue;
            if (!first) hw_sb_add(&sql, ",");
            first = 0;
            hw_sb_addf(&sql, "%s=excluded.%s", fields[i], fields[i]);
        }
    }

    vals = json_array();
    json_append(vals, json_string(room_id));
    json_append(vals, json_string(gateway));
    json_append(vals, json_number(epoch));
    json_append(vals, json_number(revision));
    json_append(vals, json_copy(digest_v));
    json_append(vals, json_string(record_json ? record_json : ""));
    json_append(vals, json_string("current"));
    if (pending) {
        json_append(vals, json_string(target_install_id));
        json_append(vals, route_generation ? json_string(route_generation) : json_null());
        json_append(vals, json_string("pending"));
    }

    out = json_object();
    json_set(out, "sql", json_string(sql.p ? sql.p : ""));
    json_set(out, "values", vals);
    hw_sb_take(&sql);
    return out;
}

/* ── budget ──────────────────────────────────────────────────────────────── */

/* PoP: w34_hostedwork_56ec19_budget_refused @ persistence/sqlite/gateway/_hosted_work.py:_budget
 * total - (old or 0) + size > MAX_STORE_BYTES, or a new row past MAX_STORE_ROWS. */
int w34_hostedwork_56ec19_budget_refused(const json_t *in) {
    const json_t *total_v, *count_v, *old_v, *size_v;
    double total = 0.0, count = 0.0, size = 0.0;
    double old_size = 0.0;
    int old_present = 0;

    if (!hw_is_obj(in)) return 0;
    total_v = json_obj_get(in, "total");
    count_v = json_obj_get(in, "count");
    old_v = json_obj_get(in, "old_size");
    size_v = json_obj_get(in, "size");
    if (!hw_int(total_v, &total) || !hw_int(count_v, &count) || !hw_int(size_v, &size)) return 0;
    if (old_v && old_v->type != JSON_NULL) {
        if (!hw_int(old_v, &old_size)) return 0;
        old_present = 1;
    }
    if (total - old_size + size > (double)W34_HOSTEDWORK_56EC19_MAX_STORE_BYTES) return 1;
    if (count - (double)old_present + 1.0 > (double)W34_HOSTEDWORK_56EC19_MAX_STORE_ROWS) return 1;
    return 0;
}

/* ── authority gateway identity ──────────────────────────────────────────── */

/* identifier(..., label="authority gateway", max_chars=128) then _gateway's
 * canonical check (the value must be its own stripped form). */
static const char *hw_gateway(const json_t *v, const char **err) {
    const char *s;
    size_t i, len, lead = 0, trail;
    if (!v || v->type != JSON_STRING || !v->str_val) {
        *err = "authority gateway must be a string";
        return NULL;
    }
    s = v->str_val;
    len = strlen(s);
    trail = len;
    while (lead < len && strchr(" \t\n\r\f\v", s[lead])) lead++;
    while (trail > lead && strchr(" \t\n\r\f\v", s[trail - 1])) trail--;
    if (trail == lead || trail - lead > 128) {
        *err = "invalid authority gateway";
        return NULL;
    }
    if (!((s[lead] >= 'A' && s[lead] <= 'Z') || (s[lead] >= 'a' && s[lead] <= 'z')
          || (s[lead] >= '0' && s[lead] <= '9'))) {
        *err = "invalid authority gateway";
        return NULL;
    }
    for (i = lead + 1; i < trail; i++) {
        char c = s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
              || c == '.' || c == '_' || c == ':' || c == '-')) {
            *err = "invalid authority gateway";
            return NULL;
        }
    }
    if (lead != 0 || trail != len) {
        *err = "authority gateway must be canonical";
        return NULL;
    }
    return s;
}

/* ── authority history ───────────────────────────────────────────────────── */

typedef struct {
    char *gateway;
    double epoch;
    double from_seq;
} hw_span;

static void hw_spans_free(hw_span *spans, size_t n) {
    size_t i;
    if (!spans) return;
    for (i = 0; i < n; i++) free(spans[i].gateway);
    free(spans);
}

/* validate_history(value, gateway_id=..., epoch=...). */
static int hw_validate_history(const hw_span *spans, size_t n, const char *gateway_id, double epoch,
                               const char **err) {
    size_t i;
    if (n < 1 || n > (size_t)W34_HOSTEDWORK_56EC19_MAX_AUTHORITY_SPANS) {
        *err = "authority history exceeds its bound or is absent";
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (!spans[i].gateway) {
            *err = "authority history fields are invalid";
            return 0;
        }
        if (spans[i].epoch < 1.0 || spans[i].from_seq < 0.0) {
            *err = "authority history coordinates are invalid";
            return 0;
        }
        if (i == 0) {
            if (spans[i].epoch != 1.0 || spans[i].from_seq != 0.0) {
                *err = "authority history has no verified starting point";
                return 0;
            }
        } else if (spans[i].epoch != spans[i - 1].epoch + 1.0 || spans[i].from_seq <= spans[i - 1].from_seq) {
            *err = "authority history has a gap or unordered boundary";
            return 0;
        }
    }
    if (strcmp(spans[n - 1].gateway, gateway_id ? gateway_id : "") != 0 || spans[n - 1].epoch != epoch) {
        *err = "authority history does not reach the current owner";
        return 0;
    }
    return 1;
}

/* One {gateway_id, epoch, from_seq} item of a history array. */
static int hw_span_from_json(const json_t *item, hw_span *out, const char **err) {
    const char *gw;
    double epoch = 0.0, from_seq = 0.0;
    if (!hw_is_obj(item) || json_object_size(item) != 3
            || !json_has(item, "gateway_id") || !json_has(item, "epoch") || !json_has(item, "from_seq")) {
        *err = "authority history fields are invalid";
        return 0;
    }
    gw = hw_gateway(json_obj_get(item, "gateway_id"), err);
    if (!gw) return 0;
    if (!hw_int(json_obj_get(item, "epoch"), &epoch) || epoch < 1.0
            || !hw_int(json_obj_get(item, "from_seq"), &from_seq) || from_seq < 0.0) {
        *err = "authority history coordinates are invalid";
        return 0;
    }
    out->gateway = hw_strdup(gw);
    out->epoch = epoch;
    out->from_seq = from_seq;
    return out->gateway != NULL;
}

/* PoP: w34_hostedwork_56ec19_read_history @ persistence/sqlite/gateway/_hosted_work.py:read_history_locked
 * Rebuilds the committed authority chain from the room events: the first
 * transfer's previous owner, then one span per transfer that extends its
 * predecessor, then validate_history against the caller's authority. */
json_t *w34_hostedwork_56ec19_read_history(const json_t *rows, const char *gateway_id, double epoch) {
    size_t n = hw_is_arr(rows) ? json_len(rows) : 0;
    size_t i;
    hw_span *spans;
    const char *err = NULL;
    json_t *out, *hist;

    if (n >= (size_t)W34_HOSTEDWORK_56EC19_MAX_AUTHORITY_SPANS)
        return hw_error("authority history exceeds its bound");
    if (n == 0) {
        if (epoch == 1.0) {
            out = json_object();
            json_set(out, "none", json_bool(true));
            return out;
        }
        return hw_error("current authority has no committed lineage");
    }
    spans = (hw_span *)calloc(n + 2, sizeof(hw_span));
    if (!spans) return hw_error("authority history exceeds its bound");

    {
        const json_t *row0 = json_get(rows, 0);
        json_t *payload = hw_is_obj(row0) ? json_parse(json_get_str(row0, "payload_json", "null"), NULL) : NULL;
        const char *gw;

        if (hw_is_obj(payload) && hw_str_eq(json_obj_get(payload, "previous_gateway_id"), "legacy")) {
            json_free(payload);
            hw_spans_free(spans, n + 2);
            out = json_object();
            json_set(out, "none", json_bool(true));
            return out;
        }
        if (!hw_is_obj(payload) || !json_has(payload, "previous_gateway_id")) {
            json_free(payload);
            hw_spans_free(spans, n + 2);
            return hw_error("authority history has no previous owner");
        }
        gw = hw_gateway(json_obj_get(payload, "previous_gateway_id"), &err);
        if (!gw) {
            json_free(payload);
            hw_spans_free(spans, n + 2);
            return hw_error(err);
        }
        spans[0].gateway = hw_strdup(gw);
        spans[0].epoch = 1.0;
        spans[0].from_seq = 0.0;
        json_free(payload);
        if (!spans[0].gateway) {
            hw_spans_free(spans, n + 2);
            return hw_error("authority history fields are invalid");
        }
    }

    for (i = 0; i < n; i++) {
        const json_t *row = json_get(rows, i);
        json_t *actor = hw_is_obj(row) ? json_parse(json_get_str(row, "actor_json", "null"), NULL) : NULL;
        json_t *payload = hw_is_obj(row) ? json_parse(json_get_str(row, "payload_json", "null"), NULL) : NULL;
        double expected = spans[i].epoch + 1.0;
        double payload_epoch = 0.0, row_epoch = 0.0, seq = 0.0;
        const json_t *pgw, *aepoch;
        const char *gw;
        int actor_ok;

        actor_ok = hw_is_obj(actor) && json_object_size(actor) == 2
                   && hw_str_eq(json_obj_get(actor, "kind"), "system")
                   && hw_str_eq(json_obj_get(actor, "id"), "authority-control");
        if (!actor_ok || !hw_is_obj(payload)) {
            json_free(actor);
            json_free(payload);
            hw_spans_free(spans, n + 2);
            return hw_error("authority claim provenance is invalid");
        }
        pgw = json_obj_get(payload, "previous_gateway_id");
        aepoch = json_obj_get(payload, "authority_epoch");
        if (!hw_str_eq(pgw, spans[i].gateway)
                || !hw_int(aepoch, &payload_epoch) || payload_epoch != expected
                || !hw_int(json_obj_get(row, "authority_epoch"), &row_epoch) || row_epoch != expected
                || !hw_int(json_obj_get(row, "seq"), &seq)) {
            json_free(actor);
            json_free(payload);
            hw_spans_free(spans, n + 2);
            return hw_error("authority claim does not extend its predecessor");
        }
        gw = hw_gateway(json_obj_get(payload, "authority_gateway_id"), &err);
        if (!gw) {
            json_free(actor);
            json_free(payload);
            hw_spans_free(spans, n + 2);
            return hw_error(err);
        }
        spans[i + 1].gateway = hw_strdup(gw);
        spans[i + 1].epoch = expected;
        spans[i + 1].from_seq = seq;
        json_free(actor);
        json_free(payload);
        if (!spans[i + 1].gateway) {
            hw_spans_free(spans, n + 2);
            return hw_error("authority history fields are invalid");
        }
    }

    if (!hw_validate_history(spans, n + 1, gateway_id, epoch, &err)) {
        hw_spans_free(spans, n + 2);
        return hw_error(err);
    }
    hist = json_array();
    for (i = 0; i <= n; i++) {
        json_t *item = json_object();
        json_set(item, "gateway_id", json_string(spans[i].gateway));
        json_set(item, "epoch", json_number(spans[i].epoch));
        json_set(item, "from_seq", json_number(spans[i].from_seq));
        json_append(hist, item);
    }
    hw_spans_free(spans, n + 2);
    out = json_object();
    json_set(out, "history", hist);
    return out;
}

/* PoP: w34_hostedwork_56ec19_prefix_check @ persistence/sqlite/gateway/_hosted_work.py:source_prefix_locked
 * Every event up to seq must sit in the span its authority epoch names, and a
 * transfer event must be exactly the first event of a later span. */
json_t *w34_hostedwork_56ec19_prefix_check(const json_t *spans, const json_t *events, double seq,
                                           const char *gateway_id, double epoch) {
    size_t ns = hw_is_arr(spans) ? json_len(spans) : 0;
    size_t ne = hw_is_arr(events) ? json_len(events) : 0;
    size_t i;
    hw_span *chain;
    const char *err = NULL;
    json_t *out;
    double count = 0.0;

    if (ns < 1 || ns > (size_t)W34_HOSTEDWORK_56EC19_MAX_AUTHORITY_SPANS)
        return hw_error("authority history exceeds its bound or is absent");
    /* read_history_locked saw one row per span after the first: a single span
     * is an empty transfer log, whose disposition decides the prefix error. */
    if (ns == 1)
        return hw_error(epoch == 1.0 ? "successor work evidence has no verified lineage"
                                     : "current authority has no committed lineage");
    chain = (hw_span *)calloc(ns, sizeof(hw_span));
    if (!chain) return hw_error("authority history exceeds its bound or is absent");
    for (i = 0; i < ns; i++) {
        if (!hw_span_from_json(json_get(spans, i), &chain[i], &err)) {
            hw_spans_free(chain, ns);
            return hw_error(err);
        }
    }
    /* A chain whose root owner is the legacy sentinel is not a history of
     * physical-host transfers; read_history_locked declines it unvalidated. */
    if (strcmp(chain[0].gateway, "legacy") == 0) {
        hw_spans_free(chain, ns);
        return hw_error("successor work evidence has no verified lineage");
    }
    if (!hw_validate_history(chain, ns, gateway_id, epoch, &err)) {
        hw_spans_free(chain, ns);
        return hw_error(err);
    }

    for (i = 0; i < ne; i++) {
        const json_t *event = json_get(events, i);
        json_t *actor = NULL;
        const hw_span *span = NULL;
        size_t k;
        int boundary, transfer;
        double ev_seq = 0.0, ev_epoch = 0.0;
        const char *kind;

        count += 1.0;
        for (k = ns; k > 0; k--) {
            if (count >= chain[k - 1].from_seq) { span = &chain[k - 1]; break; }
        }
        if (!span) {
            hw_spans_free(chain, ns);
            return hw_error("event precedes the known authority history");
        }
        if (hw_is_obj(event)) actor = json_parse(json_get_str(event, "actor_json", "null"), NULL);
        boundary = span->epoch > 1.0 && count == span->from_seq;
        kind = json_get_str(event, "kind", "");
        transfer = strcmp(kind, "authority.claimed") == 0 || strcmp(kind, "authority.lost") == 0;
        if (!hw_is_obj(event)
                || !hw_int(json_obj_get(event, "seq"), &ev_seq) || ev_seq != count
                || !hw_int(json_obj_get(event, "authority_epoch"), &ev_epoch) || ev_epoch != span->epoch
                || (hw_str_eq(json_obj_get(actor, "kind"), "gateway")
                    && !hw_str_eq(json_obj_get(actor, "id"), span->gateway))
                || (transfer != boundary)) {
            json_free(actor);
            hw_spans_free(chain, ns);
            return hw_error("work record history prefix is not verified");
        }
        json_free(actor);
    }
    hw_spans_free(chain, ns);
    if (count != seq) return hw_error("work record history prefix is unavailable");
    out = json_object();
    json_set(out, "spans", json_copy(spans));
    return out;
}

/* ── capture tasks ───────────────────────────────────────────────────────── */

static json_t *hw_capture_result(json_t *tasks, json_t *receipts, const char *reason) {
    json_t *out = json_object();
    json_set(out, "tasks", tasks);
    json_set(out, "receipts", receipts);
    json_set(out, "reason", reason ? json_string(reason) : json_null());
    return out;
}

static json_t *hw_capture_reject(json_t *tasks, json_t *receipts, const char *reason) {
    json_free(tasks);
    json_free(receipts);
    return hw_capture_result(json_array(), json_array(), reason);
}

/* PoP: w34_hostedwork_56ec19_capture_tasks @ persistence/sqlite/gateway/_hosted_work.py:_capture_tasks
 * The bounded task/receipt slice of a capture, or empty lists and the reason
 * they are unavailable.  store_missing is table_exists(); normalized holds
 * normalize_task_payload()'s (payload, encoded, digest) per task row. */
json_t *w34_hostedwork_56ec19_capture_tasks(const json_t *in) {
    static const char *const task_row_fields[8] = {
        "task_id", "thread_id", "turn_id", "source_event_seq",
        "execution_generation", "cancel_generation", "settlement_id", "cancel_id",
    };
    static const char *const receipt_fields[11] = {
        "room_id", "home_install_id", "authority_gateway_id", "authority_epoch", "member_id",
        "target_install_id", "target_profile", "task_id", "execution_generation", "run_id", "session_id",
    };
    const json_t *tasks_in, *receipts_in, *normalized_in, *missing;
    json_t *tasks_out = json_array();
    json_t *receipts_out = json_array();
    size_t nt = 0, nr = 0, nn = 0, i, k;

    if (!hw_is_obj(in)) return hw_capture_reject(tasks_out, receipts_out, "task_store_missing");
    missing = json_obj_get(in, "store_missing");
    if (missing && missing->type == JSON_BOOL && missing->bool_val)
        return hw_capture_reject(tasks_out, receipts_out, "task_store_missing");

    tasks_in = json_obj_get(in, "tasks");
    receipts_in = json_obj_get(in, "receipts");
    normalized_in = json_obj_get(in, "normalized");
    nt = hw_is_arr(tasks_in) ? json_len(tasks_in) : 0;
    nr = hw_is_arr(receipts_in) ? json_len(receipts_in) : 0;
    nn = hw_is_arr(normalized_in) ? json_len(normalized_in) : 0;

    if (nt > (size_t)W34_HOSTEDWORK_56EC19_MAX_TASKS || nr > (size_t)W34_HOSTEDWORK_56EC19_MAX_RECEIPTS)
        return hw_capture_reject(tasks_out, receipts_out, "bounds_exceeded");

    for (i = 0; i < nt; i++) {
        const json_t *row = json_get(tasks_in, i);
        const json_t *nz = i < nn ? json_get(normalized_in, i) : NULL;
        const json_t *payload;
        json_t *task;

        if (!hw_is_obj(row) || !hw_is_obj(nz))
            return hw_capture_reject(tasks_out, receipts_out, "unsupported_task");
        payload = json_obj_get(nz, "payload");
        if (!hw_is_obj(payload)) return hw_capture_reject(tasks_out, receipts_out, "unsupported_task");
        if (!hw_str_eq(json_obj_get(nz, "digest"), json_get_str(row, "payload_digest", "\x01"))
                || strcmp(json_get_str(nz, "encoded", "\x01"), json_get_str(row, "payload_json", "\x02")) != 0)
            return hw_capture_reject(tasks_out, receipts_out, "unsupported_task");
        {
            double a = 0.0, b = 0.0;
            if (!hw_int(json_obj_get(payload, "source_event_seq"), &a)
                    || !hw_int(json_obj_get(row, "source_event_seq"), &b) || a != b)
                return hw_capture_reject(tasks_out, receipts_out, "unsupported_task");
        }
        {
            const json_t *status = json_obj_get(row, "status");
            const json_t *profile = json_obj_get(payload, "target_profile");
            const json_t *member = json_obj_get(payload, "target_member_id");
            if (!status || !profile) return hw_capture_reject(tasks_out, receipts_out, "unsupported_task");
            task = json_object();
            for (k = 0; k < 8; k++) {
                const json_t *v = json_obj_get(row, task_row_fields[k]);
                if (!v) {
                    json_free(task);
                    return hw_capture_reject(tasks_out, receipts_out, "unsupported_task");
                }
                json_set(task, task_row_fields[k], json_copy(v));
            }
            json_set(task, "payload_sha256", json_copy(json_obj_get(nz, "digest")));
            json_set(task, "phase", json_copy(status));
            json_set(task, "profile", json_copy(profile));
            json_set(task, "member_id", member ? json_copy(member) : json_copy(profile));
            json_append(tasks_out, task);
        }
    }
    for (i = 0; i < nr; i++) {
        const json_t *row = json_get(receipts_in, i);
        json_t *receipt;
        if (!hw_is_obj(row)) return hw_capture_reject(tasks_out, receipts_out, "unsupported_task");
        receipt = json_object();
        for (k = 0; k < 11; k++) {
            const json_t *v = json_obj_get(row, receipt_fields[k]);
            if (!v) {
                json_free(receipt);
                return hw_capture_reject(tasks_out, receipts_out, "unsupported_task");
            }
            json_set(receipt, receipt_fields[k], json_copy(v));
        }
        json_append(receipts_out, receipt);
    }
    return hw_capture_result(tasks_out, receipts_out, NULL);
}

/* ── constants ───────────────────────────────────────────────────────────── */

/* PoP: w34_hostedwork_56ec19_constants @ persistence/sqlite/gateway/_hosted_work.py:<module>
 * TRANSFER_KINDS and the limits the capture is bounded by. */
json_t *w34_hostedwork_56ec19_constants(void) {
    static const char *const limitations[5] = {
        "process_local_approvals_not_captured", "field_journals_not_captured",
        "external_effects_not_captured", "absent_record_is_not_non_admission", "not_execution_checkpoint",
    };
    json_t *o = json_object();
    json_t *kinds = json_array();
    json_t *lims = json_array();
    json_t *tables = json_object();
    size_t i;
    json_append(kinds, json_string("authority.claimed"));
    json_append(kinds, json_string("authority.lost"));
    for (i = 0; i < 5; i++) json_append(lims, json_string(limitations[i]));
    json_set(tables, "source", json_string(W34_HOSTEDWORK_56EC19_SOURCE_TABLE));
    json_set(tables, "target", json_string(W34_HOSTEDWORK_56EC19_TARGET_TABLE));
    json_set(tables, "pending", json_string(W34_HOSTEDWORK_56EC19_PENDING_TABLE));
    json_set(tables, "invalid", json_string(W34_HOSTEDWORK_56EC19_INVALID_TABLE));
    json_set(o, "max_tasks", json_number(W34_HOSTEDWORK_56EC19_MAX_TASKS));
    json_set(o, "max_receipts", json_number(W34_HOSTEDWORK_56EC19_MAX_RECEIPTS));
    json_set(o, "max_bytes", json_number(W34_HOSTEDWORK_56EC19_MAX_BYTES));
    json_set(o, "max_store_bytes", json_number(W34_HOSTEDWORK_56EC19_MAX_STORE_BYTES));
    json_set(o, "max_store_rows", json_number(W34_HOSTEDWORK_56EC19_MAX_STORE_ROWS));
    json_set(o, "max_authority_spans", json_number(W34_HOSTEDWORK_56EC19_MAX_AUTHORITY_SPANS));
    json_set(o, "transfer_kinds", kinds);
    json_set(o, "limitations", lims);
    json_set(o, "tables", tables);
    return o;
}
