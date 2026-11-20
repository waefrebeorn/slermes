/*
 * turns_flow3.c - FLOW cores of the REMAINING unclaimed pure helpers of
 * session_rules/turns.py beyond turns_flow2 (libjson-only shape).
 *
 * A CRLF-aware PoP grep over src/ + include/ shows 39 turns.py defs already
 * claimed (19 by turns_flow2.c, 20 by chat_completion_helpers_flow.c,
 * store_rules_flow2.c, store_rules_flow.c and store_identity_flow.c). This
 * module claims ONLY the remaining unclaimed string/dict helpers of the
 * "turn policy" cluster: summary detection, user-turn classification,
 * handoff unwrapping, display projection, rewind, and user-index scanning.
 * Every PoP below names a def with no prior claim. Claimed helpers this
 * cluster needs (_content_text_for_contains, _part_text, _with_part_text,
 * typed_prompt_content, withheld_prompt_text, _display_metadata_str,
 * drop_stale_api_content, copy.deepcopy) are transcribed as private tf3_
 * helpers WITHOUT a PoP annotation so credit stays with their owners.
 *
 * Ported (17 defs, all UNCLAIMED at authoring time):
 *   _starts_with_summary_prefix, classify_summary_content,
 *   _is_context_summary_content, _has_compressed_summary_metadata,
 *   _is_context_summary_message, _is_synthetic_compression_user_turn,
 *   _is_blank_user_turn, _is_actionable_user_turn, _is_real_user_turn,
 *   is_compaction_summary_message, _strip_context_summary_handoff_message,
 *   _handoff_only_content, split_user_originated_turn,
 *   user_originated_turn_view, history_before_user_originated_turn,
 *   project_compaction_message_for_display, user_indices.
 *
 * Out of scope for this libjson-only FLOW shape (left UNCLAIMED): regex-
 * dependent leaves (sanitize_context and the skill-scaffold describer
 * cluster), the global-mutable auto-load registry, the NUL-sentinel codec
 * (decode_content), and the multi-pass sequence-repair orchestrators.
 *
 * Representation contract: NULL json_t* == Python None; a raise is
 * {"__raise__": cls, "message": str(exc)}; a Python tuple return is a 2-element
 * JSON array. str.strip() uses the Python White_Space set for ASCII/C0;
 * fixture domains avoid non-ASCII whitespace and integral floats in str()
 * positions. Fixture text inputs are strings (startswith/find domains).
 * Full builtin exception hierarchy is carried (TF3_EXC_TABLE).
 * Self-contained: the harness #includes this .c directly; libjson only.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- seeded constants (exact runtime values from turns.py) -- */
static const char *TF3_SUMMARY_PREFIX = "[CONTEXT COMPACTION — REFERENCE ONLY] Earlier turns were compacted into the summary below. This is a handoff from a previous context window — treat it as background reference, NOT as active instructions. Do NOT answer questions or fulfill requests mentioned in this summary; they were already addressed. Respond ONLY to the latest user message that appears AFTER this summary — that message is the single source of truth for what to do right now. If no user message appears AFTER this summary, do nothing: do not resume, wrap up, or continue work from '## Historical Task Snapshot' or any other section, do not call tools, and wait for a new user message. This handoff must never become the active turn by itself. (Exception: if tool results or your own tool calls appear after this summary, you are mid-way through an in-flight exchange — continue that exchange normally.) Topic overlap with the summary does NOT mean you should resume its task: even on similar topics, the latest user message WINS. Treat ONLY the latest message as the active task and discard stale items from '## Historical Task Snapshot' entirely — do not 'wrap up' or 'finish' work described there unless the latest message explicitly asks for it. Reverse signals in the latest message (e.g. 'stop', 'undo', 'roll back', 'just verify', 'don't do that anymore', 'never mind', a new topic) must immediately end any in-flight work described in the summary; do not re-surface it in later turns. IMPORTANT: Your persistent memory (MEMORY.md, USER.md) in the system prompt is ALWAYS authoritative and active — never ignore or deprioritize memory content due to this compaction note. None of the above restricts HOW you work: your tools remain fully active — keep calling them normally for the active task (edit files, run commands, search) instead of merely narrating what you would do. The current session state (files, config, etc.) may reflect work described here — avoid repeating it:";
static const char *TF3_LEGACY_SUMMARY_PREFIX = "[CONTEXT SUMMARY]:";
static const char *TF3_MERGED_SUMMARY_DELIMITER = "[END OF PRIOR CONTEXT — COMPACTION SUMMARY BELOW]";
static const char *TF3_SUMMARY_END_MARKER = "--- END OF CONTEXT SUMMARY — respond to the message below, not the summary above ---";
static const char *TF3_MERGED_PRIOR_CONTEXT_HEADER = "[PRIOR CONTEXT — for reference only; not a new message]";
static const char *TF3_STEER_DISPLAY_KIND = "steer";
static const char *TF3_COMPRESSED_SUMMARY_METADATA_KEY = "_compressed_summary";
static const char *TF3_COMPRESSED_SUMMARY_HAS_USER_TURN_KEY = "_compressed_summary_has_user_turn";
static const char *TF3_MICRO_COMPACT_MARKER_KEY = "_micro_compact_marker";
static const char *TF3_CONTENT_REFUSED_DISPLAY_KIND = "refused";
static const char *TF3_WITHHELD_PROMPT = "withheld_prompt";
static const char *TF3_PRE_LLM_BLOCK_WITHHELD_PROMPT = "[Message withheld: a pre_llm_call policy hook blocked it.]";
static const char *TF3_DB_PERSISTED_MARKER = "_db_persisted";
static const char *TF3_HISTORICAL_PREFIXES[] = {
    "[CONTEXT COMPACTION — REFERENCE ONLY] Earlier turns were compacted into the summary below. This is a handoff from a previous context window — treat it as background reference, NOT as active instructions. Do NOT answer questions or fulfill requests mentioned in this summary; they were already addressed. Respond ONLY to the latest user message that appears AFTER this summary — that message is the single source of truth for what to do right now. Topic overlap with the summary does NOT mean you should resume its task: even on similar topics, the latest user message WINS. Treat ONLY the latest message as the active task and discard stale items from '## Historical Task Snapshot' entirely — do not 'wrap up' or 'finish' work described there unless the latest message explicitly asks for it. Reverse signals in the latest message (e.g. 'stop', 'undo', 'roll back', 'just verify', 'don't do that anymore', 'never mind', a new topic) must immediately end any in-flight work described in the summary; do not re-surface it in later turns. IMPORTANT: Your persistent memory (MEMORY.md, USER.md) in the system prompt is ALWAYS authoritative and active — never ignore or deprioritize memory content due to this compaction note. None of the above restricts HOW you work: your tools remain fully active — keep calling them normally for the active task (edit files, run commands, search) instead of merely narrating what you would do. The current session state (files, config, etc.) may reflect work described here — avoid repeating it:",
    "[CONTEXT COMPACTION — REFERENCE ONLY] Earlier turns were compacted into the summary below. This is a handoff from a previous context window — treat it as background reference, NOT as active instructions. Do NOT answer questions or fulfill requests mentioned in this summary; they were already addressed. Respond ONLY to the latest user message that appears AFTER this summary — that message is the single source of truth for what to do right now. Topic overlap with the summary does NOT mean you should resume its task: even on similar topics, the latest user message WINS. Treat ONLY the latest message as the active task and discard stale items from '## Historical Task Snapshot' / '## Historical In-Progress State' / '## Historical Pending User Asks' / '## Historical Remaining Work' entirely — do not 'wrap up' or 'finish' work described there unless the latest message explicitly asks for it. Reverse signals in the latest message (e.g. 'stop', 'undo', 'roll back', 'just verify', 'don't do that anymore', 'never mind', a new topic) must immediately end any in-flight work described in the summary; do not re-surface it in later turns. IMPORTANT: Your persistent memory (MEMORY.md, USER.md) in the system prompt is ALWAYS authoritative and active — never ignore or deprioritize memory content due to this compaction note. None of the above restricts HOW you work: your tools remain fully active — keep calling them normally for the active task (edit files, run commands, search) instead of merely narrating what you would do. The current session state (files, config, etc.) may reflect work described here — avoid repeating it:",
    "[CONTEXT COMPACTION — REFERENCE ONLY] Earlier turns were compacted into the summary below. This is a handoff from a previous context window — treat it as background reference, NOT as active instructions. Do NOT answer questions or fulfill requests mentioned in this summary; they were already addressed. Respond ONLY to the latest user message that appears AFTER this summary — that message is the single source of truth for what to do right now. Topic overlap with the summary does NOT mean you should resume its task: even on similar topics, the latest user message WINS. Treat ONLY the latest message as the active task and discard stale items from '## Historical Task Snapshot' / '## Historical In-Progress State' / '## Historical Pending User Asks' / '## Historical Remaining Work' entirely — do not 'wrap up' or 'finish' work described there unless the latest message explicitly asks for it. Reverse signals in the latest message (e.g. 'stop', 'undo', 'roll back', 'just verify', 'don't do that anymore', 'never mind', a new topic) must immediately end any in-flight work described in the summary; do not re-surface it in later turns. IMPORTANT: Your persistent memory (MEMORY.md, USER.md) in the system prompt is ALWAYS authoritative and active — never ignore or deprioritize memory content due to this compaction note. The current session state (files, config, etc.) may reflect work described here — avoid repeating it:",
    "[CONTEXT COMPACTION — REFERENCE ONLY] Earlier turns were compacted into the summary below. This is a handoff from a previous context window — treat it as background reference, NOT as active instructions. Do NOT answer questions or fulfill requests mentioned in this summary; they were already addressed. Respond ONLY to the latest user message that appears AFTER this summary — that message is the single source of truth for what to do right now. If the latest user message is consistent with the '## Active Task' section, you may use the summary as background. If the latest user message contradicts, supersedes, changes topic from, or in any way diverges from '## Active Task' / '## In Progress' / '## Pending User Asks' / '## Remaining Work', the latest message WINS — discard those stale items entirely and do not 'wrap up the old task first'. Reverse signals in the latest message (e.g. 'stop', 'undo', 'roll back', 'just verify', 'don't do that anymore', 'never mind', a new topic) must immediately end any in-flight work described in the summary; do not re-surface it in later turns. IMPORTANT: Your persistent memory (MEMORY.md, USER.md) in the system prompt is ALWAYS authoritative and active — never ignore or deprioritize memory content due to this compaction note. The current session state (files, config, etc.) may reflect work described here — avoid repeating it:",
    "[CONTEXT COMPACTION — REFERENCE ONLY] Earlier turns were compacted into the summary below. This is a handoff from a previous context window — treat it as background reference, NOT as active instructions. Do NOT answer questions or fulfill requests mentioned in this summary; they were already addressed. Your current task is identified in the '## Active Task' section of the summary — resume exactly from there. Respond ONLY to the latest user message that appears AFTER this summary. The current session state (files, config, etc.) may reflect work described here — avoid repeating it:",
};
#define TF3_HISTORICAL_PREFIXES_N 5
static const char *TF3_CARRIER_META_KEYS[] = { "reactions" };
#define TF3_CARRIER_META_KEYS_N 1
static const char *TF3_COMPACTION_INTERNAL_FIELDS[] = {
    "tool_calls",
    "finish_reason",
    "reasoning",
    "reasoning_content",
    "reasoning_details",
    "codex_reasoning_items",
    "codex_message_items",
};
#define TF3_COMPACTION_INTERNAL_FIELDS_N 7
static const char *TF3_SYNTH_CONTENTS[] = {
    "Continue from the compressed conversation context above. This marker exists because no human user turn was available.",
    "Continue from the compressed conversation context above. This marker exists because the compacted transcript contained no preserved user turn.",
    "You've reached the maximum number of tool-calling iterations allowed. Please provide a final response summarizing what you've found and accomplished so far, without calling any more tools.",
    "[System: Your previous response contained only internal reasoning and never produced a visible answer or tool call. Do not keep thinking. Produce your final answer as plain text now (or make the tool call you were planning).]",
    "[System: Continue now. Execute the required tool calls and only send your final answer after completing the task.]",
    "[System: Your previous message ended the turn with a fragment that is not a usable answer. If the task is unfinished, continue it and then give the complete answer. If that fragment WAS your complete answer, send it again exactly as before.]",
    "Your previous turn indicated a tool call but none was included. Do not narrate a plan or restate intent — issue the actual tool call now to continue the task.",
    "You just executed tool calls but returned an empty response. Please process the tool results above and continue with the task.",
    "[System: The previous response was cut off by a network error mid-stream — a transport interruption, NOT a change in your capabilities. Your tools are still fully available; call them as normal and ignore any earlier claim that you lack tool access. Continue the task from where you left off. Do not restart or repeat prior text.]",
    "[System: The previous response was cut off by a network error mid-stream. Continue exactly where you left off. Do not restart or repeat prior text. Finish the answer directly.]",
    "[System: Your previous response was truncated by the output length limit. Continue exactly where you left off. Do not restart or repeat prior text. Finish the answer directly.]",
};
#define TF3_SYNTH_CONTENTS_N 11
static const char *TF3_SYNTH_PREFIXES[] = {
    "[IMPORTANT: Background process ",
    "[Your active task list was preserved across context compression]\n",
    "[System: Your previous tool call ",
};
#define TF3_SYNTH_PREFIXES_N 3


/* -- universal seam context (carried for contract; no seams in this cluster) -- */
typedef struct tf3_ctx {
    const json_t *io;
    json_t *cursors;
    json_t *trace;
} tf3_ctx_t;

/* -- basic type helpers -- */
static bool tf3_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool tf3_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool tf3_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static bool tf3_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool tf3_is_list(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool tf3_is_dict(const json_t *v) { return v && v->type == JSON_OBJECT; }
static const char *tf3_str(const json_t *v) { return tf3_is_str(v) ? v->str_val : NULL; }

static bool tf3_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool tf3_eq_str(const json_t *v, const char *lit) {
    return tf3_is_str(v) && v->str_val && strcmp(v->str_val, lit) == 0;
}
static json_t *tf3_get(const json_t *o, const char *k) {
    return tf3_is_dict(o) ? json_obj_get(o, k) : NULL;
}
static const char *tf3_get_str(const json_t *o, const char *k) { return tf3_str(tf3_get(o, k)); }

/* -- full builtin exception hierarchy (py3.14) -- */
typedef struct { const char *name; const char *parent; } tf3_exc_ent_t;
static const tf3_exc_ent_t TF3_EXC_TABLE[] = {
    { "BaseException", NULL }, { "Exception", "BaseException" },
    { "TypeError", "Exception" }, { "StopIteration", "Exception" },
    { "StopAsyncIteration", "Exception" }, { "ArithmeticError", "Exception" },
    { "FloatingPointError", "ArithmeticError" }, { "OverflowError", "ArithmeticError" },
    { "ZeroDivisionError", "ArithmeticError" }, { "AssertionError", "Exception" },
    { "AttributeError", "Exception" }, { "BufferError", "Exception" },
    { "EOFError", "Exception" }, { "ImportError", "Exception" },
    { "ModuleNotFoundError", "ImportError" }, { "LookupError", "Exception" },
    { "IndexError", "LookupError" }, { "KeyError", "LookupError" },
    { "MemoryError", "Exception" }, { "NameError", "Exception" },
    { "UnboundLocalError", "NameError" }, { "OSError", "Exception" },
    { "BlockingIOError", "OSError" }, { "ChildProcessError", "OSError" },
    { "ConnectionError", "OSError" }, { "BrokenPipeError", "ConnectionError" },
    { "ConnectionAbortedError", "ConnectionError" },
    { "ConnectionRefusedError", "ConnectionError" },
    { "ConnectionResetError", "ConnectionError" }, { "FileExistsError", "OSError" },
    { "FileNotFoundError", "OSError" }, { "InterruptedError", "OSError" },
    { "IsADirectoryError", "OSError" }, { "NotADirectoryError", "OSError" },
    { "PermissionError", "OSError" }, { "ProcessLookupError", "OSError" },
    { "TimeoutError", "OSError" }, { "ReferenceError", "Exception" },
    { "RuntimeError", "Exception" }, { "NotImplementedError", "RuntimeError" },
    { "RecursionError", "RuntimeError" }, { "SyntaxError", "Exception" },
    { "IndentationError", "SyntaxError" }, { "TabError", "IndentationError" },
    { "SystemError", "Exception" }, { "ValueError", "Exception" },
    { "UnicodeError", "ValueError" }, { "UnicodeDecodeError", "UnicodeError" },
    { "UnicodeEncodeError", "UnicodeError" }, { "UnicodeTranslateError", "UnicodeError" },
    { "Warning", "Exception" }, { "DeprecationWarning", "Warning" },
    { "PendingDeprecationWarning", "Warning" }, { "RuntimeWarning", "Warning" },
    { "SyntaxWarning", "Warning" }, { "UserWarning", "Warning" },
    { "FutureWarning", "Warning" }, { "ImportWarning", "Warning" },
    { "UnicodeWarning", "Warning" }, { "BytesWarning", "Warning" },
    { "EncodingWarning", "Warning" }, { "ResourceWarning", "Warning" },
    { "ExceptionGroup", "Exception" }, { "BaseExceptionGroup", "BaseException" },
    { "GeneratorExit", "BaseException" }, { "KeyboardInterrupt", "BaseException" },
    { "SystemExit", "BaseException" },
};
#define TF3_EXC_TABLE_N (sizeof(TF3_EXC_TABLE) / sizeof(TF3_EXC_TABLE[0]))
static const char *tf3_exc_parent(const char *name) {
    for (size_t i = 0; i < TF3_EXC_TABLE_N; i++)
        if (strcmp(TF3_EXC_TABLE[i].name, name) == 0) return TF3_EXC_TABLE[i].parent;
    return "Exception";
}
static bool tf3_exc_is(const char *name, const char *cls) {
    const char *cur = name;
    for (int guard = 0; guard < 16 && cur; guard++) {
        if (strcmp(cur, cls) == 0) return true;
        cur = tf3_exc_parent(cur);
    }
    return false;
}

/* -- string builder -- */
typedef struct { char *b; size_t n, cap; } tf3_sb_t;
static void tf3_sb_init(tf3_sb_t *s) { s->cap = 128; s->n = 0; s->b = malloc(s->cap); if (s->b) s->b[0] = '\0'; }
static void tf3_sb_putc(tf3_sb_t *s, char c) {
    if (!s->b) return;
    if (s->n + 2 > s->cap) { s->cap *= 2; char *nb = realloc(s->b, s->cap); if (!nb) return; s->b = nb; }
    s->b[s->n++] = c; s->b[s->n] = '\0';
}
static void tf3_sb_puts(tf3_sb_t *s, const char *t) { for (; t && *t; t++) tf3_sb_putc(s, *t); }
static void tf3_sb_free(tf3_sb_t *s) { free(s->b); s->b = NULL; }
static char *tf3_sb_take(tf3_sb_t *s) { char *r = s->b; s->b = NULL; s->n = s->cap = 0; return r; }

/* -- Python str operations on NUL-terminated strings -- */
static bool tf3_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}
static char *tf3_lstrip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), i = 0;
    while (i < n && tf3_isspace((unsigned char)s[i])) i++;
    return strdup(s + i);
}
static char *tf3_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), i = 0, j;
    while (i < n && tf3_isspace((unsigned char)s[i])) i++;
    j = n;
    while (j > i && tf3_isspace((unsigned char)s[j - 1])) j--;
    char *r = malloc(j - i + 1);
    memcpy(r, s + i, j - i); r[j - i] = '\0'; return r;
}
static bool tf3_startswith(const char *s, const char *p) {
    return s && p && strncmp(s, p, strlen(p)) == 0;
}
static bool tf3_contains(const char *s, const char *p) { return s && p && strstr(s, p) != NULL; }
static long tf3_find(const char *s, const char *p) {
    if (!s || !p) return -1;
    const char *r = strstr(s, p); return r ? (long)(r - s) : -1;
}
static long tf3_rfind(const char *s, const char *p) {
    if (!s || !p) return -1;
    size_t n = strlen(s), m = strlen(p);
    if (m == 0) return (long)n; if (m > n) return -1;
    for (long i = (long)(n - m); i >= 0; i--) if (strncmp(s + i, p, m) == 0) return i;
    return -1;
}
/* split(s, sep, 1): fill before/after (malloc'd), return whether sep found. */
static bool tf3_split2(const char *s, const char *sep, char **before, char **after) {
    const char *r = strstr(s, sep);
    if (!r) return false;
    size_t m = strlen(sep);
    *before = strndup(s, (size_t)(r - s));
    *after = strdup(r + m);
    return true;
}
static char *tf3_replace_all(const char *s, const char *old, const char *rep) {
    if (!s) return NULL;
    if (!old || !*old) return strdup(s);
    size_t m = strlen(old), rlen = strlen(rep), n = strlen(s), cnt = 0;
    const char *p = s;
    while ((p = strstr(p, old))) { cnt++; p += m; }
    if (!cnt) return strdup(s);
    size_t grow = rlen > m ? rlen - m : 0;
    size_t cap = n + cnt * grow + 1;
    char *out = malloc(cap); size_t o = 0; const char *q = s;
    while ((p = strstr(q, old))) {
        size_t k = (size_t)(p - q);
        memcpy(out + o, q, k); o += k;
        memcpy(out + o, rep, rlen); o += rlen;
        q = p + m;
    }
    size_t tail = strlen(q); memcpy(out + o, q, tail); o += tail; out[o] = '\0';
    return out;
}
static char *tf3_concat(const char *a, const char *b) {
    if (!a) a = ""; if (!b) b = "";
    size_t na = strlen(a), nb = strlen(b);
    char *r = malloc(na + nb + 1);
    memcpy(r, a, na); memcpy(r + na, b, nb); r[na + nb] = '\0'; return r;
}
/* Python str(x) for the scalar domains exercised by these helpers. */
static char *tf3_py_str(const json_t *v) {
    if (tf3_is_null(v)) return strdup("None");
    if (tf3_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (tf3_is_num(v)) {
        double d = v->num_val; char b[64];
        if (d == floor(d) && fabs(d) < 1e16) snprintf(b, sizeof b, "%lld", (long long)d);
        else snprintf(b, sizeof b, "%.17g", d);
        return strdup(b);
    }
    if (tf3_is_str(v)) return strdup(v->str_val);
    return json_serialize(v); /* dict/list: outside fixture domain; approximate */
}

/* -- JSON building helpers -- */
static void tf3_set(json_t *o, const char *k, json_t *v) { json_set(o, k, v); }
static void tf3_del(json_t *o, const char *k) { if (tf3_is_dict(o)) json_obj_del(o, k); }
static json_t *tf3_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set_hdr(o, "__raise__", cls); json_set_hdr(o, "message", msg ? msg : ""); return o;
}

/* =====================================================================
 * PRIVATE DEPENDENCIES (claimed elsewhere; transcribed WITHOUT PoP).
 * ===================================================================== */

/* _part_text: string -> itself; dict -> its "text"; else None (NULL). */
static const json_t *tf3_part_text(const json_t *item) {
    if (tf3_is_str(item)) return item;
    if (tf3_is_dict(item)) return json_obj_get(item, "text");
    return NULL;
}
/* _with_part_text: dict copy carrying "text"; string -> the text. */
static json_t *tf3_with_part_text(const json_t *item, const char *text) {
    if (tf3_is_dict(item)) {
        json_t *o = json_copy(item);
        tf3_set(o, "text", json_string(text ? text : "")); return o;
    }
    return json_string(text ? text : "");
}
/* _content_text_for_contains */
static char *tf3_content_text(const json_t *content) {
    if (tf3_is_list(content)) {
        tf3_sb_t sb; tf3_sb_init(&sb); bool first = true;
        for (size_t i = 0; i < content->c.count; i++) {
            const json_t *t = tf3_part_text(json_get(content, i));
            if (tf3_is_str(t) && t->str_val && t->str_val[0]) {
                if (!first) tf3_sb_putc(&sb, '\n');
                tf3_sb_puts(&sb, t->str_val); first = false;
            }
        }
        return tf3_sb_take(&sb);
    }
    if (tf3_is_null(content)) return strdup("");
    if (tf3_is_str(content)) return strdup(content->str_val);
    return tf3_py_str(content);
}
/* drop_stale_api_content: msg.pop("api_content", None) */
static void tf3_drop_stale_api(json_t *msg) { tf3_del(msg, "api_content"); }
/* _display_metadata_str: message.display_metadata[key] when it is a str. */
static const char *tf3_display_metadata_str(const json_t *message, const char *key) {
    const json_t *metadata = tf3_is_dict(message) ? json_obj_get(message, "display_metadata") : NULL;
    return tf3_str(tf3_is_dict(metadata) ? json_obj_get(metadata, key) : NULL);
}
/* withheld_prompt_text */
static const char *tf3_withheld_prompt(const json_t *message) {
    return tf3_display_metadata_str(message, TF3_WITHHELD_PROMPT);
}
/* typed_prompt_content */
static json_t *tf3_typed_prompt_content(const json_t *content, const json_t *message) {
    const char *typed = tf3_withheld_prompt(message);
    if (!typed) return json_copy(content);
    if (tf3_is_str(content)) {
        char *r = tf3_replace_all(content->str_val, TF3_PRE_LLM_BLOCK_WITHHELD_PROMPT, typed);
        json_t *out = json_string(r); free(r); return out;
    }
    if (tf3_is_list(content)) {
        json_t *arr = json_array();
        for (size_t i = 0; i < content->c.count; i++) {
            const json_t *part = json_get(content, i);
            const char *pt = tf3_str(tf3_is_dict(part) ? json_obj_get(part, "text") : NULL);
            if (tf3_is_dict(part) && pt) {
                char *r = tf3_replace_all(pt, TF3_PRE_LLM_BLOCK_WITHHELD_PROMPT, typed);
                json_t *o = json_copy(part); tf3_set(o, "text", json_string(r));
                json_append(arr, o); free(r);
            } else {
                json_append(arr, json_copy(part));
            }
        }
        return arr;
    }
    return json_copy(content);
}

/* =====================================================================
 * CORE LOGIC (shared by the PoP entry points).
 * ===================================================================== */

static bool tf3_startswith_any(const char *text) {
    if (!text) return false;
    if (tf3_startswith(text, TF3_SUMMARY_PREFIX)) return true;
    if (tf3_startswith(text, TF3_LEGACY_SUMMARY_PREFIX)) return true;
    for (size_t i = 0; i < TF3_HISTORICAL_PREFIXES_N; i++)
        if (tf3_startswith(text, TF3_HISTORICAL_PREFIXES[i])) return true;
    return false;
}

/* classify_summary_content core: returns malloc'd "standalone"/"merged"/NULL. */
static char *tf3_classify_core(const json_t *content) {
    char *ct = tf3_content_text(content);
    char *text = tf3_lstrip(ct); free(ct);
    if (tf3_contains(text, TF3_MERGED_SUMMARY_DELIMITER)) {
        char *before = NULL, *after = NULL;
        tf3_split2(text, TF3_MERGED_SUMMARY_DELIMITER, &before, &after);
        free(before);
        char *al = tf3_lstrip(after); free(after);
        if (tf3_startswith_any(al)) { free(al); return strdup("merged"); }
        free(al); return NULL;
    }
    if (tf3_startswith_any(text)) { free(text); return strdup("standalone"); }
    free(text); return NULL;
}
static bool tf3_is_ctx_summary_content(const json_t *content) {
    char *c = tf3_classify_core(content); if (!c) return false; free(c); return true;
}
static bool tf3_has_compressed_meta(const json_t *message) {
    return tf3_is_dict(message) && tf3_truthy(tf3_get(message, TF3_COMPRESSED_SUMMARY_METADATA_KEY));
}
static bool tf3_is_ctx_summary_message(const json_t *message) {
    if (!tf3_is_dict(message)) return false;
    if (tf3_has_compressed_meta(message)) return true;
    return tf3_is_ctx_summary_content(tf3_get(message, "content"));
}

static bool tf3_text_in_synth_set(const char *text) {
    if (!text) return false;
    for (size_t i = 0; i < TF3_SYNTH_CONTENTS_N; i++)
        if (strcmp(text, TF3_SYNTH_CONTENTS[i]) == 0) return true;
    return false;
}
static bool tf3_is_synth(const json_t *message) {
    if (!tf3_is_dict(message) || !tf3_eq_str(tf3_get(message, "role"), "user")) return false;
    if (tf3_is_ctx_summary_message(message)) return true;
    char *ct = tf3_content_text(tf3_get(message, "content"));
    char *text = tf3_strip(ct); free(ct);
    bool hit = false;
    if (tf3_text_in_synth_set(text)) hit = true;
    else for (size_t i = 0; i < TF3_SYNTH_PREFIXES_N; i++)
        if (tf3_startswith(text, TF3_SYNTH_PREFIXES[i])) { hit = true; break; }
    free(text); return hit;
}

static bool tf3_blank_part(const json_t *part) {
    if (tf3_is_str(part)) {
        char *s = tf3_strip(part->str_val); bool b = (s[0] == '\0'); free(s); return b;
    }
    if (tf3_is_dict(part)) {
        const json_t *t = tf3_get(part, "type");
        if (tf3_eq_str(t, "text") || tf3_eq_str(t, "input_text")) {
            const char *s = tf3_str(tf3_get(part, "text"));
            if (!s) return false;
            char *st = tf3_strip(s); bool b = (st[0] == '\0'); free(st); return b;
        }
    }
    return false;
}
static bool tf3_is_blank(const json_t *message) {
    if (!tf3_is_dict(message) || !tf3_eq_str(tf3_get(message, "role"), "user")) return false;
    if (tf3_is_ctx_summary_message(message)) return false;
    const json_t *content = tf3_get(message, "content");
    if (tf3_is_null(content)) return true;
    if (tf3_is_str(content)) {
        char *s = tf3_strip(content->str_val); bool b = (s[0] == '\0'); free(s); return b;
    }
    if (!tf3_is_list(content)) return false;
    for (size_t i = 0; i < content->c.count; i++)
        if (!tf3_blank_part(json_get(content, i))) return false;
    return true; /* all([]) == True */
}
static bool tf3_is_actionable(const json_t *message) {
    if (!tf3_is_dict(message) || !tf3_eq_str(tf3_get(message, "role"), "user")) return false;
    const json_t *dk = tf3_get(message, "display_kind");
    if ((tf3_truthy(dk) && !tf3_eq_str(dk, TF3_STEER_DISPLAY_KIND)) || tf3_is_ctx_summary_message(message))
        return false;
    return !tf3_is_blank(message);
}
static bool tf3_is_real_user(const json_t *message) {
    return tf3_is_actionable(message) && !tf3_is_synth(message);
}

/* _unwrapped: copy of message with content replaced, compressed key popped. */
static json_t *tf3_unwrapped(const json_t *message, json_t *new_content) {
    json_t *o = json_copy(message);
    tf3_set(o, "content", new_content); /* takes ownership */
    tf3_del(o, TF3_COMPRESSED_SUMMARY_METADATA_KEY);
    return o;
}

/* _through_end_marker */
static char *tf3_through_end_marker(const char *text) {
    long idx = tf3_find(text, TF3_SUMMARY_END_MARKER);
    if (idx < 0) return strdup(text);
    size_t n = (size_t)idx + strlen(TF3_SUMMARY_END_MARKER);
    char *r = malloc(n + 1); memcpy(r, text, n); r[n] = '\0'; return r;
}

/* _handoff_only_content core */
static json_t *tf3_handoff_only(const json_t *content) {
    if (tf3_is_str(content)) {
        char *c = strdup(content->str_val);
        if (tf3_contains(c, TF3_MERGED_SUMMARY_DELIMITER)) {
            char *before = NULL, *after = NULL;
            tf3_split2(c, TF3_MERGED_SUMMARY_DELIMITER, &before, &after);
            char *al = tf3_lstrip(after);
            char *t = tf3_through_end_marker(al);
            free(c); free(before); free(after); free(al);
            json_t *out = json_string(t); free(t); return out;
        }
        char *t = tf3_through_end_marker(c); free(c);
        json_t *out = json_string(t); free(t); return out;
    }
    if (!tf3_is_list(content)) return json_copy(content);
    for (size_t i = 0; i < content->c.count; i++) {
        const json_t *item = json_get(content, i);
        const json_t *tx = tf3_part_text(item);
        if (!tf3_is_str(tx) || !tf3_contains(tx->str_val, TF3_MERGED_SUMMARY_DELIMITER)) continue;
        char *before = NULL, *after = NULL;
        tf3_split2(tx->str_val, TF3_MERGED_SUMMARY_DELIMITER, &before, &after); free(before);
        char *al = tf3_lstrip(after); free(after);
        char *suf = tf3_through_end_marker(al); free(al);
        if (suf[0]) { json_t *out = json_array(); json_append(out, tf3_with_part_text(item, suf)); free(suf); return out; }
        free(suf); return json_array();
    }
    json_t *projected = json_array();
    for (size_t i = 0; i < content->c.count; i++) {
        const json_t *item = json_get(content, i);
        const json_t *tx = tf3_part_text(item);
        if (!tf3_is_str(tx)) continue;
        if (tf3_contains(tx->str_val, TF3_SUMMARY_END_MARKER)) {
            long idx = tf3_find(tx->str_val, TF3_SUMMARY_END_MARKER);
            size_t n = (size_t)idx + strlen(TF3_SUMMARY_END_MARKER);
            char *joined = malloc(n + 1); memcpy(joined, tx->str_val, n); joined[n] = '\0';
            json_append(projected, tf3_with_part_text(item, joined)); free(joined);
            return projected;
        }
        json_append(projected, json_copy(item));
    }
    return projected;
}

/* _strip_context_summary_handoff_message core: returns a NEW dict or NULL. */
static json_t *tf3_strip_handoff(const json_t *message) {
    if (!tf3_is_dict(message)) return json_copy(message);
    if (!tf3_is_ctx_summary_message(message)) return json_copy(message);
    const json_t *content = tf3_get(message, "content");
    if (tf3_is_str(content)) {
        char *res = NULL; bool have = false;
        if (tf3_contains(content->str_val, TF3_MERGED_SUMMARY_DELIMITER)) {
            char *before = NULL, *after = NULL;
            tf3_split2(content->str_val, TF3_MERGED_SUMMARY_DELIMITER, &before, &after); free(after);
            res = tf3_strip(before); free(before); have = true;
            if (tf3_startswith(res, TF3_MERGED_PRIOR_CONTEXT_HEADER)) {
                char *rest = res + strlen(TF3_MERGED_PRIOR_CONTEXT_HEADER);
                char *al = tf3_lstrip(rest);
                free(res); res = al;
            }
        } else if (tf3_contains(content->str_val, TF3_SUMMARY_END_MARKER)) {
            char *before = NULL, *after = NULL;
            tf3_split2(content->str_val, TF3_SUMMARY_END_MARKER, &before, &after); free(before);
            res = tf3_lstrip(after); free(after); have = true;
        } else { res = strdup(""); have = true; }
        if (!have || !res || res[0] == '\0') { free(res); return NULL; }
        json_t *jc = json_string(res); free(res);
        return tf3_unwrapped(message, jc);
    }
    if (tf3_is_list(content)) {
        json_t *prior = json_array(); bool found = false;
        for (size_t i = 0; i < content->c.count; i++) {
            const json_t *item = json_get(content, i);
            const json_t *tx = tf3_part_text(item);
            if (tf3_is_str(tx) && tf3_contains(tx->str_val, TF3_MERGED_SUMMARY_DELIMITER)) {
                char *before = NULL, *after = NULL;
                tf3_split2(tx->str_val, TF3_MERGED_SUMMARY_DELIMITER, &before, &after); free(after);
                char *bs = tf3_strip(before); free(before);
                if (bs[0]) json_append(prior, tf3_with_part_text(item, bs));
                free(bs); found = true; break;
            }
            json_append(prior, json_copy(item));
        }
        if (!found) {
            json_free(prior);
            for (size_t i = 0; i < content->c.count; i++) {
                const json_t *item = json_get(content, i);
                const json_t *tx = tf3_part_text(item);
                if (tf3_is_str(tx) && tf3_contains(tx->str_val, TF3_SUMMARY_END_MARKER)) {
                    char *before = NULL, *after = NULL;
                    tf3_split2(tx->str_val, TF3_SUMMARY_END_MARKER, &before, &after); free(before);
                    char *rem = tf3_lstrip(after); free(after);
                    json_t *legacy = json_array();
                    if (rem[0]) json_append(legacy, tf3_with_part_text(item, rem));
                    free(rem);
                    for (size_t j = i + 1; j < content->c.count; j++)
                        json_append(legacy, json_copy(json_get(content, j)));
                    if (legacy->c.count) return tf3_unwrapped(message, legacy);
                    json_free(legacy); return NULL;
                }
            }
            return NULL;
        }
        for (size_t i = 0; i < prior->c.count; i++) {
            const json_t *item = json_get(prior, i);
            const json_t *tx = tf3_part_text(item);
            if (tf3_is_str(tx)) {
                char *ls = tf3_lstrip(tx->str_val);
                if (tf3_startswith(ls, TF3_MERGED_PRIOR_CONTEXT_HEADER)) {
                    char *rest = ls + strlen(TF3_MERGED_PRIOR_CONTEXT_HEADER);
                    char *lead = tf3_lstrip(rest);
                    if (lead[0]) {
                        json_t *rep = tf3_with_part_text(item, lead);
                        json_free(json_get(prior, i)); prior->c.items[i] = rep;
                    } else {
                        json_array_remove(prior, i);
                    }
                    free(lead); free(ls); break;
                }
                free(ls);
            }
        }
        if (prior->c.count) return tf3_unwrapped(message, prior);
        json_free(prior); return NULL;
    }
    return NULL;
}

static bool tf3_carrier_key(const char *k) {
    for (size_t i = 0; i < TF3_CARRIER_META_KEYS_N; i++)
        if (strcmp(k, TF3_CARRIER_META_KEYS[i]) == 0) return true;
    return false;
}

/* split_user_originated_turn core: fills handoff/candidate (each json_t* or NULL). */
static void tf3_split_user(const json_t *message, json_t **handoff_out, json_t **candidate_out) {
    *handoff_out = NULL; *candidate_out = NULL;
    if (!tf3_is_dict(message) || !tf3_eq_str(tf3_get(message, "role"), "user")) return;
    bool is_summary = tf3_is_ctx_summary_message(message);
    json_t *handoff = NULL;
    if (is_summary) {
        handoff = json_object();
        tf3_set(handoff, "role", json_string("user"));
        tf3_set(handoff, "content", tf3_handoff_only(tf3_get(message, "content")));
        tf3_set(handoff, TF3_COMPRESSED_SUMMARY_METADATA_KEY, json_bool(true));
        tf3_set(handoff, "display_kind", json_string("hidden"));
        if (tf3_get(message, TF3_COMPRESSED_SUMMARY_HAS_USER_TURN_KEY) != NULL)
            tf3_set(handoff, TF3_COMPRESSED_SUMMARY_HAS_USER_TURN_KEY,
                    json_bool(tf3_truthy(tf3_get(message, TF3_COMPRESSED_SUMMARY_HAS_USER_TURN_KEY))));
        if (tf3_truthy(tf3_get(message, TF3_MICRO_COMPACT_MARKER_KEY)))
            tf3_set(handoff, TF3_MICRO_COMPACT_MARKER_KEY, json_bool(true));
        const json_t *ts = tf3_get(message, "timestamp");
        if (ts && ts->type != JSON_NULL) tf3_set(handoff, "timestamp", json_copy(ts));
        tf3_drop_stale_api(handoff);
        const json_t *dk = tf3_get(message, "display_kind");
        json_t *candidate;
        if (tf3_truthy(dk) && !tf3_eq_str(dk, "hidden")) candidate = NULL;
        else candidate = tf3_strip_handoff(message);
        if (!candidate) { *handoff_out = handoff; *candidate_out = NULL; return; }
        /* fall through with candidate */
        const char *pops[] = { TF3_COMPRESSED_SUMMARY_METADATA_KEY,
                               TF3_COMPRESSED_SUMMARY_HAS_USER_TURN_KEY,
                               TF3_MICRO_COMPACT_MARKER_KEY, TF3_DB_PERSISTED_MARKER,
                               "_row_id", "display_kind", "display_metadata" };
        for (size_t i = 0; i < sizeof(pops) / sizeof(pops[0]); i++) tf3_del(candidate, pops[i]);
        const json_t *cm = tf3_get(message, "display_metadata");
        if (tf3_is_dict(cm)) {
            json_t *durable = json_object();
            for (size_t i = 0; i < cm->c.count; i++) {
                const char *k = cm->c.keys[i];
                if (tf3_carrier_key(k)) tf3_set(durable, k, json_copy(cm->c.items[i]));
            }
            if (durable->c.count) tf3_set(candidate, "display_metadata", durable);
            else json_free(durable);
        }
        tf3_drop_stale_api(candidate);
        if (!tf3_is_real_user(candidate)) {
            json_free(candidate); *handoff_out = handoff; *candidate_out = NULL; return;
        }
        *handoff_out = handoff; *candidate_out = candidate; return;
    }
    const json_t *dk = tf3_get(message, "display_kind");
    if (tf3_truthy(dk) && !tf3_eq_str(dk, TF3_STEER_DISPLAY_KIND)) return;
    json_t *candidate = json_copy(message);
    const char *pops[] = { TF3_COMPRESSED_SUMMARY_METADATA_KEY,
                           TF3_COMPRESSED_SUMMARY_HAS_USER_TURN_KEY,
                           TF3_MICRO_COMPACT_MARKER_KEY, TF3_DB_PERSISTED_MARKER,
                           "display_kind", "display_metadata" };
    for (size_t i = 0; i < sizeof(pops) / sizeof(pops[0]); i++) tf3_del(candidate, pops[i]);
    const json_t *cm = tf3_get(message, "display_metadata");
    if (tf3_is_dict(cm)) {
        json_t *durable = json_object();
        for (size_t i = 0; i < cm->c.count; i++) {
            const char *k = cm->c.keys[i];
            if (tf3_carrier_key(k)) tf3_set(durable, k, json_copy(cm->c.items[i]));
        }
        if (durable->c.count) tf3_set(candidate, "display_metadata", durable);
        else json_free(durable);
    }
    tf3_drop_stale_api(candidate);
    if (!tf3_is_real_user(candidate)) { json_free(candidate); return; }
    *candidate_out = candidate;
}

/* =====================================================================
 * PoP ENTRY POINTS (one annotation each).
 * ===================================================================== */

/* PoP: tf3_starts_with_summary_prefix @ session_rules/turns.py:_starts_with_summary_prefix */
static json_t *tf3_starts_with_summary_prefix(tf3_ctx_t *ctx, json_t *args) {
    (void)ctx; return json_bool(tf3_startswith_any(tf3_get_str(args, "text")));
}
/* PoP: tf3_classify_summary_content @ session_rules/turns.py:classify_summary_content */
static json_t *tf3_classify_summary_content(tf3_ctx_t *ctx, json_t *args) {
    (void)ctx; char *c = tf3_classify_core(tf3_get(args, "content"));
    if (!c) return json_null(); json_t *o = json_string(c); free(c); return o;
}
/* PoP: tf3_is_context_summary_content @ session_rules/turns.py:_is_context_summary_content */
static json_t *tf3_is_context_summary_content(tf3_ctx_t *ctx, json_t *args) {
    (void)ctx; return json_bool(tf3_is_ctx_summary_content(tf3_get(args, "content")));
}
/* PoP: tf3_has_compressed_summary_metadata @ session_rules/turns.py:_has_compressed_summary_metadata */
static json_t *tf3_has_compressed_summary_metadata(tf3_ctx_t *ctx, json_t *args) {
    (void)ctx; return json_bool(tf3_has_compressed_meta(tf3_get(args, "message")));
}
/* PoP: tf3_is_context_summary_message @ session_rules/turns.py:_is_context_summary_message */
static json_t *tf3_is_context_summary_message(tf3_ctx_t *ctx, json_t *args) {
    (void)ctx; return json_bool(tf3_is_ctx_summary_message(tf3_get(args, "message")));
}
/* PoP: tf3_is_synthetic_compression_user_turn @ session_rules/turns.py:_is_synthetic_compression_user_turn */
static json_t *tf3_is_synthetic_compression_user_turn(tf3_ctx_t *ctx, json_t *args) {
    (void)ctx; return json_bool(tf3_is_synth(tf3_get(args, "message")));
}
/* PoP: tf3_is_blank_user_turn @ session_rules/turns.py:_is_blank_user_turn */
static json_t *tf3_is_blank_user_turn(tf3_ctx_t *ctx, json_t *args) {
    (void)ctx; return json_bool(tf3_is_blank(tf3_get(args, "message")));
}
/* PoP: tf3_is_actionable_user_turn @ session_rules/turns.py:_is_actionable_user_turn */
static json_t *tf3_is_actionable_user_turn(tf3_ctx_t *ctx, json_t *args) {
    (void)ctx; return json_bool(tf3_is_actionable(tf3_get(args, "message")));
}
/* PoP: tf3_is_real_user_turn @ session_rules/turns.py:_is_real_user_turn */
static json_t *tf3_is_real_user_turn(tf3_ctx_t *ctx, json_t *args) {
    (void)ctx; return json_bool(tf3_is_real_user(tf3_get(args, "message")));
}
/* PoP: tf3_is_compaction_summary_message @ session_rules/turns.py:is_compaction_summary_message */
static json_t *tf3_is_compaction_summary_message(tf3_ctx_t *ctx, json_t *args) {
    (void)ctx; json_t *m = tf3_get(args, "message");
    return json_bool(tf3_is_dict(m) ? tf3_is_ctx_summary_message(m) : tf3_is_ctx_summary_content(m));
}
/* PoP: tf3_strip_context_summary_handoff_message @ session_rules/turns.py:_strip_context_summary_handoff_message */
static json_t *tf3_strip_context_summary_handoff_message(tf3_ctx_t *ctx, json_t *args) {
    (void)ctx; json_t *r = tf3_strip_handoff(tf3_get(args, "message"));
    return r ? r : json_null();
}
/* PoP: tf3_handoff_only_content @ session_rules/turns.py:_handoff_only_content */
static json_t *tf3_handoff_only_content(tf3_ctx_t *ctx, json_t *args) {
    (void)ctx; return tf3_handoff_only(tf3_get(args, "content"));
}
/* PoP: tf3_split_user_originated_turn @ session_rules/turns.py:split_user_originated_turn */
static json_t *tf3_split_user_originated_turn(tf3_ctx_t *ctx, json_t *args) {
    (void)ctx; json_t *handoff = NULL, *cand = NULL;
    tf3_split_user(tf3_get(args, "message"), &handoff, &cand);
    json_t *r = json_array();
    json_append(r, handoff ? handoff : json_null());
    json_append(r, cand ? cand : json_null());
    return r;
}
/* PoP: tf3_user_originated_turn_view @ session_rules/turns.py:user_originated_turn_view */
static json_t *tf3_user_originated_turn_view(tf3_ctx_t *ctx, json_t *args) {
    (void)ctx; json_t *handoff = NULL, *cand = NULL;
    tf3_split_user(tf3_get(args, "message"), &handoff, &cand);
    if (handoff) json_free(handoff);
    return cand ? cand : json_null();
}
/* PoP: tf3_history_before_user_originated_turn @ session_rules/turns.py:history_before_user_originated_turn */
static json_t *tf3_history_before_user_originated_turn(tf3_ctx_t *ctx, json_t *args) {
    (void)ctx; json_t *msgs = tf3_get(args, "messages");
    json_t *iv = tf3_get(args, "index");
    long idx = tf3_is_num(iv) ? (long)iv->num_val : 0;
    if (!tf3_is_list(msgs) || idx < 0 || (size_t)idx >= msgs->c.count)
        return tf3_raise("IndexError", "user turn index is outside the transcript");
    const json_t *sel = json_get(msgs, (size_t)idx);
    json_t *handoff = NULL, *cand = NULL;
    tf3_split_user(sel, &handoff, &cand);
    if (!cand) { if (handoff) json_free(handoff); return tf3_raise("ValueError", "selected row is not a user-originated turn"); }
    json_t *newc = tf3_typed_prompt_content(tf3_get(cand, "content"), sel);
    tf3_set(cand, "content", newc);
    json_t *prefix = json_array();
    for (long i = 0; i < idx; i++) json_append(prefix, json_copy(json_get(msgs, (size_t)i)));
    if (handoff) json_append(prefix, handoff);
    json_t *r = json_array(); json_append(r, prefix); json_append(r, cand); return r;
}
/* PoP: tf3_project_compaction_message_for_display @ session_rules/turns.py:project_compaction_message_for_display */
static json_t *tf3_project_compaction_message_for_display(tf3_ctx_t *ctx, json_t *args) {
    (void)ctx; json_t *m = tf3_get(args, "message");
    if (!tf3_is_dict(m)) return json_null();
    if (!tf3_is_ctx_summary_message(m)) return json_copy(m);
    json_t *p = tf3_strip_handoff(m);
    if (!p) return json_null();
    for (size_t i = 0; i < TF3_COMPACTION_INTERNAL_FIELDS_N; i++)
        tf3_del(p, TF3_COMPACTION_INTERNAL_FIELDS[i]);
    const json_t *dk = tf3_get(p, "display_kind");
    if (!tf3_eq_str(dk, TF3_CONTENT_REFUSED_DISPLAY_KIND)) tf3_del(p, "display_kind");
    return p;
}
/* PoP: tf3_user_indices @ session_rules/turns.py:user_indices */
static json_t *tf3_user_indices(tf3_ctx_t *ctx, json_t *args) {
    (void)ctx; json_t *msgs = tf3_get(args, "messages");
    json_t *r = json_array();
    if (!tf3_is_list(msgs)) return r;
    for (size_t i = 0; i < msgs->c.count; i++) {
        json_t *h = NULL, *c = NULL;
        tf3_split_user(json_get(msgs, i), &h, &c);
        if (h) json_free(h);
        if (c) { json_append(r, json_number((double)i)); json_free(c); }
    }
    return r;
}

static json_t *tf3_proj(const json_t *v) { return v ? json_copy(v) : json_null(); }
