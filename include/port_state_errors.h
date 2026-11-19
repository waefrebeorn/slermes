/*
 * port_state_errors.h -- pure error-classification predicates and message
 * formatting ported from persistence/sqlite/_state/errors.py.
 *
 * The exception TYPES are modelled by se_kind_t (the C engine has no Python
 * exception objects): sqlite3 kinds, the backend contract types declared in
 * errors.py, plain strings (RPC-wrapped messages) and None. The sqlite3
 * result-code attribute is modelled by has_code/errorcode (manually raised
 * exceptions carry no code, exactly like Python's getattr fallback).
 *
 * The shared storage_errors.py classifier that classify_persistence_error
 * delegates to is reproduced for string/sqlite inputs inside the .c unit.
 */
#ifndef PORT_STATE_ERRORS_H
#define PORT_STATE_ERRORS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    SE_NONE = 0,                  /* None */
    SE_STRING,                    /* plain str / RPC-wrapped message */
    SE_OPERATIONAL,               /* sqlite3.OperationalError */
    SE_DATABASE,                  /* sqlite3.DatabaseError (non-Operational) */
    SE_OTHER_EXC,                 /* any other BaseException */
    SE_TURN_LEASE_LOST,           /* SessionTurnLeaseLostError */
    SE_COMPRESSION_CLOSED,        /* CompressionSessionClosedError */
    SE_COMPRESSION_BUSY,          /* CompressionSessionBusyError */
    SE_COMPRESSION_IN_PROGRESS,   /* SessionCompressionInProgressError (Busy subclass) */
    SE_DELETED_WAL,               /* DeletedWalGenerationError (Replaced subclass) */
    SE_REPLACED,                  /* StateDbReplacedError */
    SE_CORRUPT                    /* StateDbCorruptError (sqlite3.DatabaseError subclass) */
} se_kind_t;

typedef struct {
    se_kind_t kind;
    const char *message;   /* str(exc); NULL reads as "" */
    int has_code;          /* sqlite_errorcode attribute present */
    int errorcode;         /* full (possibly extended) result code */
} se_exc_t;

/* _is_no_more_rows -- message-scoped transient WAL-append engine error. */
bool se_is_no_more_rows(const se_exc_t *exc);

/* is_transient_sqlite_error -- "busy right now", not "damaged". */
bool se_is_transient_sqlite_error(const se_exc_t *exc);

/* is_malformed_schema_error -- only SQLite's explicit malformed-schema text
 * on a DatabaseError (OperationalError and StateDbCorruptError included). */
bool se_is_malformed_schema_error(const se_exc_t *exc);

/* classify_persistence_error -- coarse cause bucket; returns a static
 * string from PERSISTENCE_ERROR_CAUSES ("unknown" for None). */
const char *se_classify_persistence_error(const se_exc_t *exc);

/* CompressionSessionClosedError.__init__ message (Python repr of the id).
 * Returns a malloc'd string the caller frees. */
char *se_compression_session_closed_message(const char *session_id);

/* The three FATAL operator messages (module constants). Malloc'd copies. */
char *se_state_db_replaced_message(void);
char *se_deleted_wal_generation_message(void);
char *se_state_db_corrupt_message(void);

#endif /* PORT_STATE_ERRORS_H */
