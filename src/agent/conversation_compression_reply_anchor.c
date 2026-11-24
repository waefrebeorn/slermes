/*
 * conversation_compression_reply_anchor.c - C11 port of upstream
 * agent/conversation_compression_reply_anchor.py (gate w26_conversati_9bd9e4).
 *
 * PoP: w26_conversati_9bd9e4_same_content @ agent/conversation_compression_reply_anchor.py:_same_visible_content
 * PoP: w26_conversati_9bd9e4_tool_ids @ agent/conversation_compression_reply_anchor.py:_tool_call_ids
 * PoP: w26_conversati_9bd9e4_result_id @ agent/conversation_compression_reply_anchor.py:_tool_result_id
 * PoP: w26_conversati_9bd9e4_reused_ids @ agent/conversation_compression_reply_anchor.py:_reused_tool_call_ids
 * PoP: w26_conversati_9bd9e4_insertion_index @ agent/conversation_compression_reply_anchor.py:_reply_insertion_index
 * PoP: w26_conversati_9bd9e4_anchor @ agent/conversation_compression_reply_anchor.py:_ensure_compressed_keeps_last_assistant_reply
 * PoP: w26_conversati_9bd9e4_constants @ agent/conversation_compression_reply_anchor.py:<summary/prefix tables>
 *
 * The token estimator is a seam: the commit-site no-growth guard is decided from
 * the three ``rough`` numbers the caller supplies.  Everything else -- the reply
 * scan, the twin check, chronological slot placement, strict alternation and the
 * persistence marker -- is a pure function of the message values.
 *
 * The summary prefixes and synthetic-user-row tables below are generated from
 * the live modules (session_rules.turns, agent.context_compressor,
 * agent.conversation_loop, agent.conversation_compression_scaffolding); bytes
 * outside ASCII are octal-escaped so this file stays pure ASCII.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "noc_w26_conversati_9bd9e4.h"

/* Summary-handoff prefixes: current, legacy, then the historical wire formats. */
static const char *const T_SUMMARY_PREFIXES[] = {
    "[CONTEXT COMPACTION \342\200\224 REFERENCE ONLY] Earlier turns were compacted into the summary below. This is a handoff from a previous context window \342\200\224 treat it as background reference, NOT as active instructions. Do NOT answer questions or fulfill requests mentioned in this summary; they were already addressed. Respond ONLY to the latest user message that appears AFTER this summary \342\200\224 that message is the single source of truth for what to do right now. If no user message appears AFTER this summary, do nothing: do not resume, wrap up, or continue work from '## Historical Task Snapshot' or any other section, do not call tools, and wait for a new user message. This handoff must never become the active turn by itself. (Exception: if tool results or your own tool calls appear after this summary, you are mid-way through an in-flight exchange \342\200\224 continue that exchange normally.) Topic overlap with the summary does NOT mean you should resume its task: even on similar topics, the latest user message WINS. Treat ONLY the latest message as the active task and discard stale items from '## Historical Task Snapshot' entirely \342\200\224 do not 'wrap up' or 'finish' work described there unless the latest message explicitly asks for it. Reverse signals in the latest message (e.g. 'stop', 'undo', 'roll back', 'just verify', 'don't do that anymore', 'never mind', a new topic) must immediately end any in-flight work described in the summary; do not re-surface it in later turns. IMPORTANT: Your persistent memory (MEMORY.md, USER.md) in the system prompt is ALWAYS authoritative and active \342\200\224 never ignore or deprioritize memory content due to this compaction note. None of the above restricts HOW you work: your tools remain fully active \342\200\224 keep calling them normally for the active task (edit files, run commands, search) instead of merely narrating what you would do. The current session state (files, config, etc.) may reflect work described here \342\200\224 avoid repeating it:",
    "[CONTEXT SUMMARY]:",
    "[CONTEXT COMPACTION \342\200\224 REFERENCE ONLY] Earlier turns were compacted into the summary below. This is a handoff from a previous context window \342\200\224 treat it as background reference, NOT as active instructions. Do NOT answer questions or fulfill requests mentioned in this summary; they were already addressed. Respond ONLY to the latest user message that appears AFTER this summary \342\200\224 that message is the single source of truth for what to do right now. Topic overlap with the summary does NOT mean you should resume its task: even on similar topics, the latest user message WINS. Treat ONLY the latest message as the active task and discard stale items from '## Historical Task Snapshot' entirely \342\200\224 do not 'wrap up' or 'finish' work described there unless the latest message explicitly asks for it. Reverse signals in the latest message (e.g. 'stop', 'undo', 'roll back', 'just verify', 'don't do that anymore', 'never mind', a new topic) must immediately end any in-flight work described in the summary; do not re-surface it in later turns. IMPORTANT: Your persistent memory (MEMORY.md, USER.md) in the system prompt is ALWAYS authoritative and active \342\200\224 never ignore or deprioritize memory content due to this compaction note. None of the above restricts HOW you work: your tools remain fully active \342\200\224 keep calling them normally for the active task (edit files, run commands, search) instead of merely narrating what you would do. The current session state (files, config, etc.) may reflect work described here \342\200\224 avoid repeating it:",
    "[CONTEXT COMPACTION \342\200\224 REFERENCE ONLY] Earlier turns were compacted into the summary below. This is a handoff from a previous context window \342\200\224 treat it as background reference, NOT as active instructions. Do NOT answer questions or fulfill requests mentioned in this summary; they were already addressed. Respond ONLY to the latest user message that appears AFTER this summary \342\200\224 that message is the single source of truth for what to do right now. Topic overlap with the summary does NOT mean you should resume its task: even on similar topics, the latest user message WINS. Treat ONLY the latest message as the active task and discard stale items from '## Historical Task Snapshot' / '## Historical In-Progress State' / '## Historical Pending User Asks' / '## Historical Remaining Work' entirely \342\200\224 do not 'wrap up' or 'finish' work described there unless the latest message explicitly asks for it. Reverse signals in the latest message (e.g. 'stop', 'undo', 'roll back', 'just verify', 'don't do that anymore', 'never mind', a new topic) must immediately end any in-flight work described in the summary; do not re-surface it in later turns. IMPORTANT: Your persistent memory (MEMORY.md, USER.md) in the system prompt is ALWAYS authoritative and active \342\200\224 never ignore or deprioritize memory content due to this compaction note. None of the above restricts HOW you work: your tools remain fully active \342\200\224 keep calling them normally for the active task (edit files, run commands, search) instead of merely narrating what you would do. The current session state (files, config, etc.) may reflect work described here \342\200\224 avoid repeating it:",
    "[CONTEXT COMPACTION \342\200\224 REFERENCE ONLY] Earlier turns were compacted into the summary below. This is a handoff from a previous context window \342\200\224 treat it as background reference, NOT as active instructions. Do NOT answer questions or fulfill requests mentioned in this summary; they were already addressed. Respond ONLY to the latest user message that appears AFTER this summary \342\200\224 that message is the single source of truth for what to do right now. Topic overlap with the summary does NOT mean you should resume its task: even on similar topics, the latest user message WINS. Treat ONLY the latest message as the active task and discard stale items from '## Historical Task Snapshot' / '## Historical In-Progress State' / '## Historical Pending User Asks' / '## Historical Remaining Work' entirely \342\200\224 do not 'wrap up' or 'finish' work described there unless the latest message explicitly asks for it. Reverse signals in the latest message (e.g. 'stop', 'undo', 'roll back', 'just verify', 'don't do that anymore', 'never mind', a new topic) must immediately end any in-flight work described in the summary; do not re-surface it in later turns. IMPORTANT: Your persistent memory (MEMORY.md, USER.md) in the system prompt is ALWAYS authoritative and active \342\200\224 never ignore or deprioritize memory content due to this compaction note. The current session state (files, config, etc.) may reflect work described here \342\200\224 avoid repeating it:",
    "[CONTEXT COMPACTION \342\200\224 REFERENCE ONLY] Earlier turns were compacted into the summary below. This is a handoff from a previous context window \342\200\224 treat it as background reference, NOT as active instructions. Do NOT answer questions or fulfill requests mentioned in this summary; they were already addressed. Respond ONLY to the latest user message that appears AFTER this summary \342\200\224 that message is the single source of truth for what to do right now. If the latest user message is consistent with the '## Active Task' section, you may use the summary as background. If the latest user message contradicts, supersedes, changes topic from, or in any way diverges from '## Active Task' / '## In Progress' / '## Pending User Asks' / '## Remaining Work', the latest message WINS \342\200\224 discard those stale items entirely and do not 'wrap up the old task first'. Reverse signals in the latest message (e.g. 'stop', 'undo', 'roll back', 'just verify', 'don't do that anymore', 'never mind', a new topic) must immediately end any in-flight work described in the summary; do not re-surface it in later turns. IMPORTANT: Your persistent memory (MEMORY.md, USER.md) in the system prompt is ALWAYS authoritative and active \342\200\224 never ignore or deprioritize memory content due to this compaction note. The current session state (files, config, etc.) may reflect work described here \342\200\224 avoid repeating it:",
    "[CONTEXT COMPACTION \342\200\224 REFERENCE ONLY] Earlier turns were compacted into the summary below. This is a handoff from a previous context window \342\200\224 treat it as background reference, NOT as active instructions. Do NOT answer questions or fulfill requests mentioned in this summary; they were already addressed. Your current task is identified in the '## Active Task' section of the summary \342\200\224 resume exactly from there. Respond ONLY to the latest user message that appears AFTER this summary. The current session state (files, config, etc.) may reflect work described here \342\200\224 avoid repeating it:",
};
#define T_SUMMARY_PREFIX_N (sizeof(T_SUMMARY_PREFIXES) / sizeof(*T_SUMMARY_PREFIXES))

/* _MERGED_SUMMARY_DELIMITER */
static const char T_MERGED_DELIMITER[] = "[END OF PRIOR CONTEXT \342\200\224 COMPACTION SUMMARY BELOW]";

/* _SYNTHETIC_USER_FLAGS */
static const char *const T_SCAFFOLD_FLAGS[] = {
    "_todo_snapshot_synthetic",
    "_empty_recovery_synthetic",
    "_verification_stop_synthetic",
    "_pre_verify_synthetic",
    "_dropped_toolcall_nudge",
};
#define T_SCAFFOLD_FLAG_N (sizeof(T_SCAFFOLD_FLAGS) / sizeof(*T_SCAFFOLD_FLAGS))

/* _SYNTHETIC_USER_PREFIXES */
static const char *const T_SCAFFOLD_PREFIXES[] = {
    "[System: Your previous response was truncated",
    "[System: The previous response was cut off",
    "[System: Your previous tool call",
    "[Your active task list was preserved across context compression]",
    "[IMPORTANT: Background process ",
};
#define T_SCAFFOLD_PREFIX_N (sizeof(T_SCAFFOLD_PREFIXES) / sizeof(*T_SCAFFOLD_PREFIXES))

/* _is_synthetic_compression_user_turn: content markers (stripped, exact). */
static const char *const T_SYN_TEXT[] = {
    "Continue from the compressed conversation context above. This marker exists because no human user turn was available.",
    "Continue from the compressed conversation context above. This marker exists because the compacted transcript contained no preserved user turn.",
    "You've reached the maximum number of tool-calling iterations allowed. Please provide a final response summarizing what you've found and accomplished so far, without calling any more tools.",
    "[System: Continue now. Execute the required tool calls and only send your final answer after completing the task.]",
    "[System: Your previous response contained only internal reasoning and never produced a visible answer or tool call. Do not keep thinking. Produce your final answer as plain text now (or make the tool call you were planning).]",
    "[System: Your previous message ended the turn with a fragment that is not a usable answer. If the task is unfinished, continue it and then give the complete answer. If that fragment WAS your complete answer, send it again exactly as before.]",
    "Your previous turn indicated a tool call but none was included. Do not narrate a plan or restate intent \342\200\224 issue the actual tool call now to continue the task.",
    "You just executed tool calls but returned an empty response. Please process the tool results above and continue with the task.",
    "[System: The previous response was cut off by a network error mid-stream \342\200\224 a transport interruption, NOT a change in your capabilities. Your tools are still fully available; call them as normal and ignore any earlier claim that you lack tool access. Continue the task from where you left off. Do not restart or repeat prior text.]",
    "[System: The previous response was cut off by a network error mid-stream. Continue exactly where you left off. Do not restart or repeat prior text. Finish the answer directly.]",
    "[System: Your previous response was truncated by the output length limit. Continue exactly where you left off. Do not restart or repeat prior text. Finish the answer directly.]",
};
#define T_SYN_TEXT_N (sizeof(T_SYN_TEXT) / sizeof(*T_SYN_TEXT))

/* _is_synthetic_compression_user_turn: content prefixes. */
static const char *const T_SYN_PREFIX[] = {
    "[IMPORTANT: Background process ",
    "[Your active task list was preserved across context compression]\n",
    "[System: Your previous tool call ",
};
#define T_SYN_PREFIX_N (sizeof(T_SYN_PREFIX) / sizeof(*T_SYN_PREFIX))

/* _DB_PERSISTED_MARKER */
static const char T_DB_MARKER[] = "_db_persisted";

/* --------------------------------------------------------------- utilities */
typedef struct { char *p; size_t n, cap; } cb;

static void cb_addn(cb *b, const char *s, size_t l) {
    char *np;
    size_t c;
    if (b->n + l + 1 > b->cap) {
        c = b->cap ? b->cap : 64;
        while (c < b->n + l + 1) c *= 2;
        np = (char *)realloc(b->p, c);
        if (!np) return;
        b->p = np;
        b->cap = c;
    }
    memcpy(b->p + b->n, s, l);
    b->n += l;
    b->p[b->n] = '\0';
}

static void cb_add(cb *b, const char *s) { cb_addn(b, s ? s : "", s ? strlen(s) : 0); }

static char *cb_take(cb *b) {
    char *out;
    if (!b->p) return strdup("");
    out = b->p;
    b->p = NULL;
    b->n = b->cap = 0;
    return out;
}


static int cc_truthy(const json_t *v) {
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

static const json_t *cc_get(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT ? json_obj_get(obj, key) : NULL;
}

/* v is the string s (a missing key / a non-string is never a match). */
static int cc_is_str(const json_t *v, const char *s) {
    return v && v->type == JSON_STRING && !strcmp(v->str_val ? v->str_val : "", s);
}

/* Python '==' over the shapes a message field can hold. */
static int cc_eq(const json_t *a, const json_t *b) {
    int an = !a || a->type == JSON_NULL, bn = !b || b->type == JSON_NULL;
    size_t i, n;
    if (an || bn) return an && bn;
    if (a->type == JSON_STRING || b->type == JSON_STRING) {
        return a->type == JSON_STRING && b->type == JSON_STRING
               && !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
    }
    if ((a->type == JSON_NUMBER || a->type == JSON_BOOL)
        && (b->type == JSON_NUMBER || b->type == JSON_BOOL)) {
        double x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++) if (!cc_eq(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !cc_eq(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

/* str(v) for an f-string / str() argument. */
static char *cc_py_str(const json_t *v) {
    char tmp[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
    case JSON_BOOL:  return strdup(v->bool_val ? "True" : "False");
    case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e16) snprintf(tmp, sizeof(tmp), "%lld", (long long)d);
        else snprintf(tmp, sizeof(tmp), "%.15g", d);
        return strdup(tmp);
    }
    default: {
        char *s = json_dumps(v, 0);
        return s ? s : strdup("None");
    }
    }
}

static int cc_space_at(const char *s, size_t *len) {
    unsigned char c = (unsigned char)s[0];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') { *len = 1; return 1; }
    if (c == 0xC2 && (unsigned char)s[1] == 0xA0) { *len = 2; return 1; }
    return 0;
}

/* Python str.strip() / str.lstrip(). */
static char *cc_strip(const char *s) {
    const char *a = s ? s : "";
    const char *b;
    size_t l = 0, n;
    char *out;
    while (*a && cc_space_at(a, &l)) a += l;
    b = a + strlen(a);
    while (b > a) {
        const char *p = b - 1;
        while (p > a && ((unsigned char)*p & 0xC0) == 0x80) p--;
        if (cc_space_at(p, &l) && p + l == b) b = p;
        else break;
    }
    n = (size_t)(b - a);
    out = (char *)malloc(n + 1);
    if (!out) return strdup("");
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

static char *cc_lstrip(const char *s) {
    const char *a = s ? s : "";
    size_t l = 0;
    while (*a && cc_space_at(a, &l)) a += l;
    return strdup(a);
}

static int cc_starts_with_any(const char *text, const char *const *tbl, size_t n) {
    size_t i;
    if (!text) return 0;
    for (i = 0; i < n; i++) {
        if (!tbl[i] || !tbl[i][0]) continue;
        if (!strncmp(text, tbl[i], strlen(tbl[i]))) return 1;
    }
    return 0;
}

/* agent.conversation_compression_scaffolding._message_text */
static char *cc_msg_text(const json_t *msg) {
    const json_t *content = cc_get(msg, "content");
    cb b = {0};
    int first = 1;
    b.p = NULL;
    if (!content) return strdup("");
    if (content->type == JSON_STRING) return strdup(content->str_val ? content->str_val : "");
    if (content->type == JSON_ARRAY) {
        size_t i, n = json_len(content);
        for (i = 0; i < n; i++) {
            const json_t *part = json_get(content, i);
            const json_t *t;
            char *s;
            if (!part || part->type != JSON_OBJECT) continue;
            t = json_obj_get(part, "text");
            if (!cc_truthy(t)) t = json_obj_get(part, "content");
            if (!first) cb_add(&b, "\n");
            first = 0;
            if (!cc_truthy(t)) continue;
            s = cc_py_str(t);
            cb_add(&b, s);
            free(s);
        }
        return cb_take(&b);
    }
    return strdup("");
}

/* session_rules.turns._content_text_for_contains */
static char *cc_content_text(const json_t *content) {
    cb b = {0};
    b.p = NULL;
    if (!content) return strdup("");
    if (content->type == JSON_ARRAY) {
        size_t i, n = json_len(content);
        int first = 1;
        for (i = 0; i < n; i++) {
            const json_t *part = json_get(content, i);
            const json_t *t = NULL;
            if (part && part->type == JSON_STRING) t = part;
            else if (part && part->type == JSON_OBJECT) t = json_obj_get(part, "text");
            if (!t || t->type != JSON_STRING || !t->str_val || !t->str_val[0]) continue;
            if (!first) cb_add(&b, "\n");
            first = 0;
            cb_add(&b, t->str_val);
        }
        return cb_take(&b);
    }
    if (content->type == JSON_STRING) return strdup(content->str_val ? content->str_val : "");
    return cc_py_str(content);
}

/* session_rules.turns.classify_summary_content(content) is not None */
static int cc_is_summary_content(const json_t *content) {
    char *raw = cc_content_text(content);
    char *text = cc_lstrip(raw);
    char *delim;
    int res;
    free(raw);
    delim = strstr(text, T_MERGED_DELIMITER);
    if (delim) {
        char *after = cc_lstrip(delim + strlen(T_MERGED_DELIMITER));
        res = cc_starts_with_any(after, T_SUMMARY_PREFIXES, T_SUMMARY_PREFIX_N);
        free(after);
    } else {
        res = cc_starts_with_any(text, T_SUMMARY_PREFIXES, T_SUMMARY_PREFIX_N);
    }
    free(text);
    return res;
}

static int cc_is_summary(const json_t *msg) {
    if (!msg || msg->type != JSON_OBJECT) return 0;
    if (cc_truthy(json_obj_get(msg, "_compressed_summary"))) return 1;
    return cc_is_summary_content(json_obj_get(msg, "content"));
}

/* is_user_role_scaffolding */
static int cc_is_scaffolding(const json_t *msg) {
    char *raw, *text;
    size_t i;
    int res;
    if (!msg || msg->type != JSON_OBJECT) return 0;
    for (i = 0; i < T_SCAFFOLD_FLAG_N; i++)
        if (cc_truthy(json_obj_get(msg, T_SCAFFOLD_FLAGS[i]))) return 1;
    raw = cc_msg_text(msg);
    text = cc_strip(raw);
    free(raw);
    res = cc_starts_with_any(text, T_SCAFFOLD_PREFIXES, T_SCAFFOLD_PREFIX_N);
    free(text);
    return res;
}

/* ContextCompressor._is_synthetic_compression_user_turn */
static int cc_is_synthetic_turn(const json_t *msg) {
    char *raw, *text;
    size_t i;
    int res = 0;
    if (!msg || msg->type != JSON_OBJECT || !cc_is_str(json_obj_get(msg, "role"), "user")) return 0;
    if (cc_is_summary(msg)) return 1;
    raw = cc_content_text(json_obj_get(msg, "content"));
    text = cc_strip(raw);
    free(raw);
    for (i = 0; i < T_SYN_TEXT_N; i++)
        if (!strcmp(text, T_SYN_TEXT[i])) { res = 1; break; }
    if (!res) res = cc_starts_with_any(text, T_SYN_PREFIX, T_SYN_PREFIX_N);
    free(text);
    return res;
}

/* conversation_compression._is_real_user_message */
static int cc_is_real_user(const json_t *msg) {
    char *raw, *text;
    int res;
    if (!msg || msg->type != JSON_OBJECT) return 0;
    if (!cc_is_str(json_obj_get(msg, "role"), "user")) return 0;
    if (cc_is_scaffolding(msg)) return 0;
    raw = cc_msg_text(msg);
    text = cc_strip(raw);
    free(raw);
    res = text[0] != '\0';
    free(text);
    if (!res) return 0;
    return cc_is_synthetic_turn(msg) ? 0 : 1;
}

/* ---------------------------------------------------------- tool-call ids */
typedef struct { char **p; size_t n, cap; } ids;

static void ids_add(ids *s, const char *v) {
    size_t i;
    if (!v || !v[0]) return;
    for (i = 0; i < s->n; i++) if (!strcmp(s->p[i], v)) return;
    if (s->n == s->cap) {
        size_t c = s->cap ? s->cap * 2 : 4;
        void *np = realloc(s->p, c * sizeof(char *));
        if (!np) return;
        s->p = (char **)np;
        s->cap = c;
    }
    s->p[s->n++] = strdup(v);
}

static int ids_has(const ids *s, const char *v) {
    size_t i;
    if (!v) return 0;
    for (i = 0; i < s->n; i++) if (!strcmp(s->p[i], v)) return 1;
    return 0;
}

static int ids_overlap(const ids *a, const ids *b) {
    size_t i;
    for (i = 0; i < a->n; i++) if (ids_has(b, a->p[i])) return 1;
    return 0;
}

static int ids_equal(const ids *a, const ids *b) {
    if (a->n != b->n) return 0;
    return ids_overlap(a, b);
}

static void ids_free(ids *s) {
    size_t i;
    for (i = 0; i < s->n; i++) free(s->p[i]);
    free(s->p);
    s->p = NULL;
    s->n = s->cap = 0;
}

/* session_rules.turns.coalesce_tool_call_id */
static char *cc_coalesce_id(const json_t *tc) {
    static const char *const names[2] = {"call_id", "id"};
    size_t i;
    for (i = 0; i < 2; i++) {
        const json_t *raw = cc_get(tc, names[i]);
        char *value, *head, *cut, *stripped;
        if (!raw || raw->type != JSON_STRING) continue;
        value = cc_strip(raw->str_val);
        if (!value[0]) { free(value); continue; }
        head = strdup(value);
        cut = strchr(head, '|');
        if (cut) *cut = '\0';
        stripped = cc_strip(head);
        free(head);
        if (stripped[0]) { free(value); return stripped; }
        free(stripped);
        return value;
    }
    return strdup("");
}

/* _tool_call_ids(message) */
static void cc_tool_ids(const json_t *msg, ids *out) {
    const json_t *calls = cc_get(msg, "tool_calls");
    size_t i, n;
    if (!calls) return;
    if (calls->type != JSON_ARRAY) return; /* strings/dicts iterate to "" -> dropped */
    n = json_len(calls);
    for (i = 0; i < n; i++) {
        char *id = cc_coalesce_id(json_get(calls, i));
        ids_add(out, id);
        free(id);
    }
}

/* _tool_result_id(message) */
static char *cc_result_id(const json_t *msg) {
    json_t *holder = json_object();
    char *out;
    json_set(holder, "id", json_copy(cc_get(msg, "tool_call_id")));
    out = cc_coalesce_id(holder);
    json_free(holder);
    return out;
}

/* _row_ids(message) */
static void cc_row_ids(const json_t *msg, ids *out) {
    char *rid;
    cc_tool_ids(msg, out);
    rid = cc_result_id(msg);
    ids_add(out, rid);
    free(rid);
}

/* _reused_tool_call_ids(messages) */
static void cc_reused_ids(const json_t *messages, ids *out) {
    size_t i, j, n = messages && messages->type == JSON_ARRAY ? json_len(messages) : 0;
    ids all = {0};
    for (i = 0; i < n; i++) {
        const json_t *m = json_get(messages, i);
        ids mine = {0};
        if (!m || m->type != JSON_OBJECT) continue;
        if (!cc_is_str(json_obj_get(m, "role"), "assistant")) continue;
        cc_tool_ids(m, &mine);
        for (j = 0; j < mine.n; j++) {
            size_t k, count = 0;
            for (k = 0; k < all.n; k++) if (!strcmp(all.p[k], mine.p[j])) count++;
            if (count == 0) ids_add(&all, mine.p[j]);
            else if (count == 1) ids_add(out, mine.p[j]);
        }
        ids_free(&mine);
    }
    ids_free(&all);
}

/* _same_visible_content */
static int cc_same_content(const json_t *left, const json_t *right, const char *right_text) {
    char *raw = cc_msg_text(left);
    char *text = cc_strip(raw);
    int res;
    free(raw);
    if (text[0] || (right_text && right_text[0])) {
        res = !strcmp(text, right_text ? right_text : "");
    } else {
        res = cc_eq(cc_get(left, "content"), cc_get(right, "content"));
    }
    free(text);
    return res;
}

/* ------------------------------------------------------- slot placement */
/* _reply_insertion_index(followers, compressed, reused_ids) */
static long cc_insertion_index(const json_t *orig, size_t start, const json_t *compressed, const ids *reused) {
    size_t nf = orig && orig->type == JSON_ARRAY ? json_len(orig) : 0;
    size_t nc = compressed && compressed->type == JSON_ARRAY ? json_len(compressed) : 0;
    size_t fi;
    int any_follower = 0, ambiguous = 0;

    for (fi = start; fi < nf; fi++) {
        const json_t *f = json_get(orig, fi);
        const json_t *frole;
        char *fraw, *ftext;
        ids fids = {0}, frow = {0};
        size_t pos;

        if (!f || f->type != JSON_OBJECT) continue;
        any_follower = 1;
        frole = json_obj_get(f, "role");
        fraw = cc_msg_text(f);
        ftext = cc_strip(fraw);
        free(fraw);
        cc_tool_ids(f, &fids);
        cc_row_ids(f, &frow);
        if (ids_overlap(&frow, reused)) {
            if (!ambiguous) {
                size_t j;
                for (j = 0; j < nc; j++) {
                    const json_t *m = json_get(compressed, j);
                    ids mrow = {0};
                    if (!m || m->type != JSON_OBJECT) continue;
                    cc_row_ids(m, &mrow);
                    if (ids_overlap(&mrow, reused)) ambiguous = 1;
                    ids_free(&mrow);
                    if (ambiguous) break;
                }
            }
            ids_free(&frow);
            ids_free(&fids);
            free(ftext);
            continue;
        }
        for (pos = nc; pos-- > 0;) {
            const json_t *m = json_get(compressed, pos);
            ids mids = {0};
            if (!m || m->type != JSON_OBJECT) continue;
            if (!cc_eq(json_obj_get(m, "role"), frole)) continue;
            cc_tool_ids(m, &mids);
            if (fids.n || mids.n) {
                if (ids_equal(&mids, &fids)) {
                    ids_free(&mids);
                    ids_free(&frow);
                    ids_free(&fids);
                    free(ftext);
                    return (long)pos;
                }
                ids_free(&mids);
                continue;
            }
            ids_free(&mids);
            if (frole && frole->type == JSON_STRING && !strcmp(frole->str_val ? frole->str_val : "", "tool")) {
                char *rf = cc_result_id(f), *rm = cc_result_id(m);
                if (rf[0] || rm[0]) {
                    int same = !strcmp(rm, rf);
                    free(rf);
                    free(rm);
                    if (same) {
                        ids_free(&frow);
                        ids_free(&fids);
                        free(ftext);
                        return (long)pos;
                    }
                    continue;
                }
                free(rf);
                free(rm);
            }
            if (cc_same_content(m, f, ftext)) {
                ids_free(&frow);
                ids_free(&fids);
                free(ftext);
                return (long)pos;
            }
        }
        ids_free(&frow);
        ids_free(&fids);
        free(ftext);
    }
    if (ambiguous) return -1;
    if (any_follower && nc > 0) {
        size_t i;
        int has_real = 0;
        for (i = start; i < nf; i++) {
            const json_t *f = json_get(orig, i);
            if (cc_is_real_user(f)) { has_real = 1; break; }
        }
        if (has_real && cc_is_real_user(json_get(compressed, nc - 1))) return (long)(nc - 1);
    }
    return (long)nc;
}

/* ------------------------------------------------------------- the anchor */
static int cc_is_reply_twin(const json_t *m, const json_t *reply, const char *reply_text) {
    if (!m || m->type != JSON_OBJECT) return 0;
    if (!cc_is_str(json_obj_get(m, "role"), "assistant")) return 0;
    if (cc_truthy(json_obj_get(m, "tool_calls"))) return 0;
    return cc_same_content(m, reply, reply_text);
}

static json_t *cc_with_anchor(const json_t *compressed, size_t index, json_t *anchor) {
    json_t *out = json_array();
    size_t i, n = compressed && compressed->type == JSON_ARRAY ? json_len(compressed) : 0;
    if (index > n) index = n;
    for (i = 0; i < index; i++) json_append(out, json_copy(json_get(compressed, i)));
    json_append(out, anchor);
    for (i = index; i < n; i++) json_append(out, json_copy(json_get(compressed, i)));
    return out;
}

/* Nothing placed: the caller names a carried row, so the candidate it gets back
 * is the one it passed in, untouched. */
static json_t *cc_null_result(const json_t *compressed) {
    json_t *out = json_object();
    json_set(out, "anchor", json_null());
    json_set(out, "compressed",
             compressed && compressed->type == JSON_ARRAY ? json_copy(compressed) : json_array());
    return out;
}

json_t *w26_conversati_9bd9e4_anchor(const json_t *req) {
    const json_t *orig = cc_get(req, "original_messages");
    const json_t *comp = cc_get(req, "compressed");
    const json_t *rough = cc_get(req, "rough");
    const json_t *reply = NULL;
    char *reply_text = NULL;
    size_t n = orig && orig->type == JSON_ARRAY ? json_len(orig) : 0;
    size_t nc = comp && comp->type == JSON_ARRAY ? json_len(comp) : 0;
    long reply_pos = -1, index;
    ids reused = {0};
    json_t *anchor, *out;

    /* the latest visible assistant reply (not a tool round, not a summary) */
    {
        size_t i = n;
        while (i-- > 0) {
            const json_t *m = json_get(orig, i);
            char *raw, *text;
            if (!m || m->type != JSON_OBJECT) continue;
            if (!cc_is_str(json_obj_get(m, "role"), "assistant")) continue;
            if (cc_truthy(json_obj_get(m, "tool_calls"))) continue;
            if (cc_is_summary(m)) continue;
            raw = cc_msg_text(m);
            text = cc_strip(raw);
            free(raw);
            if (!text[0]) { free(text); continue; }
            reply = m;
            reply_text = text;
            reply_pos = (long)i;
            break;
        }
    }
    if (!reply) { free(reply_text); return cc_null_result(comp); }

    /* already carried by the candidate? */
    {
        size_t i;
        for (i = 0; i < nc; i++) {
            if (cc_is_reply_twin(json_get(comp, i), reply, reply_text)) {
                free(reply_text);
                return cc_null_result(comp);
            }
        }
    }
    anchor = json_copy(reply);
    json_obj_del(anchor, T_DB_MARKER);
    cc_reused_ids(orig, &reused);
    index = cc_insertion_index(orig, (size_t)reply_pos + 1, comp, &reused);
    ids_free(&reused);
    if (index < 0) { json_free(anchor); free(reply_text); return cc_null_result(comp); }

    /* neighbours at the seam */
    {
        long cand[2];
        int k;
        cand[0] = index - 1;
        cand[1] = index;
        for (k = 0; k < 2; k++) {
            const json_t *m;
            if (cand[k] < 0 || (size_t)cand[k] >= nc) continue;
            m = json_get(comp, (size_t)cand[k]);
            if (cc_is_reply_twin(m, reply, reply_text)
                || (m && m->type == JSON_OBJECT                    && cc_is_str(json_obj_get(m, "role"), "assistant"))) {
                json_free(anchor);
                free(reply_text);
                return cc_null_result(comp);
            }
        }
    }
    /* the commit-site no-growth guard, measured with the seam's numbers */
    {
        double rin = json_get_num(rough, "in", 0.0);
        double rout = json_get_num(rough, "out", 0.0);
        double ranch = json_get_num(rough, "anchor", 0.0);
        if (rout < rin && rin < rout + ranch) {
            json_free(anchor);
            free(reply_text);
            return cc_null_result(comp);
        }
    }
    free(reply_text);
    json_set(anchor, T_DB_MARKER, json_bool(1));
    out = json_object();
    json_set(out, "anchor", anchor);
    json_set(out, "compressed", cc_with_anchor(comp, (size_t)index, json_copy(anchor)));
    return out;
}

/* --------------------------------------------------------------- surfaces */
json_t *w26_conversati_9bd9e4_same_content(const json_t *req) {
    const json_t *left = cc_get(req, "left");
    const json_t *right = cc_get(req, "right");
    const json_t *rt = cc_get(req, "right_text");
    char *right_text = NULL;
    int res;
    if (rt && rt->type == JSON_STRING) {
        right_text = strdup(rt->str_val ? rt->str_val : "");
    } else {
        char *raw = cc_msg_text(right);
        right_text = cc_strip(raw);
        free(raw);
    }
    res = cc_same_content(left, right, right_text);
    free(right_text);
    return json_bool(res ? 1 : 0);
}

json_t *w26_conversati_9bd9e4_tool_ids(const json_t *message) {
    ids s = {0};
    json_t *out;
    size_t i, j;
    cc_tool_ids(message, &s);
    out = json_array();
    for (i = 0; i < s.n; i++) {
        for (j = i + 1; j < s.n; j++)
            if (strcmp(s.p[i], s.p[j]) > 0) { char *t = s.p[i]; s.p[i] = s.p[j]; s.p[j] = t; }
    }
    for (i = 0; i < s.n; i++) json_append(out, json_string(s.p[i]));
    ids_free(&s);
    return out;
}

json_t *w26_conversati_9bd9e4_result_id(const json_t *message) {
    char *id = cc_result_id(message);
    json_t *out = json_string(id);
    free(id);
    return out;
}

json_t *w26_conversati_9bd9e4_reused_ids(const json_t *messages) {
    ids s = {0};
    json_t *out;
    size_t i, j;
    cc_reused_ids(messages, &s);
    out = json_array();
    for (i = 0; i < s.n; i++) {
        for (j = i + 1; j < s.n; j++)
            if (strcmp(s.p[i], s.p[j]) > 0) { char *t = s.p[i]; s.p[i] = s.p[j]; s.p[j] = t; }
    }
    for (i = 0; i < s.n; i++) json_append(out, json_string(s.p[i]));
    ids_free(&s);
    return out;
}

json_t *w26_conversati_9bd9e4_insertion_index(const json_t *req) {
    const json_t *followers = cc_get(req, "followers");
    const json_t *compressed = cc_get(req, "compressed");
    const json_t *reused_in = cc_get(req, "reused_ids");
    ids reused = {0};
    long idx;
    size_t i, n = reused_in && reused_in->type == JSON_ARRAY ? json_len(reused_in) : 0;
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(reused_in, i);
        if (v && v->type == JSON_STRING) ids_add(&reused, v->str_val);
    }
    idx = cc_insertion_index(followers, 0, compressed, &reused);
    ids_free(&reused);
    if (idx < 0) return json_null();
    return json_number((double)idx);
}

json_t *w26_conversati_9bd9e4_constants(void) {
    json_t *o = json_object(), *flags = json_array();
    size_t i;
    for (i = 0; i < T_SCAFFOLD_FLAG_N; i++) json_append(flags, json_string(T_SCAFFOLD_FLAGS[i]));
    json_set(o, "db_persisted_marker", json_string(T_DB_MARKER));
    json_set(o, "summary_prefix_count", json_number((double)T_SUMMARY_PREFIX_N));
    json_set(o, "merged_summary_delimiter", json_string(T_MERGED_DELIMITER));
    json_set(o, "legacy_summary_prefix", json_string(T_SUMMARY_PREFIXES[1]));
    json_set(o, "scaffolding_flags", flags);
    json_set(o, "synthetic_turn_text_count", json_number((double)T_SYN_TEXT_N));
    return o;
}
