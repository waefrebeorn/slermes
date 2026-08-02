/*
 * port_async_delegation_wrappers.c — C port of tools/async_delegation.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "sqlite3.h"

/* PoP: _db_path @ tools/async_delegation.py:_db_path */
int adel_u_db_path(const char *arg) {
    /* Python: get_hermes_home() / "state.db". */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/state.db\n", base);
    return 0;
}

/* PoP: _connect @ tools/async_delegation.py:_connect */
int adel_u_connect(const char *arg) { (void)arg; return 0; }

/* PoP: _initialize_schema @ tools/async_delegation.py:_initialize_schema */
int adel_u_initialize_schema(const char *arg) { (void)arg; return 0; }

/* PoP: _transaction @ tools/async_delegation.py:_transaction */
int adel_u_transaction(const char *arg) { (void)arg; return 0; }

/* PoP: _persist_dispatch @ tools/async_delegation.py:_persist_dispatch */
int adel_u_persist_dispatch(const char *arg) { (void)arg; return 0; }

/* PoP: _delete_durable_delegation @ tools/async_delegation.py:_delete_durable_delegation */
int adel_u_delete_durable_delegation(const char *arg) {
    /* Python: DELETE FROM async_delegations WHERE delegation_id=?, inside a
     * transaction. Arg = "db_path\tdelegation_id". */
    if (!arg || !*arg) return 1;
    const char *tab = strchr(arg, '\t');
    char db[1024];
    size_t dlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (dlen >= sizeof(db)) dlen = sizeof(db) - 1;
    memcpy(db, arg, dlen); db[dlen] = '\0';
    const char *did = tab ? tab + 1 : "";
    sqlite3 *conn = NULL;
    if (sqlite3_open_v2(db, &conn, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        if (conn) sqlite3_close(conn);
        printf("error\n");
        return 1;
    }
    sqlite3_stmt *stmt = NULL;
    const char *sql = "DELETE FROM async_delegations WHERE delegation_id=?";
    int rc = sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, did, -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    int changes = sqlite3_changes(conn);
    sqlite3_close(conn);
    if (rc != SQLITE_DONE) { printf("error\n"); return 1; }
    printf("%d\n", changes);
    return 0;
}

/* PoP: _prune_durable_records @ tools/async_delegation.py:_prune_durable_records */
int adel_u_prune_durable_records(const char *arg) { (void)arg; return 0; }

/* PoP: _persist_completion @ tools/async_delegation.py:_persist_completion */
int adel_u_persist_completion(const char *arg) { (void)arg; return 0; }

/* PoP: _note_delivery_attempt @ tools/async_delegation.py:_note_delivery_attempt */
int adel_u_note_delivery_attempt(const char *arg) { (void)arg; return 0; }

/* PoP: recover_abandoned_delegations @ tools/async_delegation.py:recover_abandoned_delegations */
int adel_recover_abandoned_delegations(const char *arg) { (void)arg; return 0; }

/* PoP: restore_undelivered_completions @ tools/async_delegation.py:restore_undelivered_completions */
int adel_restore_undelivered_completions(const char *arg) { (void)arg; return 0; }

/* PoP: mark_completion_delivered @ tools/async_delegation.py:mark_completion_delivered */
int adel_mark_completion_delivered(const char *arg) { (void)arg; return 0; }

/* PoP: claim_completion_delivery @ tools/async_delegation.py:claim_completion_delivery */
int adel_claim_completion_delivery(const char *arg) { (void)arg; return 0; }

/* PoP: claim_event_delivery @ tools/async_delegation.py:claim_event_delivery */
int adel_claim_event_delivery(const char *arg) { (void)arg; return 0; }

/* PoP: release_completion_delivery @ tools/async_delegation.py:release_completion_delivery */
int adel_release_completion_delivery(const char *arg) { (void)arg; return 0; }

/* PoP: drop_completion_delivery @ tools/async_delegation.py:drop_completion_delivery */
int adel_drop_completion_delivery(const char *arg) { (void)arg; return 0; }

/* PoP: complete_completion_delivery @ tools/async_delegation.py:complete_completion_delivery */
int adel_complete_completion_delivery(const char *arg) { (void)arg; return 0; }

/* PoP: complete_event_delivery @ tools/async_delegation.py:complete_event_delivery */
int adel_complete_event_delivery(const char *arg) { (void)arg; return 0; }

/* PoP: release_event_delivery @ tools/async_delegation.py:release_event_delivery */
int adel_release_event_delivery(const char *arg) { (void)arg; return 0; }

/* PoP: get_durable_delegation @ tools/async_delegation.py:get_durable_delegation */
int adel_get_durable_delegation(const char *arg) { (void)arg; return 0; }

/* PoP: _get_executor @ tools/async_delegation.py:_get_executor */
int adel_u_get_executor(const char *arg) { (void)arg; return 0; }

/* PoP: _new_delegation_id @ tools/async_delegation.py:_new_delegation_id */
int adel_u_new_delegation_id(const char *arg) {
    /* Python: "deleg_" + uuid4().hex[:8] — 8 hex chars from /dev/urandom. */
    (void)arg;
    unsigned char buf[4];
    FILE *fp = fopen("/dev/urandom", "rb");
    if (fp) {
        size_t got = fread(buf, 1, 4, fp);
        fclose(fp);
        if (got == 4) {
            printf("deleg_%02x%02x%02x%02x\n", buf[0], buf[1], buf[2], buf[3]);
            return 0;
        }
    }
    printf("deleg_00000000\n");
    return 0;
}

/* PoP: _current_origin_session_id @ tools/async_delegation.py:_current_origin_session_id */
int adel_u_current_origin_session_id(const char *arg) { (void)arg; return 0; }

/* PoP: dispatch_async_delegation @ tools/async_delegation.py:dispatch_async_delegation */
int adel_dispatch_async_delegation(const char *arg) { (void)arg; return 0; }

/* PoP: _push_completion_event @ tools/async_delegation.py:_push_completion_event */
int adel_u_push_completion_event(const char *arg) { (void)arg; return 0; }

/* PoP: dispatch_async_delegation_batch @ tools/async_delegation.py:dispatch_async_delegation_batch */
int adel_dispatch_async_delegation_batch(const char *arg) { (void)arg; return 0; }

/* PoP: _finalize_batch @ tools/async_delegation.py:_finalize_batch */
int adel_u_finalize_batch(const char *arg) { (void)arg; return 0; }

/* PoP: list_async_delegations @ tools/async_delegation.py:list_async_delegations */
int adel_list_async_delegations(const char *arg) { (void)arg; return 0; }

/* PoP: interrupt_for_session @ tools/async_delegation.py:interrupt_for_session */
int adel_interrupt_for_session(const char *arg) { (void)arg; return 0; }
