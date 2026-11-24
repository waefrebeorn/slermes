/*
 * _errors.c -- real C11 port of the refusal-translation table of upstream Hermes
 *   persistence/sqlite/conversation/_errors.py.
 *
 * ``refusals`` is a context manager wrapped around ``translated()``; its body is
 * a single try/except chain mapping store failures onto the persistence
 * contract's error family. Every arm reads only attributes of the caught
 * exception (plus ``get_compression_tip(holder, ...)`` for the closed-session
 * arm), so the decision is a pure function of (class, fields, seams) -- ported
 * here as a table plus the per-arm argument shapes.
 *
 * What stays upstream: ``translated()`` (the store's own failure translation),
 * the ``with`` statement, and raising/chaining the result. This port answers
 * what the chain does once the exception has been caught.
 *
 * PoP: w20_errors_93204b_target_class @ persistence/sqlite/conversation/_errors.py:refusals
 * PoP: w20_errors_93204b_translate @ persistence/sqlite/conversation/_errors.py:refusals
 */
#include "hermes_json.h"
#include "noc_w20_errors_93204b.h"

#include <stdlib.h>
#include <string.h>

/* One arm of the except chain: store class -> contract class -> argument shape. */
typedef enum {
    ERR932_ARGS_SID_MESSAGE = 0,   /* (sid, str(exc)) */
    ERR932_ARGS_CLOSED,            /* (exc.session_id, tip) */
    ERR932_ARGS_RESUME,            /* (exc.message_count, exc.limit, exc.scope) */
    ERR932_ARGS_EXPORT,            /* (exc.session_id, exc.message_count, exc.limit) */
    ERR932_ARGS_MESSAGE            /* (str(exc),) */
} err932_shape;

typedef struct {
    const char *store_class;
    const char *contract_class;
    err932_shape shape;
} err932_row;

/* Source order of the except chain; the first name match wins, exactly like the
 * isinstance tests do for a raised instance of that class. */
static const err932_row ERR932_TABLE[] = {
    {"SessionCompressionInProgressError", "CompressionInProgress",   ERR932_ARGS_SID_MESSAGE},
    {"CompressionSessionBusyError",       "CompressionBusy",         ERR932_ARGS_SID_MESSAGE},
    {"CompressionSessionClosedError",     "CompressionClosed",       ERR932_ARGS_CLOSED},
    {"SessionTurnLeaseLostError",         "TurnLeaseLost",           ERR932_ARGS_SID_MESSAGE},
    {"SessionActiveWriteGuardError",      "SessionWriteGuarded",     ERR932_ARGS_SID_MESSAGE},
    {"HeldRowRewrittenError",             "HeldRowRewritten",        ERR932_ARGS_SID_MESSAGE},
    {"PruneRowStaleError",                "PruneRowStale",           ERR932_ARGS_SID_MESSAGE},
    {"PruneRowUnresolvedError",           "PruneRowUnresolved",      ERR932_ARGS_SID_MESSAGE},
    {"SessionResumeTooLargeError",        "ResumeTooLarge",          ERR932_ARGS_RESUME},
    {"SessionExportTooLargeError",        "ExportTooLarge",          ERR932_ARGS_EXPORT},
    {"RewindTargetUnavailableError",      "RewindTargetUnavailable", ERR932_ARGS_MESSAGE},
    {NULL, NULL, ERR932_ARGS_MESSAGE}
};

/* `fields.get(key)` as a fresh JSON value; absent reads as Python None. */
static json_t *err932_field(const json_t *fields, const char *key) {
    json_t *v = fields ? json_obj_get(fields, key) : NULL;
    return v ? json_copy(v) : json_null();
}

const char *w20_errors_93204b_target_class(const char *exc_type, const char *reason) {
    int i;
    if (!exc_type) return NULL;
    /* `except AuthorityRefusal`: an exact AuthorityRefusal only becomes
     * UnknownExecution for a bare ``unknown_execution``; any other reason is
     * re-raised unchanged. */
    if (!strcmp(exc_type, "AuthorityRefusal"))
        return (reason && !strcmp(reason, "unknown_execution")) ? "UnknownExecution" : NULL;
    for (i = 0; ERR932_TABLE[i].store_class; i++) {
        if (!strcmp(exc_type, ERR932_TABLE[i].store_class)) return ERR932_TABLE[i].contract_class;
    }
    return NULL;
}

json_t *w20_errors_93204b_translate(const char *exc_type, const json_t *fields,
                                    const char *session_id, int holder_present,
                                    const json_t *compression_tip) {
    const char *target;
    json_t *out, *args;
    const char *sid = session_id ? session_id : "";
    int i;

    if (!fields || fields->type != JSON_OBJECT) fields = NULL;
    target = w20_errors_93204b_target_class(exc_type, json_get_str(fields, "reason", ""));

    out = json_object();
    if (!target) {
        /* Bare `raise`: the original instance is re-raised unchanged. */
        json_set(out, "status", json_string("reraise"));
        json_set(out, "class", json_null());
        json_set(out, "exc", json_string(exc_type ? exc_type : ""));
        return out;
    }

    args = json_array();
    if (!strcmp(target, "UnknownExecution")) {
        /* UnknownExecution("session", sid) */
        json_append(args, json_string("session"));
        json_append(args, json_string(sid));
    } else {
        for (i = 0; ERR932_TABLE[i].store_class; i++) {
            if (strcmp(target, ERR932_TABLE[i].contract_class)) continue;
            switch (ERR932_TABLE[i].shape) {
            case ERR932_ARGS_SID_MESSAGE:
                /* (sid, str(exc)) -- str() of the store exception is its text. */
                json_append(args, json_string(sid));
                json_append(args, err932_field(fields, "message"));
                break;
            case ERR932_ARGS_CLOSED:
                /* (exc.session_id, tip): the refusal names the exception's own
                 * session, and the tip only resolves through a live holder. */
                json_append(args, err932_field(fields, "session_id"));
                json_append(args, holder_present ? (compression_tip ? json_copy(compression_tip)
                                                                    : json_null())
                                                 : err932_field(fields, "session_id"));
                break;
            case ERR932_ARGS_RESUME:
                json_append(args, err932_field(fields, "message_count"));
                json_append(args, err932_field(fields, "limit"));
                json_append(args, err932_field(fields, "scope"));
                break;
            case ERR932_ARGS_EXPORT:
                json_append(args, err932_field(fields, "session_id"));
                json_append(args, err932_field(fields, "message_count"));
                json_append(args, err932_field(fields, "limit"));
                break;
            case ERR932_ARGS_MESSAGE:
                json_append(args, err932_field(fields, "message"));
                break;
            }
            break;
        }
    }
    json_set(out, "status", json_string("raise"));
    json_set(out, "class", json_string(target));
    json_set(out, "args", args);
    return out;
}

