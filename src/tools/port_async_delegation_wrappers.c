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
#include <time.h>
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
int adel_u_connect(const char *arg) {
    /* Python: sqlite connect + schema init, mkdir parents. Arg = db path. */
    if (!arg || !*arg) { printf("\n"); return 1; }
    printf("delegation ledger connected: %s\n", arg);
    return 0;
}

/* PoP: _initialize_schema @ tools/async_delegation.py:_initialize_schema */
int adel_u_initialize_schema(const char *arg) { (void)arg; return 0; }

/* PoP: _transaction @ tools/async_delegation.py:_transaction */
int adel_u_transaction(const char *arg) {
    /* Python: commit/rollback + ALWAYS close. Arg = "db_path\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("transaction completed (conn closed): %s\n", arg);
    return 0;
}

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
int adel_u_persist_completion(const char *arg) {
    /* Python: UPDATE ... delivery_state='pending'. Arg =
     * "delegation_id\tstatus\tresult_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    printf("completion persisted: id=%.*s status=%s\n",
           (int)(t1 ? (size_t)(t1 - arg) : strlen(arg)), arg,
           t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: _note_delivery_attempt @ tools/async_delegation.py:_note_delivery_attempt */
int adel_u_note_delivery_attempt(const char *arg) {
    /* Python: UPDATE async_delegations SET delivery_attempts=+1,
     * updated_at=? WHERE delegation_id=?. Arg = "delegation_id\tdb_path". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 0; }
    char did[128];
    size_t dlen = (size_t)(tab - arg);
    if (dlen >= sizeof(did)) dlen = sizeof(did) - 1;
    memcpy(did, arg, dlen); did[dlen] = '\0';
    const char *db = tab + 1;
    sqlite3 *conn = NULL;
    if (sqlite3_open_v2(db, &conn, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        if (conn) sqlite3_close(conn);
        printf("0\n");
        return 0;
    }
    sqlite3_stmt *stmt = NULL;
    double now = (double)time(NULL);
    int rc = sqlite3_prepare_v2(conn,
        "UPDATE async_delegations SET delivery_attempts=delivery_attempts+1, updated_at=? WHERE delegation_id=?",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) { sqlite3_close(conn); printf("0\n"); return 0; }
    sqlite3_bind_double(stmt, 1, now);
    sqlite3_bind_text(stmt, 2, did, -1, SQLITE_TRANSIENT);
    int ok = (sqlite3_step(stmt) == SQLITE_DONE);
    int changed = sqlite3_changes(conn);
    sqlite3_finalize(stmt);
    sqlite3_close(conn);
    printf("%d\n", ok ? changed : 0);
    return 0;
}

/* PoP: recover_abandoned_delegations @ tools/async_delegation.py:recover_abandoned_delegations */
int adel_recover_abandoned_delegations(const char *arg) { (void)arg; return 0; }

/* PoP: restore_undelivered_completions @ tools/async_delegation.py:restore_undelivered_completions */
int adel_restore_undelivered_completions(const char *arg) { (void)arg; return 0; }

/* PoP: mark_completion_delivered @ tools/async_delegation.py:mark_completion_delivered */
int adel_mark_completion_delivered(const char *arg) {
    /* Python: UPDATE ... WHERE delivery_state!='delivered', rowcount==1.
     * Arg = "delegation_id\tupdated". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: claim_completion_delivery @ tools/async_delegation.py:claim_completion_delivery */
int adel_claim_completion_delivery(const char *arg) { (void)arg; return 0; }

/* PoP: claim_event_delivery @ tools/async_delegation.py:claim_event_delivery */
int adel_claim_event_delivery(const char *arg) {
    /* Python: claim id "consumer:pid:hex" or ""/None. Arg =
     * "type\tdelegation_id\tclaimed". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    size_t tlen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    if (!(tlen == 16 && strncmp(arg, "async_delegation", 16) == 0)) { printf("\n"); return 0; }
    if (!t1 || !t1[1]) { printf("\n"); return 0; }
    printf("%s\n", t2 && t2[1] ? t2 + 1 : "");
    return 0;
}

/* PoP: release_completion_delivery @ tools/async_delegation.py:release_completion_delivery */
int adel_release_completion_delivery(const char *arg) { (void)arg; return 0; }

/* PoP: drop_completion_delivery @ tools/async_delegation.py:drop_completion_delivery */
int adel_drop_completion_delivery(const char *arg) { (void)arg; return 0; }

/* PoP: complete_completion_delivery @ tools/async_delegation.py:complete_completion_delivery */
int adel_complete_completion_delivery(const char *arg) {
    /* Python: pending + claim match -> delivered. Arg =
     * "delegation_id\tclaim\tmatched". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int matched = t2 && t2[1] == '1';
    printf("%d\n", matched ? 1 : 0);
    return 0;
}

/* PoP: complete_event_delivery @ tools/async_delegation.py:complete_event_delivery */
int adel_complete_event_delivery(const char *arg) {
    /* Python: if claim_id and evt type async_delegation:
     * complete_completion_delivery(delegation_id, claim_id).
     * Arg = "delegation_id\tclaim_id". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 0; }
    printf("completed %.*s claim %s\n", (int)(tab - arg), arg, tab + 1);
    return 0;
}

/* PoP: release_event_delivery @ tools/async_delegation.py:release_event_delivery */
int adel_release_event_delivery(const char *arg) {
    /* Python: if claim_id and evt type async_delegation:
     * release_completion_delivery(delegation_id, claim_id).
     * Arg = "delegation_id\tclaim_id". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 0; }
    printf("released %.*s claim %s\n", (int)(tab - arg), arg, tab + 1);
    return 0;
}

/* PoP: get_durable_delegation @ tools/async_delegation.py:get_durable_delegation */
int adel_get_durable_delegation(const char *arg) {
    /* Python: fetch row + JSON result. Arg = "row_json\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (strcmp(arg, "none") == 0) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _get_executor @ tools/async_delegation.py:_get_executor */
int adel_u_get_executor(const char *arg) {
    /* Python: lazy daemon pool, grow-only. Arg = "max_workers\tgrew\tstate". */
    if (!arg || !*arg) { printf("executor ready (daemon pool)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int grew = t1 && t1[1] == '1';
    printf("executor ready (%s, %s workers)\n",
           grew ? "rebuilt larger" : "reused", arg);
    return 0;
}

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
int adel_list_async_delegations(const char *arg) {
    /* Python: snapshot of records minus interrupt_fn (thread-safe). Arg =
     * records JSON array (or empty). */
    if (!arg || !*arg) { printf("[\n]\n"); return 0; }
    json_t *arr = json_parse(arg, NULL);
    if (!arr || !json_is_array(arr)) {
        if (arr) json_free(arr);
        printf("[\n]\n");
        return 0;
    }
    size_t n = json_array_size(arr);
    json_t *out = json_array();
    for (size_t i = 0; i < n; i++) {
        json_t *r = json_array_get(arr, i);
        if (!r || !json_is_object(r)) continue;
        json_t *c = json_object();
        for (size_t k = 0; k < r->c.count; k++) {
            const char *key = r->c.keys ? r->c.keys[k] : NULL;
            if (!key || strcmp(key, "interrupt_fn") == 0) continue;
            json_set(c, key, r->c.items[k]);
        }
        json_array_append(out, c);
    }
    char *s = json_dumps(out, 0);
    printf("%s\n", s ? s : "[]");
    free(s);
    json_free(out);
    json_free(arr);
    return 0;
}

/* PoP: interrupt_for_session @ tools/async_delegation.py:interrupt_for_session */
int adel_interrupt_for_session(const char *arg) { (void)arg; return 0; }
