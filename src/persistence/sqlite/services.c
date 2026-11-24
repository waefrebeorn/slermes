/*
 * services.c -- real C11 port of the readiness probe of upstream Hermes
 * persistence/sqlite/services.py.
 *
 * PoP: w21_services_87bff3_probe @ persistence/sqlite/services.py:probe_state_db
 * PoP: w21_services_87bff3_health_state @ persistence/sqlite/services.py:SQLiteStorageHealth.storage_state
 *
 * `probe_state_db` never creates, opens for writing, migrates or repairs
 * anything: it answers "not initialized" for a missing file, "corrupt" for a
 * latched store, "ok" when a `mode=ro` connection can read the schema, and
 * otherwise "degraded" with either the latch's verdict or the failing
 * exception's class name.  The order matters -- a latched store is reported
 * without opening a connection at all.
 *
 * Left upstream: `Path.exists()`, `connect_tracked(read_only_db_uri(path), ...)`,
 * `PRAGMA query_only = ON` and the `sqlite_master` read, and
 * `note_storage_error`'s structural-corruption classification (a DB seam: the
 * caller reports whether the latch took the error).
 */
#include "hermes_json.h"
#include "noc_w21_services_87bff3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

json_t *w21_services_87bff3_probe(const json_t *req) {
    json_t *out = json_object();
    int exists = req ? json_get_bool(req, "exists", false) : 0;
    const char *state = req ? json_get_str(req, "state", W21_SERVICES_87BFF3_STORAGE_OK) : "";
    const char *connect = req ? json_get_str(req, "connect", "ok") : "ok";
    const char *exc_type = req ? json_get_str(req, "exc_type", "DatabaseError") : "";
    const char *note = req ? json_get_str(req, "note", "none") : "none";
    if (!out) return NULL;
    if (!exists) {
        json_set(out, "status", json_string("ok"));
        json_set(out, "detail", json_string("not initialized"));
        return out;
    }
    if (state && !strcmp(state, W21_SERVICES_87BFF3_STORAGE_CORRUPT)) {
        json_set(out, "status", json_string("degraded"));
        json_set(out, "detail", json_string(W21_SERVICES_87BFF3_STORAGE_CORRUPT));
        return out;
    }
    if (connect && !strcmp(connect, "raise")) {
        json_set(out, "status", json_string("degraded"));
        if (note && !strcmp(note, "latched"))
            json_set(out, "detail", json_string(W21_SERVICES_87BFF3_STORAGE_CORRUPT));
        else
            json_set(out, "detail", json_string(exc_type ? exc_type : ""));
        return out;
    }
    json_set(out, "status", json_string("ok"));
    json_set(out, "detail", json_null());
    return out;
}

const char *w21_services_87bff3_health_state(const char *state) {
    return (state && !strcmp(state, W21_SERVICES_87BFF3_STORAGE_CORRUPT)) ? "CORRUPT" : "OK";
}

