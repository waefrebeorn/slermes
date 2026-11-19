/*
 * port_state_errors.c -- Port of persistence/sqlite/_state/errors.py pure
 * defs: the error-classification predicates and the operator message
 * formatting. Exception types are modelled by se_kind_t (see the header);
 * the shared storage_errors.py classifier that classify_persistence_error
 * delegates to is reproduced below for string / sqlite3 inputs (the
 * persistence.core.errors contract types stay in Python).
 *
 *   se_is_no_more_rows                  <- _is_no_more_rows
 *   se_is_transient_sqlite_error        <- is_transient_sqlite_error
 *   se_is_malformed_schema_error        <- is_malformed_schema_error
 *   se_classify_persistence_error       <- classify_persistence_error
 *   se_compression_session_closed_message <- CompressionSessionClosedError.__init__
 *   se_state_db_replaced_message        <- _STATE_DB_REPLACED_MSG
 *   se_deleted_wal_generation_message   <- _DELETED_WAL_GENERATION_MSG
 *   se_state_db_corrupt_message         <- _STATE_DB_CORRUPT_MSG
 */

#include "port_state_errors.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* errors.py:_DISK_IO_ERROR_MARKER + _TRANSIENT_SQLITE_MARKERS ("Store BUSY,
 * not gone"; corruption deliberately absent). */
static const char *const SE_TRANSIENT_MARKERS[] = {
    "disk i/o error", "database is locked", "database table is locked",
    "busy", NULL,
};

/* storage_errors.py:_MALFORMED_SCHEMA_MARKERS */
static const char *const SE_MALFORMED_SCHEMA_MARKERS[] = {
    "malformed database schema", NULL,
};

/* storage_errors.py:_DB_CORRUPTION_MARKERS */
static const char *const SE_DB_CORRUPTION_MARKERS[] = {
    "malformed", "file is not a database", "not a database",
    "database corruption", NULL,
};

/* storage_errors.py:_DISK_FULL_MARKERS */
static const char *const SE_DISK_FULL_MARKERS[] = {
    "no space left on device", "not enough space", "database or disk is full",
    "disk full", "full disk", "enospc", NULL,
};

/* sqlite3 result codes (stable across SQLite releases). */
#define SE_SQLITE_BUSY 5
#define SE_SQLITE_LOCKED 6
#define SE_SQLITE_CONSTRAINT_FOREIGNKEY 787
#define SE_SQLITE_CORRUPT_VTAB 267

/* --- shared helpers --- */

/* ASCII lowercased copy; NULL reads as "". Caller frees. */
static char *se_lower(const char *s)
{
    if (!s)
        s = "";
    char *out = strdup(s);
    if (!out)
        return NULL;
    for (char *p = out; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    return out;
}

/* Python `marker in str(exc).lower()` over a NULL-terminated marker tuple. */
static bool se_contains_any_lower(const char *msg, const char *const *markers)
{
    char *lower = se_lower(msg);
    if (!lower)
        return false;
    bool hit = false;
    for (int i = 0; markers[i]; i++) {
        if (strstr(lower, markers[i])) {
            hit = true;
            break;
        }
    }
    free(lower);
    return hit;
}

/* storage_errors.py:_sqlite_primary_code -- extended codes keep the primary
 * in the low byte; -1 when the attribute is absent (strings, manual raises). */
static int se_primary_code(const se_exc_t *exc)
{
    if (!exc || !exc->has_code)
        return -1;
    return exc->errorcode & 0xFF;
}

/* storage_errors.py:is_sqlite_lock_error -- a known result code decides;
 * only without one does the text ("locked" / "busy"). */
static bool se_lock_error(const se_exc_t *exc)
{
    if (exc->has_code) {
        int code = se_primary_code(exc);
        return code == SE_SQLITE_BUSY || code == SE_SQLITE_LOCKED;
    }
    char *lower = se_lower(exc->message);
    if (!lower)
        return false;
    bool hit = strstr(lower, "locked") != NULL || strstr(lower, "busy") != NULL;
    free(lower);
    return hit;
}

/* storage_errors.py:_FTS_OBJECT_RE = \bmessages_fts\w* -- an occurrence of
 * "messages_fts" with a word boundary before it (\w* after always matches).
 * ASCII word chars approximate Python's Unicode \w. */
static bool se_fts_object(const char *lower)
{
    const char *p = lower;
    while ((p = strstr(p, "messages_fts")) != NULL) {
        if (p == lower)
            return true;
        unsigned char prev = (unsigned char)p[-1];
        if (!isalnum(prev) && prev != '_')
            return true;
        p += strlen("messages_fts");
    }
    return false;
}

/* storage_errors.py:is_fts_scoped_corruption_error -- a known result code
 * outranks prose (only SQLITE_CORRUPT_VTAB is FTS-scoped); without a code an
 * fts5: corruption report or a corruption marker naming messages_fts* counts. */
static bool se_fts_scoped(const se_exc_t *exc)
{
    if (!exc || exc->kind == SE_NONE)
        return false;
    if (exc->has_code)
        return exc->errorcode == SE_SQLITE_CORRUPT_VTAB;
    char *lower = se_lower(exc->message);
    if (!lower)
        return false;
    bool hit = false;
    if (se_fts_object(lower)) {
        if (strncmp(lower, "fts5:", 5) == 0 && strstr(lower, "corrupt"))
            hit = true;
        else
            for (int i = 0; SE_DB_CORRUPTION_MARKERS[i]; i++)
                if (strstr(lower, SE_DB_CORRUPTION_MARKERS[i])) {
                    hit = true;
                    break;
                }
    }
    free(lower);
    return hit;
}

/* storage_errors.py:classify_persistence_error for string / sqlite3 inputs:
 * provenance (FTS code, lock code, FK code) before prose, corruption before
 * the lock/disk buckets. The PersistenceError contract-type branch
 * (_typed_cause / DiskFull / StoreContention) stays in Python. */
static const char *se_classify_storage(const se_exc_t *exc)
{
    if (se_fts_scoped(exc))
        return "fts_index";
    int primary = se_primary_code(exc);
    if (primary == SE_SQLITE_BUSY || primary == SE_SQLITE_LOCKED)
        return "locked";
    if (exc->has_code && exc->errorcode == SE_SQLITE_CONSTRAINT_FOREIGNKEY)
        return "session_row_missing";

    char *text = se_lower(exc->message);
    if (!text)
        return "unknown";
    const char *cause = NULL;
    /* storage_errors.py:_PERSISTENCE_CAUSE_BY_PHRASE (order matters). */
    if (strstr(text, "turn lease"))
        cause = "turn_lease";
    else if (strstr(text, "closed by compression"))
        cause = "compression_closed";
    else if (strstr(text, "being compressed") || strstr(text, "compression lease"))
        cause = "compression";
    else if (strstr(text, "deleted state.db-wal") || strstr(text, "deleted state.db-shm"))
        cause = "deleted_wal";
    else if (strstr(text, "was replaced underneath"))
        cause = "replaced";
    else if (strstr(text, "malformed") || strstr(text, "file is not a database") ||
             strstr(text, "not a database") || strstr(text, "database corruption"))
        cause = "corrupt";
    else if (strstr(text, "locked") || strstr(text, "busy"))
        cause = "locked";
    else if (strstr(text, "foreign key constraint failed"))
        cause = "session_row_missing";
    if (!cause) {
        bool disk = strstr(text, "disk") || strstr(text, "readonly") ||
                    strstr(text, "read-only");
        if (!disk)
            for (int i = 0; SE_DISK_FULL_MARKERS[i]; i++)
                if (strstr(text, SE_DISK_FULL_MARKERS[i])) {
                    disk = true;
                    break;
                }
        if (disk)
            cause = "disk";
    }
    free(text);
    return cause ? cause : "unknown";
}

/* --- predicates (errors.py) --- */

/* PoP: se_is_no_more_rows @ persistence/sqlite/_state/errors.py:_is_no_more_rows */
bool se_is_no_more_rows(const se_exc_t *exc)
{
    /* Message-scoped because some builds raise it as InterfaceError. */
    if (!exc)
        return false;
    static const char *const markers[] = { "no more rows available", NULL };
    return se_contains_any_lower(exc->message, markers);
}

/* PoP: se_is_transient_sqlite_error @ persistence/sqlite/_state/errors.py:is_transient_sqlite_error */
bool se_is_transient_sqlite_error(const se_exc_t *exc)
{
    /* OperationalError only: "busy right now", not "damaged". */
    if (!exc || exc->kind != SE_OPERATIONAL)
        return false;
    if (se_lock_error(exc))
        return true;
    return se_contains_any_lower(exc->message, SE_TRANSIENT_MARKERS);
}

/* PoP: se_is_malformed_schema_error @ persistence/sqlite/_state/errors.py:is_malformed_schema_error */
bool se_is_malformed_schema_error(const se_exc_t *exc)
{
    /* isinstance(exc, sqlite3.DatabaseError): OperationalError and
     * StateDbCorruptError subclass it; RuntimeError-family types do not. */
    if (!exc)
        return false;
    if (exc->kind != SE_OPERATIONAL && exc->kind != SE_DATABASE &&
        exc->kind != SE_CORRUPT)
        return false;
    return se_contains_any_lower(exc->message, SE_MALFORMED_SCHEMA_MARKERS);
}

/* PoP: se_classify_persistence_error @ persistence/sqlite/_state/errors.py:classify_persistence_error */
const char *se_classify_persistence_error(const se_exc_t *exc)
{
    if (!exc || exc->kind == SE_NONE)
        return "unknown";
    /* errors.py:_PERSISTENCE_CAUSE_BY_TYPE -- lease refusals contain neither
     * "locked" nor "busy"; the backend types answer first, in order
     * (DeletedWalGenerationError before its StateDbReplacedError parent,
     * SessionCompressionInProgressError under CompressionSessionBusyError). */
    switch (exc->kind) {
    case SE_TURN_LEASE_LOST:
        return "turn_lease";
    case SE_COMPRESSION_CLOSED:
        return "compression_closed";
    case SE_COMPRESSION_BUSY:
    case SE_COMPRESSION_IN_PROGRESS:
        return "compression";
    case SE_DELETED_WAL:
        return "deleted_wal";
    case SE_REPLACED:
        return "replaced";
    case SE_CORRUPT:
        return "corrupt";
    default:
        break;
    }
    return se_classify_storage(exc);
}

/* --- message formatting --- */

/* Python str repr: quote choice (' unless the text holds ' and no "), then
 * per-character escapes. Non-ASCII is kept verbatim when printable; the
 * common non-printables (C0/C1, DEL, soft hyphen, NBSP, Unicode space /
 * format / line / paragraph separators, surrogates) escape as \xNN / \uNNNN.
 * Exotic non-printables beyond this set are approximated as printable. */
static char *se_py_repr(const char *s)
{
    if (!s)
        return strdup("None"); /* repr(None) */
    char quote = '\'';
    if (strchr(s, '\'') && !strchr(s, '"'))
        quote = '"';

    size_t cap = strlen(s) * 2 + 8, len = 0;
    char *out = (char *)malloc(cap);
    if (!out)
        return NULL;
#define REPR_PUT(c) do { \
        if (len + 2 > cap) { \
            cap *= 2; \
            char *nb = (char *)realloc(out, cap); \
            if (!nb) { free(out); return NULL; } \
            out = nb; \
        } \
        out[len++] = (char)(c); \
    } while (0)
#define REPR_PUTS(t) do { \
        for (const char *_p = (t); *_p; _p++) \
            REPR_PUT(*_p); \
    } while (0)

    REPR_PUT(quote);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        char esc[16];
        if (c == '\\') {
            REPR_PUTS("\\\\");
        } else if (c == (unsigned char)quote) {
            REPR_PUT('\\');
            REPR_PUT(c);
        } else if (c == '\n') {
            REPR_PUTS("\\n");
        } else if (c == '\r') {
            REPR_PUTS("\\r");
        } else if (c == '\t') {
            REPR_PUTS("\\t");
        } else if (c < 0x20 || c == 0x7F) {
            snprintf(esc, sizeof(esc), "\\x%02x", c);
            REPR_PUTS(esc);
        } else if (c < 0x80) {
            REPR_PUT(c);
        } else {
            /* Decode one UTF-8 code point (invalid bytes decode as-is). */
            unsigned long cp = c;
            int need = 0;
            if ((c & 0xE0) == 0xC0) { need = 1; cp = c & 0x1F; }
            else if ((c & 0xF0) == 0xE0) { need = 2; cp = c & 0x0F; }
            else if ((c & 0xF8) == 0xF0) { need = 3; cp = c & 0x07; }
            int ok = need > 0;
            for (int i = 1; ok && i <= need; i++) {
                if ((p[i] & 0xC0) != 0x80)
                    ok = 0;
                else
                    cp = (cp << 6) | (p[i] & 0x3F);
            }
            if (!ok) {
                REPR_PUT(c);
                continue;
            }
            bool nonprint =
                (cp >= 0x80 && cp <= 0x9F) || cp == 0xAD || cp == 0xA0 ||
                cp == 0x1680 || cp == 0x180E ||
                (cp >= 0x2000 && cp <= 0x200F) ||
                (cp >= 0x2028 && cp <= 0x202F) ||
                cp == 0x205F || (cp >= 0x2060 && cp <= 0x206F) ||
                cp == 0x3000 || cp == 0xFEFF ||
                (cp >= 0xD800 && cp <= 0xDFFF);
            if (nonprint) {
                if (cp <= 0xFF)
                    snprintf(esc, sizeof(esc), "\\x%02x", (unsigned)cp);
                else
                    snprintf(esc, sizeof(esc), "\\u%04lx", cp);
                REPR_PUTS(esc);
            } else {
                for (int i = 0; i <= need; i++)
                    REPR_PUT(p[i]);
            }
            p += need;
        }
    }
    REPR_PUT(quote);
    out[len] = '\0';
#undef REPR_PUT
#undef REPR_PUTS
    return out;
}

/* PoP: se_compression_session_closed_message @ persistence/sqlite/_state/errors.py:CompressionSessionClosedError.__init__ */
char *se_compression_session_closed_message(const char *session_id)
{
    char *repr = se_py_repr(session_id);
    if (!repr)
        return NULL;
    static const char tail[] =
        " is closed by compression; "
        "adopt its live continuation before appending messages";
    size_t n = strlen("Session ") + strlen(repr) + sizeof(tail);
    char *out = (char *)malloc(n);
    if (out)
        snprintf(out, n, "Session %s%s", repr, tail);
    free(repr);
    return out;
}

/* errors.py:_STATE_DB_REPLACED_MSG */
static const char SE_STATE_DB_REPLACED_MSG[] =
    "FATAL: state.db was replaced underneath the gateway; refusing further "
    "writes to this file. Divert transcripts to sessions/<id>.jsonl (and the "
    "gateway pending_messages spool) and restore or reopen after operator intervention.";

/* PoP: se_state_db_replaced_message @ persistence/sqlite/_state/errors.py:_STATE_DB_REPLACED_MSG */
char *se_state_db_replaced_message(void)
{
    return strdup(SE_STATE_DB_REPLACED_MSG);
}

/* errors.py:_DELETED_WAL_GENERATION_MSG (STORAGE_RECOVERY_DOCS_URL interpolated). */
static const char SE_DELETED_WAL_GENERATION_MSG[] =
    "FATAL: session storage stopped writing because another Hermes process still holds a deleted "
    "state.db-wal or state.db-shm inode (an old copy of the write-ahead log). Nothing is lost: quit "
    "every Hermes process on this profile (Desktop app, gateway, dashboard, cron), run `hermes doctor` "
    "(it names the processes still holding the log), then start Hermes again. Do not delete the WAL "
    "yourself and do not run `hermes doctor --fix` while they are running. "
    "Guide: https://hermes-agent.nousresearch.com/docs/user-guide/session-storage-recovery "
    "Detail: the path names a different (or missing) generation than the one this process holds "
    "open; opening or writing through it would mint a second WAL (split-brain). "
    "database.journal_mode: delete is operator containment, not a new default.";

/* PoP: se_deleted_wal_generation_message @ persistence/sqlite/_state/errors.py:_DELETED_WAL_GENERATION_MSG */
char *se_deleted_wal_generation_message(void)
{
    return strdup(SE_DELETED_WAL_GENERATION_MSG);
}

/* errors.py:_STATE_DB_CORRUPT_MSG */
static const char SE_STATE_DB_CORRUPT_MSG[] =
    "FATAL: state.db reported structural corruption (database disk image is "
    "malformed outside the FTS shadow tables) on a live handle; refusing further "
    "writes, automatic reopen, and the close-time WAL checkpoint on this file. "
    "Stop the gateway, then run `hermes sessions recover --source <state.db> "
    "--inspect-only` or restore a snapshot. Unwritten transcripts are diverted to "
    "sessions/<id>.jsonl (and the gateway pending_messages spool).";

/* PoP: se_state_db_corrupt_message @ persistence/sqlite/_state/errors.py:_STATE_DB_CORRUPT_MSG */
char *se_state_db_corrupt_message(void)
{
    return strdup(SE_STATE_DB_CORRUPT_MSG);
}
