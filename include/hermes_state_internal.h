/* hermes_state_internal.h — INTERNAL shared handle for the SessionDB port.
 * Not a public API header: only the hermes_state units include it. Declares
 * the opaque db + the schema bootstrap + the shared helpers (now_epoch)
 * so the split units stay self-contained without a god header. Public
 * surface lives in include/hermes_state_db.h.
 */

#ifndef SLERMES_HERMES_STATE_INTERNAL_H
#define SLERMES_HERMES_STATE_INTERNAL_H

#include <stdbool.h>
#include <time.h>
#include "sqlite3.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque state.db handle (mirrors Python SessionDB's sqlite connection). */
typedef struct hermes_state_db {
    sqlite3 *db;
} hermes_state_db_t;

/* Shared schema DDL (sessions + messages + session_model_usage) — the real
 * state.db surface these units read/write through. Defined once in
 * hermes_state_open.c. */
extern const char *HERMES_STATE_SCHEMA_SQL;

double hermes_state_now_epoch(void);

/* Open + schema-bootstrap. Returns NULL on failure. */
hermes_state_db_t *hermes_state_db_open(const char *path);
void hermes_state_db_close(hermes_state_db_t *db);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_HERMES_STATE_INTERNAL_H */
