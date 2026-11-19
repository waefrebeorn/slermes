/*
 * sqlite_messages_flow.c -- FLOW + leaf ports of
 * persistence/sqlite/conversation/messages.py (SQLiteMessageRepository).
 *
 * Self-contained, libjson-only. Every function is static (sm_*); the harness
 * #includes this file and drives sm_dispatch through the universal seam:
 *   io->call(ctx, name, args) returns a NEW json_t the CALLER frees.
 *   NULL == Python None. {"__raise__": cls, "message": msg} == an exception.
 *   args is OWNED by sm_seam (freed after the call); the stub copies it for
 *   the trace. KeyError specs carry the RAW key (string keys; an INTEGER key
 *   carries "key_kind":"int" and its decimal text); str(KeyError(k))==repr(k)
 *   is applied once at exc/ret projection (sm_exc_project). Raise specs may
 *   carry "cause" (a nested spec, the exc.__cause__ the classify seam reads)
 *   and constructor fields ("session_id"/"reason"); projections keep only
 *   __raise__/message (+batch_progress when append_batch attached it).
 *
 * Seamed collaborators (outside the ported set; the oracle installs identical
 * recording stubs):
 *   refusals.enter / refusals.exit  STRUCTURAL (no script, never raise):
 *       the _errors.py refusals() CM as a passthrough; enter {session_id},
 *       exit {session_id, exc} where exc is the str-form raise spec leaving
 *       the inner with-body BEFORE the ported _refusals conversion.
 *   _execute_transcript_write.enter / .exit  STRUCTURAL passthrough driver:
 *       enter {messages, patience_s}, the _do body runs between the events,
 *       exit {exc}; the seamed retry/pre-state restore is outside the port.
 *   Store verbs (append_message, get_messages, replace_messages,
 *   archive_and_compact, rewrite_pruned_rows, set_message_reaction,
 *   take_unseen_reactions, get_active_message_watermark, conversation_revision,
 *   get_messages_around, latest_message_row_id,
 *   get_compression_failure_cooldown_row, restore_compression_failure_cooldown_row,
 *   plan_user_turn_rewind, get_session_timeline, get_session_messages_around):
 *       {"args": [positionals after holder/conn], "kwargs": {...}} with ALL
 *       kwargs recorded as passed (None-valued included). holder() and the
 *       read-ctx CMs are silent state doubles.
 *   MUTATING verbs return a SCRIPT the port applies to its own dicts (the
 *   oracle stub applies the same script to the live dicts, then returns the
 *   live scalar): _append_messages_in_transaction {"inserted", "stamps":[per
 *   msg updates]}, replace_messages {"stamps"}, archive_and_compact
 *   {"active", "stamps"}, rewrite_pruned_rows {"rewritten", "stamps" (the
 *   replacement dicts)}, _stamp_tool_call_uids {"msg": updates, "index": the
 *   new full batch index}, _restore_row_identity {"msg": updates}. A null
 *   script applies nothing. The trace ret is the script on both sides.
 *   _parse_tool_calls {"tool_calls"} -> parsed; _active_logical_message_row
 *   {"session_id","role","message_uid"} -> row dict or null;
 *   transcript_row_snapshot {"row"} -> str; message_uid_or_none {"msg"} ->
 *   uid or null; classify_persistence_error {"cause": str-form spec or null}
 *   -> the bucket string (claimed sibling); from_openai_message {"message"}
 *   -> a ConversationMessage FIELD dict (claimed sibling; the oracle
 *   materializes the record, the port projects all 33 fields with defaults);
 *   plan_user_turn_rewind's ret is materialized as the contract RewindPlan
 *   (all 10 fields required, unknown keys refused); conn.execute
 *   {"sql","params"} -> the fetchone() row as a KEYED dict or null.
 *
 * Decision chains transcribed statement-for-statement: _message_dict /
 * _held_dict insertion order and truthiness-vs-None gates, _pruned_original's
 * filtered-before + held-over merge, _transcript_row's fields-first then
 * id/session_id KeyError order and `is True` identity (JSON_BOOL only),
 * _message_record's reactions-strip + `or None` and sidecar json.loads,
 * _stamp's stored[row_id] int-key KeyError and the message_uid `or` chain,
 * _row_outcomes' fresh-set (descending unique ids, [:count]) then per-row
 * disposition, append_batch's chunking + shared tool-uid index + BaseException
 * batch_progress attachment, _append_chunk's restored-row repair, compact's
 * kwarg evaluation order (carried `or None`, held_for_cas empty-list stays),
 * replace_transcript's kept-any AFTER the stamps, read_compression_cooldown's
 * composed snapshot call, restore_compression_cooldown's RuntimeError ->
 * CooldownRestoreRefused with the PersistenceError identity passthrough, and
 * _refusals' PersistenceError -> classify -> SessionRowMissing / RuntimeError
 * -> _REWIND_REFUSALS -> RewindRefused conversions. BaseException passthrough
 * at every except site (the catches match the PersistenceError / RuntimeError
 * families only; the full builtin + persistence parent table below decides).
 *
 * Fixture domain (ASCII, shared with the oracle/generator): record specs are
 * VALID post-__post_init__ (the claimed validators are outside the ported
 * set); construction mechanics (required/unknown kwargs) are reproduced.
 * JSON text cells are canonical strict JSON (sm_json_loads validates the
 * Python grammar BEFORE libjson's lenient parser; no NaN/Infinity, lone
 * surrogates, leading zeros, raw controls, invalid escapes, trailing commas;
 * depth <= 32). int()/float() strings use ASCII whitespace and finite values.
 * stored maps carry decimal-string keys for the int row ids. tool_calls /
 * display_metadata / tool_call_uids cells are lists/objects or falsy/null.
 * SessionId/MessageId are NewType (identity); records project to field-order
 * dicts with enums as .value; tuples as lists. Fail-closed: a missing seam
 * channel or a missing/exhausted script raises RuntimeError through the same
 * channel.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

typedef struct sm_io {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} sm_io_t;

/* -- Python value semantics -- */

static bool sm_truth(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static bool sm_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

/* copy-or-None: a missing key must emit null, not a NULL pointer. */
static json_t *sm_cp(const json_t *v) { return v ? json_copy(v) : json_null(); }

static const char *sm_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER:
            return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* str() of a scalar for error messages (None/bool/int/float/str). */
static void sm_py_str(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    switch (v->type) {
        case JSON_BOOL: snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val) {
                snprintf(buf, cap, "%.0f", v->num_val);
            } else {
                for (int prec = 1; prec <= 17; prec++) {
                    snprintf(buf, cap, "%.*g", prec, v->num_val);
                    if (strtod(buf, NULL) == v->num_val) break;
                }
            }
            return;
        case JSON_STRING: snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return;
        default: snprintf(buf, cap, "<%s>", sm_typename(v)); return;
    }
}

/* repr() of a string (KeyError keys, int()/float() ValueError text). */
static char *sm_py_repr(const char *s) {
    if (!s) s = "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char quote = (has_sq && !has_dq) ? '"' : '\'';
    size_t n = strlen(s);
    char *out = (char *)malloc(n * 4 + 3);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = quote;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == (unsigned char)quote || ch == '\\') { out[o++] = '\\'; out[o++] = (char)ch; }
        else if (ch == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (ch == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (ch == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (ch < 0x20 || ch == 0x7f) { o += (size_t)sprintf(out + o, "\\x%02x", ch); }
        else out[o++] = (char)ch;
    }
    out[o++] = quote;
    out[o] = '\0';
    return out;
}

/* -- raise specs -- */

static void sm_raise(json_t **out, const char *cls, const char *msg) {
    if (!out || *out) return;
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "message", json_string(msg ? msg : ""));
    *out = r;
}

static void sm_raise_fmt(json_t **out, const char *cls, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    sm_raise(out, cls, buf);
}

/* KeyError with a STRING key: the raw key; repr applied at projection. */
static void sm_raise_key(json_t **out, const char *key) {
    sm_raise(out, "KeyError", key);
}

/* KeyError with an INT key: str(KeyError(n)) == the decimal text (no quotes). */
static void sm_raise_key_int(json_t **out, double n) {
    if (!out || *out) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "%.0f", n);
    json_t *r = json_object();
    json_set(r, "__raise__", json_string("KeyError"));
    json_set(r, "message", json_string(buf));
    json_set(r, "key_kind", json_string("int"));
    *out = r;
}

static bool sm_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *sm_kind(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}

/* -- the FULL exception parent table (builtins + the persistence family) --
 * Two parents per entry carry the multiple-inheritance refusals
 * (Conflict+RuntimeError, PersistenceError+ValueError, ...). A kind is
 * "base" (never caught by except Exception-family sites) when its chain
 * does not reach Exception; unknown scripted kinds are Exception subclasses
 * (the oracle builds type(kind, (Exception,))). */
typedef struct { const char *name; const char *p1; const char *p2; } sm_exc_entry_t;

static const sm_exc_entry_t SM_EXC_TABLE[] = {
    /* builtins (the full 3.14 hierarchy) */
    {"BaseException", NULL, NULL},
    {"SystemExit", "BaseException", NULL},
    {"KeyboardInterrupt", "BaseException", NULL},
    {"GeneratorExit", "BaseException", NULL},
    {"CancelledError", "BaseException", NULL},
    {"BaseExceptionGroup", "BaseException", NULL},
    {"Exception", "BaseException", NULL},
    {"ExceptionGroup", "Exception", NULL},
    {"TypeError", "Exception", NULL},
    {"StopAsyncIteration", "Exception", NULL},
    {"StopIteration", "Exception", NULL},
    {"ArithmeticError", "Exception", NULL},
    {"FloatingPointError", "ArithmeticError", NULL},
    {"OverflowError", "ArithmeticError", NULL},
    {"ZeroDivisionError", "ArithmeticError", NULL},
    {"AssertionError", "Exception", NULL},
    {"AttributeError", "Exception", NULL},
    {"BufferError", "Exception", NULL},
    {"EOFError", "Exception", NULL},
    {"ImportError", "Exception", NULL},
    {"ModuleNotFoundError", "ImportError", NULL},
    {"LookupError", "Exception", NULL},
    {"IndexError", "LookupError", NULL},
    {"KeyError", "LookupError", NULL},
    {"MemoryError", "Exception", NULL},
    {"NameError", "Exception", NULL},
    {"UnboundLocalError", "NameError", NULL},
    {"OSError", "Exception", NULL},
    {"BlockingIOError", "OSError", NULL},
    {"ChildProcessError", "OSError", NULL},
    {"ConnectionError", "OSError", NULL},
    {"BrokenPipeError", "ConnectionError", NULL},
    {"ConnectionAbortedError", "ConnectionError", NULL},
    {"ConnectionRefusedError", "ConnectionError", NULL},
    {"ConnectionResetError", "ConnectionError", NULL},
    {"FileExistsError", "OSError", NULL},
    {"FileNotFoundError", "OSError", NULL},
    {"InterruptedError", "OSError", NULL},
    {"IsADirectoryError", "OSError", NULL},
    {"NotADirectoryError", "OSError", NULL},
    {"PermissionError", "OSError", NULL},
    {"ProcessLookupError", "OSError", NULL},
    {"TimeoutError", "OSError", NULL},
    {"ReferenceError", "Exception", NULL},
    {"RuntimeError", "Exception", NULL},
    {"NotImplementedError", "RuntimeError", NULL},
    {"RecursionError", "RuntimeError", NULL},
    {"SyntaxError", "Exception", NULL},
    {"IndentationError", "SyntaxError", NULL},
    {"TabError", "IndentationError", NULL},
    {"SystemError", "Exception", NULL},
    {"ValueError", "Exception", NULL},
    {"UnicodeError", "ValueError", NULL},
    {"UnicodeEncodeError", "UnicodeError", NULL},
    {"UnicodeDecodeError", "UnicodeError", NULL},
    {"UnicodeTranslateError", "UnicodeError", NULL},
    {"Warning", "Exception", NULL},
    {"DeprecationWarning", "Warning", NULL},
    {"PendingDeprecationWarning", "Warning", NULL},
    {"RuntimeWarning", "Warning", NULL},
    {"SyntaxWarning", "Warning", NULL},
    {"UserWarning", "Warning", NULL},
    {"FutureWarning", "Warning", NULL},
    {"ImportWarning", "Warning", NULL},
    {"UnicodeWarning", "Warning", NULL},
    {"BytesWarning", "Warning", NULL},
    {"EncodingWarning", "Warning", NULL},
    {"ResourceWarning", "Warning", NULL},
    /* the persistence family (persistence/core/errors.py + the contract) */
    {"PersistenceError", "Exception", NULL},
    {"NotFound", "PersistenceError", NULL},
    {"SessionRowMissing", "NotFound", NULL},
    {"Conflict", "PersistenceError", NULL},
    {"_SessionConflict", "Conflict", NULL},
    {"CompressionClosed", "Conflict", "RuntimeError"},
    {"CompressionBusy", "_SessionConflict", "RuntimeError"},
    {"CompressionInProgress", "CompressionBusy", NULL},
    {"TurnLeaseLost", "_SessionConflict", "RuntimeError"},
    {"SessionWriteGuarded", "_SessionConflict", "RuntimeError"},
    {"RewindRefused", "Conflict", "RuntimeError"},
    {"HeldRowRewritten", "_SessionConflict", "RuntimeError"},
    {"PruneRowUnresolved", "_SessionConflict", "RuntimeError"},
    {"PruneRowStale", "PruneRowUnresolved", NULL},
    {"CooldownRestoreRefused", "_SessionConflict", "RuntimeError"},
    {"CompressionPublishRefused", "_SessionConflict", "RuntimeError"},
    {"SessionLedgered", "_SessionConflict", "RuntimeError"},
    {"TransientStoreError", "PersistenceError", NULL},
    {"StoreContention", "TransientStoreError", NULL},
    {"DiskFull", "TransientStoreError", NULL},
    {"PermanentStoreError", "PersistenceError", NULL},
    {"CorruptStoreError", "PermanentStoreError", NULL},
    {"StoreQuarantined", "CorruptStoreError", NULL},
    {"MalformedSchema", "CorruptStoreError", NULL},
    {"SearchIndexCorrupt", "CorruptStoreError", NULL},
    {"DeclarativeDrift", "PermanentStoreError", "RuntimeError"},
    {"StoreUnavailable", "PersistenceError", NULL},
    {"StoreReplaced", "PersistenceError", "RuntimeError"},
    {"DeletedWalGeneration", "StoreReplaced", NULL},
    {"ExportTooLarge", "PersistenceError", "ValueError"},
    {"CapabilityUnsupported", "PersistenceError", NULL},
    {"AuthorityRefusal", "PersistenceError", "ValueError"},
    {"FenceStale", "AuthorityRefusal", NULL},
    {"UnknownExecution", "AuthorityRefusal", NULL},
    {"ProofRejected", "AuthorityRefusal", NULL},
    {"HandoffUnsettled", "AuthorityRefusal", NULL},
    {"RetiredSessionError", "AuthorityRefusal", NULL},
    {"ResumeTooLarge", "PersistenceError", "ValueError"},
    {"RewindTargetUnavailable", "PersistenceError", "ValueError"},
};

static const sm_exc_entry_t *sm_exc_find(const char *kind) {
    size_t n = sizeof(SM_EXC_TABLE) / sizeof(SM_EXC_TABLE[0]);
    for (size_t i = 0; i < n; i++)
        if (strcmp(SM_EXC_TABLE[i].name, kind) == 0) return &SM_EXC_TABLE[i];
    return NULL;
}

/* Does kind's parent chain reach ancestor? (BFS, depth-capped.) */
static bool sm_kind_is(const char *kind, const char *ancestor) {
    if (strcmp(kind, ancestor) == 0) return true;
    const char *queue[16];
    size_t qn = 0;
    const sm_exc_entry_t *e = sm_exc_find(kind);
    if (!e) return false;
    if (e->p1) queue[qn++] = e->p1;
    if (e->p2) queue[qn++] = e->p2;
    for (size_t i = 0; i < qn && qn < 16; i++) {
        const char *cur = queue[i];
        if (strcmp(cur, ancestor) == 0) return true;
        const sm_exc_entry_t *ce = sm_exc_find(cur);
        if (!ce) continue;
        if (ce->p1 && qn < 16) queue[qn++] = ce->p1;
        if (ce->p2 && qn < 16) queue[qn++] = ce->p2;
    }
    return false;
}

/* except PersistenceError / except RuntimeError never catch these. */
static bool sm_is_base_exc(const char *kind) {
    if (!sm_exc_find(kind)) return false; /* unknown kinds are Exception subclasses */
    return !sm_kind_is(kind, "Exception");
}

static bool sm_is_persistence(const char *kind) {
    return sm_kind_is(kind, "PersistenceError");
}

static bool sm_is_runtime(const char *kind) {
    return sm_kind_is(kind, "RuntimeError");
}

/* Project a raise spec into its str-form exc/ret shape: __raise__ + message
 * (KeyError repr'd once; int keys keep their decimal text) + batch_progress
 * when attached. Constructor fields / cause drop here. */
static json_t *sm_exc_project(const json_t *spec) {
    json_t *r = json_object();
    const char *kind = sm_kind(spec);
    json_set(r, "__raise__", json_string(kind));
    const char *msg = json_get_str(spec, "message", "");
    if (strcmp(kind, "KeyError") == 0
        && strcmp(json_get_str(spec, "key_kind", ""), "int") != 0) {
        char *rep = sm_py_repr(msg);
        json_set(r, "message", json_string(rep ? rep : ""));
        free(rep);
    } else {
        json_set(r, "message", json_string(msg));
    }
    const json_t *bp = json_obj_get(spec, "batch_progress");
    if (bp) json_set(r, "batch_progress", json_copy(bp));
    return r;
}

/* -- universal seam -- */

/* Scripted seam: args owned here (freed after the call); result caller-freed.
 * A raise spec result transfers into *raise_out. Fail-closed without a channel. */
static json_t *sm_seam(sm_io_t *io, const char *name, json_t *args, json_t **raise_out) {
    if (!io || !io->call) {
        json_free(args);
        sm_raise(raise_out, "RuntimeError", "no seam channel");
        return NULL;
    }
    json_t *r = io->call(io->ctx, name, args);
    json_free(args);
    if (sm_is_raise(r)) { *raise_out = r; return NULL; }
    return r;
}

/* Structural refusals events: record-only, never scripted, never raise. */
static void sm_refusals_enter(sm_io_t *io, const char *sid, json_t **raise_out) {
    json_t *a = json_object();
    json_set(a, "session_id", sid ? json_string(sid) : json_null());
    if (!io || !io->call) {
        json_free(a);
        sm_raise(raise_out, "RuntimeError", "no seam channel");
        return;
    }
    json_t *r = io->call(io->ctx, "refusals.enter", a);
    json_free(a);
    json_free(r);
}

static void sm_refusals_exit(sm_io_t *io, const char *sid, const json_t *raise_spec) {
    json_t *a = json_object();
    json_set(a, "session_id", sid ? json_string(sid) : json_null());
    json_set(a, "exc", raise_spec ? sm_exc_project(raise_spec) : json_null());
    if (!io || !io->call) { json_free(a); return; }
    json_t *r = io->call(io->ctx, "refusals.exit", a);
    json_free(a);
    json_free(r);
}

/* Structural _execute_transcript_write driver events (the _do body runs
 * between them; the seamed retry/restore is outside the ported set). */
static void sm_etw_enter(sm_io_t *io, const json_t *messages, double patience,
                         json_t **raise_out) {
    json_t *a = json_object();
    json_set(a, "messages", sm_cp(messages));
    json_set(a, "patience_s", json_number(patience));
    if (!io || !io->call) {
        json_free(a);
        sm_raise(raise_out, "RuntimeError", "no seam channel");
        return;
    }
    json_t *r = io->call(io->ctx, "_execute_transcript_write.enter", a);
    json_free(a);
    json_free(r);
}

static void sm_etw_exit(sm_io_t *io, const json_t *raise_spec) {
    json_t *a = json_object();
    json_set(a, "exc", raise_spec ? sm_exc_project(raise_spec) : json_null());
    if (!io || !io->call) { json_free(a); return; }
    json_t *r = io->call(io->ctx, "_execute_transcript_write.exit", a);
    json_free(a);
    json_free(r);
}

/* -- common error shapes -- */

/* obj.get on a non-mapping. */
static void sm_get_attr_err(json_t **out, const json_t *v) {
    sm_raise_fmt(out, "AttributeError", "'%s' object has no attribute 'get'",
                 sm_typename(v));
}

/* obj["strkey"] on a non-mapping (the live subscript TypeErrors). */
static void sm_subscript_err(json_t **out, const json_t *v) {
    if (!v || v->type == JSON_NULL) {
        sm_raise(out, "TypeError", "'NoneType' object is not subscriptable");
    } else if (v->type == JSON_ARRAY) {
        sm_raise(out, "TypeError", "list indices must be integers or slices, not str");
    } else if (v->type == JSON_STRING) {
        sm_raise(out, "TypeError", "string indices must be integers");
    } else {
        sm_raise_fmt(out, "TypeError", "'%s' object is not subscriptable",
                     sm_typename(v));
    }
}

/* for x in v on a non-iterable (lists are the fixture domain's iterables). */
static void sm_iter_err(json_t **out, const json_t *v) {
    sm_raise_fmt(out, "TypeError", "'%s' object is not iterable", sm_typename(v));
}

/* TypeError "missing N required positional arguments" (CPython formatting). */
static void sm_missing_args(json_t **raise_out, const char *cls,
                            const char **names, size_t n) {
    char buf[1024];
    size_t o = 0;
    o += (size_t)snprintf(buf + o, sizeof(buf) - o,
                          "%s.__init__() missing %zu required positional argument%s: ",
                          cls, n, n == 1 ? "" : "s");
    for (size_t i = 0; i < n; i++) {
        if (i > 0) {
            if (n == 2) o += (size_t)snprintf(buf + o, sizeof(buf) - o, " and ");
            else if (i + 1 == n) o += (size_t)snprintf(buf + o, sizeof(buf) - o, ", and ");
            else o += (size_t)snprintf(buf + o, sizeof(buf) - o, ", ");
        }
        o += (size_t)snprintf(buf + o, sizeof(buf) - o, "'%s'", names[i]);
    }
    sm_raise(raise_out, "TypeError", buf);
}

/* Enum-value validation message: "'x' is not a valid Cls" for strings. */
static void sm_enum_invalid(json_t **raise_out, const char *cls, const json_t *v) {
    if (v && v->type == JSON_STRING) {
        char *rep = sm_py_repr(v->str_val);
        sm_raise_fmt(raise_out, "ValueError", "%s is not a valid %s", rep ? rep : "''", cls);
        free(rep);
    } else {
        char buf[64];
        sm_py_str(v, buf, sizeof(buf));
        sm_raise_fmt(raise_out, "ValueError", "%s is not a valid %s", buf, cls);
    }
}

/* dict.update from a stamp object (explicit nulls set with json_null()). */
static void sm_obj_update(json_t *dst, const json_t *updates) {
    if (!updates || updates->type != JSON_OBJECT) return;
    for (size_t i = 0; i < updates->c.count; i++)
        json_set(dst, updates->c.keys[i], sm_cp(updates->c.items[i]));
}
/* -- ASCII whitespace / scalar coercions -- */

static bool sm_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

/* Python str.strip() over the ASCII domain. Returns malloc'd trimmed copy. */
static char *sm_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && sm_is_ws(s[a])) a++;
    while (b > a && sm_is_ws(s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static bool sm_is_digit(char c) { return c >= '0' && c <= '9'; }

/* A digit run with PEP 515 underscores: >=1 digit, first/last digits, '_'
 * only between two digits. */
static bool sm_digits_ok(const char *s, size_t n) {
    if (n == 0 || !sm_is_digit(s[0]) || !sm_is_digit(s[n - 1])) return false;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '_') {
            if (i == 0 || i + 1 >= n || !sm_is_digit(s[i - 1]) || !sm_is_digit(s[i + 1]))
                return false;
        } else if (!sm_is_digit(s[i])) {
            return false;
        }
    }
    return true;
}

static void sm_strip_uscore(char *s) {
    char *w = s;
    for (char *r = s; *r; r++) if (*r != '_') *w++ = *r;
    *w = '\0';
}

/* int(v): null/bool/number/string (ASCII domain). Raises ValueError/TypeError
 * with the live messages; returns 0 on raise. */
static double sm_py_int(const json_t *v, json_t **raise_out) {
    if (!v || v->type == JSON_NULL) {
        sm_raise(raise_out, "TypeError",
                 "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
        return 0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return trunc(v->num_val);
    if (v->type == JSON_STRING) {
        char *t = sm_strip(v->str_val);
        char *p = t;
        if (*p == '+' || *p == '-') p++;
        if (!sm_digits_ok(p, strlen(p))) {
            char *rep = sm_py_repr(v->str_val);
            sm_raise_fmt(raise_out, "ValueError",
                         "invalid literal for int() with base 10: %s", rep ? rep : "''");
            free(rep);
            free(t);
            return 0;
        }
        sm_strip_uscore(p);
        double d = (double)strtoll(p, NULL, 10);
        if (t[0] == '-') d = -d; /* carry the consumed sign into the value */
        free(t);
        return d;
    }
    sm_raise_fmt(raise_out, "TypeError",
                 "int() argument must be a string, a bytes-like object or a real number, not '%s'",
                 sm_typename(v));
    return 0;
}

/* float(v): finite ASCII domain (inf/nan strings are outside the fixture
 * domain and fail closed). */
static double sm_py_float(const json_t *v, json_t **raise_out) {
    if (!v || v->type == JSON_NULL) {
        sm_raise(raise_out, "TypeError",
                 "float() argument must be a string or a real number, not 'NoneType'");
        return 0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_STRING) {
        char *t = sm_strip(v->str_val);
        char *p = t;
        bool ok = true;
        if (*p == '+' || *p == '-') p++;
        char *int_start = p;
        while (sm_is_digit(*p) || *p == '_') p++;
        size_t int_n = (size_t)(p - int_start);
        bool has_frac = false;
        size_t frac_n = 0;
        if (*p == '.') {
            has_frac = true;
            p++;
            char *frac_start = p;
            while (sm_is_digit(*p) || *p == '_') p++;
            frac_n = (size_t)(p - frac_start);
        }
        if (int_n == 0 && frac_n == 0) ok = false;
        if (ok && int_n > 0 && !sm_digits_ok(int_start, int_n)) ok = false;
        if (ok && has_frac && frac_n > 0) {
            if (!sm_digits_ok(int_start + int_n + 1, frac_n)) ok = false;
        }
        if (ok && (*p == 'e' || *p == 'E')) {
            p++;
            if (*p == '+' || *p == '-') p++;
            char *exp_start = p;
            while (sm_is_digit(*p) || *p == '_') p++;
            size_t exp_n = (size_t)(p - exp_start);
            if (exp_n == 0 || !sm_digits_ok(exp_start, exp_n)) ok = false;
        }
        if (ok && *p != '\0') ok = false;
        if (!ok) {
            char *rep = sm_py_repr(v->str_val);
            sm_raise_fmt(raise_out, "ValueError",
                         "could not convert string to float: %s", rep ? rep : "''");
            free(rep);
            free(t);
            return 0;
        }
        sm_strip_uscore(t);
        double d = strtod(t, NULL);
        free(t);
        if (!isfinite(d)) {
            char *rep = sm_py_repr(v->str_val);
            sm_raise_fmt(raise_out, "ValueError",
                         "could not convert string to float: %s", rep ? rep : "''");
            free(rep);
            return 0;
        }
        return d;
    }
    sm_raise_fmt(raise_out, "TypeError",
                 "float() argument must be a string or a real number, not '%s'",
                 sm_typename(v));
    return 0;
}

/* int(x or 0) / float(x or 0.0): the truthiness `or` fires first. */
static double sm_int_or_0(const json_t *v, json_t **raise_out) {
    if (!sm_truth(v)) return 0;
    return sm_py_int(v, raise_out);
}

static double sm_float_or_0(const json_t *v, json_t **raise_out) {
    if (!sm_truth(v)) return 0.0;
    return sm_py_float(v, raise_out);
}

/* -- strict Python json.loads grammar over libjson -- */

static void sm_jws(const char **p, const char *end) {
    while (*p < end && (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r')) (*p)++;
}

static bool sm_jhex4(const char *p, const char *end, unsigned long *cp) {
    if (p + 4 > end) return false;
    unsigned long v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned long)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned long)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned long)(c - 'A' + 10);
        else return false;
    }
    *cp = v;
    return true;
}

static bool sm_jstring(const char **p, const char *end) {
    (*p)++; /* opening quote */
    while (*p < end && **p != '"') {
        unsigned char c = (unsigned char)**p;
        if (c < 0x20) return false; /* raw control: Python strict rejects */
        if (c == '\\') {
            (*p)++;
            if (*p >= end) return false;
            char e = **p;
            if (e == 'u') {
                unsigned long cp;
                if (!sm_jhex4(*p + 1, end, &cp)) return false;
                if (cp >= 0xD800 && cp <= 0xDFFF) return false; /* lone surrogates out of domain */
                *p += 4;
            } else if (e != '"' && e != '\\' && e != '/' && e != 'b' && e != 'f'
                       && e != 'n' && e != 'r' && e != 't') {
                return false;
            }
        }
        (*p)++;
    }
    if (*p >= end) return false;
    (*p)++; /* closing quote */
    return true;
}

static bool sm_jnumber(const char **p, const char *end) {
    const char *s = *p;
    if (*p < end && **p == '-') (*p)++;
    if (*p >= end) return false;
    if (**p == '0') {
        (*p)++;
        if (*p < end && sm_is_digit(**p)) return false; /* leading zero */
    } else if (sm_is_digit(**p)) {
        while (*p < end && sm_is_digit(**p)) (*p)++;
    } else {
        return false;
    }
    if (*p < end && **p == '.') {
        (*p)++;
        if (*p >= end || !sm_is_digit(**p)) return false;
        while (*p < end && sm_is_digit(**p)) (*p)++;
    }
    if (*p < end && (**p == 'e' || **p == 'E')) {
        (*p)++;
        if (*p < end && (**p == '+' || **p == '-')) (*p)++;
        if (*p >= end || !sm_is_digit(**p)) return false;
        while (*p < end && sm_is_digit(**p)) (*p)++;
    }
    return *p > s;
}

static bool sm_jvalue(const char **p, const char *end, int depth) {
    if (depth > 32) return false;
    sm_jws(p, end);
    if (*p >= end) return false;
    char c = **p;
    if (c == '"') return sm_jstring(p, end);
    if (c == 't') {
        if ((size_t)(end - *p) < 4 || strncmp(*p, "true", 4) != 0) return false;
        *p += 4; return true;
    }
    if (c == 'f') {
        if ((size_t)(end - *p) < 5 || strncmp(*p, "false", 5) != 0) return false;
        *p += 5; return true;
    }
    if (c == 'n') {
        if ((size_t)(end - *p) < 4 || strncmp(*p, "null", 4) != 0) return false;
        *p += 4; return true;
    }
    if (c == '-' || sm_is_digit(c)) return sm_jnumber(p, end);
    if (c == '[') {
        (*p)++;
        sm_jws(p, end);
        if (*p < end && **p == ']') { (*p)++; return true; }
        for (;;) {
            if (!sm_jvalue(p, end, depth + 1)) return false;
            sm_jws(p, end);
            if (*p >= end) return false;
            if (**p == ']') { (*p)++; return true; }
            if (**p != ',') return false;
            (*p)++;
        }
    }
    if (c == '{') {
        (*p)++;
        sm_jws(p, end);
        if (*p < end && **p == '}') { (*p)++; return true; }
        for (;;) {
            sm_jws(p, end);
            if (*p >= end || **p != '"') return false;
            if (!sm_jstring(p, end)) return false;
            sm_jws(p, end);
            if (*p >= end || **p != ':') return false;
            (*p)++;
            if (!sm_jvalue(p, end, depth + 1)) return false;
            sm_jws(p, end);
            if (*p >= end) return false;
            if (**p == '}') { (*p)++; return true; }
            if (**p != ',') return false;
            (*p)++;
        }
    }
    return false;
}

/* json.loads(text) for the ASCII strict domain: validate the Python grammar,
 * then build the tree with libjson (whose leniency never fires on validated
 * text). NULL == ValueError (the _sidecar catch). */
static json_t *sm_json_loads(const char *text) {
    if (!text) return NULL;
    const char *p = text, *end = text + strlen(text);
    if (!sm_jvalue(&p, end, 0)) return NULL;
    sm_jws(&p, end);
    if (p != end) return NULL;
    char *err = NULL;
    json_t *v = json_parse(text, &err);
    free(err);
    return v;
}

/* -- record construction mechanics (required/unknown kwargs) --
 * The claimed __post_init__ validators are outside the ported set; the
 * fixture domain carries valid records. */

static bool sm_fields_check(const json_t *spec, const char *cls,
                            const char *const *fields, size_t nf,
                            const char *const *required, size_t nr,
                            json_t **raise_out) {
    if (sm_is_null(spec)) {
        sm_raise_fmt(raise_out, "TypeError",
                     "argument of type '%s' is not a mapping", sm_typename(spec));
        return false;
    }
    if (spec->type != JSON_OBJECT) {
        sm_raise_fmt(raise_out, "TypeError",
                     "argument of type '%s' is not a mapping", sm_typename(spec));
        return false;
    }
    for (size_t i = 0; i < spec->c.count; i++) {
        const char *k = spec->c.keys[i];
        bool known = false;
        for (size_t j = 0; j < nf && !known; j++) known = strcmp(k, fields[j]) == 0;
        if (!known) {
            sm_raise_fmt(raise_out, "TypeError",
                         "%s.__init__() got an unexpected keyword argument '%s'", cls, k);
            return false;
        }
    }
    const char *missing[32];
    size_t nm = 0;
    for (size_t j = 0; j < nr; j++)
        if (!json_has(spec, required[j])) missing[nm++] = required[j];
    if (nm > 0) { sm_missing_args(raise_out, cls, missing, nm); return false; }
    return true;
}

static const char *SM_NM_FIELDS[] = {
    "role", "content", "timestamp", "tool_calls", "tool_name", "tool_call_id",
    "display_kind", "compressed_summary", "api_content", "display_metadata",
    "platform_message_id", "observed", "effect_disposition", "finish_reason",
    "reasoning", "reasoning_content", "reasoning_details", "codex_reasoning_items",
    "codex_message_items", "repair_target", "db_row_snapshot", "message_uid",
    "absorbed_message_uids", "tool_call_uids", "tool_call_uid", "token_count",
    "data_use", "merged_durable_rows", "unnamed_durable_rows", "absorbed_row_ids",
    "restored_from_store",
};
static const char *SM_NM_REQ[] = { "role", "content" };

static const char *SM_HM_FIELDS[] = {
    "role", "content", "tool_call_id", "tool_calls", "timestamp", "row_id",
    "persisted", "db_row_snapshot", "message_uid", "absorbed_row_ids",
    "merged_durable_rows", "retired_durable_rows", "tool_call_ids", "own_row",
    "retired_row",
};
static const char *SM_HM_REQ[] = { "role", "content" };

/* -- ported leaves -- */

/* PoP: sm_visibility @ persistence/sqlite/conversation/messages.py:_visibility */
static json_t *sm_visibility(const json_t *active, const json_t *compacted) {
    if (sm_truth(active)) return json_string("active");
    return json_string(sm_truth(compacted) ? "compaction_archived" : "rewound");
}

/* PoP: sm_sidecar @ persistence/sqlite/conversation/messages.py:_sidecar */
static json_t *sm_sidecar(const json_t *value) {
    if (!value || value->type != JSON_STRING) return sm_cp(value);
    json_t *parsed = sm_json_loads(value->str_val);
    if (!parsed) return json_copy(value); /* except ValueError: the text itself */
    return parsed;
}

/* The _MESSAGE_OPTIONALS tuple, in order. */
static const char *SM_MESSAGE_OPTIONALS[] = {
    "tool_name", "tool_call_id", "display_kind", "api_content",
    "platform_message_id", "effect_disposition", "finish_reason", "reasoning",
    "reasoning_content", "reasoning_details", "codex_reasoning_items",
    "codex_message_items", "token_count", "message_uid",
};

/* PoP: sm_message_dict @ persistence/sqlite/conversation/messages.py:_message_dict */
static json_t *sm_message_dict(const json_t *message, json_t **raise_out) {
    if (!sm_fields_check(message, "NewMessage", SM_NM_FIELDS,
                         sizeof(SM_NM_FIELDS) / sizeof(SM_NM_FIELDS[0]),
                         SM_NM_REQ, 2, raise_out)) return NULL;
    json_t *msg = json_object();
    json_set(msg, "role", sm_cp(json_obj_get(message, "role")));
    json_set(msg, "content", sm_cp(json_obj_get(message, "content")));
    const json_t *ts = json_obj_get(message, "timestamp");
    if (!sm_is_null(ts)) json_set(msg, "timestamp", json_copy(ts));
    const json_t *tcs = json_obj_get(message, "tool_calls");
    if (sm_truth(tcs)) json_set(msg, "tool_calls", json_copy(tcs));
    for (size_t i = 0; i < sizeof(SM_MESSAGE_OPTIONALS) / sizeof(SM_MESSAGE_OPTIONALS[0]); i++) {
        const char *name = SM_MESSAGE_OPTIONALS[i];
        const json_t *v = json_obj_get(message, name);
        if (!sm_is_null(v)) json_set(msg, name, json_copy(v));
    }
    const json_t *dm = json_obj_get(message, "display_metadata");
    if (!sm_is_null(dm)) json_set(msg, "display_metadata", json_copy(dm));
    if (sm_truth(json_obj_get(message, "compressed_summary")))
        json_set(msg, "_compressed_summary", json_bool(true));
    if (sm_truth(json_obj_get(message, "observed")))
        json_set(msg, "observed", json_bool(true));
    const json_t *rt = json_obj_get(message, "repair_target");
    if (!sm_is_null(rt)) {
        double d = sm_py_int(rt, raise_out);
        if (*raise_out) { json_free(msg); return NULL; }
        json_set(msg, "_row_id", json_number(d));
    }
    const json_t *snap = json_obj_get(message, "db_row_snapshot");
    if (!sm_is_null(snap)) json_set(msg, "_db_row_snapshot", json_copy(snap));
    const json_t *amu = json_obj_get(message, "absorbed_message_uids");
    if (sm_truth(amu)) json_set(msg, "_absorbed_message_uids", json_copy(amu));
    const json_t *tcu = json_obj_get(message, "tool_call_uids");
    if (!sm_is_null(tcu)) json_set(msg, "_tool_call_uids", json_copy(tcu));
    const json_t *tcu1 = json_obj_get(message, "tool_call_uid");
    if (!sm_is_null(tcu1)) json_set(msg, "_tool_call_uid", json_copy(tcu1));
    const json_t *du = json_obj_get(message, "data_use");
    if (sm_truth(du)) json_set(msg, "data_use", json_copy(du));
    const json_t *mdr = json_obj_get(message, "merged_durable_rows");
    if (sm_truth(mdr)) json_set(msg, "_merged_durable_rows", json_copy(mdr));
    const json_t *udr = json_obj_get(message, "unnamed_durable_rows");
    if (sm_truth(udr)) json_set(msg, "_unnamed_durable_rows", json_copy(udr));
    const json_t *ari = json_obj_get(message, "absorbed_row_ids");
    if (sm_truth(ari)) {
        json_t *ids = json_array();
        if (ari->type != JSON_ARRAY) {
            sm_iter_err(raise_out, ari);
            json_free(ids); json_free(msg); return NULL;
        }
        for (size_t i = 0; i < ari->c.count; i++) {
            double d = sm_py_int(ari->c.items[i], raise_out);
            if (*raise_out) { json_free(ids); json_free(msg); return NULL; }
            json_append(ids, json_number(d));
        }
        json_set(msg, "_absorbed_row_ids", ids);
    }
    if (sm_truth(json_obj_get(message, "restored_from_store")))
        json_set(msg, "_restored_from_store", json_bool(true));
    return msg;
}

/* PoP: sm_held_dict @ persistence/sqlite/conversation/messages.py:_held_dict */
static json_t *sm_held_dict(const json_t *held, json_t **raise_out) {
    if (!sm_fields_check(held, "HeldMessage", SM_HM_FIELDS,
                         sizeof(SM_HM_FIELDS) / sizeof(SM_HM_FIELDS[0]),
                         SM_HM_REQ, 2, raise_out)) return NULL;
    json_t *msg = json_object();
    json_set(msg, "role", sm_cp(json_obj_get(held, "role")));
    json_set(msg, "content", sm_cp(json_obj_get(held, "content")));
    const json_t *tcid = json_obj_get(held, "tool_call_id");
    if (!sm_is_null(tcid)) json_set(msg, "tool_call_id", json_copy(tcid));
    const json_t *tcs = json_obj_get(held, "tool_calls");
    if (sm_truth(tcs)) json_set(msg, "tool_calls", json_copy(tcs));
    const json_t *ts = json_obj_get(held, "timestamp");
    if (!sm_is_null(ts)) json_set(msg, "timestamp", json_copy(ts));
    const json_t *rid = json_obj_get(held, "row_id");
    if (!sm_is_null(rid)) {
        double d = sm_py_int(rid, raise_out);
        if (*raise_out) { json_free(msg); return NULL; }
        json_set(msg, "_row_id", json_number(d));
    }
    if (sm_truth(json_obj_get(held, "persisted")))
        json_set(msg, "_db_persisted", json_bool(true));
    const json_t *snap = json_obj_get(held, "db_row_snapshot");
    if (!sm_is_null(snap)) json_set(msg, "_db_row_snapshot", json_copy(snap));
    const json_t *uid = json_obj_get(held, "message_uid");
    if (!sm_is_null(uid)) json_set(msg, "message_uid", json_copy(uid));
    const json_t *ari = json_obj_get(held, "absorbed_row_ids");
    if (sm_truth(ari)) {
        if (ari->type != JSON_ARRAY) {
            sm_iter_err(raise_out, ari);
            json_free(msg); return NULL;
        }
        json_t *ids = json_array();
        for (size_t i = 0; i < ari->c.count; i++) {
            double d = sm_py_int(ari->c.items[i], raise_out);
            if (*raise_out) { json_free(ids); json_free(msg); return NULL; }
            json_append(ids, json_number(d));
        }
        json_set(msg, "_absorbed_row_ids", ids);
    }
    const json_t *mdr = json_obj_get(held, "merged_durable_rows");
    if (sm_truth(mdr)) json_set(msg, "_merged_durable_rows", json_copy(mdr));
    const json_t *rdr = json_obj_get(held, "retired_durable_rows");
    if (sm_truth(rdr)) {
        if (rdr->type != JSON_ARRAY) {
            sm_iter_err(raise_out, rdr);
            json_free(msg); return NULL;
        }
        json_t *rows = json_array();
        for (size_t i = 0; i < rdr->c.count; i++) {
            json_t *row = sm_held_dict(rdr->c.items[i], raise_out);
            if (*raise_out) { json_free(rows); json_free(msg); return NULL; }
            json_append(rows, row);
        }
        json_set(msg, "_retired_durable_rows", rows);
    }
    const json_t *tcids = json_obj_get(held, "tool_call_ids");
    if (sm_truth(tcids)) json_set(msg, "tool_call_ids", json_copy(tcids));
    if (sm_truth(json_obj_get(held, "own_row")))
        json_set(msg, "_own_row", json_bool(true));
    if (sm_truth(json_obj_get(held, "retired_row")))
        json_set(msg, "_retired_row", json_bool(true));
    return msg;
}

static const char *SM_PRC_FIELDS[] = { "original", "replacement", "before" };

/* PoP: sm_pruned_original @ persistence/sqlite/conversation/messages.py:_pruned_original */
static json_t *sm_pruned_original(const json_t *change, json_t **raise_out) {
    if (!sm_fields_check(change, "PrunedRowChange", SM_PRC_FIELDS, 3,
                         SM_PRC_FIELDS, 3, raise_out)) return NULL;
    json_t *before_full = sm_message_dict(json_obj_get(change, "before"), raise_out);
    if (*raise_out) return NULL;
    /* {k: v for k, v in _message_dict(before).items() if k not in
     * ("_row_id", DB_ROW_SNAPSHOT)} */
    json_t *before = json_object();
    for (size_t i = 0; i < before_full->c.count; i++) {
        const char *k = before_full->c.keys[i];
        if (strcmp(k, "_row_id") == 0 || strcmp(k, "_db_row_snapshot") == 0) continue;
        json_set(before, k, json_copy(before_full->c.items[i]));
    }
    json_free(before_full);
    json_t *held = sm_held_dict(json_obj_get(change, "original"), raise_out);
    if (*raise_out) { json_free(before); return NULL; }
    /* {**before, **held}: held keys overwrite, keeping before's positions. */
    sm_obj_update(before, held);
    json_free(held);
    return before;
}

/* The _WIRE_FIELDS tuple, in order. */
static const char *SM_WIRE_FIELDS[] = {
    "role", "content", "timestamp", "tool_call_id", "tool_calls", "tool_name",
    "effect_disposition", "token_count", "finish_reason", "reasoning",
    "reasoning_content", "reasoning_details", "codex_reasoning_items",
    "codex_message_items", "platform_message_id", "api_content", "display_kind",
    "display_metadata", "message_uid", "absorbed_message_uids", "tool_call_uids",
    "tool_call_uid",
};

/* TranscriptRow's field order (the projection). */
static const char *SM_TR_FIELDS[] = {
    "id", "session_id", "role", "content", "timestamp", "visibility",
    "tool_call_id", "tool_calls", "tool_name", "effect_disposition",
    "token_count", "finish_reason", "reasoning", "reasoning_content",
    "reasoning_details", "codex_reasoning_items", "codex_message_items",
    "platform_message_id", "observed", "compressed_summary", "api_content",
    "display_kind", "display_metadata", "message_uid", "absorbed_message_uids",
    "tool_call_uids", "tool_call_uid",
};

/* PoP: sm_transcript_row @ persistence/sqlite/conversation/messages.py:_transcript_row */
static json_t *sm_transcript_row(const json_t *row, json_t **raise_out) {
    if (!row || row->type != JSON_OBJECT) {
        sm_get_attr_err(raise_out, row);
        return NULL;
    }
    /* fields = {name: row.get(name) for name in _WIRE_FIELDS} -- first. */
    json_t *fields = json_object();
    for (size_t i = 0; i < sizeof(SM_WIRE_FIELDS) / sizeof(SM_WIRE_FIELDS[0]); i++)
        json_set(fields, SM_WIRE_FIELDS[i], sm_cp(json_obj_get(row, SM_WIRE_FIELDS[i])));
    /* id=MessageId(int(row["id"])) */
    if (!json_has(row, "id")) { sm_raise_key(raise_out, "id"); json_free(fields); return NULL; }
    double id = sm_py_int(json_obj_get(row, "id"), raise_out);
    if (*raise_out) { json_free(fields); return NULL; }
    /* session_id=SessionId(row["session_id"]) */
    if (!json_has(row, "session_id")) { sm_raise_key(raise_out, "session_id"); json_free(fields); return NULL; }
    json_t *out = json_object();
    json_set(out, "id", json_number(id));
    json_set(out, "session_id", sm_cp(json_obj_get(row, "session_id")));
    json_set(out, "role", sm_cp(json_obj_get(fields, "role")));
    json_set(out, "content", sm_cp(json_obj_get(fields, "content")));
    json_set(out, "timestamp", sm_cp(json_obj_get(fields, "timestamp")));
    json_set(out, "visibility",
             sm_visibility(json_obj_get(row, "active"), json_obj_get(row, "compacted")));
    for (size_t i = 6; i < 18; i++) /* tool_call_id .. platform_message_id */
        json_set(out, SM_TR_FIELDS[i], sm_cp(json_obj_get(fields, SM_TR_FIELDS[i])));
    json_set(out, "observed", json_bool(sm_truth(json_obj_get(row, "observed"))));
    /* compressed_summary = row.get("_compressed_summary") is True: identity. */
    const json_t *cs = json_obj_get(row, "_compressed_summary");
    json_set(out, "compressed_summary", json_bool(cs && cs->type == JSON_BOOL && cs->bool_val));
    for (size_t i = 20; i < sizeof(SM_TR_FIELDS) / sizeof(SM_TR_FIELDS[0]); i++)
        json_set(out, SM_TR_FIELDS[i], sm_cp(json_obj_get(fields, SM_TR_FIELDS[i])));
    json_free(fields);
    return out;
}

/* The _SIDECARS tuple, in order. */
static const char *SM_SIDECARS[] = {
    "reasoning_details", "codex_reasoning_items", "codex_message_items",
};

/* MessageRecord's field order (the projection). */
static const char *SM_MR_FIELDS[] = {
    "id", "role", "content", "timestamp", "tool_calls", "tool_name",
    "tool_call_id", "display_kind", "compressed_summary", "visibility",
    "api_content", "display_metadata", "platform_message_id", "observed",
    "effect_disposition", "finish_reason", "reasoning", "reasoning_content",
    "reasoning_details", "codex_reasoning_items", "codex_message_items",
    "message_uid",
};

/* PoP: sm_message_record @ persistence/sqlite/conversation/messages.py:_message_record */
static json_t *sm_message_record(const json_t *row, json_t **raise_out) {
    if (!row || row->type != JSON_OBJECT) {
        sm_get_attr_err(raise_out, row);
        return NULL;
    }
    /* metadata = row.get("display_metadata"); the reactions strip + or None. */
    const json_t *meta_in = json_obj_get(row, "display_metadata");
    json_t *metadata = NULL;
    if (meta_in && meta_in->type == JSON_OBJECT) {
        metadata = json_object();
        for (size_t i = 0; i < meta_in->c.count; i++) {
            if (strcmp(meta_in->c.keys[i], "reactions") == 0) continue;
            json_set(metadata, meta_in->c.keys[i], json_copy(meta_in->c.items[i]));
        }
        if (metadata->c.count == 0) { json_free(metadata); metadata = NULL; }
    } else {
        metadata = sm_cp(meta_in);
    }
    /* kwarg order: id, role, content, timestamp, tool_calls, ... */
    if (!json_has(row, "id")) { sm_raise_key(raise_out, "id"); json_free(metadata); return NULL; }
    double id = sm_py_int(json_obj_get(row, "id"), raise_out);
    if (*raise_out) { json_free(metadata); return NULL; }
    if (!json_has(row, "role")) { sm_raise_key(raise_out, "role"); json_free(metadata); return NULL; }
    if (!json_has(row, "timestamp")) { sm_raise_key(raise_out, "timestamp"); json_free(metadata); return NULL; }
    double ts = sm_py_float(json_obj_get(row, "timestamp"), raise_out);
    if (*raise_out) { json_free(metadata); return NULL; }
    /* tool_calls=tuple(row.get("tool_calls") or ()) */
    const json_t *tcs = json_obj_get(row, "tool_calls");
    json_t *tool_calls;
    if (!sm_truth(tcs)) {
        tool_calls = json_array();
    } else if (tcs->type == JSON_ARRAY) {
        tool_calls = json_copy(tcs);
    } else {
        sm_iter_err(raise_out, tcs);
        json_free(metadata);
        return NULL;
    }
    json_t *out = json_object();
    json_set(out, "id", json_number(id));
    json_set(out, "role", sm_cp(json_obj_get(row, "role")));
    json_set(out, "content", sm_cp(json_obj_get(row, "content")));
    json_set(out, "timestamp", json_number(ts));
    json_set(out, "tool_calls", tool_calls);
    json_set(out, "tool_name", sm_cp(json_obj_get(row, "tool_name")));
    json_set(out, "tool_call_id", sm_cp(json_obj_get(row, "tool_call_id")));
    json_set(out, "display_kind", sm_cp(json_obj_get(row, "display_kind")));
    json_set(out, "compressed_summary", json_bool(sm_truth(json_obj_get(row, "_compressed_summary"))));
    json_set(out, "visibility",
             sm_visibility(json_obj_get(row, "active"), json_obj_get(row, "compacted")));
    json_set(out, "api_content", sm_cp(json_obj_get(row, "api_content")));
    json_set(out, "display_metadata", metadata ? metadata : json_null());
    json_set(out, "platform_message_id", sm_cp(json_obj_get(row, "platform_message_id")));
    json_set(out, "observed", json_bool(sm_truth(json_obj_get(row, "observed"))));
    json_set(out, "effect_disposition", sm_cp(json_obj_get(row, "effect_disposition")));
    json_set(out, "finish_reason", sm_cp(json_obj_get(row, "finish_reason")));
    json_set(out, "reasoning", sm_cp(json_obj_get(row, "reasoning")));
    json_set(out, "reasoning_content", sm_cp(json_obj_get(row, "reasoning_content")));
    for (size_t i = 0; i < 3; i++)
        json_set(out, SM_SIDECARS[i], sm_sidecar(json_obj_get(row, SM_SIDECARS[i])));
    json_set(out, "message_uid", sm_cp(json_obj_get(row, "message_uid")));
    return out;
}

/* PoP: sm_stamp @ persistence/sqlite/conversation/messages.py:_stamp */
static json_t *sm_stamp(const json_t *msg, const json_t *stored, json_t **raise_out) {
    /* row_id = int(msg["_row_id"]) */
    if (!msg || msg->type != JSON_OBJECT) {
        sm_subscript_err(raise_out, msg);
        return NULL;
    }
    if (!json_has(msg, "_row_id")) { sm_raise_key(raise_out, "_row_id"); return NULL; }
    double row_id = sm_py_int(json_obj_get(msg, "_row_id"), raise_out);
    if (*raise_out) return NULL;
    char keybuf[64];
    snprintf(keybuf, sizeof(keybuf), "%.0f", row_id);
    /* timestamp = msg.get("timestamp"); None -> stored[row_id]["timestamp"] */
    const json_t *ts = json_obj_get(msg, "timestamp");
    json_t *ts_owned = NULL;
    if (sm_is_null(ts)) {
        if (!stored || stored->type != JSON_OBJECT) {
            sm_subscript_err(raise_out, stored);
            return NULL;
        }
        const json_t *srow = json_obj_get(stored, keybuf);
        if (!srow) { sm_raise_key_int(raise_out, row_id); return NULL; }
        if (srow->type != JSON_OBJECT) { sm_subscript_err(raise_out, srow); return NULL; }
        if (!json_has(srow, "timestamp")) { sm_raise_key(raise_out, "timestamp"); return NULL; }
        ts_owned = json_copy(json_obj_get(srow, "timestamp"));
        ts = ts_owned;
    }
    double timestamp = sm_py_float(ts, raise_out);
    if (*raise_out) { json_free(ts_owned); return NULL; }
    /* message_uid = msg.get("message_uid") or stored.get(row_id, {}).get("message_uid") or "" */
    const json_t *uid = json_obj_get(msg, "message_uid");
    json_t *uid_out = NULL;
    if (sm_truth(uid)) {
        uid_out = json_copy(uid);
    } else {
        const json_t *srow = (stored && stored->type == JSON_OBJECT)
                             ? json_obj_get(stored, keybuf) : NULL;
        const json_t *suid = NULL;
        if (srow) {
            if (srow->type != JSON_OBJECT) {
                sm_get_attr_err(raise_out, srow);
                json_free(ts_owned);
                return NULL;
            }
            suid = json_obj_get(srow, "message_uid");
        }
        uid_out = sm_truth(suid) ? json_copy(suid) : json_string("");
    }
    json_t *out = json_object();
    json_set(out, "id", json_number(row_id));
    json_set(out, "timestamp", json_number(timestamp));
    json_set(out, "db_row_snapshot", sm_cp(json_obj_get(msg, "_db_row_snapshot")));
    json_set(out, "message_uid", uid_out);
    json_set(out, "tool_call_uids", sm_cp(json_obj_get(msg, "_tool_call_uids")));
    json_set(out, "tool_call_uid", sm_cp(json_obj_get(msg, "_tool_call_uid")));
    json_free(ts_owned);
    return out;
}

/* PoP: sm_row_outcomes @ persistence/sqlite/conversation/messages.py:_row_outcomes */
static json_t *sm_row_outcomes(const json_t *rows, const json_t *count, json_t **raise_out) {
    if (!rows || rows->type != JSON_ARRAY) {
        sm_iter_err(raise_out, rows);
        return NULL;
    }
    /* fresh = set(sorted({int(row["_row_id"]) for row in rows}, reverse=True)[:count]) */
    json_t *ids = json_array(); /* unique, insertion order */
    for (size_t i = 0; i < rows->c.count; i++) {
        const json_t *row = rows->c.items[i];
        if (!row || row->type != JSON_OBJECT) {
            sm_subscript_err(raise_out, row);
            json_free(ids); return NULL;
        }
        if (!json_has(row, "_row_id")) { sm_raise_key(raise_out, "_row_id"); json_free(ids); return NULL; }
        double d = sm_py_int(json_obj_get(row, "_row_id"), raise_out);
        if (*raise_out) { json_free(ids); return NULL; }
        bool dup = false;
        for (size_t j = 0; j < ids->c.count && !dup; j++)
            dup = ids->c.items[j]->num_val == d;
        if (!dup) json_append(ids, json_number(d));
    }
    /* sort descending (simple selection sort; the id sets are small) */
    for (size_t i = 0; i < ids->c.count; i++) {
        size_t mx = i;
        for (size_t j = i + 1; j < ids->c.count; j++)
            if (ids->c.items[j]->num_val > ids->c.items[mx]->num_val) mx = j;
        if (mx != i) {
            json_t *t = ids->c.items[i];
            ids->c.items[i] = ids->c.items[mx];
            ids->c.items[mx] = t;
        }
    }
    /* [:count] -- None reads unbounded; a non-integral slice index raises. */
    size_t stop = ids->c.count;
    if (!sm_is_null(count)) {
        double c;
        if (count->type == JSON_BOOL) c = count->bool_val ? 1 : 0;
        else if (count->type == JSON_NUMBER && count->num_val == trunc(count->num_val))
            c = count->num_val;
        else {
            sm_raise(raise_out, "TypeError",
                     "slice indices must be integers or None or have an __index__ method");
            json_free(ids); return NULL;
        }
        if (c >= 0) stop = ((size_t)c < ids->c.count) ? (size_t)c : ids->c.count;
        else {
            double s = (double)ids->c.count + c;
            stop = s > 0 ? (size_t)s : 0;
        }
    }
    json_t *out = json_array();
    for (size_t i = 0; i < rows->c.count; i++) {
        const json_t *row = rows->c.items[i];
        double row_id = sm_py_int(json_obj_get(row, "_row_id"), raise_out);
        if (*raise_out) { json_free(ids); json_free(out); return NULL; }
        const json_t *canonical = json_obj_get(row, "_canonical_row");
        bool fresh = false;
        for (size_t j = 0; j < stop && !fresh; j++)
            fresh = ids->c.items[j]->num_val == row_id;
        const char *disposition;
        if (fresh) {
            disposition = "appended";
        } else if (!sm_is_null(canonical)) {
            if (canonical->type != JSON_OBJECT) {
                sm_get_attr_err(raise_out, canonical);
                json_free(ids); json_free(out); return NULL;
            }
            disposition = sm_truth(json_obj_get(canonical, "_metadata_only"))
                          ? "repaired" : "adopted";
        } else {
            disposition = "repaired";
        }
        if (!json_has(row, "timestamp")) { sm_raise_key(raise_out, "timestamp"); json_free(ids); json_free(out); return NULL; }
        double ts = sm_py_float(json_obj_get(row, "timestamp"), raise_out);
        if (*raise_out) { json_free(ids); json_free(out); return NULL; }
        json_t *o = json_object();
        json_set(o, "id", json_number(row_id));
        json_set(o, "disposition", json_string(disposition));
        json_set(o, "timestamp", json_number(ts));
        json_set(o, "db_row_snapshot", sm_cp(json_obj_get(row, "_db_row_snapshot")));
        json_set(o, "canonical_row", sm_cp(canonical));
        json_set(o, "message_uid", sm_cp(json_obj_get(row, "message_uid")));
        json_set(o, "tool_call_uids", sm_cp(json_obj_get(row, "_tool_call_uids")));
        json_set(o, "tool_call_uid", sm_cp(json_obj_get(row, "_tool_call_uid")));
        json_append(out, o);
    }
    json_free(ids);
    return out;
}

/* ConversationMessage's 33 fields, in dataclass order. */
static const char *SM_CM_FIELDS[] = {
    "role", "content", "db_row_snapshot", "row_id", "message_uid",
    "tool_call_uids", "tool_call_uid", "absorbed_message_uids", "api_content",
    "display_kind", "display_metadata", "data_use", "compressed_summary",
    "timestamp", "tool_call_id", "tool_name", "effect_disposition", "tool_calls",
    "message_id", "observed", "finish_reason", "reasoning", "reasoning_content",
    "reasoning_details", "codex_reasoning_items", "codex_message_items",
    "absorbed_row_ids", "merged_turn_prefix", "merged_durable_rows",
    "unnamed_durable_rows", "retired_durable_rows", "db_persisted",
    "_tool_call_uid_stored",
};
static const char *SM_CM_REQ[] = { "role", "content" };

/* The record defaults for an absent script field (tuples as lists). */
static json_t *sm_cm_default(const char *name) {
    if (strcmp(name, "absorbed_message_uids") == 0 || strcmp(name, "data_use") == 0
        || strcmp(name, "absorbed_row_ids") == 0)
        return json_array();
    if (strcmp(name, "compressed_summary") == 0 || strcmp(name, "observed") == 0
        || strcmp(name, "_tool_call_uid_stored") == 0)
        return json_bool(false);
    if (strcmp(name, "db_persisted") == 0) return json_bool(true);
    return json_null();
}

/* PoP: sm_conversation @ persistence/sqlite/conversation/messages.py:_conversation */
static json_t *sm_conversation(const json_t *messages, sm_io_t *io, json_t **raise_out) {
    if (!messages || messages->type != JSON_ARRAY) {
        sm_iter_err(raise_out, messages);
        return NULL;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *a = json_object();
        json_set(a, "message", sm_cp(messages->c.items[i]));
        json_t *rec = sm_seam(io, "from_openai_message", a, raise_out);
        if (*raise_out) { json_free(out); return NULL; }
        if (!sm_fields_check(rec, "ConversationMessage", SM_CM_FIELDS,
                             sizeof(SM_CM_FIELDS) / sizeof(SM_CM_FIELDS[0]),
                             SM_CM_REQ, 2, raise_out)) {
            json_free(rec); json_free(out); return NULL;
        }
        json_t *proj = json_object();
        for (size_t j = 0; j < sizeof(SM_CM_FIELDS) / sizeof(SM_CM_FIELDS[0]); j++) {
            const char *name = SM_CM_FIELDS[j];
            const json_t *v = json_obj_get(rec, name);
            json_set(proj, name, v ? json_copy(v) : sm_cm_default(name));
        }
        json_free(rec);
        json_append(out, proj);
    }
    return out;
}

/* -- _refusals: the transcript writes' refusal conversions -- */

/* The legacy store's rewind refusal texts, by contract reason. */
static const struct { const char *text; const char *reason; } SM_REWIND_REFUSALS[] = {
    {"session history changed before the rewind could be persisted", "history_changed"},
    {"active transcript changed before the rewind could be persisted", "history_changed"},
    {"rewind target has no durable row identity", "target_unidentified"},
    {"rewind target changed before it could be persisted", "target_changed"},
    {"rewind did not retain its compaction handoff", "handoff_lost"},
};

static const char *sm_rewind_reason(const char *text) {
    for (size_t i = 0; i < sizeof(SM_REWIND_REFUSALS) / sizeof(SM_REWIND_REFUSALS[0]); i++)
        if (strcmp(SM_REWIND_REFUSALS[i].text, text) == 0)
            return SM_REWIND_REFUSALS[i].reason;
    return NULL;
}

/* The ported _refusals except-clauses, applied to the spec leaving the inner
 * refusals CM (the exit event already recorded the ORIGINAL). PersistenceError
 * -> classify_persistence_error(exc.__cause__) == "session_row_missing" ->
 * SessionRowMissing; RuntimeError -> a known rewind text -> RewindRefused.
 * Everything else (incl. the BaseException trio) passes through. */
static void sm_refusals_transform(sm_io_t *io, const char *sid, json_t **raise_out) {
    if (!raise_out || !*raise_out) return;
    const char *kind = sm_kind(*raise_out);
    if (sm_is_persistence(kind)) {
        if (sid) {
            const json_t *cause = json_obj_get(*raise_out, "cause");
            json_t *a = json_object();
            json_set(a, "cause", cause ? sm_exc_project(cause) : json_null());
            json_t *bucket = sm_seam(io, "classify_persistence_error", a, raise_out);
            if (*raise_out) return; /* the classify failure replaces the original */
            bool missing = bucket && bucket->type == JSON_STRING
                           && strcmp(bucket->str_val, "session_row_missing") == 0;
            json_free(bucket);
            if (missing) {
                char *rep = sm_py_repr(sid);
                json_t *r = json_object();
                json_set(r, "__raise__", json_string("SessionRowMissing"));
                char *msg = (char *)malloc(strlen(rep ? rep : sid) + 64);
                sprintf(msg, "FOREIGN KEY constraint failed: session %s has no row",
                        rep ? rep : sid);
                json_set(r, "message", json_string(msg));
                free(msg); free(rep);
                json_free(*raise_out);
                *raise_out = r;
            }
        }
        return;
    }
    if (sm_is_runtime(kind)) {
        const char *text = json_get_str(*raise_out, "message", "");
        if (sid && sm_rewind_reason(text)) {
            json_t *r = json_object();
            json_set(r, "__raise__", json_string("RewindRefused"));
            json_set(r, "message", json_string(text));
            json_free(*raise_out);
            *raise_out = r;
        }
    }
}

/* PoP: sm_refusals @ persistence/sqlite/conversation/messages.py:_refusals */
/* Driven as the CM around a scripted body: body {"ret": X} returns X;
 * body {"raise": spec} raises through the conversion. */
static json_t *sm_refusals(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    sm_refusals_enter(io, sid, raise_out);
    if (*raise_out) return NULL;
    const json_t *body = json_obj_get(in, "body");
    const json_t *rspec = body ? json_obj_get(body, "raise") : NULL;
    if (rspec && sm_is_raise(rspec)) {
        *raise_out = json_copy(rspec);
        sm_refusals_exit(io, sid, *raise_out);
        sm_refusals_transform(io, sid, raise_out);
        return NULL;
    }
    sm_refusals_exit(io, sid, NULL);
    return body ? sm_cp(json_obj_get(body, "ret")) : NULL;
}

/* -- _append_chunk: one batch write transaction -- */

/* Apply a mutating verb's stamps script (a per-msg updates array) to dicts. */
static void sm_apply_stamps(json_t *dicts, const json_t *script) {
    if (!script || script->type != JSON_OBJECT) return;
    const json_t *stamps = json_obj_get(script, "stamps");
    if (!stamps || stamps->type != JSON_ARRAY) return;
    size_t n = stamps->c.count < dicts->c.count ? stamps->c.count : dicts->c.count;
    for (size_t i = 0; i < n; i++)
        sm_obj_update(dicts->c.items[i], stamps->c.items[i]);
}

/* PoP: sm_append_chunk @ persistence/sqlite/conversation/messages.py:_append_chunk */
static json_t *sm_append_chunk(sm_io_t *io, const char *sid, json_t *messages,
                               const json_t *guards, double patience, json_t **raise_out) {
    /* if not messages: return 0 -- None/empty alike, before any event. */
    if (!sm_truth(messages)) return json_number(0);
    if (messages->type != JSON_ARRAY) {
        sm_iter_err(raise_out, messages);
        return NULL;
    }
    sm_etw_enter(io, messages, patience, raise_out);
    if (*raise_out) return NULL;
    json_t *inserted = NULL;
    json_t *a = json_object();
    json_t *pos = json_array();
    json_append(pos, sid ? json_string(sid) : json_null());
    json_append(pos, json_copy(messages));
    json_set(a, "args", pos);
    json_set(a, "kwargs", sm_cp(guards));
    json_t *script = sm_seam(io, "_append_messages_in_transaction", a, raise_out);
    if (!*raise_out) {
        if (script && script->type != JSON_OBJECT) {
            sm_get_attr_err(raise_out, script);
        } else if (script) {
            sm_apply_stamps(messages, script);
            inserted = sm_cp(json_obj_get(script, "inserted"));
        }
    }
    json_free(script);
    /* the restored-row repair: persisted + restored + no _row_id */
    if (!*raise_out) {
        for (size_t i = 0; i < messages->c.count && !*raise_out; i++) {
            json_t *msg = messages->c.items[i];
            if (!sm_truth(json_obj_get(msg, "_restored_from_store"))
                || !sm_truth(json_obj_get(msg, "_db_persisted"))
                || json_has(msg, "_row_id"))
                continue;
            if (!json_has(msg, "role")) { sm_raise_key(raise_out, "role"); break; }
            json_t *ua = json_object();
            json_set(ua, "msg", json_copy(msg));
            json_t *uid = sm_seam(io, "message_uid_or_none", ua, raise_out);
            if (*raise_out) break;
            json_t *ra = json_object();
            json_set(ra, "session_id", sid ? json_string(sid) : json_null());
            json_set(ra, "role", sm_cp(json_obj_get(msg, "role")));
            json_set(ra, "message_uid", sm_cp(uid));
            json_free(uid);
            json_t *row = sm_seam(io, "_active_logical_message_row", ra, raise_out);
            if (*raise_out) break;
            /* msg.update({"_row_id": int(row["id"]), "timestamp": row["timestamp"],
             *             DB_ROW_SNAPSHOT: transcript_row_snapshot(row)}) */
            if (!row || row->type != JSON_OBJECT) {
                sm_subscript_err(raise_out, row);
                json_free(row); break;
            }
            if (!json_has(row, "id")) { sm_raise_key(raise_out, "id"); json_free(row); break; }
            double rid = sm_py_int(json_obj_get(row, "id"), raise_out);
            if (*raise_out) { json_free(row); break; }
            if (!json_has(row, "timestamp")) { sm_raise_key(raise_out, "timestamp"); json_free(row); break; }
            json_t *ts = json_copy(json_obj_get(row, "timestamp"));
            json_t *sa = json_object();
            json_set(sa, "row", json_copy(row));
            json_t *snapshot = sm_seam(io, "transcript_row_snapshot", sa, raise_out);
            if (*raise_out) { json_free(ts); json_free(row); break; }
            json_set(msg, "_row_id", json_number(rid));
            json_set(msg, "timestamp", ts);
            json_set(msg, "_db_row_snapshot", sm_cp(snapshot));
            json_free(snapshot);
            json_t *ia = json_object();
            json_set(ia, "row", json_copy(row));
            json_set(ia, "msg", json_copy(msg));
            json_free(row);
            json_t *iscript = sm_seam(io, "_restore_row_identity", ia, raise_out);
            if (!*raise_out) {
                if (iscript && iscript->type != JSON_OBJECT) {
                    sm_get_attr_err(raise_out, iscript);
                } else if (iscript) {
                    sm_obj_update(msg, json_obj_get(iscript, "msg"));
                }
            }
            json_free(iscript);
        }
    }
    sm_etw_exit(io, *raise_out);
    if (*raise_out) { json_free(inserted); return NULL; }
    return inserted ? inserted : json_null();
}

/* -- repository write flows -- */

/* PoP: sm_append @ persistence/sqlite/conversation/messages.py:append */
static json_t *sm_append(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    const json_t *spec = json_obj_get(in, "message");
    json_t *msg = sm_message_dict(spec, raise_out);
    if (*raise_out) return NULL;
    sm_refusals_enter(io, sid, raise_out);
    if (*raise_out) { json_free(msg); return NULL; }
    json_t *a = json_object();
    json_t *pos = json_array();
    json_append(pos, sm_cp(sid_node));
    json_append(pos, sm_cp(json_obj_get(spec, "role")));
    json_set(a, "args", pos);
    json_t *kw = json_object();
    json_set(kw, "content", sm_cp(json_obj_get(spec, "content")));
    json_set(kw, "tool_name", sm_cp(json_obj_get(spec, "tool_name")));
    json_set(kw, "tool_calls", sm_cp(json_obj_get(msg, "tool_calls")));
    json_set(kw, "tool_call_id", sm_cp(json_obj_get(spec, "tool_call_id")));
    json_set(kw, "token_count", sm_cp(json_obj_get(spec, "token_count")));
    json_set(kw, "finish_reason", sm_cp(json_obj_get(spec, "finish_reason")));
    json_set(kw, "reasoning", sm_cp(json_obj_get(spec, "reasoning")));
    json_set(kw, "reasoning_content", sm_cp(json_obj_get(spec, "reasoning_content")));
    json_set(kw, "reasoning_details", sm_cp(json_obj_get(spec, "reasoning_details")));
    json_set(kw, "codex_reasoning_items", sm_cp(json_obj_get(spec, "codex_reasoning_items")));
    json_set(kw, "codex_message_items", sm_cp(json_obj_get(spec, "codex_message_items")));
    json_set(kw, "platform_message_id", sm_cp(json_obj_get(spec, "platform_message_id")));
    const json_t *obs = json_obj_get(spec, "observed");
    json_set(kw, "observed", obs ? json_copy(obs) : json_bool(false));
    json_set(kw, "effect_disposition", sm_cp(json_obj_get(spec, "effect_disposition")));
    const json_t *cs = json_obj_get(spec, "compressed_summary");
    json_set(kw, "_compressed_summary", cs ? json_copy(cs) : json_bool(false));
    json_set(kw, "timestamp", sm_cp(json_obj_get(spec, "timestamp")));
    json_set(kw, "api_content", sm_cp(json_obj_get(spec, "api_content")));
    json_set(kw, "display_kind", sm_cp(json_obj_get(spec, "display_kind")));
    json_set(kw, "display_metadata", sm_cp(json_obj_get(msg, "display_metadata")));
    json_set(kw, "turn_lease_holder", sm_cp(json_obj_get(in, "turn_lease_holder")));
    const json_t *ttl = json_obj_get(in, "turn_lease_ttl_seconds");
    json_set(kw, "turn_lease_ttl_seconds", ttl ? json_copy(ttl) : json_number(300.0));
    json_set(kw, "message_uid", sm_cp(json_obj_get(spec, "message_uid")));
    json_set(a, "kwargs", kw);
    json_free(msg);
    json_t *r = sm_seam(io, "append_message", a, raise_out);
    sm_refusals_exit(io, sid, *raise_out);
    sm_refusals_transform(io, sid, raise_out);
    if (*raise_out) { json_free(r); return NULL; }
    return r ? r : json_null(); /* MessageId is NewType(int): identity */
}

/* The RowUids projection for one batch dict (message_uid `or None`). */
static json_t *sm_row_uids(const json_t *msg) {
    json_t *u = json_object();
    const json_t *uid = json_obj_get(msg, "message_uid");
    json_set(u, "message_uid", sm_truth(uid) ? json_copy(uid) : json_null());
    json_set(u, "tool_call_uids", sm_cp(json_obj_get(msg, "_tool_call_uids")));
    json_set(u, "tool_call_uid", sm_cp(json_obj_get(msg, "_tool_call_uid")));
    return u;
}

/* PoP: sm_append_batch @ persistence/sqlite/conversation/messages.py:append_batch */
static json_t *sm_append_batch(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    const json_t *specs = json_obj_get(in, "messages");
    if (!specs || specs->type != JSON_ARRAY) {
        sm_iter_err(raise_out, specs);
        return NULL;
    }
    json_t *dicts = json_array();
    for (size_t i = 0; i < specs->c.count; i++) {
        json_t *d = sm_message_dict(specs->c.items[i], raise_out);
        if (*raise_out) { json_free(dicts); return NULL; }
        json_append(dicts, d);
    }
    double patience = json_get_num(in, "patience_s", 30.0);
    json_t *guards = json_object();
    json_set(guards, "turn_lease_holder", sm_cp(json_obj_get(in, "turn_lease_holder")));
    const json_t *ttl = json_obj_get(in, "turn_lease_ttl_seconds");
    json_set(guards, "turn_lease_ttl_seconds", ttl ? json_copy(ttl) : json_number(300.0));
    /* chunks = [dicts]; chunk_rows splits only when len(dicts) > chunk_rows. */
    json_t *chunks = json_array();
    const json_t *cr = json_obj_get(in, "chunk_rows");
    bool split = false;
    size_t step = 0;
    if (!sm_is_null(cr)) {
        double crv;
        if (cr->type == JSON_BOOL) crv = cr->bool_val ? 1 : 0;
        else if (cr->type == JSON_NUMBER) crv = cr->num_val;
        else {
            sm_raise_fmt(raise_out, "TypeError",
                         "'>' not supported between instances of 'int' and '%s'",
                         sm_typename(cr));
            json_free(dicts); json_free(guards); json_free(chunks);
            return NULL;
        }
        split = (double)dicts->c.count > crv;
    }
    if (split) {
        json_t *index = json_object();
        for (size_t i = 0; i < dicts->c.count; i++) {
            json_t *msg = dicts->c.items[i];
            json_t *pa = json_object();
            json_set(pa, "tool_calls", sm_cp(json_obj_get(msg, "tool_calls")));
            json_t *parsed = sm_seam(io, "_parse_tool_calls", pa, raise_out);
            if (*raise_out) { json_free(index); json_free(dicts); json_free(guards); json_free(chunks); return NULL; }
            json_t *sa = json_object();
            json_set(sa, "msg", json_copy(msg));
            json_set(sa, "tool_calls", sm_cp(parsed));
            json_free(parsed);
            json_set(sa, "index", json_copy(index));
            json_t *script = sm_seam(io, "_stamp_tool_call_uids", sa, raise_out);
            if (*raise_out) { json_free(index); json_free(dicts); json_free(guards); json_free(chunks); return NULL; }
            if (script && script->type != JSON_OBJECT) {
                sm_get_attr_err(raise_out, script);
                json_free(script); json_free(index); json_free(dicts); json_free(guards); json_free(chunks);
                return NULL;
            }
            if (script) {
                sm_obj_update(msg, json_obj_get(script, "msg"));
                const json_t *ni = json_obj_get(script, "index");
                json_t *new_index = (ni && ni->type == JSON_OBJECT) ? json_copy(ni) : json_object();
                json_free(index);
                index = new_index;
            }
            json_free(script);
        }
        json_free(index);
        double stepv = (cr->type == JSON_BOOL) ? (cr->bool_val ? 1 : 0) : cr->num_val;
        if (stepv != trunc(stepv)) {
            /* range(0, n, <float>) refuses a non-integral step */
            sm_raise(raise_out, "TypeError",
                     "'float' object cannot be interpreted as an integer");
            json_free(dicts); json_free(guards); json_free(chunks);
            return NULL;
        }
        if (stepv == 0) {
            /* range(0, n, 0) raises in the live chunk split */
            sm_raise(raise_out, "ValueError", "range() arg 3 must not be zero");
            json_free(dicts); json_free(guards); json_free(chunks);
            return NULL;
        }
        if (stepv > 0) {
            step = (size_t)stepv;
            for (size_t start = 0; start < dicts->c.count; start += step) {
                json_t *chunk = json_array();
                for (size_t j = start; j < dicts->c.count && j < start + step; j++)
                    json_append(chunk, json_copy(dicts->c.items[j]));
                json_append(chunks, chunk);
            }
        }
        /* a negative step leaves range() empty: no chunks, no writes */
    } else {
        json_append(chunks, json_copy(dicts));
    }
    json_t *committed = json_array();
    sm_refusals_enter(io, sid, raise_out);
    if (!*raise_out) {
        for (size_t i = 0; i < chunks->c.count && !*raise_out; i++) {
            json_t *chunk = chunks->c.items[i];
            size_t start = split ? i * step : 0;
            json_t *inserted = sm_append_chunk(io, sid, chunk, guards, patience, raise_out);
            /* live chunks SHARE the batch's dict objects: every stamp the
             * write applied lands on the batch dicts (the uids read them). */
            for (size_t j = 0; j < chunk->c.count; j++) {
                json_free(dicts->c.items[start + j]);
                dicts->c.items[start + j] = json_copy(chunk->c.items[j]);
            }
            if (*raise_out) { json_free(inserted); break; }
            json_t *outcomes = sm_row_outcomes(chunk, inserted, raise_out);
            json_free(inserted);
            if (*raise_out) { json_free(outcomes); break; }
            for (size_t j = 0; j < outcomes->c.count; j++)
                json_append(committed, json_copy(outcomes->c.items[j]));
            json_free(outcomes);
        }
    }
    sm_refusals_exit(io, sid, *raise_out);
    sm_refusals_transform(io, sid, raise_out);
    json_free(chunks);
    json_free(guards);
    if (*raise_out) {
        /* except BaseException: attach the progress, then re-raise. */
        json_t *uids = json_array();
        for (size_t i = 0; i < dicts->c.count; i++)
            json_append(uids, sm_row_uids(dicts->c.items[i]));
        json_t *bp = json_object();
        json_set(bp, "outcomes", json_copy(committed));
        json_set(bp, "uids", uids);
        json_set(*raise_out, "batch_progress", bp);
        json_free(committed);
        json_free(dicts);
        return NULL;
    }
    json_free(dicts);
    json_t *ret = json_object();
    json_set(ret, "outcomes", committed);
    return ret;
}

/* PoP: sm_replace_transcript @ persistence/sqlite/conversation/messages.py:replace_transcript */
static json_t *sm_replace_transcript(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *mode = json_obj_get(in, "mode");
    if (!mode || mode->type != JSON_STRING
        || (strcmp(mode->str_val, "destructive") != 0
            && strcmp(mode->str_val, "preserve_existing_archive") != 0
            && strcmp(mode->str_val, "archive_dropped") != 0)) {
        sm_enum_invalid(raise_out, "ReplaceMode", mode);
        return NULL;
    }
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    const json_t *specs = json_obj_get(in, "messages");
    if (!specs || specs->type != JSON_ARRAY) {
        sm_iter_err(raise_out, specs);
        return NULL;
    }
    json_t *dicts = json_array();
    for (size_t i = 0; i < specs->c.count; i++) {
        json_t *d = sm_message_dict(specs->c.items[i], raise_out);
        if (*raise_out) { json_free(dicts); return NULL; }
        json_obj_del(d, "_db_row_snapshot"); /* a replace stamps no kept-row version */
        json_append(dicts, d);
    }
    sm_refusals_enter(io, sid, raise_out);
    if (*raise_out) { json_free(dicts); return NULL; }
    json_t *a = json_object();
    json_t *pos = json_array();
    json_append(pos, sm_cp(sid_node));
    json_append(pos, json_copy(dicts));
    json_set(a, "args", pos);
    json_t *kw = json_object();
    json_set(kw, "active_only", json_bool(strcmp(mode->str_val, "preserve_existing_archive") == 0));
    json_set(kw, "archive_dropped", json_bool(strcmp(mode->str_val, "archive_dropped") == 0));
    json_set(kw, "reject_active_turn_lease", json_bool(json_get_bool(in, "reject_active_turn_lease", false)));
    json_set(a, "kwargs", kw);
    json_t *script = sm_seam(io, "replace_messages", a, raise_out);
    if (!*raise_out) {
        if (script && script->type != JSON_OBJECT) sm_get_attr_err(raise_out, script);
        else sm_apply_stamps(dicts, script);
    }
    json_free(script);
    /* kept = any(m.get("timestamp") is None or not m.get("message_uid")) */
    bool kept = false;
    if (!*raise_out) {
        for (size_t i = 0; i < dicts->c.count && !kept; i++) {
            const json_t *m = dicts->c.items[i];
            kept = sm_is_null(json_obj_get(m, "timestamp"))
                   || !sm_truth(json_obj_get(m, "message_uid"));
        }
    }
    json_t *stored = json_object();
    if (!*raise_out && kept) {
        json_t *ga = json_object();
        json_t *gpos = json_array();
        json_append(gpos, sm_cp(sid_node));
        json_set(ga, "args", gpos);
        json_set(ga, "kwargs", json_object());
        json_t *rows = sm_seam(io, "get_messages", ga, raise_out);
        if (!*raise_out) {
            if (!rows || rows->type != JSON_ARRAY) {
                sm_iter_err(raise_out, rows);
            } else {
                for (size_t i = 0; i < rows->c.count && !*raise_out; i++) {
                    const json_t *r = rows->c.items[i];
                    if (!r || r->type != JSON_OBJECT) { sm_subscript_err(raise_out, r); break; }
                    if (!json_has(r, "id")) { sm_raise_key(raise_out, "id"); break; }
                    double rid = sm_py_int(json_obj_get(r, "id"), raise_out);
                    if (*raise_out) break;
                    char keybuf[64];
                    snprintf(keybuf, sizeof(keybuf), "%.0f", rid);
                    json_set(stored, keybuf, json_copy(r));
                }
            }
        }
        json_free(rows);
    }
    sm_refusals_exit(io, sid, *raise_out);
    sm_refusals_transform(io, sid, raise_out);
    if (*raise_out) { json_free(dicts); json_free(stored); return NULL; }
    /* ReplaceResult rows are stamped OUTSIDE the with block. */
    json_t *rows_out = json_array();
    for (size_t i = 0; i < dicts->c.count && !*raise_out; i++) {
        json_t *s = sm_stamp(dicts->c.items[i], stored, raise_out);
        if (*raise_out) { json_free(s); break; }
        json_append(rows_out, s);
    }
    json_free(dicts);
    json_free(stored);
    if (*raise_out) { json_free(rows_out); return NULL; }
    json_t *ret = json_object();
    json_set(ret, "rows", rows_out);
    return ret;
}

static const char *SM_WM_FIELDS[] = { "last_active" };
static const char *SM_HC_FIELDS[] = { "covered_ids", "unresolved" };

/* Convert a held-spec array with _held_dict (the kwarg-position comprehension). */
static json_t *sm_held_list(const json_t *specs, json_t **raise_out) {
    json_t *out = json_array();
    if (sm_is_null(specs)) return out; /* drive materializes () for null */
    if (specs->type != JSON_ARRAY) {
        sm_iter_err(raise_out, specs);
        json_free(out); return NULL;
    }
    for (size_t i = 0; i < specs->c.count; i++) {
        json_t *d = sm_held_dict(specs->c.items[i], raise_out);
        if (*raise_out) { json_free(out); return NULL; }
        json_append(out, d);
    }
    return out;
}

/* Construction-only check for a held-spec array (drive materialization). */
static bool sm_held_list_check(const json_t *specs, json_t **raise_out) {
    if (sm_is_null(specs)) return true;
    if (specs->type != JSON_ARRAY) { sm_iter_err(raise_out, specs); return false; }
    for (size_t i = 0; i < specs->c.count; i++) {
        if (!sm_fields_check(specs->c.items[i], "HeldMessage", SM_HM_FIELDS,
                             sizeof(SM_HM_FIELDS) / sizeof(SM_HM_FIELDS[0]),
                             SM_HM_REQ, 2, raise_out)) return false;
    }
    return true;
}

/* PoP: sm_compact @ persistence/sqlite/conversation/messages.py:compact */
static json_t *sm_compact(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    const json_t *specs = json_obj_get(in, "replacement");
    if (!specs || specs->type != JSON_ARRAY) {
        sm_iter_err(raise_out, specs);
        return NULL;
    }
    json_t *dicts = json_array();
    for (size_t i = 0; i < specs->c.count; i++) {
        json_t *d = sm_message_dict(specs->c.items[i], raise_out);
        if (*raise_out) { json_free(dicts); return NULL; }
        json_append(dicts, d);
    }
    /* drive materialization order: watermark, carried, coverage, held_for_cas */
    const json_t *wm = json_obj_get(in, "watermark");
    if (!sm_is_null(wm)
        && !sm_fields_check(wm, "Watermark", SM_WM_FIELDS, 1, SM_WM_FIELDS, 1, raise_out)) {
        json_free(dicts); return NULL;
    }
    const json_t *carried = json_obj_get(in, "carried");
    if (!sm_held_list_check(carried, raise_out)) { json_free(dicts); return NULL; }
    const json_t *coverage = json_obj_get(in, "coverage");
    if (!sm_is_null(coverage)) {
        if (!sm_fields_check(coverage, "HeldCoverage", SM_HC_FIELDS, 2, NULL, 0, raise_out)) {
            json_free(dicts); return NULL;
        }
        if (!sm_held_list_check(json_obj_get(coverage, "unresolved"), raise_out)) {
            json_free(dicts); return NULL;
        }
    }
    const json_t *hfc = json_obj_get(in, "held_for_cas");
    if (!sm_is_null(hfc) && !sm_held_list_check(hfc, raise_out)) {
        json_free(dicts); return NULL;
    }
    sm_refusals_enter(io, sid, raise_out);
    if (*raise_out) { json_free(dicts); return NULL; }
    /* kwargs in call order; the conversions fire INSIDE the with block. */
    json_t *kw = json_object();
    const json_t *mcp = json_obj_get(in, "model_config_patch");
    json_set(kw, "model_config_patch", sm_is_null(mcp) ? json_null() : json_copy(mcp));
    if (sm_is_null(wm)) {
        json_set(kw, "watermark", json_null());
    } else {
        double w = sm_int_or_0(json_obj_get(wm, "last_active"), raise_out);
        if (*raise_out) { json_free(kw); json_free(dicts); return NULL; }
        json_set(kw, "watermark", json_number(w));
    }
    json_set(kw, "lock_holder", sm_cp(json_obj_get(in, "lock_holder")));
    const json_t *tc = json_obj_get(in, "carried_tail_count");
    json_set(kw, "tail_count", tc ? json_copy(tc) : json_number(0));
    json_t *carried_dicts = sm_held_list(carried, raise_out);
    if (*raise_out) { json_free(kw); json_free(dicts); return NULL; }
    /* [_held_dict(m) for m in carried] or None */
    json_set(kw, "carried_messages",
             carried_dicts->c.count > 0 ? carried_dicts : json_null());
    if (carried_dicts->c.count == 0) json_free(carried_dicts);
    if (sm_is_null(coverage)) {
        json_set(kw, "covered_ids", json_null());
        json_set(kw, "unresolved_held", json_null());
    } else {
        const json_t *cids = json_obj_get(coverage, "covered_ids");
        json_t *ids = json_array();
        if (!sm_is_null(cids)) {
            if (cids->type != JSON_ARRAY) {
                sm_iter_err(raise_out, cids);
                json_free(ids); json_free(kw); json_free(dicts); return NULL;
            }
            for (size_t i = 0; i < cids->c.count; i++) {
                double d = sm_py_int(cids->c.items[i], raise_out);
                if (*raise_out) { json_free(ids); json_free(kw); json_free(dicts); return NULL; }
                json_append(ids, json_number(d));
            }
        }
        json_set(kw, "covered_ids", ids);
        json_t *unres = sm_held_list(json_obj_get(coverage, "unresolved"), raise_out);
        if (*raise_out) { json_free(kw); json_free(dicts); return NULL; }
        json_set(kw, "unresolved_held", unres);
    }
    if (sm_is_null(hfc)) {
        json_set(kw, "held_for_cas", json_null());
    } else {
        json_t *hfc_dicts = sm_held_list(hfc, raise_out);
        if (*raise_out) { json_free(kw); json_free(dicts); return NULL; }
        json_set(kw, "held_for_cas", hfc_dicts);
    }
    json_t *a = json_object();
    json_t *pos = json_array();
    json_append(pos, sm_cp(sid_node));
    json_append(pos, json_copy(dicts));
    json_set(a, "args", pos);
    json_set(a, "kwargs", kw);
    json_t *script = sm_seam(io, "archive_and_compact", a, raise_out);
    json_t *active = NULL;
    if (!*raise_out) {
        if (script && script->type != JSON_OBJECT) {
            sm_get_attr_err(raise_out, script);
        } else if (script) {
            sm_apply_stamps(dicts, script);
            active = sm_cp(json_obj_get(script, "active"));
        }
    }
    json_free(script);
    sm_refusals_exit(io, sid, *raise_out);
    sm_refusals_transform(io, sid, raise_out);
    if (*raise_out) { json_free(active); json_free(dicts); return NULL; }
    /* CompactionResult rows stamped with an EMPTY stored map, outside the with. */
    json_t *empty = json_object();
    json_t *rows_out = json_array();
    for (size_t i = 0; i < dicts->c.count && !*raise_out; i++) {
        json_t *s = sm_stamp(dicts->c.items[i], empty, raise_out);
        if (*raise_out) { json_free(s); break; }
        json_append(rows_out, s);
    }
    json_free(empty);
    json_free(dicts);
    if (*raise_out) { json_free(active); json_free(rows_out); return NULL; }
    json_t *ret = json_object();
    json_set(ret, "active_count", active ? active : json_null());
    json_set(ret, "rows", rows_out);
    return ret;
}

/* PoP: sm_rewrite_pruned_rows @ persistence/sqlite/conversation/messages.py:rewrite_pruned_rows */
static json_t *sm_rewrite_pruned_rows(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    const json_t *changes = json_obj_get(in, "changes");
    if (!changes || changes->type != JSON_ARRAY) {
        sm_iter_err(raise_out, changes);
        return NULL;
    }
    json_t *pairs = json_array();
    for (size_t i = 0; i < changes->c.count; i++) {
        const json_t *c = changes->c.items[i];
        json_t *original = sm_pruned_original(c, raise_out);
        if (*raise_out) { json_free(pairs); return NULL; }
        json_t *replacement = sm_message_dict(
            (c && c->type == JSON_OBJECT) ? json_obj_get(c, "replacement") : NULL, raise_out);
        if (*raise_out) { json_free(original); json_free(pairs); return NULL; }
        json_obj_del(replacement, "_db_row_snapshot");
        json_t *pair = json_array();
        json_append(pair, original);
        json_append(pair, replacement);
        json_append(pairs, pair);
    }
    sm_refusals_enter(io, sid, raise_out);
    if (*raise_out) { json_free(pairs); return NULL; }
    json_t *a = json_object();
    json_t *pos = json_array();
    json_append(pos, sm_cp(sid_node));
    json_append(pos, json_copy(pairs));
    json_set(a, "args", pos);
    json_t *kw = json_object();
    const json_t *mcp = json_obj_get(in, "model_config_patch");
    json_set(kw, "model_config_patch", sm_is_null(mcp) ? json_null() : json_copy(mcp));
    json_set(kw, "turn_lease_holder", sm_cp(json_obj_get(in, "turn_lease_holder")));
    const json_t *ttl = json_obj_get(in, "turn_lease_ttl_seconds");
    json_set(kw, "turn_lease_ttl_seconds", ttl ? json_copy(ttl) : json_number(300.0));
    json_set(a, "kwargs", kw);
    json_t *script = sm_seam(io, "rewrite_pruned_rows", a, raise_out);
    json_t *rewritten = NULL;
    if (!*raise_out) {
        if (script && script->type != JSON_OBJECT) {
            sm_get_attr_err(raise_out, script);
        } else if (script) {
            const json_t *stamps = json_obj_get(script, "stamps");
            if (stamps && stamps->type == JSON_ARRAY) {
                size_t n = stamps->c.count < pairs->c.count ? stamps->c.count : pairs->c.count;
                for (size_t i = 0; i < n; i++)
                    sm_obj_update(pairs->c.items[i]->c.items[1], stamps->c.items[i]);
            }
            rewritten = sm_cp(json_obj_get(script, "rewritten"));
        }
    }
    json_free(script);
    sm_refusals_exit(io, sid, *raise_out);
    sm_refusals_transform(io, sid, raise_out);
    if (*raise_out) { json_free(rewritten); json_free(pairs); return NULL; }
    json_t *snapshots = json_array();
    for (size_t i = 0; i < pairs->c.count; i++)
        json_append(snapshots, sm_cp(json_obj_get(pairs->c.items[i]->c.items[1], "_db_row_snapshot")));
    json_free(pairs);
    json_t *ret = json_object();
    json_set(ret, "rewritten", rewritten ? rewritten : json_null());
    json_set(ret, "snapshots", snapshots);
    return ret;
}

/* PoP: sm_set_reaction @ persistence/sqlite/conversation/messages.py:set_reaction */
static json_t *sm_set_reaction(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    sm_refusals_enter(io, sid, raise_out);
    if (*raise_out) return NULL;
    json_t *a = json_object();
    json_t *pos = json_array();
    json_append(pos, sm_cp(sid_node));
    json_append(pos, sm_cp(json_obj_get(in, "message_id")));
    json_append(pos, sm_cp(json_obj_get(in, "emoji")));
    json_set(a, "args", pos);
    json_t *kw = json_object();
    json_set(kw, "author", sm_cp(json_obj_get(in, "author")));
    json_set(a, "kwargs", kw);
    json_t *reactions = sm_seam(io, "set_message_reaction", a, raise_out);
    sm_refusals_exit(io, sid, *raise_out);
    sm_refusals_transform(io, sid, raise_out);
    if (*raise_out) { json_free(reactions); return NULL; }
    if (sm_is_null(reactions)) { json_free(reactions); return NULL; }
    if (reactions->type != JSON_ARRAY) {
        sm_iter_err(raise_out, reactions);
        json_free(reactions); return NULL;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < reactions->c.count && !*raise_out; i++) {
        const json_t *r = reactions->c.items[i];
        if (!r || r->type != JSON_OBJECT) {
            sm_get_attr_err(raise_out, r);
            break;
        }
        double at = sm_float_or_0(json_obj_get(r, "at"), raise_out);
        if (*raise_out) break;
        json_t *e = json_object();
        json_set(e, "emoji", sm_cp(json_obj_get(r, "emoji")));
        json_set(e, "author", sm_cp(json_obj_get(r, "author")));
        json_set(e, "at", json_number(at));
        json_set(e, "seen", json_bool(sm_truth(json_obj_get(r, "seen"))));
        json_append(out, e);
    }
    json_free(reactions);
    if (*raise_out) { json_free(out); return NULL; }
    return out;
}

/* PoP: sm_take_unseen_reactions @ persistence/sqlite/conversation/messages.py:take_unseen_reactions */
static json_t *sm_take_unseen_reactions(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    sm_refusals_enter(io, sid, raise_out);
    if (*raise_out) return NULL;
    json_t *a = json_object();
    json_t *pos = json_array();
    json_append(pos, sm_cp(sid_node));
    json_set(a, "args", pos);
    json_t *kw = json_object();
    const json_t *author = json_obj_get(in, "author");
    json_set(kw, "author", author ? json_copy(author) : json_string("user"));
    json_set(a, "kwargs", kw);
    json_t *pending = sm_seam(io, "take_unseen_reactions", a, raise_out);
    sm_refusals_exit(io, sid, *raise_out);
    sm_refusals_transform(io, sid, raise_out);
    if (*raise_out) { json_free(pending); return NULL; }
    if (!pending || pending->type != JSON_ARRAY) {
        sm_iter_err(raise_out, pending);
        json_free(pending); return NULL;
    }
    json_t *out = json_array();
    static const char *KEYS[] = { "row_id", "role", "emoji", "text" };
    for (size_t i = 0; i < pending->c.count && !*raise_out; i++) {
        const json_t *p = pending->c.items[i];
        if (!p || p->type != JSON_OBJECT) {
            sm_subscript_err(raise_out, p);
            break;
        }
        json_t *e = json_object();
        for (size_t j = 0; j < 4; j++) {
            if (!json_has(p, KEYS[j])) { sm_raise_key(raise_out, KEYS[j]); break; }
            json_set(e, KEYS[j], sm_cp(json_obj_get(p, KEYS[j])));
        }
        if (*raise_out) { json_free(e); break; }
        json_append(out, e);
    }
    json_free(pending);
    if (*raise_out) { json_free(out); return NULL; }
    return out;
}

/* -- repository read flows -- */

static const char *SM_TREQ_FIELDS[] = {
    "view", "limit", "offset", "latest", "after_id", "include_lineage",
};

/* PoP: sm_read_transcript @ persistence/sqlite/conversation/messages.py:read_transcript */
static json_t *sm_read_transcript(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *req = json_obj_get(in, "request");
    if (!sm_is_null(req)
        && !sm_fields_check(req, "TranscriptRead", SM_TREQ_FIELDS, 6, NULL, 0, raise_out))
        return NULL;
    const json_t *view = req ? json_obj_get(req, "view") : NULL;
    if (!sm_is_null(view) && view->type != JSON_STRING) {
        sm_enum_invalid(raise_out, "TranscriptView", view);
        return NULL;
    }
    const char *view_s = sm_is_null(view) ? "active" : view->str_val;
    if (strcmp(view_s, "active") != 0 && strcmp(view_s, "display") != 0
        && strcmp(view_s, "all") != 0) {
        sm_enum_invalid(raise_out, "TranscriptView", view);
        return NULL;
    }
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    sm_refusals_enter(io, sid, raise_out);
    if (*raise_out) return NULL;
    json_t *a = json_object();
    json_t *pos = json_array();
    json_append(pos, sm_cp(sid_node));
    json_set(a, "args", pos);
    json_t *kw = json_object();
    json_set(kw, "include_inactive", json_bool(strcmp(view_s, "all") == 0));
    json_set(kw, "include_compacted", json_bool(strcmp(view_s, "display") == 0));
    json_set(kw, "limit", sm_cp(req ? json_obj_get(req, "limit") : NULL));
    const json_t *offset = req ? json_obj_get(req, "offset") : NULL;
    json_set(kw, "offset", offset ? json_copy(offset) : json_number(0));
    const json_t *latest = req ? json_obj_get(req, "latest") : NULL;
    json_set(kw, "latest", latest ? json_copy(latest) : json_bool(false));
    const json_t *after = req ? json_obj_get(req, "after_id") : NULL;
    if (sm_is_null(after)) {
        json_set(kw, "after_id", json_null());
    } else {
        double d = sm_py_int(after, raise_out);
        if (*raise_out) { json_free(a); sm_refusals_exit(io, sid, *raise_out); sm_refusals_transform(io, sid, raise_out); return NULL; }
        json_set(kw, "after_id", json_number(d));
    }
    const json_t *lineage = req ? json_obj_get(req, "include_lineage") : NULL;
    json_set(kw, "include_ancestors", lineage ? json_copy(lineage) : json_bool(false));
    json_set(a, "kwargs", kw);
    json_t *rows = sm_seam(io, "get_messages", a, raise_out);
    sm_refusals_exit(io, sid, *raise_out);
    sm_refusals_transform(io, sid, raise_out);
    if (*raise_out) { json_free(rows); return NULL; }
    if (!rows || rows->type != JSON_ARRAY) {
        sm_iter_err(raise_out, rows);
        json_free(rows); return NULL;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < rows->c.count && !*raise_out; i++) {
        json_t *r = sm_transcript_row(rows->c.items[i], raise_out);
        if (*raise_out) { json_free(r); break; }
        json_append(out, r);
    }
    json_free(rows);
    if (*raise_out) { json_free(out); return NULL; }
    return out;
}

/* PoP: sm_active_watermark @ persistence/sqlite/conversation/messages.py:active_watermark */
static json_t *sm_active_watermark(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    sm_refusals_enter(io, sid, raise_out);
    if (*raise_out) return NULL;
    json_t *a = json_object();
    json_t *pos = json_array();
    json_append(pos, sm_cp(sid_node));
    json_set(a, "args", pos);
    json_set(a, "kwargs", json_object());
    json_t *last = sm_seam(io, "get_active_message_watermark", a, raise_out);
    sm_refusals_exit(io, sid, *raise_out);
    sm_refusals_transform(io, sid, raise_out);
    if (*raise_out) { json_free(last); return NULL; }
    json_t *ret = json_object();
    json_set(ret, "last_active", sm_truth(last) ? last : json_null());
    if (!sm_truth(last)) json_free(last);
    return ret;
}

/* PoP: sm_conversation_revision @ persistence/sqlite/conversation/messages.py:conversation_revision */
static json_t *sm_conversation_revision(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    sm_refusals_enter(io, sid, raise_out);
    if (*raise_out) return NULL;
    json_t *a = json_object();
    json_t *pos = json_array();
    json_append(pos, sm_cp(sid_node));
    json_set(a, "args", pos);
    json_t *kw = json_object();
    json_set(kw, "include_ancestors", json_bool(json_get_bool(in, "include_lineage", false)));
    json_set(a, "kwargs", kw);
    json_t *r = sm_seam(io, "conversation_revision", a, raise_out);
    /* ids, count, max_id, revision = <the 4-tuple> -- INSIDE the with block. */
    if (!*raise_out) {
        if (!r || r->type != JSON_ARRAY) {
            sm_raise_fmt(raise_out, "TypeError", "cannot unpack non-iterable %s object",
                         sm_typename(r));
        } else if (r->c.count != 4) {
            sm_raise_fmt(raise_out, "ValueError",
                         r->c.count < 4 ? "not enough values to unpack (expected 4, got %zu)"
                                        : "too many values to unpack (expected 4, got %zu)",
                         r->c.count);
        }
    }
    sm_refusals_exit(io, sid, *raise_out);
    sm_refusals_transform(io, sid, raise_out);
    if (*raise_out) { json_free(r); return NULL; }
    const json_t *ids = r->c.items[0];
    if (!ids || ids->type != JSON_ARRAY) {
        sm_iter_err(raise_out, ids);
        json_free(r); return NULL;
    }
    json_t *out = json_object();
    json_set(out, "lineage_ids", json_copy(ids));
    json_set(out, "active_count", sm_cp(r->c.items[1]));
    json_set(out, "max_active_id", sm_cp(r->c.items[2]));
    json_set(out, "transcript_revision", sm_cp(r->c.items[3]));
    json_free(r);
    return out;
}

/* PoP: sm_read_around @ persistence/sqlite/conversation/messages.py:read_around */
static json_t *sm_read_around(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    sm_refusals_enter(io, sid, raise_out);
    if (*raise_out) return NULL;
    json_t *a = json_object();
    json_t *pos = json_array();
    json_append(pos, sm_cp(sid_node));
    json_append(pos, sm_cp(json_obj_get(in, "anchor")));
    json_set(a, "args", pos);
    json_t *kw = json_object();
    const json_t *window = json_obj_get(in, "window");
    json_set(kw, "window", window ? json_copy(window) : json_number(5));
    json_set(a, "kwargs", kw);
    json_t *around = sm_seam(io, "get_messages_around", a, raise_out);
    sm_refusals_exit(io, sid, *raise_out);
    sm_refusals_transform(io, sid, raise_out);
    if (*raise_out) { json_free(around); return NULL; }
    if (!around || around->type != JSON_OBJECT) {
        sm_subscript_err(raise_out, around);
        json_free(around); return NULL;
    }
    if (!json_has(around, "window")) { sm_raise_key(raise_out, "window"); json_free(around); return NULL; }
    const json_t *win = json_obj_get(around, "window");
    if (!win || win->type != JSON_ARRAY) {
        sm_iter_err(raise_out, win);
        json_free(around); return NULL;
    }
    json_t *records = json_array();
    for (size_t i = 0; i < win->c.count && !*raise_out; i++) {
        json_t *rec = sm_message_record(win->c.items[i], raise_out);
        if (*raise_out) { json_free(rec); break; }
        json_append(records, rec);
    }
    if (*raise_out) { json_free(records); json_free(around); return NULL; }
    if (!json_has(around, "messages_before")) { sm_raise_key(raise_out, "messages_before"); json_free(records); json_free(around); return NULL; }
    if (!json_has(around, "messages_after")) { sm_raise_key(raise_out, "messages_after"); json_free(records); json_free(around); return NULL; }
    json_t *out = json_object();
    json_set(out, "window", records);
    json_set(out, "before", sm_cp(json_obj_get(around, "messages_before")));
    json_set(out, "after", sm_cp(json_obj_get(around, "messages_after")));
    json_free(around);
    return out;
}

/* PoP: sm_read_session_timeline @ persistence/sqlite/conversation/messages.py:read_session_timeline */
static json_t *sm_read_session_timeline(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    sm_refusals_enter(io, sid, raise_out);
    if (*raise_out) return NULL;
    json_t *a = json_object();
    json_t *pos = json_array();
    json_append(pos, sm_cp(sid_node));
    json_set(a, "args", pos);
    json_t *kw = json_object();
    const json_t *limit = json_obj_get(in, "limit");
    json_set(kw, "limit", limit ? json_copy(limit) : json_number(500));
    const json_t *after = json_obj_get(in, "after_row_id");
    json_set(kw, "after_row_id", after ? json_copy(after) : json_number(0));
    json_set(a, "kwargs", kw);
    json_t *result = sm_seam(io, "get_session_timeline", a, raise_out);
    sm_refusals_exit(io, sid, *raise_out);
    sm_refusals_transform(io, sid, raise_out);
    if (*raise_out) { json_free(result); return NULL; }
    if (!result || result->type != JSON_OBJECT) {
        sm_subscript_err(raise_out, result);
        json_free(result); return NULL;
    }
    if (!json_has(result, "pagination")) { sm_raise_key(raise_out, "pagination"); json_free(result); return NULL; }
    const json_t *page = json_obj_get(result, "pagination");
    if (!json_has(result, "entries")) { sm_raise_key(raise_out, "entries"); json_free(result); return NULL; }
    const json_t *entries = json_obj_get(result, "entries");
    if (!entries || entries->type != JSON_ARRAY) {
        sm_iter_err(raise_out, entries);
        json_free(result); return NULL;
    }
    json_t *entries_out = json_array();
    static const char *EKEYS[] = { "row_id", "preview", "timestamp" };
    for (size_t i = 0; i < entries->c.count && !*raise_out; i++) {
        const json_t *e = entries->c.items[i];
        if (!e || e->type != JSON_OBJECT) { sm_subscript_err(raise_out, e); break; }
        json_t *eo = json_object();
        for (size_t j = 0; j < 3; j++) {
            if (!json_has(e, EKEYS[j])) { sm_raise_key(raise_out, EKEYS[j]); break; }
            json_set(eo, EKEYS[j], sm_cp(json_obj_get(e, EKEYS[j])));
        }
        if (*raise_out) { json_free(eo); break; }
        json_append(entries_out, eo);
    }
    if (*raise_out) { json_free(entries_out); json_free(result); return NULL; }
    if (!page || page->type != JSON_OBJECT) {
        sm_subscript_err(raise_out, page);
        json_free(entries_out); json_free(result); return NULL;
    }
    static const char *PKEYS[] = {
        "limit", "after_row_id", "returned", "total", "has_more", "next_cursor",
    };
    json_t *out = json_object();
    json_set(out, "entries", entries_out);
    for (size_t j = 0; j < 6 && !*raise_out; j++) {
        if (!json_has(page, PKEYS[j])) { sm_raise_key(raise_out, PKEYS[j]); break; }
        json_set(out, PKEYS[j], sm_cp(json_obj_get(page, PKEYS[j])));
    }
    json_free(result);
    if (*raise_out) { json_free(out); return NULL; }
    return out;
}

/* PoP: sm_read_prompt_page @ persistence/sqlite/conversation/messages.py:read_prompt_page */
static json_t *sm_read_prompt_page(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    sm_refusals_enter(io, sid, raise_out);
    if (*raise_out) return NULL;
    json_t *a = json_object();
    json_t *pos = json_array();
    json_append(pos, sm_cp(sid_node));
    json_append(pos, sm_cp(json_obj_get(in, "row_id")));
    json_set(a, "args", pos);
    json_t *kw = json_object();
    const json_t *limit = json_obj_get(in, "limit");
    json_set(kw, "limit", limit ? json_copy(limit) : json_number(120));
    json_set(a, "kwargs", kw);
    json_t *result = sm_seam(io, "get_session_messages_around", a, raise_out);
    sm_refusals_exit(io, sid, *raise_out);
    sm_refusals_transform(io, sid, raise_out);
    if (*raise_out) { json_free(result); return NULL; }
    if (sm_is_null(result)) { json_free(result); return NULL; }
    if (result->type != JSON_OBJECT) {
        sm_subscript_err(raise_out, result);
        json_free(result); return NULL;
    }
    if (!json_has(result, "pagination")) { sm_raise_key(raise_out, "pagination"); json_free(result); return NULL; }
    const json_t *page = json_obj_get(result, "pagination");
    if (!json_has(result, "messages")) { sm_raise_key(raise_out, "messages"); json_free(result); return NULL; }
    const json_t *msgs = json_obj_get(result, "messages");
    if (!msgs || msgs->type != JSON_ARRAY) {
        sm_iter_err(raise_out, msgs);
        json_free(result); return NULL;
    }
    json_t *rows_out = json_array();
    for (size_t i = 0; i < msgs->c.count && !*raise_out; i++) {
        json_t *r = sm_transcript_row(msgs->c.items[i], raise_out);
        if (*raise_out) { json_free(r); break; }
        json_append(rows_out, r);
    }
    if (*raise_out) { json_free(rows_out); json_free(result); return NULL; }
    if (!page || page->type != JSON_OBJECT) {
        sm_subscript_err(raise_out, page);
        json_free(rows_out); json_free(result); return NULL;
    }
    static const char *PKEYS[] = {
        "row_id", "limit", "returned", "offset", "total", "has_older", "has_newer",
    };
    json_t *out = json_object();
    json_set(out, "messages", rows_out);
    for (size_t j = 0; j < 7 && !*raise_out; j++) {
        if (!json_has(page, PKEYS[j])) { sm_raise_key(raise_out, PKEYS[j]); break; }
        json_set(out, PKEYS[j], sm_cp(json_obj_get(page, PKEYS[j])));
    }
    json_free(result);
    if (*raise_out) { json_free(out); return NULL; }
    return out;
}

/* PoP: sm_read_compression_cooldown_snapshot @ persistence/sqlite/conversation/messages.py:read_compression_cooldown_snapshot */
static json_t *sm_read_compression_cooldown_snapshot(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    sm_refusals_enter(io, sid, raise_out);
    if (*raise_out) return NULL;
    json_t *a = json_object();
    json_t *pos = json_array();
    json_append(pos, sm_cp(sid_node));
    json_set(a, "args", pos);
    json_set(a, "kwargs", json_object());
    json_t *row = sm_seam(io, "get_compression_failure_cooldown_row", a, raise_out);
    sm_refusals_exit(io, sid, *raise_out);
    sm_refusals_transform(io, sid, raise_out);
    if (*raise_out) { json_free(row); return NULL; }
    if (!row || row->type != JSON_OBJECT) {
        sm_subscript_err(raise_out, row);
        json_free(row); return NULL;
    }
    json_t *out = json_object();
    if (!sm_truth(json_obj_get(row, "session_exists"))) {
        json_set(out, "present", json_bool(false));
        json_set(out, "until", json_null());
        json_set(out, "error", json_null());
    } else {
        if (!json_has(row, "cooldown_until")) { sm_raise_key(raise_out, "cooldown_until"); json_free(row); json_free(out); return NULL; }
        if (!json_has(row, "error")) { sm_raise_key(raise_out, "error"); json_free(row); json_free(out); return NULL; }
        json_set(out, "present", json_bool(true));
        json_set(out, "until", sm_cp(json_obj_get(row, "cooldown_until")));
        json_set(out, "error", sm_cp(json_obj_get(row, "error")));
    }
    json_free(row);
    return out;
}

/* PoP: sm_read_compression_cooldown @ persistence/sqlite/conversation/messages.py:read_compression_cooldown */
static json_t *sm_read_compression_cooldown(const json_t *in, sm_io_t *io, json_t **raise_out) {
    json_t *snapshot = sm_read_compression_cooldown_snapshot(in, io, raise_out);
    if (*raise_out) return NULL;
    const json_t *until = json_obj_get(snapshot, "until");
    if (sm_is_null(until)) { json_free(snapshot); return NULL; }
    double u = sm_py_float(until, raise_out);
    if (*raise_out) { json_free(snapshot); return NULL; }
    const json_t *at = json_obj_get(in, "at");
    if (!at || at->type != JSON_NUMBER) {
        sm_raise_fmt(raise_out, "TypeError",
                     "'<=' not supported between instances of 'float' and '%s'",
                     sm_typename(at));
        json_free(snapshot); return NULL;
    }
    if (u <= at->num_val) { json_free(snapshot); return NULL; }
    json_t *out = json_object();
    json_set(out, "until", json_number(u));
    json_set(out, "error", sm_cp(json_obj_get(snapshot, "error")));
    json_free(snapshot);
    return out;
}

static const char *SM_CS_FIELDS[] = { "present", "until", "error" };
static const char *SM_CS_REQ[] = { "present" };

/* PoP: sm_restore_compression_cooldown @ persistence/sqlite/conversation/messages.py:restore_compression_cooldown */
static json_t *sm_restore_compression_cooldown(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *snap = json_obj_get(in, "snapshot");
    if (!sm_fields_check(snap, "CooldownSnapshot", SM_CS_FIELDS, 3, SM_CS_REQ, 1, raise_out))
        return NULL;
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    json_t *row = json_object();
    json_set(row, "session_exists", sm_cp(json_obj_get(snap, "present")));
    json_set(row, "cooldown_until", sm_cp(json_obj_get(snap, "until")));
    json_set(row, "error", sm_cp(json_obj_get(snap, "error")));
    sm_refusals_enter(io, sid, raise_out);
    if (*raise_out) { json_free(row); return NULL; }
    json_t *a = json_object();
    json_t *pos = json_array();
    json_append(pos, sm_cp(sid_node));
    json_append(pos, row);
    json_set(a, "args", pos);
    json_set(a, "kwargs", json_object());
    json_t *r = sm_seam(io, "restore_compression_failure_cooldown_row", a, raise_out);
    json_free(r);
    sm_refusals_exit(io, sid, *raise_out);
    sm_refusals_transform(io, sid, raise_out);
    /* except RuntimeError: a PersistenceError passes; else CooldownRestoreRefused. */
    if (*raise_out && sm_is_runtime(sm_kind(*raise_out))
        && !sm_is_persistence(sm_kind(*raise_out))) {
        json_t *converted = json_object();
        json_set(converted, "__raise__", json_string("CooldownRestoreRefused"));
        json_set(converted, "message", json_string(json_get_str(*raise_out, "message", "")));
        json_free(*raise_out);
        *raise_out = converted;
    }
    if (*raise_out) return NULL;
    return NULL; /* the method returns None on success */
}

/* PoP: sm_latest_row_id @ persistence/sqlite/conversation/messages.py:latest_row_id */
static json_t *sm_latest_row_id(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    sm_refusals_enter(io, sid, raise_out);
    if (*raise_out) return NULL;
    json_t *a = json_object();
    json_t *pos = json_array();
    json_append(pos, sm_cp(sid_node));
    json_set(a, "args", pos);
    json_t *kw = json_object();
    const json_t *role = json_obj_get(in, "role");
    json_set(kw, "role", role ? json_copy(role) : json_string("user"));
    const json_t *offset = json_obj_get(in, "offset");
    json_set(kw, "offset", offset ? json_copy(offset) : json_number(0));
    const json_t *rt = json_obj_get(in, "require_text");
    json_set(kw, "require_text", rt ? json_copy(rt) : json_bool(true));
    json_set(a, "kwargs", kw);
    json_t *r = sm_seam(io, "latest_message_row_id", a, raise_out);
    sm_refusals_exit(io, sid, *raise_out);
    sm_refusals_transform(io, sid, raise_out);
    if (*raise_out) { json_free(r); return NULL; }
    return r ? r : json_null();
}

/* PoP: sm_message_storage_state @ persistence/sqlite/conversation/messages.py:message_storage_state */
static json_t *sm_message_storage_state(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *mid = json_obj_get(in, "message_id");
    if (!sm_truth(mid)) return NULL; /* before holder()/refusals */
    sm_refusals_enter(io, NULL, raise_out);
    if (*raise_out) return NULL;
    json_t *a = json_object();
    json_set(a, "sql", json_string(
        "SELECT session_id, active, compacted FROM messages WHERE id = ?"));
    json_t *params = json_array();
    json_append(params, json_copy(mid));
    json_set(a, "params", params);
    json_t *row = sm_seam(io, "conn.execute", a, raise_out);
    sm_refusals_exit(io, NULL, *raise_out);
    sm_refusals_transform(io, NULL, raise_out);
    if (*raise_out) { json_free(row); return NULL; }
    if (sm_is_null(row)) { json_free(row); return NULL; }
    if (row->type != JSON_OBJECT) {
        sm_subscript_err(raise_out, row);
        json_free(row); return NULL;
    }
    if (!json_has(row, "session_id")) { sm_raise_key(raise_out, "session_id"); json_free(row); return NULL; }
    if (!json_has(row, "active")) { sm_raise_key(raise_out, "active"); json_free(row); return NULL; }
    if (!json_has(row, "compacted")) { sm_raise_key(raise_out, "compacted"); json_free(row); return NULL; }
    json_t *out = json_object();
    json_set(out, "session_id", sm_cp(json_obj_get(row, "session_id")));
    json_set(out, "visibility",
             sm_visibility(json_obj_get(row, "active"), json_obj_get(row, "compacted")));
    json_free(row);
    return out;
}

static const char *SM_RP_FIELDS[] = {
    "target_row_id", "expected_active_ids", "expected_target_content", "scaffold",
    "prefix", "durable_prefix", "live_view", "live_text", "durable_user",
    "user_ordinal",
};

/* PoP: sm_plan_user_turn_rewind @ persistence/sqlite/conversation/messages.py:plan_user_turn_rewind */
static json_t *sm_plan_user_turn_rewind(const json_t *in, sm_io_t *io, json_t **raise_out) {
    const json_t *sid_node = json_obj_get(in, "session_id");
    const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
    /* warm_history=None if None else [dict(m) for m in warm_history] */
    const json_t *wh = json_obj_get(in, "warm_history");
    json_t *warm = NULL;
    if (!sm_is_null(wh)) {
        if (wh->type != JSON_ARRAY) {
            sm_iter_err(raise_out, wh);
            return NULL;
        }
        warm = json_array();
        for (size_t i = 0; i < wh->c.count; i++) {
            const json_t *m = wh->c.items[i];
            if (!m || m->type != JSON_OBJECT) {
                sm_iter_err(raise_out, m);
                json_free(warm); return NULL;
            }
            json_append(warm, json_copy(m));
        }
    }
    sm_refusals_enter(io, sid, raise_out);
    if (*raise_out) { json_free(warm); return NULL; }
    json_t *a = json_object();
    json_t *pos = json_array();
    json_append(pos, sm_cp(sid_node));
    json_append(pos, sm_cp(json_obj_get(in, "user_ordinal")));
    json_set(a, "args", pos);
    json_t *kw = json_object();
    json_set(kw, "warm_history", warm ? warm : json_null());
    json_set(kw, "require_retryable", json_bool(json_get_bool(in, "require_retryable", false)));
    json_set(kw, "require_composite", json_bool(json_get_bool(in, "require_composite", false)));
    json_set(a, "kwargs", kw);
    json_t *plan = sm_seam(io, "plan_user_turn_rewind", a, raise_out);
    /* the ret materializes as the contract RewindPlan INSIDE the with block:
     * all 10 fields required, unknown keys refused. */
    if (!*raise_out
        && !sm_fields_check(plan, "RewindPlan", SM_RP_FIELDS, 10, SM_RP_FIELDS, 10, raise_out)) {
        json_free(plan);
        plan = NULL;
    }
    sm_refusals_exit(io, sid, *raise_out);
    sm_refusals_transform(io, sid, raise_out);
    if (*raise_out) { json_free(plan); return NULL; }
    json_t *out = json_object();
    json_set(out, "target_row_id", sm_cp(json_obj_get(plan, "target_row_id")));
    static const char *ITER_KEYS[] = {
        "expected_active_ids", "prefix", "durable_prefix", "durable_user",
    };
    /* kwarg order: expected_active_ids iterate, content, scaffold, prefix
     * iterate, durable_prefix iterate, live_view, live_text, durable_user
     * iterate, user_ordinal. */
    const json_t *eai = json_obj_get(plan, "expected_active_ids");
    if (!eai || eai->type != JSON_ARRAY) {
        sm_iter_err(raise_out, eai);
        json_free(out); json_free(plan); return NULL;
    }
    json_set(out, "expected_active_ids", json_copy(eai));
    json_set(out, "expected_target_content", sm_cp(json_obj_get(plan, "expected_target_content")));
    json_set(out, "scaffold", sm_cp(json_obj_get(plan, "scaffold")));
    for (size_t k = 1; k < 3; k++) { /* prefix, durable_prefix */
        const json_t *v = json_obj_get(plan, ITER_KEYS[k]);
        if (!v || v->type != JSON_ARRAY) {
            sm_iter_err(raise_out, v);
            json_free(out); json_free(plan); return NULL;
        }
        json_set(out, ITER_KEYS[k], json_copy(v));
    }
    json_set(out, "live_view", sm_cp(json_obj_get(plan, "live_view")));
    json_set(out, "live_text", sm_cp(json_obj_get(plan, "live_text")));
    const json_t *du = json_obj_get(plan, "durable_user");
    if (!du || du->type != JSON_ARRAY) {
        sm_iter_err(raise_out, du);
        json_free(out); json_free(plan); return NULL;
    }
    json_set(out, "durable_user", json_copy(du));
    json_set(out, "user_ordinal", sm_cp(json_obj_get(plan, "user_ordinal")));
    json_free(plan);
    return out;
}

/* -- dispatch -- */

static json_t *sm_dispatch(const char *func, const json_t *in, sm_io_t *io, json_t **raise_out) {
    if (strcmp(func, "_visibility") == 0)
        return sm_visibility(json_obj_get(in, "active"), json_obj_get(in, "compacted"));
    if (strcmp(func, "_sidecar") == 0)
        return sm_sidecar(json_obj_get(in, "value"));
    if (strcmp(func, "_message_dict") == 0)
        return sm_message_dict(json_obj_get(in, "message"), raise_out);
    if (strcmp(func, "_held_dict") == 0)
        return sm_held_dict(json_obj_get(in, "held"), raise_out);
    if (strcmp(func, "_pruned_original") == 0)
        return sm_pruned_original(json_obj_get(in, "change"), raise_out);
    if (strcmp(func, "_transcript_row") == 0)
        return sm_transcript_row(json_obj_get(in, "row"), raise_out);
    if (strcmp(func, "_message_record") == 0)
        return sm_message_record(json_obj_get(in, "row"), raise_out);
    if (strcmp(func, "_stamp") == 0) {
        /* drive passes inp.get("stored") or {}: null/absent reads as {} */
        const json_t *stored = json_obj_get(in, "stored");
        json_t *empty = sm_is_null(stored) ? json_object() : NULL;
        json_t *ret = sm_stamp(json_obj_get(in, "msg"), empty ? empty : stored, raise_out);
        json_free(empty);
        return ret;
    }
    if (strcmp(func, "_row_outcomes") == 0)
        return sm_row_outcomes(json_obj_get(in, "rows"), json_obj_get(in, "count"), raise_out);
    if (strcmp(func, "_conversation") == 0)
        return sm_conversation(json_obj_get(in, "messages"), io, raise_out);
    if (strcmp(func, "_refusals") == 0)
        return sm_refusals(in, io, raise_out);
    if (strcmp(func, "_append_chunk") == 0) {
        const json_t *sid_node = json_obj_get(in, "session_id");
        const char *sid = (sid_node && sid_node->type == JSON_STRING) ? sid_node->str_val : NULL;
        const json_t *guards = json_obj_get(in, "guards");
        json_t *empty = sm_is_null(guards) ? json_object() : NULL;
        json_t *ret = sm_append_chunk(io, sid, json_obj_get(in, "messages"),
                                      empty ? empty : guards,
                                      json_get_num(in, "patience_s", 30.0), raise_out);
        json_free(empty);
        return ret;
    }
    if (strcmp(func, "append") == 0)
        return sm_append(in, io, raise_out);
    if (strcmp(func, "append_batch") == 0)
        return sm_append_batch(in, io, raise_out);
    if (strcmp(func, "replace_transcript") == 0)
        return sm_replace_transcript(in, io, raise_out);
    if (strcmp(func, "compact") == 0)
        return sm_compact(in, io, raise_out);
    if (strcmp(func, "rewrite_pruned_rows") == 0)
        return sm_rewrite_pruned_rows(in, io, raise_out);
    if (strcmp(func, "set_reaction") == 0)
        return sm_set_reaction(in, io, raise_out);
    if (strcmp(func, "take_unseen_reactions") == 0)
        return sm_take_unseen_reactions(in, io, raise_out);
    if (strcmp(func, "read_transcript") == 0)
        return sm_read_transcript(in, io, raise_out);
    if (strcmp(func, "active_watermark") == 0)
        return sm_active_watermark(in, io, raise_out);
    if (strcmp(func, "conversation_revision") == 0)
        return sm_conversation_revision(in, io, raise_out);
    if (strcmp(func, "read_around") == 0)
        return sm_read_around(in, io, raise_out);
    if (strcmp(func, "read_session_timeline") == 0)
        return sm_read_session_timeline(in, io, raise_out);
    if (strcmp(func, "read_prompt_page") == 0)
        return sm_read_prompt_page(in, io, raise_out);
    if (strcmp(func, "read_compression_cooldown_snapshot") == 0)
        return sm_read_compression_cooldown_snapshot(in, io, raise_out);
    if (strcmp(func, "read_compression_cooldown") == 0)
        return sm_read_compression_cooldown(in, io, raise_out);
    if (strcmp(func, "restore_compression_cooldown") == 0)
        return sm_restore_compression_cooldown(in, io, raise_out);
    if (strcmp(func, "latest_row_id") == 0)
        return sm_latest_row_id(in, io, raise_out);
    if (strcmp(func, "message_storage_state") == 0)
        return sm_message_storage_state(in, io, raise_out);
    if (strcmp(func, "plan_user_turn_rewind") == 0)
        return sm_plan_user_turn_rewind(in, io, raise_out);
    sm_raise_fmt(raise_out, "ValueError", "unknown func: %s", func);
    return NULL;
}
