/*
 * errors.c - real C11 port of upstream Hermes persistence/core/errors.py.
 * The taxonomy (parents, inherited `cause`, constructor message formats) is data; the
 * helpers are real functions. Backend I/O does not exist in this module.
 * PoP: w19_errors_c3b6af_cause @ persistence/core/errors.py:<class>.cause
 * PoP: w19_errors_c3b6af_message @ persistence/core/errors.py:<class>.__init__
 * PoP: w19_errors_c3b6af_one_line @ persistence/core/errors.py:_one_line
 * PoP: w19_errors_c3b6af_busy_refusal @ persistence/core/errors.py:busy_refusal
 * PoP: w19_errors_c3b6af_rewind_text @ persistence/core/errors.py:_REWIND_REFUSAL_TEXT
 */
#include "noc_w19_errors_c3b6af.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    const char *parent;   /* "" for the root */
    const char *cause;    /* "" inherits from the parent chain */
    const char *fmt;      /* NULL: class-specific fallback */
    int takes_message;    /* _SessionConflict subclasses with a message= parameter */
} err_cls;

static const err_cls CLASSES[] = {
    {"PersistenceError",             "",                        "unknown",             NULL, 0},
    {"NotFound",                     "PersistenceError",        "",                    "%e %I not found in scope", 0},
    {"SessionRowMissing",            "NotFound",                "session_row_missing", "FOREIGN KEY constraint failed: session %R has no row", 0},
    {"Conflict",                     "PersistenceError",        "",                    "conflicting concurrent state on %e %I", 0},
    {"_SessionConflict",             "Conflict",                "",                    NULL, 0},
    {"CompressionClosed",            "Conflict",                "compression_closed",  "Session %R is closed by compression; adopt its live continuation before appending messages", 0},
    {"CompressionBusy",              "_SessionConflict",        "compression",         "Compression lease lost before publication: %s", 1},
    {"CompressionInProgress",        "CompressionBusy",         "",                    "Session %R is being compressed by another writer", 0},
    {"TurnLeaseLost",                "_SessionConflict",        "turn_lease",          "Session turn lease lost; refusing transcript write for %R", 0},
    {"SessionWriteGuarded",          "_SessionConflict",        "",                    "session '%s' (or a delegate child) has an active turn lease or compression lock", 0},
    {"HeldRowRewritten",             "_SessionConflict",        "",                    "a held row of session %s was rewritten in place before the commit", 0},
    {"PruneRowUnresolved",           "_SessionConflict",        "",                    "a pruned message of session %s is not rewritten in place", 0},
    {"PruneRowStale",                "PruneRowUnresolved",      "",                    "a row of session %s no longer holds the pruned message", 0},
    {"CooldownRestoreRefused",       "_SessionConflict",        "",                    NULL, 1},
    {"CompressionPublishRefused",    "_SessionConflict",        "",                    NULL, 1},
    {"SessionLedgered",              "_SessionConflict",        "",                    "%s has gateway admission/worker history; not moved", 0},
    {"RewindRefused",                "Conflict",                "",                    NULL, 1},
    {"TransientStoreError",          "PersistenceError",        "disk",                NULL, 0},
    {"StoreContention",              "TransientStoreError",     "locked",              NULL, 0},
    {"DiskFull",                     "TransientStoreError",     "",                    NULL, 0},
    {"PermanentStoreError",          "PersistenceError",        "",                    NULL, 0},
    {"CorruptStoreError",            "PermanentStoreError",     "corrupt",             NULL, 0},
    {"StoreQuarantined",             "CorruptStoreError",       "",                    NULL, 0},
    {"MalformedSchema",              "CorruptStoreError",       "",                    NULL, 0},
    {"SearchIndexCorrupt",           "CorruptStoreError",       "fts_index",           NULL, 0},
    {"DeclarativeDrift",             "PermanentStoreError",     "",                    "%d", 0},
    {"StoreUnavailable",             "PersistenceError",        "",                    NULL, 0},
    {"StoreReplaced",                "PersistenceError",        "replaced",            NULL, 0},
    {"DeletedWalGeneration",         "StoreReplaced",           "deleted_wal",         NULL, 0},
    {"ExportTooLarge",               "PersistenceError",        "",                    "Session '%s' has more than %l exportable messages; the JSON/JSONL backup is built in memory and capped per session. Use the dashboard Sessions page's streaming Export action, or set sessions.max_export_messages: 0 in config.yaml to disable the guard.", 0},
    {"CapabilityUnsupported",        "PersistenceError",        "",                    "backend does not support capability %c", 0},
    {"AuthorityRefusal",             "PersistenceError",        "",                    NULL, 0},
    {"FenceStale",                   "AuthorityRefusal",        "",                    NULL, 0},
    {"UnknownExecution",             "AuthorityRefusal",        "",                    "unknown_execution", 0},
    {"ProofRejected",                "AuthorityRefusal",        "",                    "permission_denied", 0},
    {"HandoffUnsettled",             "AuthorityRefusal",        "",                    NULL, 0},
    {"RetiredSessionError",          "AuthorityRefusal",        "",                    NULL, 0},
};
#define NCLASSES ((int)(sizeof(CLASSES) / sizeof(CLASSES[0])))

static const err_cls *find_class(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < NCLASSES; i++)
        if (!strcmp(CLASSES[i].name, name)) return &CLASSES[i];
    return NULL;
}

const char *w19_errors_c3b6af_cause(const char *cls) {
    const err_cls *c = find_class(cls);
    while (c) {
        if (c->cause[0]) return c->cause;
        c = find_class(c->parent);
    }
    return NULL;
}

int w19_errors_c3b6af_is_kind(const char *cls, const char *base) {
    const err_cls *c = find_class(cls);
    if (!c || !base) return 0;
    while (c) {
        if (!strcmp(c->name, base)) return 1;
        if (!c->parent[0]) break;
        c = find_class(c->parent);
    }
    return 0;
}

static int cause_in_list(const char *cause) {
    static const char *const CAUSES[] = {
        "locked", "compression", "compression_closed", "turn_lease", "corrupt", "fts_index",
        "replaced", "deleted_wal", "disk", "session_row_missing", "unknown", NULL
    };
    for (int i = 0; CAUSES[i]; i++) if (!strcmp(CAUSES[i], cause)) return 1;
    return 0;
}

int w19_errors_c3b6af_in_group(const char *cls, const char *group) {
    if (!cls || !group) return 0;
    if (!strcmp(group, "store_failures"))
        return w19_errors_c3b6af_is_kind(cls, "TransientStoreError")
            || w19_errors_c3b6af_is_kind(cls, "PermanentStoreError")
            || w19_errors_c3b6af_is_kind(cls, "StoreUnavailable")
            || w19_errors_c3b6af_is_kind(cls, "StoreReplaced");
    if (!strcmp(group, "mutation_guards"))
        return !strcmp(cls, "TurnLeaseLost") || !strcmp(cls, "CompressionInProgress");
    if (!strcmp(group, "persistence_error")) return w19_errors_c3b6af_is_kind(cls, "PersistenceError");
    if (!strcmp(group, "transient")) return w19_errors_c3b6af_is_kind(cls, "TransientStoreError");
    if (!strcmp(group, "permanent")) return w19_errors_c3b6af_is_kind(cls, "PermanentStoreError");
    if (!strcmp(group, "authority")) return w19_errors_c3b6af_is_kind(cls, "AuthorityRefusal");
    if (!strcmp(group, "conflict")) return w19_errors_c3b6af_is_kind(cls, "Conflict");
    if (!strcmp(group, "not_found")) return w19_errors_c3b6af_is_kind(cls, "NotFound");
    if (!strcmp(group, "corrupt")) return w19_errors_c3b6af_is_kind(cls, "CorruptStoreError");
    if (!strcmp(group, "replaced")) return w19_errors_c3b6af_is_kind(cls, "StoreReplaced");
    if (!strcmp(group, "cause")) {
        const char *cause = w19_errors_c3b6af_cause(cls);
        return cause ? cause_in_list(cause) : 0;
    }
    return 0;
}

static const char *REWIND_TEXT[][2] = {
    {"history_changed", "session history changed before the rewind could be persisted"},
    {"target_unidentified", "rewind target has no durable row identity"},
    {"target_changed", "rewind target changed before it could be persisted"},
    {"handoff_lost", "rewind did not retain its compaction handoff"},
    {NULL, NULL}
};

const char *w19_errors_c3b6af_rewind_text(const char *reason) {
    if (!reason) return NULL;
    for (int i = 0; REWIND_TEXT[i][0]; i++)
        if (!strcmp(REWIND_TEXT[i][0], reason)) return REWIND_TEXT[i][1];
    return NULL;
}

/* ---- Python str() helpers ------------------------------------------------ */

static void put(char **p, char *end, const char *s) {
    while (*s && *p < end) *(*p)++ = *s++;
}

static void str_value(char **p, char *end, const json_t *v);
static void repr_value(char **p, char *end, const json_t *v);

static void put_repr(char **p, char *end, const char *s) {
    if (!s) s = "";
    /* Python repr: single quotes, unless the text holds a single quote and no double quote. */
    char q = '\'';
    if (strchr(s, '\'') && !strchr(s, '"')) q = '"';
    if (*p < end) *(*p)++ = q;
    for (const unsigned char *c = (const unsigned char *)s; *c; c++) {
        if (*c == (unsigned char)q || *c == '\\') {
            char tmp[3] = {'\\', (char)*c, 0};
            put(p, end, tmp);
        } else if (*c == '\n') put(p, end, "\\n");
        else if (*c == '\r') put(p, end, "\\r");
        else if (*c == '\t') put(p, end, "\\t");
        else if (*c < 0x20 || *c == 0x7f) {
            char tmp[8];
            snprintf(tmp, sizeof(tmp), "\\x%02x", *c);
            put(p, end, tmp);
        } else if (*p < end) *(*p)++ = (char)*c;
    }
    if (*p < end) *(*p)++ = q;
}

static void put_commified(char **p, char *end, long long v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", v < 0 ? -v : v);
    if (v < 0) put(p, end, "-");
    size_t n = strlen(buf);
    for (size_t i = 0; i < n; i++) {
        if (i > 0 && (n - i) % 3 == 0) put(p, end, ",");
        char one[2] = {buf[i], 0};
        put(p, end, one);
    }
}

static void py_float_repr2(double v, char *buf, size_t cap) {
    if (v == (double)(long long)v && v < 1e16 && v > -1e16) {
        snprintf(buf, cap, "%lld", (long long)v);
        return;
    }
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, cap, "%.*g", prec, v);
        if (strtod(buf, NULL) == v) return;
    }
}

/* str() of a JSON value: scalars verbatim, containers as Python repr. */
static void str_value(char **p, char *end, const json_t *v) {
    if (!v || v->type == JSON_NULL) { put(p, end, "None"); return; }
    switch (v->type) {
        case JSON_BOOL: put(p, end, v->bool_val ? "True" : "False"); return;
        case JSON_NUMBER: {
            char buf[40];
            py_float_repr2(v->num_val, buf, sizeof(buf));
            put(p, end, buf);
            return;
        }
        case JSON_STRING: put(p, end, v->str_val ? v->str_val : ""); return;
        case JSON_ARRAY: {
            put(p, end, "[");
            for (size_t i = 0; i < json_len(v); i++) {
                if (i) put(p, end, ", ");
                repr_value(p, end, json_get(v, i));
            }
            put(p, end, "]");
            return;
        }
        case JSON_OBJECT: {
            put(p, end, "{");
            for (size_t i = 0; i < json_object_size(v); i++) {
                if (i) put(p, end, ", ");
                put_repr(p, end, json_object_get_key_at(v, i));
                put(p, end, ": ");
                repr_value(p, end, json_object_get_at(v, i));
            }
            put(p, end, "}");
            return;
        }
        default: put(p, end, "None"); return;
    }
}

/* repr() of a JSON value: strings quoted, containers recursive. */
static void repr_value(char **p, char *end, const json_t *v) {
    if (v && v->type == JSON_STRING) put_repr(p, end, v->str_val);
    else str_value(p, end, v);
}

/* ---- Python whitespace / printability ----------------------------------- */

static int is_space_cp(unsigned cp) {
    if (cp == 0x09 || cp == 0x0a || cp == 0x0b || cp == 0x0c || cp == 0x0d) return 1;
    if (cp >= 0x1c && cp <= 0x20) return 1;
    if (cp == 0x85 || cp == 0xa0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200a) return 1;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000) return 1;
    return 0;
}

static int is_printable_cp(unsigned cp) {
    if (cp < 0x20 || (cp >= 0x7f && cp <= 0x9f)) return 0;
    if (cp == 0xa0 || cp == 0xad) return 0;
    if (cp == 0x600 || cp == 0x601 || cp == 0x602 || cp == 0x603 || cp == 0x604 || cp == 0x605) return 0;
    if (cp == 0x61c || cp == 0x6dd || cp == 0x70f || cp == 0x890 || cp == 0x891 || cp == 0x8e2) return 0;
    if (cp == 0x180e) return 0;
    if (cp >= 0x200b && cp <= 0x200f) return 0;
    if (cp >= 0x2028 && cp <= 0x202e) return 0;
    if (cp >= 0x2060 && cp <= 0x2064) return 0;
    if (cp >= 0x2066 && cp <= 0x206f) return 0;
    if (cp == 0xfeff) return 0;
    if (cp >= 0xfff9 && cp <= 0xfffb) return 0;
    if (cp >= 0x1d173 && cp <= 0x1d17a) return 0;
    if (cp >= 0xe0000 && cp <= 0xe007f) return 0;
    return 1;
}

/* Decode one UTF-8 codepoint; returns the byte length (>=1). */
static int decode_cp(const unsigned char *s, unsigned *cp) {
    if (s[0] < 0x80) { *cp = s[0]; return 1; }
    if ((s[0] & 0xe0) == 0xc0 && (s[1] & 0xc0) == 0x80) {
        *cp = ((unsigned)(s[0] & 0x1f) << 6) | (unsigned)(s[1] & 0x3f);
        return 2;
    }
    if ((s[0] & 0xf0) == 0xe0 && s[1] && s[2]) {
        *cp = ((unsigned)(s[0] & 0x0f) << 12) | ((unsigned)(s[1] & 0x3f) << 6) | (unsigned)(s[2] & 0x3f);
        return 3;
    }
    if ((s[0] & 0xf8) == 0xf0 && s[1] && s[2] && s[3]) {
        *cp = ((unsigned)(s[0] & 0x07) << 18) | ((unsigned)(s[1] & 0x3f) << 12)
            | ((unsigned)(s[2] & 0x3f) << 6) | (unsigned)(s[3] & 0x3f);
        return 4;
    }
    *cp = s[0];
    return 1;
}

json_t *w19_errors_c3b6af_one_line(json_t *value, long long limit) {
    if (!value || value->type == JSON_NULL) return json_null();
    char raw[8192];
    char *p = raw, *end = raw + sizeof(raw) - 1;
    str_value(&p, end, value);
    *p = '\0';

    char out[8192];
    size_t o = 0;
    const unsigned char *s = (const unsigned char *)raw;
    int pending_space = 0;
    long long kept = 0;
    while (*s) {
        unsigned cp = 0;
        int n = decode_cp(s, &cp);
        if (is_space_cp(cp)) {
            pending_space = (o > 0);
            s += n;
            continue;
        }
        if (!is_printable_cp(cp)) { s += n; continue; }
        if (pending_space && o > 0) {
            if (kept >= limit) break;
            out[o++] = ' ';
            kept++;
            pending_space = 0;
            if (kept >= limit) break;
        }
        if (kept >= limit) break;
        if (o + (size_t)n < sizeof(out)) {
            memcpy(out + o, s, (size_t)n);
            o += (size_t)n;
            kept++;
        }
        s += n;
    }
    out[o] = '\0';
    if (o == 0) return json_null();
    return json_string(out);
}

json_t *w19_errors_c3b6af_busy_refusal(const char *exc) {
    if (!exc) return json_null();
    int compression = !strcmp(exc, "CompressionInProgress");
    if (!compression && strcmp(exc, "TurnLeaseLost")) return json_null();
    json_t *out = json_object();
    json_set(out, "reason", json_string("session_busy"));
    json_set(out, "field", compression ? json_string("compression") : json_null());
    return out;
}

/* ---- message ------------------------------------------------------------- */

static const char *arg_str(json_t *args, const char *key) {
    if (!args || args->type != JSON_OBJECT) return NULL;
    json_t *v = json_obj_get(args, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static long long arg_int(json_t *args, const char *key, long long def) {
    if (!args || args->type != JSON_OBJECT) return def;
    json_t *v = json_obj_get(args, key);
    if (!v) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    return def;
}

char *w19_errors_c3b6af_message(const char *cls, json_t *args) {
    const err_cls *c = find_class(cls);
    if (!c) return NULL;
    const char *session_id = arg_str(args, "session_id");
    if (!session_id) session_id = "";
    const char *entity = arg_str(args, "entity");
    if (!entity) entity = "";
    const char *entity_id = arg_str(args, "entity_id");
    if (!entity_id) entity_id = "";
    const char *message = arg_str(args, "message");
    const char *capability = arg_str(args, "capability");
    if (!capability) capability = "";
    const char *detail = arg_str(args, "detail");
    if (!detail) detail = "";
    const char *reason = arg_str(args, "reason");

    char out[4096];
    char *p = out, *end = out + sizeof(out) - 1;

    /* Message override paths (constructors that take message=). */
    if (message && (!strcmp(c->name, "CompressionBusy") || !strcmp(c->name, "CooldownRestoreRefused")
                    || !strcmp(c->name, "CompressionPublishRefused"))) {
        put(&p, end, message);
        *p = '\0';
        char *dup = (char *)malloc(strlen(out) + 1);
        if (dup) memcpy(dup, out, strlen(out) + 1);
        return dup;
    }
    if (!strcmp(c->name, "RewindRefused")) {
        const char *text = (message != NULL) ? message : w19_errors_c3b6af_rewind_text(reason ? reason : "");
        if (!text) return NULL;
        char *dup = (char *)malloc(strlen(text) + 1);
        if (dup) memcpy(dup, text, strlen(text) + 1);
        return dup;
    }
    if (!strcmp(c->name, "FenceStale")) {
        const char *kind = arg_str(args, "kind");
        if (!kind) return NULL;
        char *dup = (char *)malloc(strlen(kind) + 1);
        if (dup) memcpy(dup, kind, strlen(kind) + 1);
        return dup;
    }
    if (!strcmp(c->name, "AuthorityRefusal") || !strcmp(c->name, "HandoffUnsettled")) {
        const char *r = reason ? reason : "";
        char *dup = (char *)malloc(strlen(r) + 1);
        if (dup) memcpy(dup, r, strlen(r) + 1);
        return dup;
    }
    if (!strcmp(c->name, "RetiredSessionError")) {
        const char *r = reason ? reason : "session_deleted";
        char *dup = (char *)malloc(strlen(r) + 1);
        if (dup) memcpy(dup, r, strlen(r) + 1);
        return dup;
    }
    if (!c->fmt) {
        /* Plain exception classes: str(exc) is the first argument. */
        const char *first = message ? message : arg_str(args, "arg");
        char *dup = (char *)malloc(strlen(first ? first : "") + 1);
        if (dup) memcpy(dup, first ? first : "", strlen(first ? first : "") + 1);
        return dup;
    }
    for (const char *f = c->fmt; *f; f++) {
        if (*f != '%') { char one[2] = {*f, 0}; put(&p, end, one); continue; }
        f++;
        switch (*f) {
            case 's': put(&p, end, session_id); break;
            case 'R': put_repr(&p, end, session_id); break;
            case 'e': put(&p, end, entity); break;
            case 'I': put_repr(&p, end, entity_id); break;
            case 'm': put(&p, end, message ? message : ""); break;
            case 'c': put_repr(&p, end, capability); break;
            case 'd': put(&p, end, detail); break;
            case 'l': put_commified(&p, end, arg_int(args, "limit", 0)); break;
            case 'M': put_commified(&p, end, arg_int(args, "message_count", 0)); break;
            case '%': put(&p, end, "%"); break;
            default: break;
        }
    }
    *p = '\0';
    char *dup = (char *)malloc(strlen(out) + 1);
    if (dup) memcpy(dup, out, strlen(out) + 1);
    return dup;
}
