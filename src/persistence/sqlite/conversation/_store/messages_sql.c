/*
 * messages_sql.c - C11 port of upstream
 * persistence/sqlite/conversation/_store/messages_sql.py
 * (gate w24_messagessq_1dd421).
 *
 * PoP: w24_messagessq_1dd421_write_columns @ persistence/sqlite/conversation/_store/messages_sql.py:_MESSAGE_WRITE_COLUMNS
 * PoP: w24_messagessq_1dd421_schema_keys @ persistence/sqlite/conversation/_store/messages_sql.py:_MESSAGE_SCHEMA_KEYS
 * PoP: w24_messagessq_1dd421_coerce_ts @ persistence/sqlite/conversation/_store/messages_sql.py:_coerce_timestamp
 * PoP: w24_messagessq_1dd421_parse_tool_calls @ persistence/sqlite/conversation/_store/messages_sql.py:_parse_tool_calls
 * PoP: w24_messagessq_1dd421_tool_calls_count @ persistence/sqlite/conversation/_store/messages_sql.py:_tool_calls_count
 * PoP: w24_messagessq_1dd421_tool_calls_len @ persistence/sqlite/conversation/_store/messages_sql.py:_tool_calls_len
 * PoP: w24_messagessq_1dd421_constants @ persistence/sqlite/conversation/_store/messages_sql.py:<statement literals>
 *
 * What is ported is everything in this module that is a function of values:
 * the transcript statements themselves, the write-column table Python derives
 * from the INSERT by regex (and the schema keys it unions), the epoch
 * coercion every write-side timestamp passes through, and the two tool_calls
 * normalisers.  The connection, the cursor and the INSERT stay seams: nothing
 * here opens a database.
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_messagessq_1dd421.h"

/* The sane epoch window of the readers' coerce_epoch. */
#define EPOCH_MIN 0.0
#define EPOCH_MAX 4200000000.0

/* ------------------------------------------------------------------ literals */

static const char *INSERT_SQL =
    "INSERT INTO messages (session_id, role, content, tool_call_id,\n"
    "                   tool_calls, tool_name, effect_disposition, timestamp, token_count, finish_reason,\n"
    "                   reasoning, reasoning_content, reasoning_details, codex_reasoning_items,\n"
    "                   codex_message_items, platform_message_id, observed, _compressed_summary, active, api_content, display_kind,\n"
    "                   display_metadata, display_identity, message_uid, absorbed_message_uids, tool_call_uids,\n"
    "                   tool_call_uid)\n"
    "                   VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

static const char *BUMP_GENERATION_SQL =
    "\n"
    "            INSERT INTO conversation_generations (source, session_key, generation)\n"
    "            VALUES (?, ?, 1)\n"
    "            ON CONFLICT(source, session_key) DO UPDATE\n"
    "                SET generation = conversation_generations.generation + 1\n"
    "            ";

static const char *TURN_LEASE_ROW_SQL =
    "SELECT holder, expires_at FROM session_turn_leases WHERE conversation_id = ?";
static const char *DELETE_COMPRESSION_LOCK_SQL =
    "DELETE FROM compression_locks WHERE session_id = ? AND holder = ?";
static const char *DISPLAY_ACTIVE_CLAUSE =
    " AND (active = 1 OR compacted = 1)";
static const char *DISPLAY_META_ROW_SQL =
    "SELECT display_metadata FROM messages WHERE id = ? AND session_id IN ({ids})"
    " AND (active = 1 OR compacted = 1)";
static const char *DISPLAY_INDEX_MISSING_SQL =
    "SELECT 1 FROM messages WHERE session_id = ?"
    " AND (active = 1 OR compacted = 1)"
    " AND (display_order IS NULL OR display_identity IS NULL) LIMIT 1";
static const char *ACTIVE_IDS_SQL =
    "SELECT id FROM messages WHERE session_id = ? AND active = 1 ORDER BY id";
static const char *LIVE_IDENTITY_SQL =
    "SELECT id, role, content, tool_call_id, tool_calls, message_uid FROM messages "
    "WHERE session_id = ? AND active = 1 ORDER BY id LIMIT ?";
static const char *SET_COUNTERS_SQL =
    "UPDATE sessions SET message_count = ?, tool_call_count = ?";
static const char *RESET_COUNTERS_SQL =
    "UPDATE sessions SET message_count = 0, tool_call_count = 0 WHERE id = ?";
static const char *SET_DISPLAY_META_SQL =
    "UPDATE messages SET display_metadata = ? WHERE id = ?";
static const char *ARCHIVE_ACTIVE_SQL =
    "UPDATE messages SET active = 0, compacted = 1 WHERE session_id = ? AND active = 1";
static const char *SHADOWED_CHECKPOINT_ROWS_SQL =
    "SELECT id, codex_reasoning_items FROM messages WHERE session_id = ? AND active = 1 "
    "AND role = 'assistant' AND id < ? AND codex_reasoning_items LIKE '%\"compaction\"%'";
static const char *SET_CODEX_REASONING_SQL =
    "UPDATE messages SET codex_reasoning_items = ? WHERE id = ?";

/* -------------------------------------------------------------- predicates */

static int py_truthy(const json_t *v) {
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

static int is_word(int c) { return isalnum((unsigned char)c) || c == '_'; }

/* float(text) as Python spells it: surrounding space, sign, inf/nan, digit groups. */
static int py_float_text(const char *s, double *out) {
    const char *p = s, *end;
    char *clean, *q, *stop;
    size_t n = strlen(s);
    int ok = 1;

    for (; *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v'; p++) {}
    end = p + strlen(p);
    while (end > p && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r'
                       || end[-1] == '\f' || end[-1] == '\v')) end--;
    if (end == p) return 0;
    clean = (char *)malloc(n + 1);
    if (!clean) return 0;
    for (q = clean; p < end; p++) {
        if (*p == '_') {
            /* Python allows a single underscore between digits only. */
            if (!(q > clean && isdigit((unsigned char)q[-1]) && p + 1 < end
                  && isdigit((unsigned char)p[1]))) {
                ok = 0;
                break;
            }
            continue;
        }
        *q++ = *p;
    }
    *q = '\0';
    if (!ok) { free(clean); return 0; }
    *out = strtod(clean, &stop);
    ok = (stop == q && q != clean);
    free(clean);
    return ok;
}

/* ---------------------------------------------------------------- surface */

json_t *w24_messagessq_1dd421_write_columns(void) {
    json_t *arr = json_array();
    const char *open = strchr(INSERT_SQL, '(');
    const char *close = open ? strchr(open, ')') : NULL;
    const char *p;

    if (!open || !close) return arr;
    p = open + 1;
    while (p < close) {
        const char *start;
        while (p < close && !is_word((unsigned char)*p)) p++;
        start = p;
        while (p < close && is_word((unsigned char)*p)) p++;
        if (p > start) {
            size_t len = (size_t)(p - start);
            char *word = (char *)malloc(len + 1);
            if (word) {
                memcpy(word, start, len);
                word[len] = '\0';
                json_append(arr, json_string(word));
                free(word);
            }
        }
    }
    return arr;
}

json_t *w24_messagessq_1dd421_schema_keys(void) {
    json_t *cols = w24_messagessq_1dd421_write_columns();
    const char *extra[3] = {"id", "compacted", "display_order"};
    size_t n = json_len(cols) + 3, i, j;
    json_t *out = json_array();
    char **names = (char **)calloc(n ? n : 1, sizeof(char *));

    if (!names) { json_free(cols); return out; }
    for (i = 0; i < json_len(cols); i++) {
        const json_t *v = json_get(cols, i);
        names[i] = strdup(v && v->type == JSON_STRING && v->str_val ? v->str_val : "");
    }
    for (i = 0; i < 3; i++) names[json_len(cols) + i] = strdup(extra[i]);
    /* Python sorted(): codepoint order, ASCII strcmp is the same order here. */
    for (i = 0; i < n; i++) {
        for (j = i + 1; j < n; j++) {
            if (names[i] && names[j] && strcmp(names[i], names[j]) > 0) {
                char *t = names[i];
                names[i] = names[j];
                names[j] = t;
            }
        }
    }
    for (i = 0; i < n; i++) {
        if (names[i]) {
            json_append(out, json_string(names[i]));
            free(names[i]);
        }
    }
    free(names);
    json_free(cols);
    return out;
}

json_t *w24_messagessq_1dd421_coerce_ts(const json_t *req) {
    const json_t *value = json_obj_get(req, "value");
    double def = json_get_num(req, "default", 0.0);
    double ts = NAN;
    int usable = 0;

    /* if value is None or value == "": return default */
    if (value && value->type == JSON_NULL) return json_number(def);
    if (value && value->type == JSON_STRING) {
        const char *s = value->str_val ? value->str_val : "";
        if (s[0] == '\0') return json_number(def);
        usable = py_float_text(s, &ts);
    } else if (value && value->type == JSON_BOOL) {
        ts = value->bool_val ? 1.0 : 0.0;
        usable = 1;
    } else if (value && value->type == JSON_NUMBER) {
        ts = value->num_val;
        usable = 1;
    }
    if (!usable) return json_number(def);          /* nan never satisfies the window */
    if (!(EPOCH_MIN <= ts && ts <= EPOCH_MAX)) return json_number(def);
    /* SQLite reads -0.0 back as 0.0; keep that single stored form. */
    return json_number(ts == 0.0 ? 0.0 : ts);
}

json_t *w24_messagessq_1dd421_parse_tool_calls(const json_t *tool_calls) {
    const json_t *parsed;
    json_t *copy;

    if (!tool_calls || tool_calls->type != JSON_STRING) {
        return tool_calls ? json_copy((json_t *)tool_calls) : json_null();
    }
    parsed = json_parse(tool_calls->str_val ? tool_calls->str_val : "", NULL);
    if (!parsed) return json_array();
    copy = json_copy((json_t *)parsed);
    json_free((json_t *)parsed);
    return copy;
}

json_t *w24_messagessq_1dd421_tool_calls_count(const json_t *tool_calls) {
    if (!tool_calls || tool_calls->type == JSON_NULL) return json_number(0);
    if (tool_calls->type == JSON_ARRAY) return json_number((double)json_len(tool_calls));
    return json_number(1);
}

json_t *w24_messagessq_1dd421_tool_calls_len(const json_t *raw, double scalar) {
    json_t *parsed = w24_messagessq_1dd421_parse_tool_calls(raw);
    double out;

    if (parsed && parsed->type == JSON_ARRAY) out = (double)json_len(parsed);
    else out = py_truthy(parsed) ? scalar : 0.0;
    json_free(parsed);
    return json_number(out);
}

json_t *w24_messagessq_1dd421_constants(void) {
    json_t *o = json_object();
    json_set(o, "insert_message_sql", json_string(INSERT_SQL));
    json_set(o, "bump_generation_sql", json_string(BUMP_GENERATION_SQL));
    json_set(o, "turn_lease_row_sql", json_string(TURN_LEASE_ROW_SQL));
    json_set(o, "delete_compression_lock_sql", json_string(DELETE_COMPRESSION_LOCK_SQL));
    json_set(o, "display_active_clause", json_string(DISPLAY_ACTIVE_CLAUSE));
    json_set(o, "display_meta_row_sql", json_string(DISPLAY_META_ROW_SQL));
    json_set(o, "display_index_missing_sql", json_string(DISPLAY_INDEX_MISSING_SQL));
    json_set(o, "active_ids_sql", json_string(ACTIVE_IDS_SQL));
    json_set(o, "live_identity_sql", json_string(LIVE_IDENTITY_SQL));
    json_set(o, "set_counters_sql", json_string(SET_COUNTERS_SQL));
    json_set(o, "reset_counters_sql", json_string(RESET_COUNTERS_SQL));
    json_set(o, "set_display_meta_sql", json_string(SET_DISPLAY_META_SQL));
    json_set(o, "archive_active_sql", json_string(ARCHIVE_ACTIVE_SQL));
    json_set(o, "shadowed_checkpoint_rows_sql", json_string(SHADOWED_CHECKPOINT_ROWS_SQL));
    json_set(o, "set_codex_reasoning_sql", json_string(SET_CODEX_REASONING_SQL));
    return o;
}
