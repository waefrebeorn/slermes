/*
 * messages.c — C11 home for upstream Hermes
 * persistence/sqlite/conversation/_store/messages.py (1980 lines, 89 defs).
 *
 * GATES: w27_messages_87d5f2_p1 .. _p4 (this one file is linked by all four
 * harnesses; each gate header declares only that gate's surface).
 *
 * PORTED (pure: data in -> data / SQL text out, no connection, no clock):
 *   p1  _encode_display_metadata          msg_encode_display_metadata
 *   p1  _decode_display_metadata          msg_decode_display_metadata
 *   p1  _reasoning_json_text              msg_reasoning_json_text
 *   p1  _decoded_repair_row               msg_decoded_repair_row
 *   p1  _bump_session_counters            msg_bump_session_counters
 *   p1  _reaction_list                    msg_reaction_list
 *   p2  _row_identity                     msg_row_identity
 *   p2  _stamp_kept_live_prefix           msg_stamp_kept_live_prefix
 *   p2  _loaded_view_content              msg_loaded_view_content
 *   p2  _clone_message_rows               msg_clone_message_rows
 *   p3  _is_model_only_row                msg_is_model_only_row
 *   p3  _fold_summary_carrier             msg_fold_summary_carrier
 *   p3  _row_to_message_dict              msg_row_to_message_dict
 *   p3  _display_rows_from_conn           msg_display_rows_from_conn
 *   p3  _active_clause                    msg_active_clause
 *   p3  _fetch_conversation_rows          msg_fetch_conversation_rows
 *   p3  get_messages (row-assembly path)  msg_get_messages
 *   p4  _canonical_replayed_user_content  msg_canonical_replayed_user_content
 *   p4  _exact_replayed_user_clone_key    msg_exact_replayed_user_clone_key
 *   p4  _find_duplicate_replayed_user_message msg_find_duplicate_replayed_user_message
 *   p4  _is_explicit_fork_child_row       msg_is_explicit_fork_child_row
 *
 * SEAMED (pure in Python, but not executable against a C string JSON value —
 * stated per symbol so nothing silently diverges):
 *   _encode_content / _decode_content: the structured-content sentinel is
 *     b"\x00json:" — a NUL byte, which a libjson string cannot carry, so the
 *     encode/decode pair (and the reserved-prefix smuggling guard) is a storage
 *     boundary codec here.  Everything that reads the codec on PLAIN text
 *     (decode_content below returns text unchanged) is ported.
 *   _display_identity: sha256(repr(python-tuple)) — the repr of a heterogeneous
 *     tuple is not a C string surface; the digest is computed where the tuple is.
 *   _display_dedupe_key, _message_row_params, _serialized_message_row,
 *     _dedupe_display_generations: ride on _display_identity +
 *     session_rules.turns.split_user_originated_turn (cross-module turn policy).
 *   _dedupe_replayed_user: compares dict identity (`is`) of the carrier rows.
 *   _split_rewind_target: raises for the composite-carrier branch of
 *     split_user_originated_turn (same cross-module policy).
 *   Every function whose body is conn.execute/_read_one/_read_all/
 *     _execute_write/_read_ctx (append_message, append_messages_batch,
 *     set_message_reaction, take_unseen_reactions, latest_message_row_id,
 *     _insert_message_rows, _prune_shadowed_checkpoints,
 *     _drop_shadowed_checkpoint_rows, replace_messages*, clear_messages,
 *     _archive_named_rows, archive_and_compact*, _ensure_display_order,
 *     _legacy_display_page, _reconcile_display_orders, display_message_count,
 *     _display_messages_from_conn, find_pr_url_messages, get_messages_around,
 *     resolve_resume_session_id, conversation_revision, _rows_to_conversation,
 *     get_resume_conversations, _resume_lineage_ids, _resume_count_scope,
 *     assert_resume_safe, get_ancestor_display_prefix, get_conversation_root,
 *     get_active_message_ids, _active_transcript_counts, rewind_to_message,
 *     message_count, has_gateway_input_owner, has_platform_message_id,
 *     is_explicit_fork_child, latest_conversation_boundary,
 *     purge_stale_tool_call_markers, _bump_conversation_generation,
 *     _check_transcript_write_guards, set_latest_matching_message_display_kind,
 *     latest_conversation_role, get_message_role, _carry_parent_timestamps,
 *     _execute_transcript_write, _message_column_names, set_user_message_content,
 *     deactivate_message, resolve_active_row_id,
 *     deactivate_messages_by_display_kind, _reaction_row_query,
 *     _tail_rows_after_watermark, _with_removed_identities, _display_set,
 *     get_messages_as_conversation).
 *
 * PoP (one line per symbol, ported direction):
 * PoP: msg_encode_display_metadata @ persistence/sqlite/conversation/_store/messages.py:_encode_display_metadata
 * PoP: msg_decode_display_metadata @ persistence/sqlite/conversation/_store/messages.py:_decode_display_metadata
 * PoP: msg_reasoning_json_text @ persistence/sqlite/conversation/_store/messages.py:_reasoning_json_text
 * PoP: msg_decoded_repair_row @ persistence/sqlite/conversation/_store/messages.py:_decoded_repair_row
 * PoP: msg_bump_session_counters @ persistence/sqlite/conversation/_store/messages.py:_bump_session_counters
 * PoP: msg_reaction_list @ persistence/sqlite/conversation/_store/messages.py:_reaction_list
 * PoP: msg_row_identity @ persistence/sqlite/conversation/_store/messages.py:_row_identity
 * PoP: msg_stamp_kept_live_prefix @ persistence/sqlite/conversation/_store/messages.py:_stamp_kept_live_prefix
 * PoP: msg_loaded_view_content @ persistence/sqlite/conversation/_store/messages.py:_loaded_view_content
 * PoP: msg_clone_message_rows @ persistence/sqlite/conversation/_store/messages.py:_clone_message_rows
 * PoP: msg_is_model_only_row @ persistence/sqlite/conversation/_store/messages.py:_is_model_only_row
 * PoP: msg_fold_summary_carrier @ persistence/sqlite/conversation/_store/messages.py:_fold_summary_carrier
 * PoP: msg_row_to_message_dict @ persistence/sqlite/conversation/_store/messages.py:_row_to_message_dict
 * PoP: msg_display_rows_from_conn @ persistence/sqlite/conversation/_store/messages.py:_display_rows_from_conn
 * PoP: msg_active_clause @ persistence/sqlite/conversation/_store/messages.py:_active_clause
 * PoP: msg_fetch_conversation_rows @ persistence/sqlite/conversation/_store/messages.py:_fetch_conversation_rows
 * PoP: msg_get_messages @ persistence/sqlite/conversation/_store/messages.py:get_messages
 * PoP: msg_canonical_replayed_user_content @ persistence/sqlite/conversation/_store/messages.py:_canonical_replayed_user_content
 * PoP: msg_exact_replayed_user_clone_key @ persistence/sqlite/conversation/_store/messages.py:_exact_replayed_user_clone_key
 * PoP: msg_find_duplicate_replayed_user_message @ persistence/sqlite/conversation/_store/messages.py:_find_duplicate_replayed_user_message
 * PoP: msg_is_explicit_fork_child_row @ persistence/sqlite/conversation/_store/messages.py:_is_explicit_fork_child_row
 */
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Upstream constants this file binds (values read from the live modules). */
#define MSG_MODEL_ONLY_KEY "model_only"
#define MSG_REACTIONS_KEY "reactions"
#define MSG_MESSAGE_UID "message_uid"
#define MSG_TOOL_CALL_UIDS "_tool_call_uids"
#define MSG_TOOL_CALL_UID "_tool_call_uid"
#define MSG_ABSORBED_UIDS "_absorbed_message_uids"
#define MSG_DISPLAY_ACTIVE_CLAUSE " AND (active = 1 OR compacted = 1)"
#define MSG_CONVERSATION_ROW_COLUMNS                                                              \
    "id, role, content, tool_call_id, tool_calls, tool_name, effect_disposition, "                \
    "finish_reason, reasoning, reasoning_content, reasoning_details, "                            \
    "codex_reasoning_items, codex_message_items, platform_message_id, observed, "                \
    "_compressed_summary, timestamp, token_count, active, api_content, display_kind, "           \
    "display_metadata, message_uid, absorbed_message_uids, tool_call_uids, tool_call_uid"
#define MSG_DISPLAY_VISIBLE_SQL                                                                   \
    " AND COALESCE(json_extract((CASE WHEN json_valid(display_metadata) THEN display_metadata "   \
    "ELSE json_object() END), '$." MSG_MODEL_ONLY_KEY "'), 0) = 0"

/* ------------------------------------------------------------------ */
/* JSON primitives: absent == null == None; Python truthiness/equality */
/* ------------------------------------------------------------------ */

static json_t *jg(const json_t *o, const char *k) {
    if (!o || o->type != JSON_OBJECT) return NULL;
    return json_obj_get(o, k);
}

static int j_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int j_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int j_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int j_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int j_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }

static const char *j_str(const json_t *v) { return j_is_str(v) ? v->str_val : NULL; }

/* Python truthiness of a decoded JSON value (None/0/""/[]/{} are false). */
static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

/* Python == over decoded JSON (True == 1, 1 == 1.0, absent == None). */
static int py_eq(const json_t *a, const json_t *b) {
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (a->type == JSON_NUMBER && b->type == JSON_BOOL) return a->num_val == (b->bool_val ? 1.0 : 0.0);
    if (a->type == JSON_BOOL && b->type == JSON_NUMBER) return (a->bool_val ? 1.0 : 0.0) == b->num_val;
    if (a->type != b->type) return 0;
    switch (a->type) {
        case JSON_BOOL: return (a->bool_val ? 1 : 0) == (b->bool_val ? 1 : 0);
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++)
                if (!py_eq(a->c.items[i], b->c.items[i])) return 0;
            return 1;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++)
                if (!py_eq(a->c.items[i], jg(b, a->c.keys[i]))) return 0;
            return 1;
        default: return 0;
    }
}

/* Python `x in (None, "", [])`. */
static int py_in_null_blank_list(const json_t *v) {
    if (j_is_null(v)) return 1;
    if (j_is_str(v) && v->str_val[0] == '\0') return 1;
    if (j_is_arr(v) && v->c.count == 0) return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Python-compatible json.dumps (separators, sort_keys, ensure_ascii)  */
/* ------------------------------------------------------------------ */

typedef struct {
    char *p;
    size_t len, cap;
} sbuf;

static void sb_grow(sbuf *s, size_t need) {
    if (s->len + need + 1 <= s->cap) return;
    size_t cap = s->cap ? s->cap : 128;
    while (cap < s->len + need + 1) cap *= 2;
    char *np = (char *)realloc(s->p, cap);
    if (!np) abort();
    s->p = np;
    s->cap = cap;
}

static void sb_putn(sbuf *s, const char *t, size_t n) {
    sb_grow(s, n);
    memcpy(s->p + s->len, t, n);
    s->len += n;
    s->p[s->len] = '\0';
}

static void sb_puts(sbuf *s, const char *t) { sb_putn(s, t, strlen(t)); }
static void sb_putc(sbuf *s, char c) { sb_putn(s, &c, 1); }

static char *sb_take(sbuf *s) {
    if (!s->p) {
        char *e = (char *)malloc(1);
        if (!e) abort();
        e[0] = '\0';
        return e;
    }
    char *r = s->p;
    s->p = NULL;
    s->len = s->cap = 0;
    return r;
}

/* Shortest round-tripping decimal, Python float repr style. */
static void put_num(sbuf *s, double v) {
    char tmp[64];
    if (v == 0.0) { sb_puts(s, "0"); return; }
    if (v == (double)(long long)v && fabs(v) < 1e16) {
        snprintf(tmp, sizeof tmp, "%.0f", v);
        sb_puts(s, tmp);
        return;
    }
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(tmp, sizeof tmp, "%.*g", prec, v);
        if (strtod(tmp, NULL) == v) break;
    }
    sb_puts(s, tmp);
}

/* json.dumps(ensure_ascii=True) string body; UTF-8 input, \uXXXX output. */
static void put_str(sbuf *s, const char *str) {
    const unsigned char *p = (const unsigned char *)(str ? str : "");
    sb_putc(s, '"');
    while (*p) {
        unsigned char c = *p;
        if (c == '"') { sb_puts(s, "\\\""); p++; continue; }
        if (c == '\\') { sb_puts(s, "\\\\"); p++; continue; }
        if (c == '\b') { sb_puts(s, "\\b"); p++; continue; }
        if (c == '\f') { sb_puts(s, "\\f"); p++; continue; }
        if (c == '\n') { sb_puts(s, "\\n"); p++; continue; }
        if (c == '\r') { sb_puts(s, "\\r"); p++; continue; }
        if (c == '\t') { sb_puts(s, "\\t"); p++; continue; }
        if (c < 0x20 || c == 0x7f) {
            char esc[8];
            snprintf(esc, sizeof esc, "\\u%04x", c);
            sb_puts(s, esc);
            p++;
            continue;
        }
        if (c < 0x80) { sb_putc(s, (char)c); p++; continue; }
        /* decode one UTF-8 codepoint */
        unsigned cp = 0;
        int n = 0;
        if ((c & 0xE0) == 0xC0) { cp = c & 0x1Fu; n = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0Fu; n = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07u; n = 3; }
        else { cp = 0xFFFD; n = 0; }
        p++;
        for (int i = 0; i < n && (*p & 0xC0) == 0x80; i++, p++) cp = (cp << 6) | (*p & 0x3Fu);
        if (cp > 0xFFFF) {
            cp -= 0x10000;
            char esc[16];
            snprintf(esc, sizeof esc, "\\u%04x\\u%04x", 0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF));
            sb_puts(s, esc);
        } else {
            char esc[8];
            snprintf(esc, sizeof esc, "\\u%04x", cp);
            sb_puts(s, esc);
        }
    }
    sb_putc(s, '"');
}

typedef struct {
    int sort_keys;
    int compact;
} dumpopt;

static int key_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void dump_node(sbuf *s, const json_t *v, const dumpopt *o) {
    if (!v) { sb_puts(s, "null"); return; }
    switch (v->type) {
        case JSON_NULL: sb_puts(s, "null"); break;
        case JSON_BOOL: sb_puts(s, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: put_num(s, v->num_val); break;
        case JSON_STRING: put_str(s, v->str_val); break;
        case JSON_ARRAY:
            sb_putc(s, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) sb_puts(s, o->compact ? "," : ", ");
                dump_node(s, v->c.items[i], o);
            }
            sb_putc(s, ']');
            break;
        case JSON_OBJECT: {
            sb_putc(s, '{');
            size_t n = v->c.count;
            const char **keys = NULL;
            if (o->sort_keys && n) {
                keys = (const char **)malloc(n * sizeof *keys);
                if (!keys) abort();
                for (size_t i = 0; i < n; i++) keys[i] = v->c.keys[i];
                qsort(keys, n, sizeof *keys, key_cmp);
            }
            for (size_t i = 0; i < n; i++) {
                const char *k = keys ? keys[i] : v->c.keys[i];
                if (i) sb_puts(s, o->compact ? "," : ", ");
                put_str(s, k);
                sb_puts(s, o->compact ? ":" : ": ");
                dump_node(s, jg(v, k), o);
            }
            free(keys);
            sb_putc(s, '}');
            break;
        }
    }
}

static json_t *py_dumps_json(const json_t *v, int sort_keys, int compact) {
    sbuf s = {NULL, 0, 0};
    dumpopt o = {sort_keys, compact};
    dump_node(&s, v, &o);
    char *text = sb_take(&s);
    json_t *r = json_string(text);
    free(text);
    return r;
}

/* ------------------------------------------------------------------ */
/* Cross-module primitives the pure bodies below call                   */
/* ------------------------------------------------------------------ */

/* session_rules.turns.sanitize_context: three regexes, applied in order. */
static int ci_match(const char *p, const char *lit) {
    while (*lit) {
        if (!*p) return 0;
        if (tolower((unsigned char)*p) != tolower((unsigned char)*lit)) return 0;
        p++;
        lit++;
    }
    return 1;
}

static int is_ws(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }

static void skip_ws(const char **p) {
    while (is_ws(**p)) (*p)++;
}

/* "<" ws* "memory-context" ws* ">" */
static size_t open_tag_at(const char *p) {
    if (p[0] != '<') return 0;
    const char *q = p + 1;
    skip_ws(&q);
    if (!ci_match(q, "memory-context")) return 0;
    q += 14;
    skip_ws(&q);
    if (*q != '>') return 0;
    return (size_t)(q + 1 - p);
}

/* "</" ws* "memory-context" ws* ">" */
static size_t close_tag_at(const char *p) {
    if (p[0] != '<' || p[1] != '/') return 0;
    const char *q = p + 2;
    skip_ws(&q);
    if (!ci_match(q, "memory-context")) return 0;
    q += 14;
    skip_ws(&q);
    if (*q != '>') return 0;
    return (size_t)(q + 1 - p);
}

/* [System note: ... Treat as (informational background data|authoritative
 * reference data[^]]*).]\s*  -> length of the match, 0 when absent. */
static size_t system_note_at(const char *p) {
    const char *q = p;
    if (!ci_match(q, "[system note:")) return 0;
    q += 13;
    skip_ws(&q);
    if (!ci_match(q, "the following is recalled memory context,")) return 0;
    q += strlen("the following is recalled memory context,");
    skip_ws(&q);
    if (!ci_match(q, "not new user input.")) return 0;
    q += strlen("not new user input.");
    skip_ws(&q);
    if (!ci_match(q, "treat as ")) return 0;
    q += strlen("treat as ");
    if (ci_match(q, "informational background data")) {
        const char *r = q + strlen("informational background data");
        if (r[0] == '.' && r[1] == ']') {
            r += 2;
            while (is_ws(*r)) r++;
            return (size_t)(r - p);
        }
        return 0;
    }
    if (ci_match(q, "authoritative reference data")) {
        const char *r = q + strlen("authoritative reference data");
        while (*r && *r != ']') r++;
        if (*r == ']' && r > q + strlen("authoritative reference data") && r[-1] == '.') {
            r++;
            while (is_ws(*r)) r++;
            return (size_t)(r - p);
        }
    }
    return 0;
}

static char *sanitize_context(const char *in) {
    const char *src = in ? in : "";
    /* pass 1: internal context blocks (non-greedy to the first close tag) */
    sbuf a = {NULL, 0, 0};
    for (const char *p = src; *p;) {
        size_t o = open_tag_at(p);
        if (o) {
            size_t found = 0;
            const char *q = p + o;
            for (; *q; q++) {
                size_t c = close_tag_at(q);
                if (c) { found = c; break; }
            }
            if (found) { p = q + found; continue; }
        }
        sb_putc(&a, *p++);
    }
    char *stage1 = sb_take(&a);
    /* pass 2: recalled-memory system notes */
    sbuf b = {NULL, 0, 0};
    for (const char *p = stage1; *p;) {
        size_t n = system_note_at(p);
        if (n) { p += n; continue; }
        sb_putc(&b, *p++);
    }
    char *stage2 = sb_take(&b);
    free(stage1);
    /* pass 3: bare fence tags */
    sbuf c = {NULL, 0, 0};
    for (const char *p = stage2; *p;) {
        size_t n = open_tag_at(p);
        if (!n) n = close_tag_at(p);
        if (n) { p += n; continue; }
        sb_putc(&c, *p++);
    }
    free(stage2);
    return sb_take(&c);
}

static char *py_strip(const char *s) {
    const char *b = s;
    while (is_ws(*b)) b++;
    size_t n = strlen(b);
    while (n > 0 && is_ws(b[n - 1])) n--;
    char *r = (char *)malloc(n + 1);
    if (!r) abort();
    memcpy(r, b, n);
    r[n] = '\0';
    return r;
}

/* messages_sql._parse_tool_calls: a JSON string is parsed, a bad one is []. */
static json_t *parse_tool_calls(const json_t *tc) {
    if (!tc || tc->type != JSON_STRING) return json_copy(tc);
    json_t *p = json_parse(tc->str_val, NULL);
    if (!p) return json_array();
    return p;
}

/* messages_sql._scrub_surrogates / rules._sanitize_surrogates: lone UTF-16
 * surrogates cannot be expressed in a JSON string value, so this is the
 * identity on every value that reaches C. */

/* _state.common._json_or(raw, fallback, warning) */
static json_t *json_or(const json_t *raw, json_t *fallback) {
    if (!raw || raw->type != JSON_STRING) return fallback;
    json_t *p = json_parse(raw->str_val, NULL);
    if (!p) return fallback;
    return p;
}

/* identity._uid_list: a str (JSON text) or a list -> unique non-empty strings. */
static json_t *uid_list(const json_t *value) {
    json_t *v = NULL;
    if (j_is_str(value)) v = json_or(value, json_array());
    else if (j_is_arr(value)) v = json_copy(value);
    json_t *out = json_array();
    if (!v) return out;
    if (!j_is_arr(v)) { json_free(v); return out; }
    for (size_t i = 0; i < v->c.count; i++) {
        const json_t *e = v->c.items[i];
        if (!j_is_str(e) || !e->str_val[0]) continue;
        int dup = 0;
        for (size_t j = 0; j < out->c.count && !dup; j++)
            if (strcmp(out->c.items[j]->str_val, e->str_val) == 0) dup = 1;
        if (!dup) json_append(out, json_string(e->str_val));
    }
    json_free(v);
    return out;
}

/* identity._uid_map: {call id: uid | [uid, ...]} with non-empty entries only. */
static json_t *uid_map(const json_t *value) {
    json_t *v = NULL;
    if (j_is_str(value)) v = json_or(value, json_object());
    else if (j_is_obj(value)) v = json_copy(value);
    json_t *out = json_object();
    if (!v) return out;
    if (!j_is_obj(v)) { json_free(v); return out; }
    for (size_t i = 0; i < v->c.count; i++) {
        const char *k = v->c.keys[i];
        const json_t *e = v->c.items[i];
        if (!k || !k[0]) continue;
        int ok = 0;
        if (j_is_str(e) && e->str_val[0]) ok = 1;
        else if (j_is_arr(e) && e->c.count) {
            ok = 1;
            for (size_t j = 0; j < e->c.count; j++)
                if (!j_is_str(e->c.items[j]) || !e->c.items[j]->str_val[0]) { ok = 0; break; }
        }
        if (ok) json_set(out, k, json_copy(e));
    }
    json_free(v);
    return out;
}

/* identity._live_extends( live, stored ) */
static int live_extends(const json_t *live, const json_t *stored) {
    if (!j_is_arr(live)) return 0;
    json_t *stored_list = j_is_arr(stored) ? json_copy(stored) : NULL;
    size_t need = stored_list ? stored_list->c.count : 1;
    if (live->c.count < need) { json_free(stored_list); return 0; }
    int ok = 1;
    for (size_t i = 0; i < need; i++) {
        const json_t *want = stored_list ? stored_list->c.items[i] : stored;
        if (!py_eq(live->c.items[i], want)) { ok = 0; break; }
    }
    json_free(stored_list);
    return ok;
}

/* identity._restore_identity_columns(row, msg) */
static void restore_identity_columns(const json_t *row, json_t *msg) {
    const json_t *uid = jg(row, MSG_MESSAGE_UID);
    if (py_truthy(uid)) json_set(msg, MSG_MESSAGE_UID, json_copy(uid));
    const json_t *tcu = jg(row, "tool_call_uids");
    if (py_truthy(tcu)) {
        json_t *stored = uid_map(tcu);
        if (stored->c.count) {
            json_t *live_src = jg(msg, MSG_TOOL_CALL_UIDS);
            json_t *live = j_is_obj(live_src) ? json_copy(live_src) : json_object();
            for (size_t i = 0; i < stored->c.count; i++) {
                const char *k = stored->c.keys[i];
                if (!live_extends(jg(live, k), stored->c.items[i]))
                    json_set(live, k, json_copy(stored->c.items[i]));
            }
            json_set(msg, MSG_TOOL_CALL_UIDS, live);
        }
        json_free(stored);
    }
    const json_t *tcu1 = jg(row, "tool_call_uid");
    if (py_truthy(tcu1)) json_set(msg, MSG_TOOL_CALL_UID, json_copy(tcu1));
    const json_t *absorbed = jg(row, "absorbed_message_uids");
    if (py_truthy(absorbed)) {
        json_t *uids = uid_list(absorbed);
        if (uids->c.count) json_set(msg, MSG_ABSORBED_UIDS, uids);
        else json_free(uids);
    }
}

/* messages._decode_content: the \x00json: sentinel cannot live in a C string,
 * so a stored value is returned unchanged (see the header seam note). */
static json_t *decode_content(const json_t *content) { return json_copy(content); }

/* messages._encode_content on its representable domain. */
static json_t *encode_content(const json_t *content) {
    if (!content) return NULL;
    if (content->type == JSON_STRING) {
        char *clean = sanitize_context(content->str_val);
        json_t *r = json_string(clean);
        free(clean);
        return r;
    }
    if (content->type == JSON_NULL || content->type == JSON_NUMBER || content->type == JSON_BOOL)
        return json_copy(content);
    return NULL; /* structured parts ride the NUL sentinel: seamed */
}

/* messages._decode_display_metadata's core, on an already-held value. */
static json_t *decode_display_metadata(const json_t *raw) {
    if (j_is_null(raw)) return NULL;
    json_t *cur = json_copy(raw);
    for (int i = 0; i < 2; i++) {
        if (!cur || cur->type != JSON_STRING) break;
        json_t *parsed = json_parse(cur->str_val, NULL);
        json_free(cur);
        cur = parsed;
    }
    if (!cur) return NULL;
    if (cur->type != JSON_OBJECT) { json_free(cur); return NULL; }
    return cur;
}

/* messages._encode_display_metadata */
static json_t *encode_display_metadata(const json_t *dm) {
    if (!py_truthy(dm)) return NULL;
    if (dm->type == JSON_STRING) {
        json_t *parsed = json_parse(dm->str_val, NULL);
        if (!parsed) return NULL;
        json_t *r = (parsed->type == JSON_OBJECT) ? py_dumps_json(parsed, 0, 0) : NULL;
        json_free(parsed);
        return r;
    }
    if (dm->type != JSON_OBJECT) return NULL;
    return py_dumps_json(dm, 0, 0);
}

/* messages._reasoning_json_text */
static json_t *reasoning_json_text(const json_t *value) {
    if (!py_truthy(value)) return NULL;
    if (value->type == JSON_STRING) return json_copy(value);
    return py_dumps_json(value, 0, 0);
}

/* messages._loaded_view_content */
static json_t *loaded_view_content(const json_t *role, const json_t *content) {
    const char *r = j_str(role);
    if (r && (!strcmp(r, "user") || !strcmp(r, "assistant")) && j_is_str(content)) {
        char *clean = sanitize_context(content->str_val);
        char *stripped = py_strip(clean);
        json_t *out = json_string(stripped);
        free(clean);
        free(stripped);
        return out;
    }
    return json_copy(content);
}

/* json_append() drops a NULL item; Python's tuple keeps a None slot. */
static void append_val(json_t *arr, json_t *v) {
    json_append(arr, v ? v : json_null());
}

/* messages._row_identity -> a JSON array (the Python tuple, element for element) */
static json_t *row_identity(const json_t *role, const json_t *content, const json_t *tool_call_id,
                            const json_t *tool_calls) {
    json_t *arr = json_array();
    append_val(arr, json_copy(role));
    append_val(arr, encode_content(loaded_view_content(role, content)));
    append_val(arr, json_copy(tool_call_id));
    append_val(arr, py_truthy(tool_calls) ? py_dumps_json(tool_calls, 0, 0) : json_null());
    return arr;
}

/* _placeholders(n) */
static char *placeholders(size_t n) {
    if (n == 0) {
        char *e = (char *)malloc(1);
        if (!e) abort();
        e[0] = '\0';
        return e;
    }
    char *r = (char *)malloc(n * 2);
    if (!r) abort();
    for (size_t i = 0; i < n; i++) {
        r[i * 2] = '?';
        r[i * 2 + 1] = (i + 1 == n) ? '\0' : ',';
    }
    return r;
}

/* ================================================================== */
/* gate p1                                                             */
/* ================================================================== */

json_t *msg_encode_display_metadata(const json_t *display_metadata) {
    return encode_display_metadata(display_metadata);
}

json_t *msg_decode_display_metadata(const json_t *raw) {
    return decode_display_metadata(raw);
}

json_t *msg_reasoning_json_text(const json_t *value) { return reasoning_json_text(value); }

json_t *msg_decoded_repair_row(const json_t *row) {
    json_t *msg = json_object();
    json_set(msg, "role", json_copy(jg(row, "role")));
    json_set(msg, "content", decode_content(jg(row, "content")));
    static const char *const scalar_cols[] = {"tool_call_id", "tool_name", "effect_disposition",
                                              "token_count", "finish_reason"};
    for (size_t i = 0; i < sizeof scalar_cols / sizeof *scalar_cols; i++) {
        const json_t *v = jg(row, scalar_cols[i]);
        if (!j_is_null(v)) json_set(msg, scalar_cols[i], json_copy(v));
    }
    if (py_truthy(jg(row, "tool_calls")))
        json_set(msg, "tool_calls", json_or(jg(row, "tool_calls"), json_array()));
    const json_t *pmid = jg(row, "platform_message_id");
    if (!j_is_null(pmid)) {
        json_set(msg, "message_id", json_copy(pmid));
        json_set(msg, "platform_message_id", json_copy(pmid));
    }
    if (py_truthy(jg(row, "observed"))) json_set(msg, "observed", json_bool(1));
    if (py_truthy(jg(row, "_compressed_summary"))) json_set(msg, "_compressed_summary", json_bool(1));
    restore_identity_columns(row, msg);
    const json_t *api = jg(row, "api_content");
    if (!j_is_null(api)) json_set(msg, "api_content", json_copy(api));
    const json_t *dk = jg(row, "display_kind");
    if (!j_is_null(dk)) json_set(msg, "display_kind", json_copy(dk));
    const json_t *dm = jg(row, "display_metadata");
    if (!j_is_null(dm)) {
        json_t *meta = decode_display_metadata(dm);
        if (meta) json_set(msg, "display_metadata", meta);
    }
    if (j_is_str(jg(row, "role")) && !strcmp(jg(row, "role")->str_val, "assistant")) {
        static const char *const text_cols[] = {"reasoning", "reasoning_content"};
        for (size_t i = 0; i < sizeof text_cols / sizeof *text_cols; i++) {
            const json_t *v = jg(row, text_cols[i]);
            if (!j_is_null(v)) json_set(msg, text_cols[i], json_copy(v));
        }
        static const char *const json_cols[] = {"reasoning_details", "codex_reasoning_items",
                                                "codex_message_items"};
        for (size_t i = 0; i < sizeof json_cols / sizeof *json_cols; i++) {
            const json_t *v = jg(row, json_cols[i]);
            if (py_truthy(v)) {
                json_t *parsed = json_or(v, NULL);
                json_set(msg, json_cols[i], parsed ? parsed : json_null());
            }
        }
    }
    return msg;
}

/* _bump_session_counters: the statement text + its bound values (the caller
 * executes it; a null statement means "no bump"). */
json_t *msg_bump_session_counters(const json_t *session_id, const json_t *inserted,
                                  const json_t *tool_calls, const json_t *unit) {
    int is_unit = py_truthy(unit);
    long long n_ins = j_is_num(inserted) ? (long long)inserted->num_val : 0;
    long long n_tc = j_is_num(tool_calls) ? (long long)tool_calls->num_val : 0;
    const char *inc = is_unit ? "1" : "?";
    json_t *out = json_object();
    json_t *params = json_array();
    char sql[512];
    const char *verb = NULL;
    if (n_tc > 0) {
        snprintf(sql, sizeof sql,
                 "UPDATE sessions SET message_count = message_count + %s, "
                 "tool_call_count = tool_call_count + ? WHERE id = ?",
                 inc);
        verb = sql;
        if (!is_unit) json_append(params, json_number((double)n_ins));
        json_append(params, json_number((double)n_tc));
    } else if (n_ins > 0) {
        snprintf(sql, sizeof sql, "UPDATE sessions SET message_count = message_count + %s WHERE id = ?",
                 inc);
        verb = sql;
        if (!is_unit) json_append(params, json_number((double)n_ins));
    }
    if (verb) {
        json_set(out, "sql", json_string(verb));
        json_append(params, json_copy(session_id));
    } else {
        json_set(out, "sql", json_null());
    }
    json_set(out, "params", params);
    return out;
}

json_t *msg_reaction_list(const json_t *meta) {
    const json_t *reactions = j_is_obj(meta) ? jg(meta, MSG_REACTIONS_KEY) : NULL;
    json_t *out = json_array();
    if (!j_is_arr(reactions)) return out;
    for (size_t i = 0; i < reactions->c.count; i++) {
        if (j_is_obj(reactions->c.items[i])) json_append(out, json_copy(reactions->c.items[i]));
    }
    return out;
}

/* ================================================================== */
/* gate p2                                                             */
/* ================================================================== */

json_t *msg_row_identity(const json_t *role, const json_t *content, const json_t *tool_call_id,
                         const json_t *tool_calls) {
    return row_identity(role, content, tool_call_id, tool_calls);
}

json_t *msg_loaded_view_content(const json_t *role, const json_t *content) {
    return loaded_view_content(role, content);
}

/* _stamp_kept_live_prefix(live, messages) -> {kept, messages}
 * live rows are the SELECT id, role, content, tool_call_id, tool_calls,
 * message_uid rows, in id order (arrays or row objects). */
json_t *msg_stamp_kept_live_prefix(const json_t *live, const json_t *messages) {
    json_t *out_msgs = json_array();
    size_t kept = 0;
    size_t n_live = j_is_arr(live) ? live->c.count : 0;
    size_t n_msg = j_is_arr(messages) ? messages->c.count : 0;
    size_t limit = n_live < n_msg ? n_live : n_msg;
    /* messages are copied first so a break still returns them untouched */
    for (size_t i = 0; i < n_msg; i++) json_append(out_msgs, json_copy(messages->c.items[i]));
    for (size_t i = 0; i < limit; i++) {
        const json_t *row = live->c.items[i];
        json_t *msg = out_msgs->c.items[i];
        const json_t *row_role = j_is_arr(row) ? json_get(row, 1) : jg(row, "role");
        const json_t *row_id = j_is_arr(row) ? json_get(row, 0) : jg(row, "id");
        const json_t *row_content = j_is_arr(row) ? json_get(row, 2) : jg(row, "content");
        const json_t *row_tcid = j_is_arr(row) ? json_get(row, 3) : jg(row, "tool_call_id");
        const json_t *row_calls = j_is_arr(row) ? json_get(row, 4) : jg(row, "tool_calls");
        const json_t *row_uid = j_is_arr(row) ? json_get(row, 5) : jg(row, "message_uid");

        const json_t *role_v = jg(msg, "role");
        const char *role = j_is_str(role_v) ? role_v->str_val : "unknown";
        json_t *role_json = json_string(role);
        int same_role = py_eq(row_role, role_json);
        json_free(role_json);
        if (!same_role) break;
        if (!py_eq(row_tcid, jg(msg, "tool_call_id"))) break;
        json_t *msg_role = json_string(role);
        json_t *want = row_identity(msg_role, jg(msg, "content"), jg(msg, "tool_call_id"),
                                    parse_tool_calls(jg(msg, "tool_calls")));
        json_t *have = row_identity(msg_role, decode_content(row_content), row_tcid,
                                    parse_tool_calls(row_calls));
        json_free(msg_role);
        int same = py_eq(want, have);
        json_free(want);
        json_free(have);
        if (!same) break;
        json_set(msg, "_row_id", json_copy(row_id));
        if (py_truthy(row_uid)) json_set(msg, MSG_MESSAGE_UID, json_copy(row_uid));
        kept++;
    }
    json_t *out = json_object();
    json_set(out, "kept", json_number((double)kept));
    json_set(out, "messages", out_msgs);
    return out;
}

/* _clone_message_rows: the INSERT ... SELECT text + binds (column names come
 * from PRAGMA table_info(messages), i.e. from the caller). */
json_t *msg_clone_message_rows(const json_t *columns, const json_t *tail_ids,
                               const json_t *session_id) {
    int retarget = !j_is_null(session_id);
    sbuf cols = {NULL, 0, 0};
    size_t ncols = j_is_arr(columns) ? columns->c.count : 0;
    for (size_t i = 0; i < ncols; i++) {
        const char *c = j_str(json_get(columns, i));
        if (!c) continue;
        if (!strcmp(c, "id") || !strcmp(c, "active") || !strcmp(c, "compacted") ||
            !strcmp(c, "display_order"))
            continue;
        if (retarget && !strcmp(c, "session_id")) continue;
        if (cols.len) sb_puts(&cols, ", ");
        sb_puts(&cols, c);
    }
    char *col_list = sb_take(&cols);
    size_t n_ids = j_is_arr(tail_ids) ? tail_ids->c.count : 0;
    char *marks = placeholders(n_ids);
    sbuf sql = {NULL, 0, 0};
    sb_puts(&sql, "INSERT INTO messages (");
    sb_puts(&sql, col_list);
    sb_puts(&sql, retarget ? ", session_id, active, compacted) SELECT "
                           : ", active, compacted) SELECT ");
    sb_puts(&sql, col_list);
    sb_puts(&sql, retarget ? ", ?, 1, 0 FROM messages WHERE id IN ("
                           : ", 1, 0 FROM messages WHERE id IN (");
    sb_puts(&sql, marks);
    sb_puts(&sql, ") ORDER BY id");
    char *text = sb_take(&sql);
    json_t *params = json_array();
    if (retarget) json_append(params, json_copy(session_id));
    for (size_t i = 0; i < n_ids; i++) json_append(params, json_copy(json_get(tail_ids, i)));
    json_t *out = json_object();
    json_set(out, "sql", json_string(text));
    json_set(out, "params", params);
    free(col_list);
    free(marks);
    free(text);
    return out;
}

/* ================================================================== */
/* gate p3                                                             */
/* ================================================================== */

json_t *msg_is_model_only_row(const json_t *row) {
    json_t *meta = decode_display_metadata(jg(row, "display_metadata"));
    int on = meta ? py_truthy(jg(meta, MSG_MODEL_ONLY_KEY)) : 0;
    json_free(meta);
    return json_bool(on ? 1 : 0);
}

/* _fold_summary_carrier(row, key, first_by_uid) -> {key, first_by_uid} */
json_t *msg_fold_summary_carrier(const json_t *row, const json_t *key, const json_t *first_by_uid) {
    json_t *map = j_is_obj(first_by_uid) ? json_copy(first_by_uid) : json_object();
    json_t *out_key = NULL;
    const json_t *uid = jg(row, MSG_MESSAGE_UID);
    if (!py_truthy(uid)) {
        out_key = json_copy(key);
    } else if (py_truthy(jg(row, "_compressed_summary"))) {
        const json_t *first = jg(map, uid->str_val);
        out_key = first ? json_copy(first) : json_copy(key);
    } else {
        if (!jg(map, uid->str_val)) json_set(map, uid->str_val, json_copy(key));
        out_key = json_copy(key);
    }
    json_t *out = json_object();
    json_set(out, "key", out_key);
    json_set(out, "first_by_uid", map);
    return out;
}

/* _row_to_message_dict(row, *, summary_flag) */
json_t *msg_row_to_message_dict(const json_t *row, const json_t *summary_flag) {
    json_t *msg = json_copy(row);
    if (!j_is_obj(msg)) {
        json_free(msg);
        msg = json_object();
    }
    json_obj_del(msg, "display_identity");
    json_obj_del(msg, "display_order");
    if (py_truthy(summary_flag)) {
        int had = py_truthy(jg(msg, "_compressed_summary"));
        json_obj_del(msg, "_compressed_summary");
        if (had) json_set(msg, "_compressed_summary", json_bool(1));
    }
    const json_t *content = jg(msg, "content");
    json_set(msg, "content", decode_content(content));
    if (py_truthy(jg(msg, "tool_calls")))
        json_set(msg, "tool_calls", json_or(jg(msg, "tool_calls"), json_array()));
    const json_t *dm = jg(msg, "display_metadata");
    if (!j_is_null(dm)) {
        json_t *meta = decode_display_metadata(dm);
        json_set(msg, "display_metadata", meta ? meta : json_null());
    }
    return msg;
}

/* _display_rows_from_conn: the page statement + binds. */
json_t *msg_display_rows_from_conn(const json_t *session_id, const json_t *limit,
                                   const json_t *offset, const json_t *latest, const json_t *after) {
    int is_latest = py_truthy(latest);
    long long off = j_is_num(offset) ? (long long)offset->num_val : 0;
    sbuf keyset = {NULL, 0, 0};
    if (!j_is_null(after)) {
        char b[64];
        snprintf(b, sizeof b, " AND display_order %s %lld", is_latest ? "<" : ">",
                 (long long)after->num_val);
        sb_puts(&keyset, b);
    }
    char *ks = sb_take(&keyset);
    sbuf sql = {NULL, 0, 0};
    sb_puts(&sql, "WITH page AS (SELECT display_order FROM messages WHERE session_id = ? AND (active = 1 "
                  "OR compacted = 1)");
    sb_puts(&sql, MSG_DISPLAY_VISIBLE_SQL);
    sb_puts(&sql,
            " AND NOT (messages.role = 'tool' AND messages.message_uid IS NOT NULL AND EXISTS (SELECT 1 "
            "FROM messages AS original WHERE original.session_id = messages.session_id AND "
            "original.timestamp = messages.timestamp AND original.message_uid = messages.message_uid AND "
            "original.role = 'tool' AND original.active = 0 AND original.compacted = 1 AND "
            "+original.display_order < messages.display_order AND COALESCE(json_extract((CASE WHEN "
            "json_valid(original.display_metadata) THEN original.display_metadata ELSE json_object() END), "
            "'$." MSG_MODEL_ONLY_KEY "'), 0) = 0))");
    sb_puts(&sql, ks);
    sb_puts(&sql, " GROUP BY display_order ORDER BY display_order ");
    sb_puts(&sql, is_latest ? "DESC" : "ASC");
    sb_puts(&sql,
            " LIMIT ? OFFSET ?) SELECT chosen.* FROM page JOIN messages AS chosen ON chosen.id = "
            "(SELECT candidate.id FROM messages AS candidate WHERE candidate.session_id = ? AND "
            "candidate.display_order = page.display_order AND (candidate.active = 1 OR candidate.compacted = 1)");
    sb_puts(&sql, MSG_DISPLAY_VISIBLE_SQL);
    sb_puts(&sql,
            " AND NOT (candidate.role = 'tool' AND candidate.message_uid IS NOT NULL AND EXISTS (SELECT 1 "
            "FROM messages AS original WHERE original.session_id = candidate.session_id AND "
            "original.timestamp = candidate.timestamp AND original.message_uid = candidate.message_uid AND "
            "original.role = 'tool' AND original.active = 0 AND original.compacted = 1 AND "
            "+original.display_order < candidate.display_order AND COALESCE(json_extract((CASE WHEN "
            "json_valid(original.display_metadata) THEN original.display_metadata ELSE json_object() END), "
            "'$." MSG_MODEL_ONLY_KEY "'), 0) = 0)) ORDER BY candidate.active DESC, candidate.id DESC LIMIT 1) "
            "ORDER BY page.display_order ASC");
    char *text = sb_take(&sql);
    json_t *params = json_array();
    json_append(params, json_copy(session_id));
    json_append(params, j_is_null(limit) ? json_number(-1) : json_number(limit->num_val));
    json_append(params, json_number((double)off));
    json_append(params, json_copy(session_id));
    json_t *out = json_object();
    json_set(out, "sql", json_string(text));
    json_set(out, "params", params);
    free(ks);
    free(text);
    return out;
}

json_t *msg_active_clause(const json_t *include_inactive, const json_t *include_compacted) {
    if (py_truthy(include_inactive)) return json_string("");
    if (py_truthy(include_compacted)) return json_string(MSG_DISPLAY_ACTIVE_CLAUSE);
    return json_string(" AND active = 1");
}

/* _fetch_conversation_rows (with_compacted=False: the plain read). */
json_t *msg_fetch_conversation_rows(const json_t *session_ids, const json_t *active_clause,
                                    const json_t *with_session_id) {
    size_t n = j_is_arr(session_ids) ? session_ids->c.count : 0;
    char *marks = placeholders(n);
    const char *ac = j_str(active_clause);
    sbuf sql = {NULL, 0, 0};
    sb_puts(&sql, "SELECT ");
    if (py_truthy(with_session_id)) sb_puts(&sql, "session_id, ");
    sb_puts(&sql, MSG_CONVERSATION_ROW_COLUMNS);
    sb_puts(&sql, " FROM messages WHERE session_id IN (");
    sb_puts(&sql, marks);
    sb_puts(&sql, ")");
    sb_puts(&sql, ac ? ac : "");
    sb_puts(&sql, " ORDER BY id");
    char *text = sb_take(&sql);
    json_t *params = json_array();
    for (size_t i = 0; i < n; i++) json_append(params, json_copy(json_get(session_ids, i)));
    json_t *out = json_object();
    json_set(out, "sql", json_string(text));
    json_set(out, "params", params);
    free(marks);
    free(text);
    return out;
}

/* get_messages: the single-segment, non-compacted read path.  The rows come
 * from the caller's reader (holder._read_all) — the statement it must run is
 * returned alongside, so the harness compares both. */
json_t *msg_get_messages(const json_t *session_id, const json_t *include_inactive,
                         const json_t *include_compacted, const json_t *limit, const json_t *offset,
                         const json_t *latest, const json_t *after_id, const json_t *include_ancestors,
                         const json_t *rows) {
    int is_latest = py_truthy(latest);
    long long off = j_is_num(offset) ? (long long)offset->num_val : 0;
    json_t *out = json_object();
    if (!j_is_null(after_id) && (is_latest || off)) {
        json_set(out, "error", json_string("after_id is incompatible with latest/offset paging"));
        return out;
    }
    if (!j_is_null(after_id) && py_truthy(include_compacted)) {
        json_set(out, "error",
                 json_string("after_id is incompatible with include_compacted (deduped display reads use "
                             "offset paging)"));
        return out;
    }
    if (!j_is_null(after_id) && py_truthy(include_ancestors)) {
        json_set(out, "error",
                 json_string("after_id is incompatible with include_ancestors (merged-lineage reads use "
                             "offset paging)"));
        return out;
    }
    json_t *ac = msg_active_clause(include_inactive, include_compacted);
    sbuf sql = {NULL, 0, 0};
    sb_puts(&sql, "SELECT * FROM messages WHERE session_id = ?");
    sb_puts(&sql, ac->str_val);
    if (!j_is_null(after_id)) sb_puts(&sql, " AND id > ?");
    sb_puts(&sql, is_latest ? " ORDER BY id DESC" : " ORDER BY id ASC");
    json_t *params = json_array();
    json_append(params, json_copy(session_id));
    if (!j_is_null(after_id)) json_append(params, json_copy(after_id));
    if (!j_is_null(limit) || off) {
        sb_puts(&sql, " LIMIT ? OFFSET ?");
        json_append(params, j_is_null(limit) ? json_number(-1) : json_number(limit->num_val));
        json_append(params, json_number((double)off));
    }
    char *text = sb_take(&sql);
    /* rows: the reader's answer, already in id order */
    json_t *list = json_array();
    size_t n = j_is_arr(rows) ? rows->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        size_t idx = is_latest ? (n - 1 - i) : i;
        json_append(list, msg_row_to_message_dict(json_get(rows, idx), json_bool(1)));
    }
    json_set(out, "sql", json_string(text));
    json_set(out, "params", params);
    json_set(out, "messages", list);
    json_free(ac);
    free(text);
    return out;
}

/* ================================================================== */
/* gate p4                                                             */
/* ================================================================== */

/* _canonical_replayed_user_content(msg) -> {content, composite}
 * Composite carriers (a compaction handoff split off a user row) are the seam:
 * they need session_rules.turns.split_user_originated_turn. */
json_t *msg_canonical_replayed_user_content(const json_t *msg) {
    const char *role = j_str(jg(msg, "role"));
    json_t *out = json_object();
    if (!role || strcmp(role, "user") != 0) {
        json_set(out, "content", json_null());
        json_set(out, "composite", json_bool(0));
        return out;
    }
    json_set(out, "content", json_copy(jg(msg, "content")));
    json_set(out, "composite", json_bool(0));
    return out;
}

/* _exact_replayed_user_clone_key(timestamp, content) -> [ts, canonical json] */
json_t *msg_exact_replayed_user_clone_key(const json_t *timestamp, const json_t *content) {
    if (j_is_null(timestamp) || py_in_null_blank_list(content)) return NULL;
    json_t *dumped = py_dumps_json(content, 1, 1);
    json_t *out = json_array();
    json_append(out, json_copy(timestamp));
    json_append(out, dumped);
    return out;
}

/* _find_duplicate_replayed_user_message(messages, msg) -> [index, prefer] | null */
json_t *msg_find_duplicate_replayed_user_message(const json_t *messages, const json_t *msg) {
    const char *role = j_str(jg(msg, "role"));
    if (!role || strcmp(role, "user") != 0) return NULL;
    json_t *canon = msg_canonical_replayed_user_content(msg);
    const json_t *content = jg(canon, "content");
    int prefer = py_truthy(jg(canon, "composite"));
    if (py_in_null_blank_list(content)) {
        json_free(canon);
        return NULL;
    }
    size_t n = j_is_arr(messages) ? messages->c.count : 0;
    json_t *out = NULL;
    for (size_t idx = n; idx > 0; idx--) {
        const json_t *prev = json_get(messages, idx - 1);
        const char *pr = j_str(jg(prev, "role"));
        if (pr && !strcmp(pr, "user")) {
            json_t *pc = msg_canonical_replayed_user_content(prev);
            int same = py_eq(jg(pc, "content"), content);
            int prev_composite = py_truthy(jg(pc, "composite"));
            json_free(pc);
            if (same && (prefer || prev_composite || j_is_str(content))) {
                out = json_array();
                json_append(out, json_number((double)(idx - 1)));
                json_append(out, json_bool(prefer ? 1 : 0));
                break;
            }
        } else if (pr && !strcmp(pr, "assistant") &&
                   (py_truthy(jg(prev, "content")) || py_truthy(jg(prev, "tool_calls")))) {
            break;
        }
    }
    json_free(canon);
    return out;
}

/* _is_explicit_fork_child_row(session, include_reset) */
json_t *msg_is_explicit_fork_child_row(const json_t *session, const json_t *include_reset) {
    const char *source = j_str(jg(session, "source"));
    if (source && !strcmp(source, "tool")) return json_bool(1);
    const json_t *cfg_v = jg(session, "model_config");
    json_t *cfg = NULL;
    if (j_is_str(cfg_v)) {
        cfg = json_parse(cfg_v->str_val, NULL);
        if (!cfg) return json_bool(0); /* json.JSONDecodeError -> False */
    } else if (j_is_obj(cfg_v)) {
        cfg = json_copy(cfg_v);
    }
    if (!cfg || !j_is_obj(cfg)) {
        json_free(cfg);
        return json_bool(0);
    }
    const json_t *markers[3] = {jg(cfg, "_branched_from"), jg(cfg, "_delegate_from"),
                                jg(cfg, "_reset_from")};
    size_t n_markers = py_truthy(include_reset) ? 3 : 2;
    const json_t *parent = jg(session, "parent_session_id");
    int result = 0;
    if (py_truthy(parent)) {
        for (size_t i = 0; i < n_markers; i++)
            if (py_eq(parent, markers[i])) { result = 1; break; }
    } else {
        for (size_t i = 0; i < n_markers; i++)
            if (!j_is_null(markers[i])) { result = 1; break; }
    }
    json_free(cfg);
    return json_bool(result ? 1 : 0);
}
