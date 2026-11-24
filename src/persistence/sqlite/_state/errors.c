/*
 * errors.c - C home for persistence/sqlite/_state/errors.py.
 * Faithful port of the state-store error classifiers. `is_sqlite_lock_error` and the
 * shared phrase classifier from storage_errors.py are reproduced here because
 * classify_persistence_error delegates to them (same order, same bucket names).
 * PoP: w19_errors_45219f_is_no_more_rows @ persistence/sqlite/_state/errors.py:_is_no_more_rows
 * PoP: w19_errors_45219f_is_transient @ persistence/sqlite/_state/errors.py:is_transient_sqlite_error
 * PoP: w19_errors_45219f_is_malformed_schema @ persistence/sqlite/_state/errors.py:is_malformed_schema_error
 * PoP: w19_errors_45219f_classify @ persistence/sqlite/_state/errors.py:classify_persistence_error
 * PoP: w19_errors_45219f_compression_closed_msg @ persistence/sqlite/_state/errors.py:CompressionSessionClosedError.__init__
 * PoP: w19_errors_45219f_is_fts_scoped @ storage_errors.py:is_fts_scoped_corruption_error
 */
#include "noc_w19_errors_45219f.h"
#include <stdlib.h>
#include <string.h>

/* SQLite result codes that matter here (stable across releases). */
#define SQLITE_BUSY 5
#define SQLITE_LOCKED 6
#define SQLITE_CORRUPT_VTAB 267
#define SQLITE_CONSTRAINT_FOREIGNKEY 787

/* Module constant mirrored from the Python source (docs link quoted in its messages). */
#define STORAGE_RECOVERY_DOCS_URL \
    "https://hermes-agent.nousresearch.com/docs/user-guide/session-storage-recovery"

static char *dupstr(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

/* ASCII-lowering copy; Python str.lower() is unicode-aware but every marker is ASCII. */
static char *lower_dup(const char *s) {
    char *p = dupstr(s ? s : "");
    size_t i;
    if (!p) return NULL;
    for (i = 0; p[i]; i++)
        if (p[i] >= 'A' && p[i] <= 'Z') p[i] = (char)(p[i] - 'A' + 'a');
    return p;
}

/* isinstance(exc, sqlite3.DatabaseError) for the modeled kinds. */
static int kind_is_db_error(const char *kind) {
    if (!kind) return 0;
    return !strcmp(kind, "operational") || !strcmp(kind, "database") ||
           !strcmp(kind, "integrity") || !strcmp(kind, "programming") ||
           !strcmp(kind, "corrupt");
}

/* The typed causes declared by this backend's own exception classes, in the order
 * _PERSISTENCE_CAUSE_BY_TYPE lists them. */
static const char *typed_cause_of(const char *kind) {
    if (!kind) return NULL;
    if (!strcmp(kind, "turn_lease")) return "turn_lease";
    if (!strcmp(kind, "compression_closed")) return "compression_closed";
    if (!strcmp(kind, "compression")) return "compression";
    if (!strcmp(kind, "compression_in_progress")) return "compression";
    if (!strcmp(kind, "deleted_wal")) return "deleted_wal";
    if (!strcmp(kind, "replaced")) return "replaced";
    if (!strcmp(kind, "corrupt")) return "corrupt";
    return NULL;
}

/* substring containment on the already-lowered text */
static int has(const char *text, const char *needle) {
    return text && needle && strstr(text, needle) != NULL;
}

static int is_word_char(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_' || c >= 0x80;
}

/* re.compile(r"\bmessages_fts\w*").search(text) */
static int has_fts_object(const char *text) {
    static const char *const needle = "messages_fts";
    const char *p = text;
    while (p && (p = strstr(p, needle)) != NULL) {
        if (p == text || !is_word_char((unsigned char)p[-1])) return 1;
        p += 1;
    }
    return 0;
}

/* ---- _is_no_more_rows(exc) ---- */
int w19_errors_45219f_is_no_more_rows(const char *msg) {
    char *lower = lower_dup(msg);
    int hit;
    if (!lower) return 0;
    hit = has(lower, "no more rows available");
    free(lower);
    return hit;
}

/* is_sqlite_lock_error(exc_or_str): a known result code decides, else the text. */
static int sqlite_lock_error(int has_code, long long code, const char *lower) {
    if (has_code) return code == SQLITE_BUSY || code == SQLITE_LOCKED;
    return has(lower, "locked") || has(lower, "busy");
}

/* is_fts_scoped_corruption_error(exc_or_str) */
int w19_errors_45219f_is_fts_scoped(int has_code, long long code, const char *msg) {
    char *lower;
    int hit = 0;
    if (has_code) return code == SQLITE_CORRUPT_VTAB;
    lower = lower_dup(msg);
    if (!lower) return 0;
    if (has_fts_object(lower)) {
        if (!strncmp(lower, "fts5:", 5) && has(lower, "corrupt")) hit = 1;
        else if (has(lower, "malformed") || has(lower, "file is not a database") ||
                 has(lower, "not a database") || has(lower, "database corruption")) hit = 1;
    }
    free(lower);
    return hit;
}

/* ---- is_transient_sqlite_error(exc) ---- */
int w19_errors_45219f_is_transient(const char *kind, int has_code, long long code,
                                   const char *msg) {
    static const char *const markers[] = {"disk i/o error", "database is locked",
                                          "database table is locked", "busy"};
    char *lower;
    int hit = 0;
    size_t i;
    if (!kind || strcmp(kind, "operational") != 0) return 0;
    lower = lower_dup(msg);
    if (!lower) return 0;
    if (sqlite_lock_error(has_code, code, lower)) {
        hit = 1;
    } else {
        for (i = 0; i < sizeof(markers) / sizeof(markers[0]); i++)
            if (has(lower, markers[i])) hit = 1;
    }
    free(lower);
    return hit;
}

/* ---- is_malformed_schema_error(exc) ---- */
int w19_errors_45219f_is_malformed_schema(const char *kind, const char *msg) {
    char *lower;
    int hit;
    if (!kind_is_db_error(kind)) return 0;
    lower = lower_dup(msg);
    if (!lower) return 0;
    hit = has(lower, "malformed database schema");
    free(lower);
    return hit;
}

/* ---- classify_persistence_error(exc_or_str) ---- */
json_t *w19_errors_45219f_classify(json_t *exc) {
    static const char *const corruption_markers[] = {
        "malformed", "file is not a database", "not a database", "database corruption"};
    static const char *const disk_full_markers[] = {
        "no space left on device", "not enough space", "database or disk is full",
        "disk full", "full disk", "enospc"};
    const char *kind = NULL;
    const char *msg = NULL;
    json_t *code_node = NULL;
    int has_code = 0;
    long long code = 0;
    char *lower = NULL;
    const char *cause = NULL;
    size_t i;

    if (!exc || exc->type == JSON_NULL) return json_string("unknown");
    if (exc->type == JSON_STRING) {
        kind = "str";
        msg = exc->str_val;
    } else if (exc->type == JSON_OBJECT) {
        kind = json_get_str(exc, "kind", "runtime");
        msg = json_get_str(exc, "msg", "");
        code_node = json_obj_get(exc, "code");
        if (code_node && code_node->type == JSON_NUMBER) {
            has_code = 1;
            code = (long long)code_node->num_val;
        }
    } else {
        return json_string("unknown");
    }

    /* The backend's own exception types answer first (lease refusals carry no "locked"). */
    cause = typed_cause_of(kind);
    if (cause) return json_string(cause);

    lower = lower_dup(msg);
    if (!lower) return json_string("unknown");

    /* Provenance before prose: FTS-scoped damage is index damage, never whole-file corruption. */
    if (w19_errors_45219f_is_fts_scoped(has_code, code, msg)) {
        free(lower);
        return json_string("fts_index");
    }
    if (has_code && (code == SQLITE_BUSY || code == SQLITE_LOCKED)) {
        free(lower);
        return json_string("locked");
    }
    if (has_code && code == SQLITE_CONSTRAINT_FOREIGNKEY) {
        free(lower);
        return json_string("session_row_missing");
    }

    /* _PERSISTENCE_CAUSE_BY_PHRASE, in order. */
    if (has(lower, "turn lease")) cause = "turn_lease";
    else if (has(lower, "closed by compression")) cause = "compression_closed";
    else if (has(lower, "being compressed") || has(lower, "compression lease")) cause = "compression";
    else if (has(lower, "deleted state.db-wal") || has(lower, "deleted state.db-shm")) cause = "deleted_wal";
    else if (has(lower, "was replaced underneath")) cause = "replaced";
    else {
        for (i = 0; i < sizeof(corruption_markers) / sizeof(corruption_markers[0]); i++)
            if (has(lower, corruption_markers[i])) { cause = "corrupt"; break; }
        if (!cause) {
            if (has(lower, "locked") || has(lower, "busy")) cause = "locked";
            else if (has(lower, "foreign key constraint failed")) cause = "session_row_missing";
        }
    }
    if (!cause) {
        for (i = 0; i < sizeof(disk_full_markers) / sizeof(disk_full_markers[0]); i++)
            if (has(lower, disk_full_markers[i])) { cause = "disk"; break; }
        if (!cause && (has(lower, "disk") || has(lower, "readonly") || has(lower, "read-only")))
            cause = "disk";
    }
    free(lower);
    return json_string(cause ? cause : "unknown");
}

/* ---- CompressionSessionClosedError(session_id) message ---- */
json_t *w19_errors_45219f_compression_closed_msg(const char *session_id) {
    /* f"Session {session_id!r} is closed by compression; adopt its live continuation ..." */
    static const char *const tail =
        " is closed by compression; adopt its live continuation before appending messages";
    const char *s = session_id ? session_id : "";
    char quote = '\'';
    size_t i, need, pos = 0;
    char *rep, *out;

    /* repr quotes: single unless the text holds a single quote and no double quote. */
    if (strchr(s, '\'') && !strchr(s, '"')) quote = '"';

    need = 1;  /* opening quote */
    for (i = 0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\' || c == (unsigned char)quote) need += 2;
        else if (c == '\n' || c == '\t' || c == '\r') need += 2;
        else need += 1;
    }
    need += 1;  /* closing quote */
    rep = (char *)malloc(need + 1);
    if (!rep) return json_null();
    rep[pos++] = quote;
    for (i = 0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\' || c == (unsigned char)quote) {
            rep[pos++] = '\\';
            rep[pos++] = (char)c;
        } else if (c == '\n') { rep[pos++] = '\\'; rep[pos++] = 'n'; }
        else if (c == '\t') { rep[pos++] = '\\'; rep[pos++] = 't'; }
        else if (c == '\r') { rep[pos++] = '\\'; rep[pos++] = 'r'; }
        else rep[pos++] = (char)c;
    }
    rep[pos++] = quote;
    rep[pos] = '\0';

    out = (char *)malloc(strlen("Session ") + strlen(rep) + strlen(tail) + 1);
    if (!out) { free(rep); return json_null(); }
    strcpy(out, "Session ");
    strcat(out, rep);
    strcat(out, tail);
    free(rep);
    {
        json_t *node = json_string(out);
        free(out);
        return node;
    }
}
