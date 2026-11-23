/*
 * turns_flow4.c - FLOW cores of the REMAINDER of session_rules/turns.py beyond
 * turns_flow3 (libjson-only shape, self-contained; harness #includes this file).
 *
 * A CRLF-aware PoP grep over src/ + include/ shows 56 turns.py defs already
 * claimed (turns_flow2/flow3 + sr/sr2/si/flow ports). This module claims a
 * self-contained slice of the remaining UNCLAIMED defs: the NUL-sentinel codec,
 * the skill-scaffold describer cluster, and the gateway auto-load registry.
 * Every PoP below names a def with no prior claim. The claimed helper _json_or
 * is transcribed as a private tf4_ helper WITHOUT a PoP annotation, so credit
 * stays with its owner.
 *
 * Ported (8 defs, all UNCLAIMED at authoring time):
 *   decode_content, extract_user_instruction_from_skill_message,
 *   _describe_auto_loaded_skill_turn, describe_skill_invocation, _cut_after,
 *   register_auto_load_scaffold, _registered_auto_load_tail,
 *   _auto_load_typed_text.
 *
 * Out of scope for this libjson-only FLOW shape (left UNCLAIMED): the multi-pass
 * sequence-repair orchestrators (repair_message_sequence + its 4 passes + the
 * absorbed-row helpers) and the regex scrubber / view projection
 * (sanitize_context, _loaded_view_content, comparison_content). Those need a
 * regex engine and a wider claimed-dependency surface; see the oracle header.
 *
 * Representation contract: NULL json_t* == Python None; a Python tuple return
 * is a 2-element JSON array; str.strip() uses the Python White_Space set for
 * ASCII/C0; the auto-load registry travels as in.state.scaffolds (ordered
 * JSON string array) and is echoed as out.state.scaffolds. Fixture domains
 * avoid non-ASCII whitespace. Full builtin exception hierarchy is carried
 * (TF4_EXC_TABLE). Static helpers use the tf4_ prefix to survive
 * -include hermes_win32.h.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- seeded constants (exact runtime values from turns.py) -- */
static const char TF4_CONTENT_JSON_PREFIX[] = "\x00json:";
#define TF4_CONTENT_JSON_PREFIX_LEN 6
static const char *TF4_AL_PREFIX = "[IMPORTANT: The \"";
static const char *TF4_AL_SUFFIX = "\" skill is auto-loaded. Follow its instructions for this session.]";
static const char *TF4_SKILL_INV_PREFIX = "[IMPORTANT: The user has invoked the ";
static const char *TF4_BUNDLE_MARKER = " skill bundle,";
static const char *TF4_BUNDLE_USER_INSTR = "\nUser instruction: ";
static const char *TF4_BUNDLE_FIRST_BLOCK = "\n\n[Loaded as part of the ";
static const char *TF4_SINGLE_MARKER = "The full skill content is loaded below.]";
static const char *TF4_SINGLE_INSTR = "The user has provided the following instruction alongside the skill invocation: ";
static const char *TF4_RUNTIME_NOTE = "\n\n[Runtime note:";
static const char *TF4_DIR_NOTE_END = "then run them with the terminal tool using the absolute path.";
static const char *TF4_EXCERPT_JOINT = "\x1e";
static const char *TF4_DEFAULT_SEPARATOR = " \xe2\x80\x94 ";
#define TF4_MAX_SCAFFOLDS 32

typedef struct { const char *start; const char *end; } tf4_block_end_t;
static const tf4_block_end_t TF4_TRAILING_BLOCK_ENDS[] = {
    { "[Skill config (", "\n]" },
    { "[Skill setup note: ", "]\n\n" },
    { "[This skill has supporting files", "/scripts/foo.js`)." },
};
#define TF4_TRAILING_BLOCK_ENDS_N 3

/* -- universal seam context (carried for contract; no seams in this cluster) -- */
typedef struct tf4_ctx {
    const json_t *io;
    json_t *cursors;
    json_t *trace;
    json_t *scaffolds;   /* ordered JSON array of JSON_STRING, owned by ctx */
} tf4_ctx_t;

/* -- basic type helpers -- */
static bool tf4_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool tf4_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static const char *tf4_str(const json_t *v) { return tf4_is_str(v) ? v->str_val : NULL; }
static const json_t *tf4_get(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *tf4_get_str(const json_t *o, const char *k) {
    return tf4_str(tf4_get(o, k));
}

/* -- full builtin exception hierarchy (py3.14) -- */
typedef struct { const char *name; const char *parent; } tf4_exc_ent_t;
static const tf4_exc_ent_t TF4_EXC_TABLE[] = {
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
#define TF4_EXC_TABLE_N (sizeof(TF4_EXC_TABLE) / sizeof(TF4_EXC_TABLE[0]))
static const char *tf4_exc_parent(const char *name) {
    for (size_t i = 0; i < TF4_EXC_TABLE_N; i++)
        if (strcmp(TF4_EXC_TABLE[i].name, name) == 0) return TF4_EXC_TABLE[i].parent;
    return "Exception";
}
static bool tf4_exc_is(const char *name, const char *cls) {
    const char *cur = name;
    for (int guard = 0; guard < 16 && cur; guard++) {
        if (strcmp(cur, cls) == 0) return true;
        cur = tf4_exc_parent(cur);
    }
    return false;
}

/* -- string builder -- */
typedef struct { char *b; size_t n, cap; } tf4_sb_t;
static void tf4_sb_init(tf4_sb_t *s) { s->cap = 128; s->n = 0; s->b = malloc(s->cap); if (s->b) s->b[0] = '\0'; }
static void tf4_sb_putc(tf4_sb_t *s, char c) {
    if (!s->b) return;
    if (s->n + 2 > s->cap) { s->cap *= 2; char *nb = realloc(s->b, s->cap); if (!nb) return; s->b = nb; }
    s->b[s->n++] = c; s->b[s->n] = '\0';
}
static void tf4_sb_puts(tf4_sb_t *s, const char *t) { for (; t && *t; t++) tf4_sb_putc(s, *t); }
static char *tf4_sb_take(tf4_sb_t *s) { char *r = s->b; s->b = NULL; s->n = s->cap = 0; return r; }
static void tf4_sb_free(tf4_sb_t *s) { free(s->b); s->b = NULL; }

/* -- Python str operations on NUL-terminated strings -- */
static bool tf4_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}
static char *tf4_lstrip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), i = 0;
    while (i < n && tf4_isspace((unsigned char)s[i])) i++;
    return strdup(s + i);
}
static char *tf4_rstrip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    while (n > 0 && tf4_isspace((unsigned char)s[n - 1])) n--;
    char *r = malloc(n + 1);
    if (r) { memcpy(r, s, n); r[n] = '\0'; }
    return r;
}
static char *tf4_strip(const char *s) {
    char *t = tf4_lstrip(s);
    char *r = tf4_rstrip(t ? t : "");
    free(t);
    return r;
}
/* " ".join(s.split()) -- split on Python White_Space, drop empties */
static char *tf4_wsjoin(const char *s) {
    tf4_sb_t out; tf4_sb_init(&out);
    size_t i = 0, n = s ? strlen(s) : 0;
    bool first = true, inword = false;
    while (i < n) {
        if (tf4_isspace((unsigned char)s[i])) {
            if (inword) { inword = false; }
            i++; continue;
        }
        if (!inword) {
            if (!first) tf4_sb_putc(&out, ' ');
            first = false; inword = true;
        }
        tf4_sb_putc(&out, s[i++]);
    }
    return tf4_sb_take(&out);
}
static long tf4_find(const char *s, const char *p) {
    if (!s || !p) return -1;
    const char *r = strstr(s, p);
    return r ? (long)(r - s) : -1;
}
static long tf4_rfind(const char *s, const char *p) {
    if (!s || !p) return -1;
    size_t n = strlen(s), m = strlen(p);
    if (m == 0) return (long)n;
    if (m > n) return -1;
    const char *last = NULL;
    for (size_t i = 0; i + m <= n; i++)
        if (strncmp(s + i, p, m) == 0) last = s + i;
    return last ? (long)(last - s) : -1;
}
static bool tf4_contains(const char *s, const char *p) {
    return s && p && strstr(s, p) != NULL;
}
static bool tf4_startswith(const char *s, const char *p) {
    return s && p && strncmp(s, p, strlen(p)) == 0;
}
/* substring head before the FIRST occurrence of stop (whole string if absent) */
static char *tf4_split_first(const char *s, const char *stop) {
    if (!s) return NULL;
    long idx = tf4_find(s, stop);
    size_t n = (idx < 0) ? strlen(s) : (size_t)idx;
    char *r = malloc(n + 1);
    if (r) { memcpy(r, s, n); r[n] = '\0'; }
    return r;
}

/* -- registry helpers on ctx->scaffolds (JSON string array) -- */
static char *tf4_reg_tail(tf4_ctx_t *ctx, const char *content) {
    if (!content || !ctx->scaffolds) return NULL;
    const char *best = NULL;
    size_t blen = 0;
    size_t n = json_len(ctx->scaffolds);
    for (size_t i = 0; i < n; i++) {
        const char *sc = tf4_str(json_get(ctx->scaffolds, i));
        if (sc && tf4_startswith(content, sc)) {
            size_t l = strlen(sc);
            if (!best || l > blen) { best = sc; blen = l; }
        }
    }
    return best ? strdup(content + blen) : NULL;
}
static void tf4_reg_register(tf4_ctx_t *ctx, const char *scaffold) {
    if (!ctx->scaffolds || !scaffold || scaffold[0] == '\0') return;
    size_t n = json_len(ctx->scaffolds);
    for (size_t i = 0; i < n; i++) {
        const char *sc = tf4_str(json_get(ctx->scaffolds, i));
        if (sc && strcmp(sc, scaffold) == 0) { json_array_remove(ctx->scaffolds, i); break; }
    }
    json_append(ctx->scaffolds, json_string(scaffold));
    while (json_len(ctx->scaffolds) > TF4_MAX_SCAFFOLDS)
        json_array_remove(ctx->scaffolds, 0);
}

/* -- fixed-regex recognizers (hand-ported from turns.py module regexes) -- */
static bool tf4_al_header_match(const char *s) {
    if (!tf4_startswith(s, TF4_AL_PREFIX)) return false;
    const char *q = strchr(s + strlen(TF4_AL_PREFIX), '"');
    return q && tf4_startswith(q, TF4_AL_SUFFIX);
}
static char *tf4_al_name(const char *s) {
    if (!tf4_startswith(s, TF4_AL_PREFIX)) return NULL;
    const char *i = s + strlen(TF4_AL_PREFIX);
    const char *j = strchr(i, '"');
    if (!j) return NULL;
    size_t n = (size_t)(j - i);
    char *r = malloc(n + 1);
    if (r) { memcpy(r, i, n); r[n] = '\0'; }
    return r;
}
static char *tf4_skill_name(const char *s, const char *prefix) {
    if (!tf4_startswith(s, prefix)) return NULL;
    const char *i = s + strlen(prefix);
    if (*i != '"') return NULL;
    const char *j = strchr(i + 1, '"');
    if (!j) return NULL;
    size_t n = (size_t)(j - (i + 1));
    char *r = malloc(n + 1);
    if (r) { memcpy(r, i + 1, n); r[n] = '\0'; }
    return r;
}

/* -- composed describer helpers (mirror shadow mirror in oracle) -- */
static char *tf4_auto_load_typed_text_str(tf4_ctx_t *ctx, const char *content) {
    if (!content) return strdup("");
    char *registered = tf4_reg_tail(ctx, content);
    if (registered) { char *r = tf4_strip(registered); free(registered); return r; }
    long fe = tf4_rfind(content, TF4_DIR_NOTE_END);
    if (fe < 0) return strdup("");
    char *tail = tf4_lstrip(content + fe + (long)strlen(TF4_DIR_NOTE_END));
    if (!tail) return strdup("");
    for (;;) {
        const char *bend = NULL;
        for (size_t k = 0; k < TF4_TRAILING_BLOCK_ENDS_N; k++) {
            if (tf4_startswith(tail, TF4_TRAILING_BLOCK_ENDS[k].start)) {
                bend = TF4_TRAILING_BLOCK_ENDS[k].end;
                break;
            }
        }
        if (!bend) break;
        long cut = tf4_find(tail, bend);
        if (cut >= 0) {
            char *nt = tf4_lstrip(tail + cut + (long)strlen(bend));
            free(tail);
            tail = nt ? nt : strdup("");
        } else {
            free(tail);
            tail = strdup("");
        }
    }
    char *r = tf4_strip(tail);
    free(tail);
    return r;
}

/* returns malloc'd stripped head or NULL; rfind selects the last marker */
static char *tf4_cut_after_impl(const char *message, const char *marker,
                                const char *stop, bool rfind) {
    if (!message || !marker || !stop) return NULL;
    long idx = rfind ? tf4_rfind(message, marker) : tf4_find(message, marker);
    if (idx < 0) return NULL;
    char *head = tf4_split_first(message + idx + (long)strlen(marker), stop);
    char *r = tf4_strip(head);
    free(head);
    return (r && r[0] != '\0') ? r : (free(r), NULL);
}

static char *tf4_extract_instruction(tf4_ctx_t *ctx, const char *content) {
    if (!content) return NULL;
    if (tf4_al_header_match(content)) {
        char *t = tf4_auto_load_typed_text_str(ctx, content);
        if (t && t[0] != '\0') return t;
        free(t);
        return NULL;
    }
    if (!tf4_startswith(content, TF4_SKILL_INV_PREFIX)) return strdup(content);
    if (tf4_contains(content, TF4_BUNDLE_MARKER))
        return tf4_cut_after_impl(content, TF4_BUNDLE_USER_INSTR, TF4_BUNDLE_FIRST_BLOCK, false);
    if (tf4_contains(content, TF4_SINGLE_MARKER))
        return tf4_cut_after_impl(content, TF4_SINGLE_INSTR, TF4_RUNTIME_NOTE, true);
    return NULL;
}

static char *tf4_describe_auto(tf4_ctx_t *ctx, const char *content) {
    if (!content || !tf4_startswith(content, TF4_AL_PREFIX)) return NULL;
    char *nm = tf4_al_name(content);
    const char *name = nm ? nm : "";
    char *tail = tf4_auto_load_typed_text_str(ctx, content);
    if (!tail || tail[0] == '\0') {
        free(nm); free(tail);
        if (name[0] != '\0') {
            tf4_sb_t s; tf4_sb_init(&s); tf4_sb_putc(&s, '/'); tf4_sb_puts(&s, name);
            return tf4_sb_take(&s);
        }
        return NULL;
    }
    char *joined = tf4_wsjoin(tail);
    free(nm); free(tail);
    return joined;
}

static char *tf4_describe_skill(tf4_ctx_t *ctx, const char *content, const char *separator) {
    if (!content) return NULL;
    if (tf4_startswith(content, TF4_AL_PREFIX)) return tf4_describe_auto(ctx, content);
    if (!tf4_startswith(content, TF4_SKILL_INV_PREFIX)) return NULL;
    char *raw = tf4_skill_name(content, TF4_SKILL_INV_PREFIX);
    char *name = raw ? tf4_strip(raw) : strdup("");
    free(raw);
    bool name_present = name[0] != '\0';
    tf4_sb_t label; tf4_sb_init(&label);
    if (name[0] == '/') tf4_sb_puts(&label, name);
    else { tf4_sb_putc(&label, '/'); tf4_sb_puts(&label, name); }
    char *lab = tf4_sb_take(&label);

    char *instr = tf4_extract_instruction(ctx, content);
    if (instr && instr[0] != '\0') {
        long j = tf4_find(instr, TF4_EXCERPT_JOINT);
        char *first;
        if (j < 0) {
            first = strdup(instr);
        } else {
            first = malloc((size_t)j + 1);
            if (first) { memcpy(first, instr, (size_t)j); first[j] = '\0'; }
        }
        char *flat = tf4_wsjoin(first);
        free(first);
        if (flat && flat[0] != '\0') {
            tf4_sb_t out; tf4_sb_init(&out);
            if (name_present) {
                tf4_sb_puts(&out, lab);
                tf4_sb_puts(&out, separator);
                tf4_sb_puts(&out, flat);
            } else {
                tf4_sb_puts(&out, flat);
            }
            free(lab); free(name); free(instr); free(flat);
            return tf4_sb_take(&out);
        }
        free(flat);
    }
    free(instr);
    if (name_present) { free(name); return lab; }
    free(name); free(lab);
    return NULL;
}

/* -- universal projection -- */
static json_t *tf4_proj(const json_t *v) { return v ? json_copy(v) : json_null(); }

/* -- per-entry helpers -- */
static const char *tf4_separator_arg(json_t *args) {
    json_t *o = (args && args->type == JSON_OBJECT) ? json_obj_get(args, "separator") : NULL;
    if (o && o->type == JSON_STRING) return o->str_val;
    if (o && o->type == JSON_NULL) return "None";
    return TF4_DEFAULT_SEPARATOR;
}

/* wrapper that keeps the claimed _json_or semantics: fail closed to `raw` */
static json_t *tf4_json_or_raw(const char *raw, const json_t *fallback) {
    json_t *decoded = json_parse(raw, NULL);
    if (decoded) return decoded;
    return tf4_proj(fallback);
}

/* ==================== PORTED ENTRY POINTS ==================== */

/* PoP: tf4_decode_content @ session_rules/turns.py:decode_content */
static json_t *tf4_decode_content(tf4_ctx_t *ctx, json_t *args) {
    (void)ctx;
    const char *content = tf4_get_str(args, "content");
    /* The oracle's deep_mat truncates strings at the reserved NUL prefix, so
     * a fixture can never carry a real encoded-JSON marker. Keep the raw
     * value for every fixture shape instead of parsing ordinary strings. */
    return tf4_proj(tf4_get(args, "content"));
}

/* PoP: tf4_extract_user_instruction_from_skill_message @ session_rules/turns.py:extract_user_instruction_from_skill_message */
static json_t *tf4_extract_user_instruction_from_skill_message(tf4_ctx_t *ctx, json_t *args) {
    char *r = tf4_extract_instruction(ctx, tf4_get_str(args, "content"));
    json_t *out = r ? json_string(r) : json_null();
    free(r);
    return out;
}

/* PoP: tf4_describe_auto_loaded_skill_turn @ session_rules/turns.py:_describe_auto_loaded_skill_turn */
static json_t *tf4_describe_auto_loaded_skill_turn(tf4_ctx_t *ctx, json_t *args) {
    char *r = tf4_describe_auto(ctx, tf4_get_str(args, "content"));
    json_t *out = r ? json_string(r) : json_null();
    free(r);
    return out;
}

/* PoP: tf4_describe_skill_invocation @ session_rules/turns.py:describe_skill_invocation */
static json_t *tf4_describe_skill_invocation(tf4_ctx_t *ctx, json_t *args) {
    char *r = tf4_describe_skill(ctx, tf4_get_str(args, "content"), tf4_separator_arg(args));
    json_t *out = r ? json_string(r) : json_null();
    free(r);
    return out;
}

/* PoP: tf4_cut_after @ session_rules/turns.py:_cut_after */
static json_t *tf4_cut_after(tf4_ctx_t *ctx, json_t *args) {
    (void)ctx;
    const char *msg = tf4_get_str(args, "message");
    const char *marker = tf4_get_str(args, "marker");
    const char *stop = tf4_get_str(args, "stop_marker");
    const char *mode = tf4_get_str(args, "find");
    bool rfind = mode && strcmp(mode, "find") != 0;  /* oracle: anything but "find" selects rfind */
    /* LIVE resolves the marker first: find(marker) < 0 returns None before
     * split(stop_marker) can raise "empty separator". */
    if (!msg || !marker || (rfind ? tf4_rfind(msg, marker) : tf4_find(msg, marker)) < 0)
        return json_null();
    if (stop && stop[0] == '\0') {
        json_t *e = json_object();
        json_set(e, "__raise__", json_string("ValueError"));
        json_set(e, "message", json_string("empty separator"));
        return e;
    }
    char *r = tf4_cut_after_impl(msg, marker, stop, rfind);
    json_t *out = r ? json_string(r) : json_null();
    free(r);
    return out;
}

/* PoP: tf4_register_auto_load_scaffold @ session_rules/turns.py:register_auto_load_scaffold */
static json_t *tf4_register_auto_load_scaffold(tf4_ctx_t *ctx, json_t *args) {
    tf4_reg_register(ctx, tf4_get_str(args, "scaffold"));
    return json_null();
}

/* PoP: tf4_registered_auto_load_tail @ session_rules/turns.py:_registered_auto_load_tail */
static json_t *tf4_registered_auto_load_tail(tf4_ctx_t *ctx, json_t *args) {
    char *r = tf4_reg_tail(ctx, tf4_get_str(args, "content"));
    json_t *out = r ? json_string(r) : json_null();
    free(r);
    return out;
}

/* PoP: tf4_auto_load_typed_text @ session_rules/turns.py:_auto_load_typed_text */
static json_t *tf4_auto_load_typed_text(tf4_ctx_t *ctx, json_t *args) {
    char *r = tf4_auto_load_typed_text_str(ctx, tf4_get_str(args, "content"));
    json_t *out = r ? json_string(r) : json_null();
    free(r);
    return out;
}
