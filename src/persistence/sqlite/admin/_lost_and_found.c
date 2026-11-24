/*
 * _lost_and_found.c - C11 port of upstream persistence/sqlite/admin/_lost_and_found.py
 * (gates w30_lostandfou_6760c6_p1 / w30_lostandfou_6760c6_p2).
 *
 * PoP: w30_lostandfou_6760c6_p1_is_session_id           @ _lost_and_found.py:_is_session_id
 * PoP: w30_lostandfou_6760c6_p1_looks_like_source       @ _lost_and_found.py:_looks_like_source
 * PoP: w30_lostandfou_6760c6_p1_session_source_index    @ _lost_and_found.py:_session_source_index
 * PoP: w30_lostandfou_6760c6_p1_classify_row            @ _lost_and_found.py:classify_lost_and_found_row
 * PoP: w30_lostandfou_6760c6_p1_heuristic_started_at    @ _lost_and_found.py:_heuristic_started_at
 * PoP: w30_lostandfou_6760c6_p1_parse_sql_default       @ _lost_and_found.py:_parse_sql_default
 * PoP: w30_lostandfou_6760c6_p1_notnull_substitutes     @ _lost_and_found.py:_notnull_defaults
 * PoP: w30_lostandfou_6760c6_p1_declared_types          @ _lost_and_found.py:_declared_types
 * PoP: w30_lostandfou_6760c6_p1_cell_fits               @ _lost_and_found.py:_cell_fits
 * PoP: w30_lostandfou_6760c6_p1_type_conflicts          @ _lost_and_found.py:_type_conflicts
 * PoP: w30_lostandfou_6760c6_p1_sentinel_holds          @ _lost_and_found.py:_sentinel_holds
 * PoP: w30_lostandfou_6760c6_p1_text_shape_holds        @ _lost_and_found.py:_text_shape_holds
 * PoP: w30_lostandfou_6760c6_p1_predicate               @ _lost_and_found.py:_is_epoch/_is_token/_is_json_start/
 *                                                          _is_path/_is_url/_is_nonempty_str
 * PoP: w30_lostandfou_6760c6_p1_version_vulnerable      @ sqlite_support._is_sqlite_wal_reset_vulnerable
 * PoP: w30_lostandfou_6760c6_p2_quoted_columns          @ _recovery._quoted_columns
 * PoP: w30_lostandfou_6760c6_p2_insert_sql              @ _lost_and_found.py:_execute_insert
 * PoP: w30_lostandfou_6760c6_p2_select_sql              @ _lost_and_found.py:_copy_direct_tables
 * PoP: w30_lostandfou_6760c6_p2_copy_columns            @ _lost_and_found.py:_copy_direct_tables
 * PoP: w30_lostandfou_6760c6_p2_sessions_prefix_columns @ _lost_and_found.py:_sessions_prefix_columns
 * PoP: w30_lostandfou_6760c6_p2_insert_prefix_plan      @ _lost_and_found.py:_insert_prefix_row
 * PoP: w30_lostandfou_6760c6_p2_insert_named_plan       @ _lost_and_found.py:_insert_named_row
 * PoP: w30_lostandfou_6760c6_p2_row_invariants_hold    @ _lost_and_found.py:_row_invariants_hold
 * PoP: w30_lostandfou_6760c6_p2_layout_consensus        @ _lost_and_found.py:infer_physical_layouts
 * PoP: w30_lostandfou_6760c6_p2_records                 @ _lost_and_found.py:map_lost_and_found_rows.records
 * PoP: w30_lostandfou_6760c6_p2_stub_plan               @ _lost_and_found.py:stub_missing_parent_sessions
 * PoP: w30_lostandfou_6760c6_p2_fts_rebuild_sql         @ _lost_and_found.py:rebuild_fts_indexes
 * PoP: w30_lostandfou_6760c6_p2_stub_title              @ _recovery._placeholder_titles
 * PoP: w30_lostandfou_6760c6_p2_unrecognized_widths     @ _lost_and_found.py:map_lost_and_found_rows
 * PoP: w30_lostandfou_6760c6_p2_recover_details         @ _lost_and_found.py:run_cli_lost_and_found_recover
 * PoP: w30_lostandfou_6760c6_p2_wal_refusal             @ _lost_and_found.py:find_sqlite3_cli
 *
 * What is ported is the part of the salvage lane that is a pure function of
 * values: which canonical table a schema-less record belongs to, which cell
 * shapes can stand for a column, which statement the lane would hand sqlite,
 * and how the report / orphan plan is shaped.  The sqlite connection, the
 * PRAGMA reads, the .recover subprocess and the temp-file handling are seams:
 * their outputs are fed in as fixtures and their inputs are the returned
 * statements.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w30_lostandfou_6760c6_p1.h"
#include "noc_w30_lostandfou_6760c6_p2.h"

#define LAF_EPOCH_LOW 1000000000.0
#define LAF_EPOCH_HIGH 4000000000.0
#define LAF_LEGACY_MINIMAL_NFIELD 14
#define LAF_MODEL_USAGE_NFIELD 18

static const char *const LAF_STUB_LABEL = "best-effort recovered";

static const char *const LAF_WAL_GUIDANCE =
    "salvage against a Hermes database with the WAL-reset bug "
    "(https://sqlite.org/wal.html#walresetbug, fixed in 3.51.3+ / backports "
    "3.50.7 / 3.44.6; the vulnerable fresh-opener can unlink a live WAL/SHM "
    "pair and split the database into two generations, losing acknowledged "
    "writes \xe2\x80\x94 #100368). Install a fixed sqlite3 CLI (3.51.3+, e.g. `brew "
    "install sqlite` or the precompiled sqlite-tools from sqlite.org)";

/* ---- Python value helpers ---------------------------------------------- */

static int laf_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static const char *laf_text(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

/* Python truthiness. */
static int laf_truthy(const json_t *v) {
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

/* isinstance(value, int): bools count, floats do not.  An integral JSON
 * number is what json.loads of an integer literal yields. */
static int laf_is_int(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) return 1;
    if (v->type != JSON_NUMBER) return 0;
    if (!(v->num_val >= -9.0e15 && v->num_val <= 9.0e15)) return 0;
    return v->num_val == (double)(long long)v->num_val;
}

/* isinstance(value, (int, float)). */
static int laf_is_number(const json_t *v) {
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}

/* isinstance(value, float). */
static int laf_is_float(const json_t *v) {
    return v && v->type == JSON_NUMBER && !laf_is_int(v);
}

/* Numeric view of an int/float/bool cell. */
static double laf_as_double(const json_t *v) {
    if (!v) return 0.0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return v->type == JSON_NUMBER ? v->num_val : 0.0;
}

static char *laf_dup(const char *s);
static char *laf_pystr(const json_t *v);

/* Python str(x or "") over the shapes a PRAGMA cell can hold. */
static char *laf_pystr_or_empty(const json_t *v) {
    return laf_truthy(v) ? laf_pystr(v) : laf_dup("");
}

static char *laf_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* Python str() over the shapes a PRAGMA cell can hold. */
static char *laf_pystr(const json_t *v) {
    char buf[64];
    const char *s = NULL;
    if (!v || v->type == JSON_NULL) s = "None";
    else if (v->type == JSON_BOOL) s = v->bool_val ? "True" : "False";
    else if (v->type == JSON_STRING) s = v->str_val ? v->str_val : "";
    if (s) return laf_dup(s);
    if (v->type == JSON_NUMBER) {
        if (laf_is_int(v)) snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return laf_dup(buf);
    }
    return laf_dup("");
}

/* ---- growable string --------------------------------------------------- */

typedef struct {
    char *buf;
    size_t len, cap;
} laf_sb;

static void laf_sb_init(laf_sb *b) { b->buf = NULL; b->len = 0; b->cap = 0; }

static int laf_sb_reserve(laf_sb *b, size_t extra) {
    size_t need = b->len + extra + 1;
    size_t cap;
    char *p;
    if (need <= b->cap) return 1;
    cap = b->cap ? b->cap : 64;
    while (cap < need) cap *= 2;
    p = (char *)realloc(b->buf, cap);
    if (!p) return 0;
    b->buf = p;
    b->cap = cap;
    if (b->len == 0) b->buf[0] = '\0';
    return 1;
}

static void laf_sb_addn(laf_sb *b, const char *s, size_t n) {
    if (!s || n == 0) {
        laf_sb_reserve(b, 0);
        return;
    }
    if (!laf_sb_reserve(b, n)) return;
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void laf_sb_add(laf_sb *b, const char *s) {
    if (s) laf_sb_addn(b, s, strlen(s));
}

static char *laf_sb_take(laf_sb *b) {
    char *p;
    if (!b->buf && !laf_sb_reserve(b, 0)) return NULL;
    p = b->buf;
    b->buf = NULL;
    b->len = 0;
    b->cap = 0;
    return p;
}

/* ---- character classes -------------------------------------------------- */

static int laf_digits(const char *s, size_t n) {
    size_t i;
    if (!s) return 0;
    for (i = 0; i < n; i++)
        if (s[i] < '0' || s[i] > '9') return 0;
    return 1;
}

static int laf_lower_hex(const char *s, size_t n) {
    size_t i;
    if (!s) return 0;
    for (i = 0; i < n; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
    }
    return 1;
}

static int laf_alnum_ascii(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

/* ^[a-z][a-z0-9_-]{0,31}$ */
static int laf_source_shape(const char *s) {
    size_t n = strlen(s), i;
    if (n == 0 || n > 32) return 0;
    if (!(s[0] >= 'a' && s[0] <= 'z')) return 0;
    for (i = 1; i < n; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
    }
    return 1;
}

static const char *const LAF_KNOWN_SOURCES[] = {
    "cli", "telegram", "discord", "slack", "whatsapp", "signal", "matrix",
    "irc", "email", "x", "twitter", "api", "gateway", "web", "dashboard",
    "tool", "subagent", "cron", "recovered", "imported", "acp", NULL,
};

/* ---- session-id recognizers (session_rules.session_ids) ---------------- */

/* SESSION_ID_PATTERN: ^\d{8}_\d{6}_ (prefix only, not anchored at the end) */
static int laf_pat_timestamp(const char *s, size_t n) {
    return n >= 16 && laf_digits(s, 8) && s[8] == '_' && laf_digits(s + 9, 6) && s[15] == '_';
}

/* ^cron_[A-Za-z0-9][A-Za-z0-9._-]*_\d{8}_\d{6}$ */
static int laf_pat_cron(const char *s, size_t n) {
    const char *body;
    size_t bl, head, i;
    if (n < 5 + 1 + 16 || memcmp(s, "cron_", 5) != 0) return 0;
    body = s + 5;
    bl = n - 5;
    if (body[bl - 16] != '_') return 0;
    if (!laf_digits(body + bl - 15, 8)) return 0;
    if (body[bl - 7] != '_') return 0;
    if (!laf_digits(body + bl - 6, 6)) return 0;
    head = bl - 16;
    if (head == 0 || !laf_alnum_ascii(body[0])) return 0;
    for (i = 1; i < head; i++) {
        char c = body[i];
        if (!(laf_alnum_ascii(c) || c == '.' || c == '_' || c == '-')) return 0;
    }
    return 1;
}

/* ^bg_(?:\d{6}_)?[0-9a-f]{6}$ */
static int laf_pat_bg(const char *s, size_t n) {
    const char *body;
    size_t bl;
    if (n < 3 || memcmp(s, "bg_", 3) != 0) return 0;
    body = s + 3;
    bl = n - 3;
    if (bl == 6) return laf_lower_hex(body, 6);
    if (bl == 13) return laf_digits(body, 6) && body[6] == '_' && laf_lower_hex(body + 7, 6);
    return 0;
}

static int laf_pat_prefixed_hex(const char *s, size_t n, const char *prefix, size_t want) {
    size_t pl = strlen(prefix);
    if (n != pl + want) return 0;
    if (memcmp(s, prefix, pl) != 0) return 0;
    return laf_lower_hex(s + pl, want);
}

/* ^api_\d{9,}_[0-9a-f]{8}$ */
static int laf_pat_api(const char *s, size_t n) {
    const char *body;
    size_t bl;
    if (n < 4 || memcmp(s, "api_", 4) != 0) return 0;
    body = s + 4;
    bl = n - 4;
    if (bl < 18) return 0;
    if (body[bl - 9] != '_') return 0;
    if (!laf_digits(body, bl - 9)) return 0;
    return laf_lower_hex(body + bl - 8, 8);
}

/* ^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$ */
static int laf_pat_uuid4(const char *s, size_t n) {
    if (n != 36) return 0;
    if (!laf_lower_hex(s, 8) || s[8] != '-') return 0;
    if (!laf_lower_hex(s + 9, 4) || s[13] != '-') return 0;
    if (s[14] != '4') return 0;
    if (!laf_lower_hex(s + 15, 3) || s[18] != '-') return 0;
    if (!(s[19] == '8' || s[19] == '9' || s[19] == 'a' || s[19] == 'b')) return 0;
    if (!laf_lower_hex(s + 20, 3) || s[23] != '-') return 0;
    return laf_lower_hex(s + 24, 12);
}

static int laf_session_id(const char *s) {
    size_t n;
    if (!s) return 0;
    n = strlen(s);
    if (laf_pat_timestamp(s, n)) return 1;
    if (laf_pat_cron(s, n)) return 1;
    if (laf_pat_bg(s, n)) return 1;
    if (laf_pat_prefixed_hex(s, n, "room_", 32)) return 1;
    if (laf_pat_api(s, n)) return 1;
    if (laf_pat_prefixed_hex(s, n, "api-", 16)) return 1;
    if (laf_pat_prefixed_hex(s, n, "run_", 32)) return 1;
    return laf_pat_uuid4(s, n);
}

/* ---- predicates --------------------------------------------------------- */

static int laf_role_text(const char *s) {
    return strcmp(s, "user") == 0 || strcmp(s, "assistant") == 0 ||
           strcmp(s, "tool") == 0 || strcmp(s, "system") == 0;
}

static int laf_is_session_id(const json_t *v) {
    const char *s = laf_text(v);
    return s ? laf_session_id(s) : 0;
}

static int laf_looks_like_source(const json_t *v) {
    const char *s = laf_text(v);
    size_t i;
    if (!s || s[0] == '\0') return 0;
    for (i = 0; LAF_KNOWN_SOURCES[i]; i++)
        if (strcmp(s, LAF_KNOWN_SOURCES[i]) == 0) return 1;
    return laf_source_shape(s);
}

static int laf_is_epoch(const json_t *v) {
    double d;
    if (!laf_is_number(v)) return 0;
    d = laf_as_double(v);
    return d >= LAF_EPOCH_LOW && d <= LAF_EPOCH_HIGH;
}

static int laf_is_nonempty_str(const json_t *v) {
    const char *s = laf_text(v);
    return s && s[0] != '\0';
}

static int laf_is_token_text(const char *s) {
    size_t n = strlen(s), i;
    if (n == 0) return 0;
    if (!(s[0] >= 'a' && s[0] <= 'z')) return 0;
    for (i = 1; i < n; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
              c == ':' || c == '-'))
            return 0;
    }
    return 1;
}

static int laf_is_token(const json_t *v) {
    const char *s = laf_text(v);
    return s ? laf_is_token_text(s) : 0;
}

/* value[:1] in "{[" -- Python's substring test makes "" a member. */
static int laf_is_json_start(const json_t *v) {
    const char *s = laf_text(v);
    if (!s) return 0;
    if (s[0] == '\0') return 1;
    return s[0] == '{' || s[0] == '[';
}

/* value[:1] in "/~" or (len > 1 and value[1] == ':') */
static int laf_is_path(const json_t *v) {
    const char *s = laf_text(v);
    if (!s) return 0;
    if (s[0] == '\0') return 1;
    if (s[0] == '/' || s[0] == '~') return 1;
    return s[1] != '\0' && s[1] == ':';
}

static int laf_has_url_scheme(const char *s) { return strstr(s, "://") != NULL; }

static int laf_is_url(const json_t *v) {
    const char *s = laf_text(v);
    return s ? laf_has_url_scheme(s) : 0;
}

static int laf_is_role(const json_t *v) {
    const char *s = laf_text(v);
    return s ? laf_role_text(s) : 0;
}

/* ---- the per-column shape rules ---------------------------------------- */

typedef enum {
    R_SESSION_ID,
    R_SOURCE,
    R_EPOCH,
    R_NONEMPTY,
    R_ROLE,
    R_MODEL,
    R_TOKEN,
    R_JSON,
    R_PATH,
    R_URL,
    R_SESSION_KEY,
    R_PRICING,
    R_TITLE_SOURCE,
    R_HANDOFF,
    R_SHA,
    R_BLANK_JSON,
    R_BLANK_TOKEN,
    R_BLANK_URL,
} laf_rule;

static int laf_hex64(const char *s) {
    size_t i;
    if (strlen(s) != 64) return 0;
    for (i = 0; i < 64; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
    }
    return 1;
}

/* ^agent:[^:]+: */
static int laf_session_key(const char *s) {
    const char *p;
    if (strncmp(s, "agent:", 6) != 0) return 0;
    p = s + 6;
    while (*p && *p != ':') p++;
    return *p == ':' && p > s + 6;
}

/* [a-z0-9][a-z0-9._-]* */
static int laf_pricing_version(const char *s) {
    size_t i;
    if (!s[0]) return 0;
    if (!((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= '0' && s[0] <= '9'))) return 0;
    for (i = 1; s[i]; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

static int laf_rule_text(laf_rule rule, const char *s) {
    size_t i;
    switch (rule) {
    case R_SESSION_ID: return laf_session_id(s);
    case R_SOURCE:
        if (!s[0]) return 0;
        for (i = 0; LAF_KNOWN_SOURCES[i]; i++)
            if (strcmp(s, LAF_KNOWN_SOURCES[i]) == 0) return 1;
        return laf_source_shape(s);
    case R_NONEMPTY:
    case R_MODEL:      return s[0] != '\0';
    case R_ROLE:       return laf_role_text(s);
    case R_TOKEN:      return laf_is_token_text(s);
    case R_JSON:       return s[0] == '\0' || s[0] == '{' || s[0] == '[';
    case R_PATH:       return s[0] == '\0' || s[0] == '/' || s[0] == '~' ||
                              (s[1] != '\0' && s[1] == ':');
    case R_URL:        return laf_has_url_scheme(s);
    case R_SESSION_KEY: return laf_session_key(s);
    case R_PRICING:    return laf_pricing_version(s);
    case R_TITLE_SOURCE:
        return strcmp(s, "derived") == 0 || strcmp(s, "llm") == 0 || strcmp(s, "user") == 0;
    case R_HANDOFF:
        return strcmp(s, "pending") == 0 || strcmp(s, "running") == 0 ||
               strcmp(s, "completed") == 0 || strcmp(s, "failed") == 0;
    case R_SHA:        return laf_hex64(s);
    case R_BLANK_JSON: return s[0] == '\0' || s[0] == '{' || s[0] == '[';
    case R_BLANK_TOKEN: return s[0] == '\0' || laf_is_token_text(s);
    case R_BLANK_URL:  return s[0] == '\0' || laf_has_url_scheme(s);
    default: return 1;
    }
}

static int laf_rule_value(laf_rule rule, const json_t *v) {
    const char *s;
    if (rule == R_EPOCH) return laf_is_epoch(v);
    s = laf_text(v);
    if (!s) return 0;
    return laf_rule_text(rule, s);
}

struct laf_rule_row {
    const char *kind;
    const char *name;
    laf_rule rule;
};

static const struct laf_rule_row LAF_LAYOUT_SENTINELS[] = {
    {"sessions", "id", R_SESSION_ID},
    {"sessions", "source", R_SOURCE},
    {"sessions", "started_at", R_EPOCH},
    {"messages", "session_id", R_NONEMPTY},
    {"messages", "role", R_ROLE},
    {"messages", "timestamp", R_EPOCH},
    {"session_model_usage", "session_id", R_NONEMPTY},
    {"session_model_usage", "model", R_MODEL},
    {NULL, NULL, 0},
};

/* _SENTINEL_RULES is keyed by column name only. */
static const struct laf_rule_row LAF_SENTINEL_RULES[] = {
    {"", "id", R_SESSION_ID},
    {"", "source", R_SOURCE},
    {"", "started_at", R_EPOCH},
    {"", "timestamp", R_EPOCH},
    {"", "session_id", R_NONEMPTY},
    {"", "role", R_ROLE},
    {"", "model", R_MODEL},
    {NULL, NULL, 0},
};

static const struct laf_rule_row LAF_TEXT_SHAPES[] = {
    {"sessions", "session_key", R_SESSION_KEY},
    {"sessions", "chat_type", R_TOKEN},
    {"sessions", "end_reason", R_TOKEN},
    {"sessions", "cost_status", R_TOKEN},
    {"sessions", "cost_source", R_TOKEN},
    {"sessions", "billing_mode", R_TOKEN},
    {"sessions", "last_activity_provenance", R_TOKEN},
    {"sessions", "handoff_platform", R_TOKEN},
    {"sessions", "pricing_version", R_PRICING},
    {"sessions", "title_source", R_TITLE_SOURCE},
    {"sessions", "handoff_state", R_HANDOFF},
    {"sessions", "parent_session_id", R_SESSION_ID},
    {"sessions", "system_prompt_hash", R_SHA},
    {"sessions", "model_config", R_JSON},
    {"sessions", "origin_json", R_JSON},
    {"sessions", "cwd", R_PATH},
    {"sessions", "git_repo_root", R_PATH},
    {"sessions", "billing_base_url", R_URL},
    {"messages", "effect_disposition", R_TOKEN},
    {"messages", "finish_reason", R_TOKEN},
    {"messages", "display_kind", R_TOKEN},
    {"messages", "tool_calls", R_BLANK_JSON},
    {"messages", "reasoning_details", R_BLANK_JSON},
    {"messages", "codex_reasoning_items", R_BLANK_JSON},
    {"messages", "codex_message_items", R_BLANK_JSON},
    {"messages", "api_content", R_BLANK_JSON},
    {"messages", "display_metadata", R_BLANK_JSON},
    {"session_model_usage", "billing_base_url", R_BLANK_URL},
    {"session_model_usage", "billing_mode", R_BLANK_TOKEN},
    {"session_model_usage", "cost_status", R_BLANK_TOKEN},
    {"session_model_usage", "cost_source", R_BLANK_TOKEN},
    {NULL, NULL, 0},
};

static int laf_is_sentinel_column(const char *kind, const char *name) {
    size_t i;
    for (i = 0; LAF_LAYOUT_SENTINELS[i].kind; i++)
        if (strcmp(LAF_LAYOUT_SENTINELS[i].kind, kind) == 0 &&
            strcmp(LAF_LAYOUT_SENTINELS[i].name, name) == 0)
            return 1;
    return 0;
}

static int laf_sentinel_holds_name(const char *name, const json_t *value) {
    size_t i;
    for (i = 0; LAF_SENTINEL_RULES[i].name; i++)
        if (strcmp(LAF_SENTINEL_RULES[i].name, name) == 0)
            return laf_rule_value(LAF_SENTINEL_RULES[i].rule, value);
    return 1;
}

static int laf_text_shape_holds_kind(const char *kind, const char *name, const json_t *value) {
    size_t i;
    const char *s;
    for (i = 0; LAF_TEXT_SHAPES[i].kind; i++)
        if (strcmp(LAF_TEXT_SHAPES[i].kind, kind) == 0 &&
            strcmp(LAF_TEXT_SHAPES[i].name, name) == 0) {
            s = laf_text(value);
            return s ? laf_rule_text(LAF_TEXT_SHAPES[i].rule, s) : 0;
        }
    return 1;
}

/* ---- declared types ----------------------------------------------------- */

static int laf_upper_contains(const char *hay, const char *needle) {
    size_t nl = strlen(needle), i, j;
    size_t hl = strlen(hay);
    if (nl > hl) return 0;
    for (i = 0; i + nl <= hl; i++) {
        for (j = 0; j < nl; j++) {
            char a = hay[i + j], b = needle[j];
            if (a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
            if (a != b) break;
        }
        if (j == nl) return 1;
    }
    return 0;
}

/* _type_conflicts: stricter than affinity, on purpose. */
static int laf_type_conflicts(const json_t *value, const char *declared) {
    if (laf_null(value)) return 0;
    if (laf_upper_contains(declared, "CHAR") || laf_upper_contains(declared, "CLOB") ||
        laf_upper_contains(declared, "TEXT"))
        return value->type != JSON_STRING;
    if (laf_upper_contains(declared, "REAL") || laf_upper_contains(declared, "FLOA") ||
        laf_upper_contains(declared, "DOUB"))
        return !laf_is_float(value);
    if (laf_upper_contains(declared, "INT")) return !laf_is_int(value);
    return 0;
}

static int laf_cell_fits(const char *kind, const char *name, const json_t *value,
                         const json_t *dest_types) {
    const json_t *declared;
    const char *declared_type;
    if (laf_null(value)) return 1;
    if (laf_is_sentinel_column(kind, name) && !laf_sentinel_holds_name(name, value)) return 0;
    declared = dest_types && dest_types->type == JSON_OBJECT ? json_obj_get(dest_types, name) : NULL;
    if (laf_null(declared)) return 1;  /* removed column: any value admissible */
    declared_type = laf_text(declared);
    if (laf_type_conflicts(value, declared_type ? declared_type : "")) return 0;
    if (value->type != JSON_STRING) return 1;
    return laf_text_shape_holds_kind(kind, name, value);
}

/* ---- classification ----------------------------------------------------- */

static int laf_source_index(const json_t *cells) {
    size_t n = cells && cells->type == JSON_ARRAY ? json_len(cells) : 0, i;
    int ok;
    if (n > 1 && laf_looks_like_source(json_get(cells, 1))) return 1;
    if (n > 3 && laf_looks_like_source(json_get(cells, 3))) {
        ok = 1;
        for (i = 1; i < 3; i++) {
            const json_t *v = json_get(cells, i);
            if (!(laf_null(v) || laf_is_int(v))) ok = 0;
        }
        if (ok) return 3;
    }
    return -1;
}

static int laf_is_message_cell(const json_t *cells, size_t n) {
    if (n < 3 || !laf_null(json_get(cells, 0))) return 0;
    return laf_is_nonempty_str(json_get(cells, 1)) && laf_is_role(json_get(cells, 2));
}

static const char *laf_classify(int nfield, const json_t *cells) {
    size_t n = cells && cells->type == JSON_ARRAY ? json_len(cells) : 0;
    const json_t *second;
    if (n >= 3 && laf_null(json_get(cells, 0)))
        return laf_is_message_cell(cells, n) ? "messages" : NULL;
    if (!(n > 0 && laf_is_session_id(json_get(cells, 0)))) return NULL;
    second = n > 1 ? json_get(cells, 1) : NULL;
    if (nfield == LAF_MODEL_USAGE_NFIELD)
        return laf_is_nonempty_str(second) ? "session_model_usage" : NULL;
    if (nfield >= LAF_LEGACY_MINIMAL_NFIELD && laf_source_index(cells) >= 0) return "sessions";
    return NULL;
}

static double laf_heuristic_started_at(const json_t *cells) {
    size_t n = cells && cells->type == JSON_ARRAY ? json_len(cells) : 0, i;
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(cells, i);
        double d;
        if (!laf_is_number(v)) continue;
        d = laf_as_double(v);
        if (d >= LAF_EPOCH_LOW && d <= LAF_EPOCH_HIGH) return d;
    }
    return 0.0;
}

/* ---- sql default coercion ---------------------------------------------- */

static int laf_isspace_ascii(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python int(text) over a str: optional whitespace/sign, digits with single
 * underscores between digits. */
static int laf_int_text(const char *s, long long *out) {
    const char *p = s;
    int neg = 0, any = 0, overflow = 0;
    long long acc = 0;
    while (laf_isspace_ascii(*p)) p++;
    if (*p == '+' || *p == '-') {
        neg = (*p == '-');
        p++;
    }
    if (!*p) return 0;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            if (acc > 922337203685477580LL) overflow = 1;
            else acc = acc * 10 + (*p - '0');
            any = 1;
            p++;
        } else if (*p == '_') {
            if (!any) return 0;
            if (!(p[1] >= '0' && p[1] <= '9')) return 0;
            p++;
        } else {
            break;
        }
    }
    while (laf_isspace_ascii(*p)) p++;
    if (*p || !any || overflow) return 0;
    *out = neg ? -acc : acc;
    return 1;
}

static int laf_case_eq(const char *a, const char *b) {
    while (*a && *b) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return 0;
        a++;
        b++;
    }
    while (laf_isspace_ascii(*a)) a++;
    while (laf_isspace_ascii(*b)) b++;
    return *a == '\0' && *b == '\0';
}

/* Python float(text) over a str. */
static int laf_float_text(const char *s, double *out) {
    const char *p = s;
    int neg = 0, mantissa_digits = 0, seen_dot = 0, seen_exp = 0, prev_digit = 0;
    char tmp[512];
    size_t ti = 0;
    while (laf_isspace_ascii(*p)) p++;
    if (*p == '+' || *p == '-') {
        neg = (*p == '-');
        p++;
    }
    if (!*p) return 0;
    if (laf_case_eq(p, "inf") || laf_case_eq(p, "infinity")) {
        *out = neg ? -INFINITY : INFINITY;
        return 1;
    }
    if (laf_case_eq(p, "nan")) {
        *out = NAN;
        return 1;
    }
    if (neg && ti + 1 < sizeof tmp) tmp[ti++] = '-';
    while (*p) {
        char c = *p;
        if (c >= '0' && c <= '9') {
            mantissa_digits++;
            prev_digit = 1;
            if (ti + 1 < sizeof tmp) tmp[ti++] = c;
            p++;
        } else if (c == '_') {
            if (!prev_digit || !(p[1] >= '0' && p[1] <= '9')) return 0;
            p++;
        } else if (c == '.' && !seen_dot && !seen_exp) {
            seen_dot = 1;
            prev_digit = 0;
            if (ti + 1 < sizeof tmp) tmp[ti++] = c;
            p++;
        } else if ((c == 'e' || c == 'E') && !seen_exp) {
            seen_exp = 1;
            prev_digit = 0;
            if (ti + 1 < sizeof tmp) tmp[ti++] = c;
            p++;
            if (*p == '+' || *p == '-') {
                if (ti + 1 < sizeof tmp) tmp[ti++] = *p;
                p++;
            }
            if (!(*p >= '0' && *p <= '9')) return 0;
        } else {
            break;
        }
    }
    while (laf_isspace_ascii(*p)) p++;
    if (*p || mantissa_digits == 0) return 0;
    tmp[ti] = '\0';
    *out = strtod(tmp, NULL);
    return 1;
}

static json_t *laf_encode_default(const char *text) {
    json_t *o = json_object();
    long long ival;
    double dval;
    size_t n = strlen(text);
    if (text[0] == '\'' && n >= 1 && text[n - 1] == '\'') {
        size_t len = n >= 2 ? n - 2 : 0;
        char *inner = (char *)malloc(len + 1);
        if (inner) {
            if (len) memcpy(inner, text + 1, len);
            inner[len] = '\0';
            json_set(o, "type", json_string("str"));
            json_set(o, "value", json_string(inner));
            free(inner);
            return o;
        }
    }
    if (laf_int_text(text, &ival)) {
        json_set(o, "type", json_string("int"));
        json_set(o, "value", json_number((double)ival));
        return o;
    }
    if (laf_float_text(text, &dval)) {
        json_set(o, "type", json_string("float"));
        json_set(o, "value", json_number(dval));
        return o;
    }
    json_set(o, "type", json_string("str"));
    json_set(o, "value", json_string(text));
    return o;
}

/* raw value of a default literal (no type tag), for substitutes */
static json_t *laf_default_value(const char *text) {
    json_t *typed = laf_encode_default(text);
    const char *type = json_get_str(typed, "type", "str");
    json_t *out;
    if (strcmp(type, "int") == 0 || strcmp(type, "float") == 0)
        out = json_number(json_get_num(typed, "value", 0.0));
    else
        out = json_string(json_get_str(typed, "value", ""));
    json_free(typed);
    return out;
}

/* ---- p1 entry points ---------------------------------------------------- */

json_t *w30_lostandfou_6760c6_p1_is_session_id(const json_t *req) {
    return json_bool(laf_is_session_id(json_obj_get(req, "value")));
}

json_t *w30_lostandfou_6760c6_p1_looks_like_source(const json_t *req) {
    return json_bool(laf_looks_like_source(json_obj_get(req, "value")));
}

json_t *w30_lostandfou_6760c6_p1_session_source_index(const json_t *req) {
    int idx = laf_source_index(json_obj_get(req, "cells"));
    return idx < 0 ? json_null() : json_number((double)idx);
}

json_t *w30_lostandfou_6760c6_p1_classify_row(const json_t *req) {
    const json_t *nf = json_obj_get(req, "nfield");
    const char *kind;
    long long nfield = 0;
    if (nf && nf->type == JSON_NUMBER) nfield = (long long)nf->num_val;
    else if (nf && nf->type == JSON_BOOL) nfield = nf->bool_val ? 1 : 0;
    else if (nf && nf->type == JSON_STRING) laf_int_text(nf->str_val ? nf->str_val : "", &nfield);
    kind = laf_classify((int)nfield, json_obj_get(req, "cells"));
    return kind ? json_string(kind) : json_null();
}

json_t *w30_lostandfou_6760c6_p1_heuristic_started_at(const json_t *req) {
    return json_number(laf_heuristic_started_at(json_obj_get(req, "cells")));
}

json_t *w30_lostandfou_6760c6_p1_parse_sql_default(const json_t *req) {
    const char *text = laf_text(json_obj_get(req, "text"));
    return laf_encode_default(text ? text : "");
}

json_t *w30_lostandfou_6760c6_p1_notnull_substitutes(const json_t *req) {
    const json_t *rows = json_obj_get(req, "rows");
    json_t *out = json_object();
    size_t n = rows && rows->type == JSON_ARRAY ? json_len(rows) : 0, i;
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(rows, i);
        const json_t *flag = json_get(row, 3);
        const json_t *dflt = json_get(row, 4);
        char key[32];
        json_t *sub;
        if (!laf_truthy(flag)) continue;
        snprintf(key, sizeof key, "%zu", i);
        if (!laf_null(dflt)) {
            char *txt = laf_pystr(dflt);
            sub = laf_default_value(txt ? txt : "");
            free(txt);
        } else {
            char *decl = laf_pystr_or_empty(json_get(row, 2));
            size_t k;
            if (!decl) decl = laf_dup("");
            for (k = 0; decl[k]; k++)
                if (decl[k] >= 'a' && decl[k] <= 'z') decl[k] = (char)(decl[k] - 'a' + 'A');
            if (strstr(decl, "INT") || strstr(decl, "REAL")) sub = json_number(0.0);
            else sub = json_string("");
            free(decl);
        }
        json_set(out, key, sub);
    }
    return out;
}

json_t *w30_lostandfou_6760c6_p1_declared_types(const json_t *req) {
    const json_t *rows = json_obj_get(req, "rows");
    json_t *out = json_object();
    size_t n = rows && rows->type == JSON_ARRAY ? json_len(rows) : 0, i;
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(rows, i);
        char *name = laf_pystr(json_get(row, 1));
        char *type = laf_pystr_or_empty(json_get(row, 2));
        json_set(out, name ? name : "", json_string(type && type[0] ? type : ""));
        free(name);
        free(type);
    }
    return out;
}

json_t *w30_lostandfou_6760c6_p1_cell_fits(const json_t *req) {
    const char *kind = json_get_str(req, "kind", "");
    const char *name = json_get_str(req, "name", "");
    return json_bool(laf_cell_fits(kind, name, json_obj_get(req, "value"),
                                   json_obj_get(req, "dest_types")));
}

json_t *w30_lostandfou_6760c6_p1_type_conflicts(const json_t *req) {
    const char *declared = json_get_str(req, "declared", "");
    return json_bool(laf_type_conflicts(json_obj_get(req, "value"), declared));
}

json_t *w30_lostandfou_6760c6_p1_sentinel_holds(const json_t *req) {
    const char *name = json_get_str(req, "name", "");
    return json_bool(laf_sentinel_holds_name(name, json_obj_get(req, "value")));
}

json_t *w30_lostandfou_6760c6_p1_text_shape_holds(const json_t *req) {
    const char *kind = json_get_str(req, "kind", "");
    const char *name = json_get_str(req, "name", "");
    return json_bool(laf_text_shape_holds_kind(kind, name, json_obj_get(req, "value")));
}

json_t *w30_lostandfou_6760c6_p1_predicate(const json_t *req) {
    const char *name = json_get_str(req, "name", "");
    const json_t *value = json_obj_get(req, "value");
    if (strcmp(name, "is_epoch") == 0) return json_bool(laf_is_epoch(value));
    if (strcmp(name, "is_nonempty_str") == 0) return json_bool(laf_is_nonempty_str(value));
    if (strcmp(name, "is_token") == 0) return json_bool(laf_is_token(value));
    if (strcmp(name, "is_json_start") == 0) return json_bool(laf_is_json_start(value));
    if (strcmp(name, "is_path") == 0) return json_bool(laf_is_path(value));
    if (strcmp(name, "is_url") == 0) return json_bool(laf_is_url(value));
    return json_string("UNKNOWN_PREDICATE");
}

/* sqlite_support._is_sqlite_wal_reset_vulnerable */
static int laf_wal_vulnerable(long long major, long long minor, long long patch) {
    if (major != 3) return 0;                    /* pre-WAL and post-3.x are safe */
    if (minor > 51) return 0;
    if (minor == 51 && patch >= 3) return 0;
    if (minor == 50 && patch >= 7) return 0;
    if (minor == 44 && patch >= 6) return 0;
    if (minor < 7) return 0;
    return 1;
}

static long long laf_version_part(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) {
        long long out = 0;
        if (laf_int_text(v->str_val ? v->str_val : "", &out)) return out;
    }
    return 0;
}

json_t *w30_lostandfou_6760c6_p1_version_vulnerable(const json_t *req) {
    const json_t *ver = json_obj_get(req, "version");
    long long p[3] = {0, 0, 0};
    size_t n = ver && ver->type == JSON_ARRAY ? json_len(ver) : 0, i;
    for (i = 0; i < 3 && i < n; i++) p[i] = laf_version_part(json_get(ver, i));
    return json_bool(laf_wal_vulnerable(p[0], p[1], p[2]));
}

/* ---- p2: statement builders -------------------------------------------- */

static void laf_quote_columns(const json_t *columns, laf_sb *quoted, laf_sb *placeholders) {
    size_t n = columns && columns->type == JSON_ARRAY ? json_len(columns) : 0, i;
    laf_sb_init(quoted);
    laf_sb_init(placeholders);
    for (i = 0; i < n; i++) {
        const char *name = laf_text(json_get(columns, i));
        if (i) {
            laf_sb_add(quoted, ", ");
            laf_sb_add(placeholders, ", ");
        }
        laf_sb_add(quoted, "\"");
        laf_sb_add(quoted, name ? name : "");
        laf_sb_add(quoted, "\"");
        laf_sb_add(placeholders, "?");
    }
}

static json_t *laf_sql_for_columns(const char *table, const json_t *columns) {
    laf_sb quoted, placeholders, stmt;
    char *q, *p, *s;
    json_t *out;
    laf_quote_columns(columns, &quoted, &placeholders);
    q = laf_sb_take(&quoted);
    p = laf_sb_take(&placeholders);
    laf_sb_init(&stmt);
    laf_sb_add(&stmt, "INSERT OR IGNORE INTO \"");
    laf_sb_add(&stmt, table);
    laf_sb_add(&stmt, "\" (");
    laf_sb_add(&stmt, q);
    laf_sb_add(&stmt, ") VALUES (");
    laf_sb_add(&stmt, p);
    laf_sb_add(&stmt, ")");
    s = laf_sb_take(&stmt);
    out = json_string(s ? s : "");
    free(s);
    free(q);
    free(p);
    return out;
}

/* ---- p2 entry points ---------------------------------------------------- */

json_t *w30_lostandfou_6760c6_p2_quoted_columns(const json_t *req) {
    laf_sb quoted, placeholders;
    json_t *out = json_object();
    char *q, *p;
    laf_quote_columns(json_obj_get(req, "columns"), &quoted, &placeholders);
    q = laf_sb_take(&quoted);
    p = laf_sb_take(&placeholders);
    json_set(out, "columns", json_string(q ? q : ""));
    json_set(out, "placeholders", json_string(p ? p : ""));
    free(q);
    free(p);
    return out;
}

json_t *w30_lostandfou_6760c6_p2_insert_sql(const json_t *req) {
    return laf_sql_for_columns(json_get_str(req, "table", ""), json_obj_get(req, "columns"));
}

json_t *w30_lostandfou_6760c6_p2_select_sql(const json_t *req) {
    laf_sb quoted, placeholders, stmt;
    char *q, *s;
    json_t *out;
    laf_quote_columns(json_obj_get(req, "columns"), &quoted, &placeholders);
    q = laf_sb_take(&quoted);
    free(laf_sb_take(&placeholders));
    laf_sb_init(&stmt);
    laf_sb_add(&stmt, "SELECT ");
    laf_sb_add(&stmt, q);
    laf_sb_add(&stmt, " FROM \"");
    laf_sb_add(&stmt, json_get_str(req, "table", ""));
    laf_sb_add(&stmt, "\"");
    s = laf_sb_take(&stmt);
    out = json_string(s ? s : "");
    free(s);
    free(q);
    return out;
}

json_t *w30_lostandfou_6760c6_p2_copy_columns(const json_t *req) {
    const json_t *source = json_obj_get(req, "source");
    const json_t *dest = json_obj_get(req, "dest");
    json_t *cols = json_array();
    json_t *out = json_object();
    size_t dn = dest && dest->type == JSON_ARRAY ? json_len(dest) : 0;
    size_t sn = source && source->type == JSON_ARRAY ? json_len(source) : 0;
    size_t i, j;
    for (i = 0; i < dn; i++) {
        const char *name = laf_text(json_get(dest, i));
        int found = 0;
        if (!name) continue;
        for (j = 0; j < sn; j++) {
            const char *sc = laf_text(json_get(source, j));
            if (sc && strcmp(sc, name) == 0) { found = 1; break; }
        }
        if (found) json_append(cols, json_string(name));
    }
    json_set(out, "columns", cols);
    json_set(out, "create_auxiliary",
             json_bool(dn == 0 && laf_truthy(json_obj_get(req, "auxiliary"))));
    return out;
}

static int laf_is_runtime_column(const char *name) {
    return name && (strcmp(name, "runtime_revision") == 0 ||
                    strcmp(name, "runtime_generation") == 0);
}

json_t *w30_lostandfou_6760c6_p2_sessions_prefix_columns(const json_t *req) {
    const json_t *columns = json_obj_get(req, "columns");
    const json_t *defaults = json_obj_get(req, "defaults");
    const json_t *cells = json_obj_get(req, "cells");
    json_t *ordered = json_array();
    json_t *named = json_object();
    json_t *out = json_object();
    size_t dn = defaults && defaults->type == JSON_OBJECT ? json_object_size(defaults) : 0;
    size_t cn = columns && columns->type == JSON_ARRAY ? json_len(columns) : 0;
    size_t i, k;
    int insert_at_head = laf_source_index(cells) == 3;

    for (i = 0; i < cn; i++) {
        const char *name = laf_text(json_get(columns, i));
        if (!laf_is_runtime_column(name)) json_append(ordered, json_string(name ? name : ""));
    }
    if (insert_at_head && json_len(ordered) > 0) {
        json_t *tmp = json_array();
        json_append(tmp, json_copy(json_get(ordered, 0)));
        json_append(tmp, json_string("runtime_revision"));
        json_append(tmp, json_string("runtime_generation"));
        for (i = 1; i < json_len(ordered); i++) json_append(tmp, json_copy(json_get(ordered, i)));
        json_free(ordered);
        ordered = tmp;
    } else if (!insert_at_head) {
        json_append(ordered, json_string("runtime_revision"));
        json_append(ordered, json_string("runtime_generation"));
    }

    for (k = 0; k < dn; k++) {
        const char *key = json_object_get_key_at(defaults, k);
        long idx = -1;
        if (key && key[0] && strspn(key, "0123456789") == strlen(key)) idx = strtol(key, NULL, 10);
        if (idx < 0 || (size_t)idx >= cn) continue;
        {
            const char *name = laf_text(json_get(columns, (size_t)idx));
            if (name) json_set(named, name, json_copy(json_object_get_at(defaults, k)));
        }
    }
    {
        json_t *remapped = json_object();
        for (i = 0; i < json_len(ordered); i++) {
            const char *name = laf_text(json_get(ordered, i));
            const json_t *val;
            char key[32];
            if (!name) continue;
            val = json_obj_get(named, name);
            if (!val) continue;
            snprintf(key, sizeof key, "%zu", i);
            json_set(remapped, key, json_copy(val));
        }
        json_free(named);
        named = remapped;
    }
    json_set(out, "columns", ordered);
    json_set(out, "defaults", named);
    return out;
}

json_t *w30_lostandfou_6760c6_p2_insert_prefix_plan(const json_t *req) {
    const json_t *columns = json_obj_get(req, "columns");
    const json_t *cells = json_obj_get(req, "cells");
    const json_t *subs = json_obj_get(req, "substitutes");
    json_t *params = json_array();
    json_t *cols = json_array();
    json_t *out = json_object();
    size_t cn = columns && columns->type == JSON_ARRAY ? json_len(columns) : 0;
    size_t vn = cells && cells->type == JSON_ARRAY ? json_len(cells) : 0;
    size_t i;
    for (i = 0; i < vn; i++) {
        const json_t *v = json_get(cells, i);
        const json_t *sub = NULL;
        char key[32];
        if (laf_null(v) && subs && subs->type == JSON_OBJECT) {
            snprintf(key, sizeof key, "%zu", i);
            sub = json_obj_get(subs, key);
        }
        json_append(params, sub ? json_copy(sub) : json_copy(v));
    }
    /* dest_columns[: len(values)] -- the caller normally pre-truncates values,
     * so a longer cell list leaves params wider than the placeholder list. */
    for (i = 0; i < vn && i < cn; i++) {
        const char *name = laf_text(json_get(columns, i));
        json_append(cols, json_string(name ? name : ""));
    }
    json_set(out, "sql", laf_sql_for_columns(json_get_str(req, "table", ""), cols));
    json_set(out, "params", params);
    json_free(cols);
    return out;
}

/* ordered name -> value map (Python dict semantics) */
typedef struct {
    char **names;
    json_t **vals;
    size_t n, cap;
} laf_map;

static int laf_map_find(const laf_map *m, const char *name) {
    size_t i;
    for (i = 0; i < m->n; i++)
        if (strcmp(m->names[i], name) == 0) return (int)i;
    return -1;
}

static int laf_map_grow(laf_map *m) {
    size_t cap;
    char **nn;
    json_t **nv;
    if (m->n < m->cap) return 1;
    cap = m->cap ? m->cap * 2 : 8;
    nn = (char **)realloc(m->names, cap * sizeof *nn);
    if (!nn) return 0;
    m->names = nn;
    nv = (json_t **)realloc(m->vals, cap * sizeof *nv);
    if (!nv) return 0;
    m->vals = nv;
    m->cap = cap;
    return 1;
}

static void laf_map_set(laf_map *m, const char *name, json_t *val) {
    int at;
    if (!name) {
        if (val) json_free(val);
        return;
    }
    at = laf_map_find(m, name);
    if (at >= 0) {
        json_free(m->vals[at]);
        m->vals[at] = val;
        return;
    }
    if (!laf_map_grow(m)) {
        if (val) json_free(val);
        return;
    }
    m->names[m->n] = laf_dup(name);
    m->vals[m->n] = val;
    m->n++;
}

static void laf_map_setdefault(laf_map *m, const char *name, json_t *val) {
    if (!name || laf_map_find(m, name) >= 0) {
        if (val) json_free(val);
        return;
    }
    laf_map_set(m, name, val);
}

json_t *w30_lostandfou_6760c6_p2_insert_named_plan(const json_t *req) {
    const json_t *layout = json_obj_get(req, "layout");
    const json_t *cells = json_obj_get(req, "cells");
    const json_t *dest = json_obj_get(req, "dest_columns");
    const json_t *subs = json_obj_get(req, "substitutes");
    const json_t *overrides = json_obj_get(req, "overrides");
    laf_map mapped;
    json_t *cols, *params, *sql, *out;
    size_t ln = layout && layout->type == JSON_ARRAY ? json_len(layout) : 0;
    size_t cn = cells && cells->type == JSON_ARRAY ? json_len(cells) : 0;
    size_t dn = dest && dest->type == JSON_ARRAY ? json_len(dest) : 0;
    size_t i, k;

    mapped.names = NULL;
    mapped.vals = NULL;
    mapped.n = mapped.cap = 0;

    for (i = 0; i < ln && i < cn; i++) {
        const char *name = laf_text(json_get(layout, i));
        const json_t *value = json_get(cells, i);
        long index = -1;
        char key[32];
        if (!name) continue;
        for (k = 0; k < dn; k++) {
            const char *dc = laf_text(json_get(dest, k));
            if (dc && strcmp(dc, name) == 0) { index = (long)k; break; }
        }
        if (index < 0) continue;
        if (laf_null(value) && subs && subs->type == JSON_OBJECT) {
            snprintf(key, sizeof key, "%ld", index);
            {
                const json_t *sub = json_obj_get(subs, key);
                if (sub) value = sub;
            }
        }
        laf_map_set(&mapped, name, json_copy(value));
    }
    if (subs && subs->type == JSON_OBJECT) {
        for (k = 0; k < json_object_size(subs); k++) {
            const char *key = json_object_get_key_at(subs, k);
            long idx = -1;
            const char *name;
            if (key && key[0] && strspn(key, "0123456789") == strlen(key))
                idx = strtol(key, NULL, 10);
            if (idx < 0 || (size_t)idx >= dn) continue;
            name = laf_text(json_get(dest, (size_t)idx));
            if (!name) continue;
            laf_map_setdefault(&mapped, name, json_copy(json_object_get_at(subs, k)));
        }
    }
    if (overrides && overrides->type == JSON_OBJECT) {
        for (k = 0; k < json_object_size(overrides); k++) {
            const char *key = json_object_get_key_at(overrides, k);
            if (!key) continue;
            laf_map_set(&mapped, key, json_copy(json_object_get_at(overrides, k)));
        }
    }

    cols = json_array();
    params = json_array();
    for (i = 0; i < mapped.n; i++) {
        json_append(cols, json_string(mapped.names[i] ? mapped.names[i] : ""));
        json_append(params, mapped.vals[i] ? mapped.vals[i] : json_null());
    }
    sql = laf_sql_for_columns(json_get_str(req, "table", ""), cols);
    out = json_object();
    json_set(out, "sql", sql);
    json_set(out, "params", params);
    json_free(cols);
    for (i = 0; i < mapped.n; i++) {
        free(mapped.names[i]);
        mapped.vals[i] = NULL;   /* ownership moved into params */
    }
    free(mapped.names);
    free(mapped.vals);
    return out;
}

/* _row_invariants_hold: handoff_error only ever accompanies handoff_state. */
json_t *w30_lostandfou_6760c6_p2_row_invariants_hold(const json_t *req) {
    const char *kind = json_get_str(req, "kind", "");
    const json_t *layout = json_obj_get(req, "layout");
    const json_t *rows = json_obj_get(req, "rows");
    long error_at = -1, state_at = -1;
    size_t ln = layout && layout->type == JSON_ARRAY ? json_len(layout) : 0;
    size_t rn = rows && rows->type == JSON_ARRAY ? json_len(rows) : 0;
    size_t i;
    if (strcmp(kind, "sessions") != 0) return json_bool(1);
    for (i = 0; i < ln; i++) {
        const char *name = laf_text(json_get(layout, i));
        if (!name) continue;
        if (strcmp(name, "handoff_error") == 0) error_at = (long)i;
        if (strcmp(name, "handoff_state") == 0) state_at = (long)i;
    }
    if (error_at < 0 || state_at < 0) return json_bool(1);
    for (i = 0; i < rn; i++) {
        const json_t *row = json_get(rows, i);
        size_t rl = row && row->type == JSON_ARRAY ? json_len(row) : 0;
        if ((size_t)error_at >= rl || (size_t)state_at >= rl) continue;
        if (!laf_null(json_get(row, (size_t)error_at)) &&
            laf_null(json_get(row, (size_t)state_at)))
            return json_bool(0);
    }
    return json_bool(1);
}

json_t *w30_lostandfou_6760c6_p2_layout_consensus(const json_t *req) {
    const json_t *survivors = json_obj_get(req, "survivors");
    json_t *out = json_array();
    size_t sn = survivors && survivors->type == JSON_ARRAY ? json_len(survivors) : 0;
    size_t width = 0, i, j;
    if (sn == 0) return out;
    {
        const json_t *first = json_get(survivors, 0);
        width = first && first->type == JSON_ARRAY ? json_len(first) : 0;
    }
    for (i = 0; i < width; i++) {
        const char *only = NULL;
        int agree = 1;
        for (j = 0; j < sn; j++) {
            const char *name = laf_text(json_get(json_get(survivors, j), i));
            if (j == 0) {
                only = name;
            } else if ((only == NULL) != (name == NULL) ||
                       (only && name && strcmp(only, name) != 0)) {
                agree = 0;
                break;
            }
        }
        json_append(out, agree && only ? json_string(only) : json_null());
    }
    return out;
}

static void laf_push_record(json_t *out, const char *kind, const json_t *rowid,
                            long long nfield, json_t *cells) {
    json_t *entry = json_object();
    json_set(entry, "kind", kind ? json_string(kind) : json_null());
    json_set(entry, "lf_rowid", rowid ? json_copy(rowid) : json_null());
    json_set(entry, "nfield", json_number((double)nfield));
    json_set(entry, "cells", cells);
    json_append(out, entry);
}

json_t *w30_lostandfou_6760c6_p2_records(const json_t *req) {
    const json_t *tables = json_obj_get(req, "tables");
    json_t *out = json_array();
    static const char *const HEAD[3] = {"rootpgno", "pgno", "nfield"};
    size_t tn = tables && tables->type == JSON_ARRAY ? json_len(tables) : 0, t;
    for (t = 0; t < tn; t++) {
        const json_t *table = json_get(tables, t);
        const json_t *columns = json_obj_get(table, "columns");
        const json_t *rows = json_obj_get(table, "rows");
        size_t cn = columns && columns->type == JSON_ARRAY ? json_len(columns) : 0;
        size_t rn = rows && rows->type == JSON_ARRAY ? json_len(rows) : 0, r, k;
        int head_ok = cn >= 3;
        for (k = 0; k < 3 && head_ok; k++) {
            const char *name = laf_text(json_get(columns, k));
            if (!name || strcmp(name, HEAD[k]) != 0) head_ok = 0;
        }
        if (!head_ok) continue;
        for (r = 0; r < rn; r++) {
            const json_t *row = json_get(rows, r);
            const json_t *nfv = json_get(row, 2);
            json_t *cells = json_array();
            long long nfield = 0;
            int ok = 1;
            size_t rl = row && row->type == JSON_ARRAY ? json_len(row) : 0;
            if (laf_null(nfv)) nfield = 0;
            else if (nfv->type == JSON_NUMBER) nfield = (long long)nfv->num_val;
            else if (nfv->type == JSON_BOOL) nfield = nfv->bool_val ? 1 : 0;
            else if (nfv->type == JSON_STRING)
                ok = laf_int_text(nfv->str_val ? nfv->str_val : "", &nfield);
            else ok = 0;
            if (!ok) {
                json_free(cells);
                laf_push_record(out, NULL, NULL, 0, json_array());
                continue;
            }
            {
                long long limit = 4 + (nfield > 0 ? nfield : 0);
                size_t i;
                for (i = 4; i < rl && (long long)i < limit; i++)
                    json_append(cells, json_copy(json_get(row, i)));
            }
            laf_push_record(out, laf_classify((int)nfield, cells), json_get(row, 3), nfield, cells);
        }
    }
    return out;
}

json_t *w30_lostandfou_6760c6_p2_stub_plan(const json_t *req) {
    const json_t *orphans = json_obj_get(req, "orphans");
    const json_t *taken = json_obj_get(req, "taken_titles");
    const char *label = json_get_str(req, "label", LAF_STUB_LABEL);
    json_t *out = json_array();
    char **ids = NULL;
    double *started = NULL;
    double *counts = NULL;
    size_t n = 0, cap = 0, i, j;
    size_t on = orphans && orphans->type == JSON_ARRAY ? json_len(orphans) : 0;
    size_t taken_n = taken && taken->type == JSON_ARRAY ? json_len(taken) : 0;

    for (i = 0; i < on; i++) {
        const json_t *entry = json_get(orphans, i);
        char *sid = laf_pystr(json_obj_get(entry, "session_id"));
        const json_t *st = json_obj_get(entry, "started_at");
        const json_t *mc = json_obj_get(entry, "message_count");
        int at = -1;
        if (!sid) continue;
        for (j = 0; j < n; j++)
            if (strcmp(ids[j], sid) == 0) { at = (int)j; break; }
        if (at >= 0 && laf_truthy(json_obj_get(entry, "setdefault"))) {
            free(sid);
            continue;
        }
        if (at >= 0) {
            started[at] = laf_is_number(st) ? laf_as_double(st) : 0.0;
            counts[at] = laf_is_number(mc) ? laf_as_double(mc) : 0.0;
            free(sid);
            continue;
        }
        if (n == cap) {
            size_t ncap = cap ? cap * 2 : 8;
            char **ni = (char **)realloc(ids, ncap * sizeof *ni);
            double *ns = (double *)realloc(started, ncap * sizeof *ns);
            double *nc = (double *)realloc(counts, ncap * sizeof *nc);
            if (!ni || !ns || !nc) { free(sid); break; }
            ids = ni;
            started = ns;
            counts = nc;
            cap = ncap;
        }
        ids[n] = sid;
        started[n] = laf_is_number(st) ? laf_as_double(st) : 0.0;
        counts[n] = laf_is_number(mc) ? laf_as_double(mc) : 0.0;
        n++;
    }

    /* sorted(orphan_ids.items()) over the str key */
    for (i = 1; i < n; i++) {
        char *sid = ids[i];
        double s0 = started[i], c0 = counts[i];
        j = i;
        while (j > 0 && strcmp(ids[j - 1], sid) > 0) {
            ids[j] = ids[j - 1];
            started[j] = started[j - 1];
            counts[j] = counts[j - 1];
            j--;
        }
        ids[j] = sid;
        started[j] = s0;
        counts[j] = c0;
    }

    {
        long long sequence = 1;
        for (i = 0; i < n; i++) {
            char title[512];
            json_t *entry;
            for (;;) {
                int exists = 0;
                snprintf(title, sizeof title, "[%s %lld] session metadata was unreadable",
                         label, sequence);
                sequence++;
                for (j = 0; j < taken_n; j++) {
                    const char *t = laf_text(json_get(taken, j));
                    if (t && strcmp(t, title) == 0) { exists = 1; break; }
                }
                if (!exists) break;
            }
            entry = json_object();
            json_set(entry, "session_id", json_string(ids[i]));
            json_set(entry, "started_at", json_number(started[i]));
            json_set(entry, "message_count", json_number(counts[i]));
            json_set(entry, "title", json_string(title));
            json_append(out, entry);
        }
    }
    for (i = 0; i < n; i++) free(ids[i]);
    free(ids);
    free(started);
    free(counts);
    return out;
}

json_t *w30_lostandfou_6760c6_p2_fts_rebuild_sql(const json_t *req) {
    const char *table = json_get_str(req, "table", "");
    laf_sb stmt;
    char *s;
    json_t *out;
    laf_sb_init(&stmt);
    laf_sb_add(&stmt, "INSERT INTO \"");
    laf_sb_add(&stmt, table);
    laf_sb_add(&stmt, "\" (\"");
    laf_sb_add(&stmt, table);
    laf_sb_add(&stmt, "\") VALUES ('rebuild')");
    s = laf_sb_take(&stmt);
    out = json_string(s ? s : "");
    free(s);
    return out;
}

json_t *w30_lostandfou_6760c6_p2_stub_title(const json_t *req) {
    const json_t *seq = json_obj_get(req, "sequence");
    const char *label = json_get_str(req, "label", LAF_STUB_LABEL);
    char buf[512];
    snprintf(buf, sizeof buf, "[%s %lld] session metadata was unreadable", label,
             seq && seq->type == JSON_NUMBER ? (long long)seq->num_val : 0LL);
    return json_string(buf);
}

json_t *w30_lostandfou_6760c6_p2_unrecognized_widths(const json_t *req) {
    const json_t *widths = json_obj_get(req, "widths");
    const json_t *width = json_obj_get(req, "width");
    json_t *out = json_array();
    size_t n = widths && widths->type == JSON_ARRAY ? json_len(widths) : 0, i;
    int present = 0;
    for (i = 0; i < n; i++) {
        const json_t *w = json_get(widths, i);
        json_append(out, json_copy(w));
        if (w && width && w->type == JSON_NUMBER && width->type == JSON_NUMBER &&
            w->num_val == width->num_val)
            present = 1;
    }
    if (!present && width && width->type != JSON_NULL) json_append(out, json_copy(width));
    return out;
}

static char *laf_strip(char *s) {
    char *end;
    while (*s && laf_isspace_ascii(*s)) s++;
    if (!*s) return s;
    end = s + strlen(s) - 1;
    while (end > s && laf_isspace_ascii(*end)) { *end = '\0'; end--; }
    return s;
}

json_t *w30_lostandfou_6760c6_p2_recover_details(const json_t *req) {
    const json_t *attempts = json_obj_get(req, "attempts");
    size_t n = attempts && attempts->type == JSON_ARRAY ? json_len(attempts) : 0, i;
    json_t *out = json_object();
    int usable = 0;
    if (n) usable = laf_truthy(json_obj_get(json_get(attempts, n - 1), "usable"));
    json_set(out, "usable", json_bool(usable ? 1 : 0));
    if (usable) {
        json_set(out, "needs_header_zeroing", json_bool(0));
        json_set(out, "details", json_string(""));
        json_set(out, "message", json_string(""));
        return out;
    }
    {
        int zeroing = 0;
        laf_sb details;
        for (i = 0; i < n; i++) {
            char *tail = laf_pystr(json_obj_get(json_get(attempts, i), "dump_stderr_tail"));
            if (tail && strstr(tail, "not a database")) zeroing = 1;
            free(tail);
        }
        laf_sb_init(&details);
        for (i = 0; i < n; i++) {
            const json_t *a = json_get(attempts, i);
            const json_t *dump_tail = json_obj_get(a, "dump_stderr_tail");
            const json_t *load_tail = json_obj_get(a, "load_stderr_tail");
            char *command = laf_pystr(json_obj_get(a, "command"));
            char *dump_rc = laf_pystr(json_obj_get(a, "dump_returncode"));
            char *load_rc = laf_pystr(json_obj_get(a, "load_returncode"));
            char *tail = laf_pystr(laf_truthy(dump_tail) ? dump_tail : load_tail);
            laf_sb part;
            char *raw, *stripped;
            if (i) laf_sb_add(&details, "; ");
            laf_sb_init(&part);
            laf_sb_add(&part, "[");
            laf_sb_add(&part, command ? command : "");
            laf_sb_add(&part, "] dump rc=");
            laf_sb_add(&part, dump_rc ? dump_rc : "");
            laf_sb_add(&part, " load rc=");
            laf_sb_add(&part, load_rc ? load_rc : "");
            laf_sb_add(&part, " ");
            laf_sb_add(&part, tail ? tail : "");
            raw = laf_sb_take(&part);
            stripped = raw ? laf_strip(raw) : NULL;
            laf_sb_add(&details, stripped ? stripped : "");
            free(raw);
            free(command);
            free(dump_rc);
            free(load_rc);
            free(tail);
        }
        {
            char *d = laf_sb_take(&details);
            laf_sb msg;
            char *m;
            laf_sb_init(&msg);
            laf_sb_add(&msg, "sqlite3 .recover did not produce a usable lost_and_found database: ");
            laf_sb_add(&msg, d ? d : "");
            m = laf_sb_take(&msg);
            json_set(out, "needs_header_zeroing", json_bool(zeroing ? 1 : 0));
            json_set(out, "details", json_string(d ? d : ""));
            json_set(out, "message", json_string(m ? m : ""));
            free(d);
            free(m);
        }
    }
    return out;
}

json_t *w30_lostandfou_6760c6_p2_wal_refusal(const json_t *req) {
    const json_t *ver = json_obj_get(req, "version");
    long long p[3] = {0, 0, 0};
    size_t n = ver && ver->type == JSON_ARRAY ? json_len(ver) : 0, i;
    char version_str[96];
    json_t *out;
    for (i = 0; i < 3 && i < n; i++) p[i] = laf_version_part(json_get(ver, i));
    if (!laf_wal_vulnerable(p[0], p[1], p[2])) return json_null();
    snprintf(version_str, sizeof version_str, "%lld.%lld.%lld", p[0], p[1], p[2]);
    out = json_object();
    json_set(out, "reason", json_string("wal_reset_vulnerable"));
    json_set(out, "binary", json_string(json_get_str(req, "binary", "")));
    json_set(out, "version", json_string(version_str));
    {
        laf_sb detail;
        char *d;
        laf_sb_init(&detail);
        laf_sb_add(&detail, "reports version ");
        laf_sb_add(&detail, version_str);
        laf_sb_add(&detail, ", which has ");
        laf_sb_add(&detail, LAF_WAL_GUIDANCE);
        d = laf_sb_take(&detail);
        json_set(out, "detail", json_string(d ? d : ""));
        free(d);
    }
    return out;
}
