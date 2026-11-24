/*
 * mutation_transcript.c -- real C11 port of the rewind body of upstream Hermes
 * persistence/sqlite/conversation/_store/mutation_transcript.py.
 *
 * PoP: w21_mutationtr_f4b296_rewind @ persistence/sqlite/conversation/_store/mutation_transcript.py:rewind_in_transaction
 * PoP: w21_mutationtr_f4b296_placeholders @ persistence/sqlite/conversation/_store/sql_common.py:_placeholders
 *
 * The sqlite connection is the request's scripted `db`; the four messages.py
 * helpers the module calls (_check_transcript_write_guards,
 * _split_rewind_target, _insert_message_rows, _active_transcript_counts) are
 * scripted per request and recorded, since they belong to messages.py. Every
 * statement this module issues, and every bound parameter, is emitted in order.
 */
#include "hermes_json.h"
#include "noc_w21_mutationtr_f4b296.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* persistence/sqlite/conversation/_store/messages_sql.py */
static const char MT_ACTIVE_IDS_SQL[] =
    "SELECT id FROM messages WHERE session_id = ? AND active = 1 ORDER BY id";
static const char MT_SET_COUNTERS_SQL[] =
    "UPDATE sessions SET message_count = ?, tool_call_count = ?";

/* --- helpers ------------------------------------------------------------- */

static char *mt_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* int(value) for the integral scalars an id can be. */
static long long mt_int_of(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING && v->str_val) return strtoll(v->str_val, NULL, 10);
    return 0;
}

/* repr(value) for the role assert's message. */
static char *mt_repr(const json_t *v) {
    char buf[128];
    if (!v || v->type == JSON_NULL) return mt_dup("None");
    switch (v->type) {
    case JSON_BOOL:   return mt_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return mt_dup(buf);
    case JSON_STRING:
        /* Python prefers single quotes unless the text holds one and no double quote */
        if (v->str_val && strchr(v->str_val, '\'') && !strchr(v->str_val, '"'))
            snprintf(buf, sizeof(buf), "\"%s\"", v->str_val);
        else
            snprintf(buf, sizeof(buf), "'%s'", v->str_val ? v->str_val : "");
        return mt_dup(buf);
    case JSON_ARRAY:  return mt_dup("[]");
    case JSON_OBJECT: return mt_dup("{}");
    }
    return mt_dup("");
}

static json_t *mt_raised(const char *type, const char *message) {
    json_t *out = json_object();
    json_set(out, "status", json_string("raised"));
    json_set(out, "type", json_string(type ? type : "Exception"));
    json_set(out, "message", json_string(message ? message : ""));
    return out;
}

static void mt_stmt(json_t *log, const char *sql, json_t *params) {
    json_t *entry = json_object();
    json_set(entry, "sql", json_string(sql));
    json_set(entry, "params", params ? params : json_array());
    json_append(log, entry);
}

char *w21_mutationtr_f4b296_placeholders(double count) {
    long n = (long)count;
    char *out;
    size_t i;
    if (n < 0) n = 0;
    out = (char *)malloc((size_t)n * 2 + 1);
    if (!out) return NULL;
    out[0] = '\0';
    for (i = 0; i < (size_t)n; i++) {
        if (i) strcat(out, ",");
        strcat(out, "?");
    }
    return out;
}

/* --- rewind_in_transaction ---------------------------------------------- */

json_t *w21_mutationtr_f4b296_rewind(const json_t *req) {
    const char *session_id = json_get_str(req, "session_id", "");
    json_t *tid_node = json_obj_get(req, "target_message_id");
    json_t *expected_ids = json_obj_get(req, "expected_active_ids");
    json_t *expected_content = json_obj_get(req, "expected_target_content");
    json_t *guards = json_obj_get(req, "guards");
    json_t *split = json_obj_get(req, "split");
    json_t *counts = json_obj_get(req, "counts");
    json_t *db = json_obj_get(req, "db");
    int preserve = json_get_bool(req, "preserve_compaction_handoff", 0);
    long long tid = mt_int_of(tid_node);
    json_t *log = json_array();
    json_t *out = json_object();
    json_t *target_row = NULL;
    json_t *ids = json_array();
    json_t *replacement = NULL;
    json_t *guard_args, *split_args = NULL, *inserts = NULL;
    json_t *head_id = json_null();
    json_t *replacement_id = json_null();
    char msgbuf[256];

    /* 1. _check_transcript_write_guards(holder, conn, session_id, None,
     *       reject_active_turn_lease=True, reject_active_compression_lock=True) */
    guard_args = json_object();
    json_set(guard_args, "session_id", json_string(session_id));
    json_set(guard_args, "compression_lock_holder", json_null());
    json_set(guard_args, "reject_active_turn_lease", json_bool(1));
    json_set(guard_args, "reject_active_compression_lock", json_bool(1));
    if (guards && json_get_str(guards, "action", "pass") &&
        !strcmp(json_get_str(guards, "action", "pass"), "raise")) {
        const char *type = json_get_str(guards, "type", "Exception");
        const char *message = json_get_str(guards, "message", "");
        json_free(guard_args);
        json_free(log);
        json_free(ids);
        json_free(out);
        return mt_raised(type, message);
    }

    /* 2. expected_active_ids CAS */
    if (expected_ids && expected_ids->type != JSON_NULL) {
        json_t *params = json_array();
        size_t i, n;
        json_append(params, json_string(session_id));
        mt_stmt(log, MT_ACTIVE_IDS_SQL, params);
        n = (expected_ids->type == JSON_ARRAY) ? expected_ids->c.count : 0;
        {
            json_t *actual = db ? json_obj_get(db, "active_ids") : NULL;
            size_t actual_n = (actual && actual->type == JSON_ARRAY) ? actual->c.count : 0;
            int same = (actual_n == n);
            for (i = 0; same && i < n; i++) {
                long long want = mt_int_of(expected_ids->c.items[i]);
                long long got = (i < actual_n) ? mt_int_of(actual->c.items[i]) : 0;
                if (want != got) same = 0;
            }
            if (!same) {
                json_free(guard_args);
                json_free(log);
                json_free(ids);
                json_free(out);
                return mt_raised("RuntimeError",
                                 "active transcript changed before the rewind could be persisted");
            }
        }
    }

    /* 3. the target row */
    {
        json_t *params = json_array();
        json_t *row;
        json_append(params, json_number((double)tid));
        json_append(params, json_string(session_id));
        mt_stmt(log, "SELECT * FROM messages WHERE id = ? AND session_id = ?", params);
        row = db ? json_obj_get(db, "row") : NULL;
        if (!row || row->type == JSON_NULL) {
            json_free(guard_args);
            json_free(log);
            json_free(ids);
            json_free(out);
            snprintf(msgbuf, sizeof(msgbuf), "message %lld not found in session %s", tid,
                     session_id);
            return mt_raised("ValueError", msgbuf);
        }
        target_row = json_copy(row);
    }

    /* 4. the role assert */
    {
        json_t *role = json_obj_get(target_row, "role");
        int is_user = role && role->type == JSON_STRING && role->str_val &&
                      !strcmp(role->str_val, "user");
        if (!is_user) {
            char *shown = mt_repr(role);
            json_free(guard_args);
            json_free(log);
            json_free(ids);
            json_free(target_row);
            json_free(out);
            snprintf(msgbuf, sizeof(msgbuf),
                     "rewind target must be a 'user' message (got role=%s, id=%lld)",
                     shown ? shown : "", tid);
            free(shown);
            return mt_raised("ValueError", msgbuf);
        }
    }

    /* 5. _split_rewind_target(holder, target_row, expected_target_content,
     *       preserve_compaction_handoff) */
    if (preserve || (expected_content && expected_content->type != JSON_NULL)) {
        const char *action = split ? json_get_str(split, "action", "none") : "none";
        split_args = json_object();
        json_set(split_args, "target_row", json_copy(target_row));
        json_set(split_args, "expected_target_content",
                 (expected_content && expected_content->type != JSON_NULL)
                     ? json_copy(expected_content) : json_null());
        json_set(split_args, "preserve_compaction_handoff", json_bool(preserve));
        if (!strcmp(action, "raise")) {
            const char *type = json_get_str(split, "type", "Exception");
            const char *message = json_get_str(split, "message", "");
            json_free(guard_args);
            json_free(log);
            json_free(ids);
            json_free(target_row);
            json_free(split_args);
            json_free(out);
            return mt_raised(type, message);
        }
        if (!strcmp(action, "return")) {
            json_t *value = json_obj_get(split, "value");
            replacement = (value && value->type != JSON_NULL) ? json_copy(value) : NULL;
        }
    }

    /* 6. the ids to deactivate */
    {
        json_t *params = json_array();
        json_t *rewound = db ? json_obj_get(db, "rewound_ids") : NULL;
        size_t i;
        json_append(params, json_string(session_id));
        json_append(params, json_number((double)tid));
        mt_stmt(log, "SELECT id FROM messages WHERE session_id = ? AND id >= ? AND active = 1",
                params);
        for (i = 0; rewound && rewound->type == JSON_ARRAY && i < rewound->c.count; i++)
            json_append(ids, json_copy(rewound->c.items[i]));
    }

    /* 7. UPDATE messages SET active = 0 WHERE id IN (?,...) */
    if (json_len(ids) > 0) {
        char *marks = w21_mutationtr_f4b296_placeholders((double)json_len(ids));
        size_t need = strlen(marks ? marks : "") + 64;
        char *sql = (char *)malloc(need);
        if (sql) {
            snprintf(sql, need, "UPDATE messages SET active = 0 WHERE id IN (%s)",
                     marks ? marks : "");
            mt_stmt(log, sql, json_copy(ids));
            free(sql);
        }
        free(marks);
    }

    /* 8. the replacement insert + last_insert_rowid */
    if (replacement) {
        inserts = json_array();
        json_append(inserts, json_copy(replacement));
        mt_stmt(log, "SELECT last_insert_rowid()", json_array());
        {
            json_t *rowid = db ? json_obj_get(db, "last_rowid") : NULL;
            json_free(replacement_id);
            replacement_id = json_number((double)mt_int_of(rowid));
        }
    }

    /* 9. rewind_count bump */
    {
        json_t *params = json_array();
        json_append(params, json_string(session_id));
        mt_stmt(log, "UPDATE sessions SET rewind_count = COALESCE(rewind_count, 0) + 1 WHERE id = ?",
                params);
    }

    /* 10/11. the active counts, then the counter rewrite */
    {
        json_t *params = json_array();
        long long message_count = counts && counts->type == JSON_ARRAY ? mt_int_of(counts->c.items[0]) : 0;
        long long tool_call_count = (counts && counts->type == JSON_ARRAY && counts->c.count > 1)
                                        ? mt_int_of(counts->c.items[1]) : 0;
        json_append(params, json_number((double)message_count));
        json_append(params, json_number((double)tool_call_count));
        json_append(params, json_string(session_id));
        {
            size_t need = strlen(MT_SET_COUNTERS_SQL) + 32;
            char *sql = (char *)malloc(need);
            if (sql) {
                snprintf(sql, need, "%s WHERE id = ?", MT_SET_COUNTERS_SQL);
                mt_stmt(log, sql, params);
                free(sql);
            } else {
                json_free(params);
            }
        }
    }

    /* 12. the new head */
    {
        json_t *params = json_array();
        json_t *head = db ? json_obj_get(db, "head_id") : NULL;
        json_append(params, json_string(session_id));
        mt_stmt(log, "SELECT MAX(id) FROM messages WHERE session_id = ? AND active = 1", params);
        json_free(head_id);
        head_id = (head && head->type != JSON_NULL) ? json_copy(head) : json_null();
    }

    json_set(out, "status", json_string("ok"));
    json_set(out, "target_row", json_copy(target_row));
    json_set(out, "ids", json_copy(ids));
    json_set(out, "head_id", json_copy(head_id));
    json_set(out, "replacement_message_id", json_copy(replacement_id));
    json_set(out, "guard_args", guard_args);
    json_set(out, "split_args", split_args ? json_copy(split_args) : json_null());
    json_set(out, "inserts", inserts ? json_copy(inserts) : json_null());
    json_set(out, "statements", json_copy(log));

    json_free(target_row);
    json_free(ids);
    json_free(replacement);
    json_free(head_id);
    json_free(replacement_id);
    json_free(split_args);
    json_free(inserts);
    json_free(log);
    return out;
}
