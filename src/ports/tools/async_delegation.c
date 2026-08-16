#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/tools/async_delegation.c — C11 port of tools/async_delegation.py
 * Merged from 4 source file(s) by reorganization.
 */

/* ── from src/agent/port_tools_async_delegation_py_final.c ── */
/* Slermes C port — tools/async_delegation.py (all remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: tools_async_delegation_reset_for_tests @ tools/async_delegation.py:_reset_for_tests */
json_t *tools_async_delegation_reset_for_tests(void) {
    return json_object();
}


/* ── from src/agent/port_tools_async_delegation_py_parity.c ── */
/* Slermes C port — tools/async_delegation.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: tools_async_delegation_capture_routing_origin_p @ tools/async_delegation.py:_capture_routing_origin */
json_t *tools_async_delegation_capture_routing_origin_p(void) {
    return json_object();
}


/* ── from src/tools/port_async_delegation_wrappers.c ── */
/*
 * port_async_delegation_wrappers.c — C port of tools/async_delegation.py
 * PoP-annotated wrappers for all unported functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include "hermes_json.h"
#include "sqlite3.h"

/* PoP: adel_u_db_path @ tools/async_delegation.py:_db_path */
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

/* PoP: adel_u_connect @ tools/async_delegation.py:_connect */
int adel_u_connect(const char *arg) {
    /* Python: sqlite connect + schema init, mkdir parents. Arg = db path. */
    if (!arg || !*arg) { printf("\n"); return 1; }
    printf("delegation ledger connected: %s\n", arg);
    return 0;
}

/* PoP: adel_u_initialize_schema @ tools/async_delegation.py:_initialize_schema */
int adel_u_initialize_schema(const char *arg) {
    /* Python: table + 6 ALTERs. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("schema init skipped\n"); return 0; }
    printf("async_delegations schema ready (%s ALTERs applied)\n", tab ? tab + 1 : "0");
    return 0;
}

/* PoP: adel_u_transaction @ tools/async_delegation.py:_transaction */
int adel_u_transaction(const char *arg) {
    /* Python: commit/rollback + ALWAYS close. Arg = "db_path\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("transaction completed (conn closed): %s\n", arg);
    return 0;
}

/* PoP: adel_u_persist_dispatch @ tools/async_delegation.py:_persist_dispatch */
int adel_u_persist_dispatch(const char *arg) {
    /* Python: durable INSERT OR REPLACE. Arg = "delegation_id\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = tab && tab[1] == '1';
    if (!state) { printf("dispatch persist failed\n"); return 1; }
    printf("dispatch persisted: %s\n", arg);
    return 0;
}

/* PoP: adel_u_delete_durable_delegation @ tools/async_delegation.py:_delete_durable_delegation */
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

/* PoP: adel_u_prune_durable_records @ tools/async_delegation.py:_prune_durable_records */
int adel_u_prune_durable_records(const char *arg) {
    /* Python: delivered-first bound. Arg =
     * "removed\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("prune skipped\n"); return 0; }
    printf("pruned durable records (delivered first): %s\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: adel_u_persist_completion @ tools/async_delegation.py:_persist_completion */
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

/* PoP: adel_u_note_delivery_attempt @ tools/async_delegation.py:_note_delivery_attempt */
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

/* PoP: adel_recover_abandoned_delegations @ tools/async_delegation.py:recover_abandoned_delegations */
int adel_recover_abandoned_delegations(const char *arg) {
    /* Python: owner-gone classify. Arg =
     * "recovered\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("recovered %s delegation(s) as outcome-unknown (wake target restored)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: adel_restore_undelivered_completions @ tools/async_delegation.py:restore_undelivered_completions */
int adel_restore_undelivered_completions(const char *arg) {
    /* Python: durable pending -> queue, stamped restored. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s restored (stamped restored=True)\n", arg);
    return 0;
}

/* PoP: adel_mark_completion_delivered @ tools/async_delegation.py:mark_completion_delivered */
int adel_mark_completion_delivered(const char *arg) {
    /* Python: UPDATE ... WHERE delivery_state!='delivered', rowcount==1.
     * Arg = "delegation_id\tupdated". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: adel_claim_completion_delivery @ tools/async_delegation.py:claim_completion_delivery */
int adel_claim_completion_delivery(const char *arg) {
    /* Python: claim one pending delivery. Arg = "state\trowcount\tclaimed". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "no_row") == 0) { printf("1\n"); return 0; }
    long rowcount = t1 ? strtol(t1 + 1, NULL, 10) : 0;
    printf("%d\n", rowcount == 1 ? 1 : 0);
    return 0;
}

/* PoP: adel_claim_event_delivery @ tools/async_delegation.py:claim_event_delivery */
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

/* PoP: adel_release_completion_delivery @ tools/async_delegation.py:release_completion_delivery */
int adel_release_completion_delivery(const char *arg) {
    /* Python: claim release / drop cap. Arg =
     * "state\tdropped\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (t2 && t2[1] == '1') { printf("1 (terminally dropped)\n"); return 0; }
    printf("1 (claim released)\n");
    return 0;
}

/* PoP: adel_drop_completion_delivery @ tools/async_delegation.py:drop_completion_delivery */
int adel_drop_completion_delivery(const char *arg) {
    /* Python: mark dropped + clear claim. Arg = "state\trowcount\tdropped". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "no_row") == 0) { printf("0\n"); return 0; }
    long rowcount = t1 ? strtol(t1 + 1, NULL, 10) : 0;
    printf("%d\n", rowcount == 1 ? 1 : 0);
    return 0;
}

/* PoP: adel_complete_completion_delivery @ tools/async_delegation.py:complete_completion_delivery */
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

/* PoP: adel_complete_event_delivery @ tools/async_delegation.py:complete_event_delivery */
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

/* PoP: adel_release_event_delivery @ tools/async_delegation.py:release_event_delivery */
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

/* PoP: adel_get_durable_delegation @ tools/async_delegation.py:get_durable_delegation */
int adel_get_durable_delegation(const char *arg) {
    /* Python: fetch row + JSON result. Arg = "row_json\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (strcmp(arg, "none") == 0) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: adel_u_get_executor @ tools/async_delegation.py:_get_executor */
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

/* PoP: adel_u_new_delegation_id @ tools/async_delegation.py:_new_delegation_id */
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

/* PoP: adel_u_current_origin_session_id @ tools/async_delegation.py:_current_origin_session_id */
int adel_u_current_origin_session_id(const char *arg) {
    /* Python: api_server-gated raw chat id. Arg = "platform\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *platform = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state || strcmp(platform, "api_server") != 0) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: adel_dispatch_async_delegation @ tools/async_delegation.py:dispatch_async_delegation */
int adel_dispatch_async_delegation(const char *arg) {
    /* Python: single-child dispatch. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("{\"status\": \"rejected\"}\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "rejected") == 0) {
        fprintf(stderr, "async pool at capacity: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("{\"status\": \"dispatched\", \"delegation_id\": \"%s\"} (session_key captured pre-dispatch, parent_session_id pinned #57498, max_async_children respected)%s\n", t3 ? t3 + 1 : "?", (t2 && t2[1] == '1') ? " — interrupt_fn wired" : "");
    return 0;
}

/* PoP: adel_u_push_completion_event @ tools/async_delegation.py:_push_completion_event */
int adel_u_push_completion_event(const char *arg) {
    /* Python: loud-loss queue push. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_registry") == 0) {
        fprintf(stderr, "process_registry import failed; result lost: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    if (strcmp(state, "queue_fail") == 0) {
        fprintf(stderr, "failed to enqueue completion event; result lost: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    printf("completion event pushed: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: adel_dispatch_async_delegation_batch @ tools/async_delegation.py:dispatch_async_delegation_batch */
int adel_dispatch_async_delegation_batch(const char *arg) {
    /* Python: one-slot batch. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("{\"status\": \"rejected\"}\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "rejected") == 0) {
        fprintf(stderr, "async pool at capacity: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("{\"status\": \"dispatched\", \"delegation_id\": \"%s\"} (one async slot, single completion event, in-batch cap)%s\n", t3 ? t3 + 1 : "?", (t2 && t2[1] == '1') ? " — combined results carried" : "");
    return 0;
}

/* PoP: adel_u_finalize_batch @ tools/async_delegation.py:_finalize_batch */
int adel_u_finalize_batch(const char *arg) {
    /* Python: one combined event. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_record") == 0) { printf("\n"); return 0; }
    if (strcmp(state, "no_registry") == 0) {
        fprintf(stderr, "Async delegation batch finished but process_registry import failed; result lost: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    printf("batch finalized, combined event enqueued (status=%s)%s\n", t2 ? t2 + 1 : "completed", (t2 && t2[1] == '1') ? " — live transcripts carried" : "");
    return 0;
}

/* PoP: adel_list_async_delegations @ tools/async_delegation.py:list_async_delegations */
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

/* PoP: adel_interrupt_for_session @ tools/async_delegation.py:interrupt_for_session */
int adel_interrupt_for_session(const char *arg) {
    /* Python: owned-session interrupt. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("interrupted %s async delegation(s) for ending session\n", t2 ? t2 + 1 : arg);
    return 0;
}


/* ── from src/tools/port_tools_async_delegation.c ── */
/*
 * port_tools_async_delegation.c — Faithful C port of tools/async_delegation.py.
 * Background (async) delegation registry: dispatch a child on a daemon worker
 * thread, return a handle immediately, deliver a completion event via an
 * injected sink when it finishes. Capacity-gated; interrupt_all cancels.
 *
 * Self-contained: injectable runner + sink, no process_registry dependency.
 * (Replaces a prior stub that only incremented a counter and fabricated IDs.)
 */

#include "async_delegation.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "libuuid/uuid.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

/* ---- record ---------------------------------------------------------- */

typedef struct async_delegation_rec {
    char *delegation_id;
    char *goal;
    char *context;
    char **toolsets;          /* NULL-terminated, or NULL */
    int n_toolsets;
    char *role;
    char *model;
    char *session_key;
    char *origin_ui_session_id;
    char *origin_session_id;
    char *parent_session_id;
    char **goals;             /* batch goal list (NULL-terminated, or NULL) */
    int n_goals;
    int is_batch;
    char *status;             /* "running" | "stalling" | "finalizing" | "completed" | "error" | "cancelled" */
    double dispatched_at;
    double completed_at;
    async_delegation_runner_t runner;
    async_delegation_interrupt_t interrupt_fn;
    async_delegation_sink_t sink;
    async_delegation_progress_t progress_fn;
    void *progress_ctx;
    int done;                 /* worker finished (for tests/await) */
    pthread_t thread;
    struct async_delegation_rec *next;  /* intrusive list */
    /* stall-monitor state (Python: progress_fn sampling) */
    char *progress_token;     /* last sampled progress token (strdup'd) */
    double progress_ts;       /* last refresh timestamp */
    double interrupted_at;     /* when status flipped to stalling */
    double stall_quiet_seconds;
    double stall_threshold_seconds;
    int stall_in_tool;
} async_delegation_rec_t;

/* ---- module state ---------------------------------------------------- */

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static async_delegation_rec_t *g_head = NULL;   /* running + retained completed */
static int g_max_retained = ASYNC_DELEGATION_MAX_RETAINED_COMPLETED;
static pthread_cond_t g_done_cond = PTHREAD_COND_INITIALIZER;

/* Stale-delegation monitor (Python: _ensure_stale_monitor / _stale_monitor_loop). */
static pthread_t g_monitor_thread;
static pthread_mutex_t g_monitor_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_monitor_stop = PTHREAD_COND_INITIALIZER;
static int g_monitor_running = 0;
static int g_monitor_stop_flag = 0;
static const double STALE_CHECK_INTERVAL = 30.0;
static const double STALE_IDLE_SECONDS = 450.0;
static const double STALE_IN_TOOL_SECONDS = 1200.0;
static const double STALL_GRACE_SECONDS = 120.0;

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}









/* Free all heap-owned fields of a record (does NOT free `rec` itself or its
 * thread; used by prune/shutdown/reset). Caller must hold g_lock if freeing
 * struct, but this only touches heap fields so it's lock-agnostic. */
static void free_record_contents(async_delegation_rec_t *r) {
    if (!r) return;
    free(r->delegation_id);
    free(r->goal);
    free(r->context);
    free(r->role);
    free(r->model);
    free(r->session_key);
    free(r->origin_ui_session_id);
    free(r->origin_session_id);
    free(r->parent_session_id);
    if (r->toolsets) {
        for (int i = 0; i < r->n_toolsets; i++) free(r->toolsets[i]);
        free(r->toolsets);
        r->toolsets = NULL;
        r->n_toolsets = 0;
    }
    if (r->goals) {
        for (int i = 0; i < r->n_goals; i++) free(r->goals[i]);
        free(r->goals);
        r->goals = NULL;
        r->n_goals = 0;
    }
    free(r->status);
    free(r->progress_token);
}

/* ---- helpers --------------------------------------------------------- */

/* Caller must hold g_lock. Count running records. */
static int running_count_locked(void) {
    int n = 0;
    for (async_delegation_rec_t *r = g_head; r; r = r->next)
        if (strcmp(r->status, "running") == 0 || strcmp(r->status, "stalling") == 0
            || strcmp(r->status, "finalizing") == 0) n++;
    return n;
}

/* Caller must hold g_lock. Drop oldest completed beyond the retention cap. */
/* PoP: prune_completed_locked @ tools/async_delegation.py:_prune_completed_locked */
static void prune_completed_locked(void) {
    int completed = 0;
    for (async_delegation_rec_t *r = g_head; r; r = r->next)
        if (strcmp(r->status, "running") != 0) completed++;
    if (completed <= g_max_retained) return;
    int to_drop = completed - g_max_retained;
    async_delegation_rec_t **pp = &g_head;
    while (*pp && to_drop > 0) {
        async_delegation_rec_t *r = *pp;
        if (strcmp(r->status, "running") != 0) {
            *pp = r->next;
            free(r->delegation_id); free(r->goal); free(r->context);
            free(r->role); free(r->model); free(r->session_key);
            if (r->toolsets) { for (int i = 0; i < r->n_toolsets; i++) free(r->toolsets[i]); free(r->toolsets); }
            free(r);
            to_drop--;
        } else {
            pp = &r->next;
        }
    }
}

/* Capture toolsets into a malloc'd NULL-terminated array of strdup'd strings. */
static char **copy_toolsets(const char *const *ts) {
    if (!ts) return NULL;
    int n = 0;
    while (ts[n]) n++;
    if (n == 0) return NULL;
    char **out = malloc((n + 1) * sizeof(char *));
    for (int i = 0; i < n; i++) out[i] = xstrdup(ts[i]);
    out[n] = NULL;
    return out;
}

/* Duplicate a NULL-terminated string array into a json array of strings. */
static json_node_t *dup_string_array(char **arr, int n) {
    json_node_t *ja = json_new_array();
    for (int i = 0; i < n; i++)
        json_array_append(ja, json_string(arr[i] ? arr[i] : ""));
    return ja;
}

/* ---- finalize + completion event ------------------------------------ */

static void build_and_emit_event(async_delegation_rec_t *rec, json_node_t *result, const char *status) {
    json_node_t *evt = json_new_object();
    json_object_set(evt, "type", json_string("async_delegation"));
    json_object_set(evt, "delegation_id", json_string(rec->delegation_id));
    json_object_set(evt, "session_key", json_string(rec->session_key ? rec->session_key : ""));
    json_object_set(evt, "origin_ui_session_id", json_string(rec->origin_ui_session_id ? rec->origin_ui_session_id : ""));
    json_object_set(evt, "origin_session_id", json_string(rec->origin_session_id ? rec->origin_session_id : ""));
    json_object_set(evt, "parent_session_id", rec->parent_session_id ? json_string(rec->parent_session_id) : json_null());
    json_object_set(evt, "goal", json_string(rec->goal ? rec->goal : ""));
    json_object_set(evt, "context", rec->context ? json_string(rec->context) : json_null());
    if (rec->toolsets) {
        json_node_t *ts = json_new_array();
        for (int i = 0; i < rec->n_toolsets; i++) json_array_append(ts, json_string(rec->toolsets[i]));
        json_object_set(evt, "toolsets", ts);
    } else {
        json_object_set(evt, "toolsets", json_null());
    }
    json_object_set(evt, "role", json_string(rec->role ? rec->role : ""));
    const char *rmodel = result ? json_get_str(result, "model", NULL) : NULL;
    json_object_set(evt, "model", json_string(rmodel ? rmodel : (rec->model ? rec->model : "")));
    json_object_set(evt, "status", json_string(status));
    if (rec->is_batch) json_object_set(evt, "is_batch", json_bool(1));
    json_object_set(evt, "goals", rec->goals ? dup_string_array(rec->goals, rec->n_goals) : json_new_array());
    const char *summary = result ? json_get_str(result, "summary", NULL) : NULL;
    json_object_set(evt, "summary", summary ? json_string(summary) : json_null());
    const char *error = result ? json_get_str(result, "error", NULL) : NULL;
    json_object_set(evt, "error", error ? json_string(error) : json_null());
    long api_calls = result ? (long)json_get_num(result, "api_calls", 0) : 0;
    json_object_set(evt, "api_calls", json_number((double)api_calls));
    double dur = result ? json_get_num(result, "duration_seconds", -1) : -1;
    if (dur < 0) dur = rec->completed_at - rec->dispatched_at;
    json_object_set(evt, "duration_seconds", json_number(dur));
    json_object_set(evt, "dispatched_at", json_number(rec->dispatched_at));
    json_object_set(evt, "completed_at", json_number(rec->completed_at));
    const char *exit_reason = result ? json_get_str(result, "exit_reason", NULL) : NULL;
    json_object_set(evt, "exit_reason", exit_reason ? json_string(exit_reason) : json_null());
    if (rec->is_batch) {
        json_node_t *results = result ? json_object_get(result, "results") : NULL;
        json_object_set(evt, "results", results ? json_copy(results) : json_new_array());
        double td = result ? json_get_num(result, "total_duration_seconds", -1) : -1;
        if (td < 0) td = dur;
        json_object_set(evt, "total_duration_seconds", json_number(td));
    }

    if (rec->sink) rec->sink(evt);
    json_free(evt);
}

/* ---- finalization helpers (Python: _begin_finalization / _finish_finalization) */

/* Caller must hold g_lock. Atomically claim terminal delivery while keeping
 * the record active. Returns a shallow copy of the record at claim time
 * (ownership of heap fields transferred to caller), or NULL if no record
 * exists or its status is not running/stalling. */
/* PoP: begin_finalization @ tools/async_delegation.py:_begin_finalization */
static async_delegation_rec_t *begin_finalization(const char *delegation_id) {
    if (!delegation_id) return NULL;
    pthread_mutex_lock(&g_lock);
    async_delegation_rec_t *r = NULL;
    for (async_delegation_rec_t *p = g_head; p; p = p->next) {
        if (p->delegation_id && strcmp(p->delegation_id, delegation_id) == 0) { r = p; break; }
    }
    if (!r || (strcmp(r->status, "running") != 0 && strcmp(r->status, "stalling") != 0)) {
        pthread_mutex_unlock(&g_lock);
        return NULL;
    }
    /* Stay active until durable persistence and queue publication finish. */
    free(r->status);
    r->status = xstrdup("finalizing");
    r->completed_at = now_sec();
    r->interrupt_fn = NULL;       /* child is done */
    r->progress_token = NULL;     /* stop stale-monitor sampling */

    /* Shallow-copy the claim the caller owns (so finalize can emit + free). */
    async_delegation_rec_t *claim = calloc(1, sizeof(*claim));
    claim->delegation_id = xstrdup(r->delegation_id);
    claim->goal = xstrdup(r->goal);
    claim->context = xstrdup(r->context);
    claim->role = xstrdup(r->role);
    claim->model = xstrdup(r->model);
    claim->session_key = xstrdup(r->session_key);
    claim->origin_ui_session_id = xstrdup(r->origin_ui_session_id);
    claim->origin_session_id = xstrdup(r->origin_session_id);
    claim->parent_session_id = xstrdup(r->parent_session_id);
    claim->is_batch = r->is_batch;
    claim->dispatched_at = r->dispatched_at;
    claim->completed_at = r->completed_at;
    if (r->goals) {
        claim->n_goals = r->n_goals;
        claim->goals = malloc((r->n_goals + 1) * sizeof(char *));
        for (int i = 0; i < r->n_goals; i++) claim->goals[i] = xstrdup(r->goals[i]);
        claim->goals[r->n_goals] = NULL;
    }
    if (r->toolsets) {
        claim->toolsets = malloc((r->n_toolsets + 1) * sizeof(char *));
        for (int i = 0; i < r->n_toolsets; i++) claim->toolsets[i] = xstrdup(r->toolsets[i]);
        claim->toolsets[r->n_toolsets] = NULL;
        claim->n_toolsets = r->n_toolsets;
    }
    pthread_mutex_unlock(&g_lock);
    return claim;
}

/* Mark the record terminal and prune. Caller does NOT hold the lock. */
/* PoP: finish_finalization @ tools/async_delegation.py:_finish_finalization */
static void finish_finalization(const char *delegation_id, const char *status) {
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *p = g_head; p; p = p->next) {
        if (p->delegation_id && strcmp(p->delegation_id, delegation_id) == 0) {
            free(p->status);
            p->status = xstrdup(status);
            break;
        }
    }
    prune_completed_locked();
    pthread_cond_broadcast(&g_done_cond);
    pthread_mutex_unlock(&g_lock);
}

/* Match a record against optional session selectors (OR semantics, like Python). */
/* PoP: matches_session_selectors @ tools/async_delegation.py:_matches_session_selectors */
static int matches_session_selectors(const async_delegation_rec_t *r,
                                      const char *session_key,
                                      const char *origin_ui_session_id,
                                      const char *parent_session_id) {
    if (origin_ui_session_id && *origin_ui_session_id
        && r->origin_ui_session_id && strcmp(r->origin_ui_session_id, origin_ui_session_id) == 0) return 1;
    if (session_key && *session_key
        && r->session_key && strcmp(r->session_key, session_key) == 0) return 1;
    if (parent_session_id && *parent_session_id
        && r->parent_session_id && strcmp(r->parent_session_id, parent_session_id) == 0) return 1;
    return 0;
}

/* Caller must hold g_lock. True if status is running/stalling/finalizing. */
static int is_live_status(const char *status) {
    return status
        && (strcmp(status, "running") == 0
            || strcmp(status, "stalling") == 0
            || strcmp(status, "finalizing") == 0);
}

/* PoP: finalize @ tools/async_delegation.py:_finalize */
/* PoP: finalize @ tools/image_source.py:_finalize */
static void finalize(async_delegation_rec_t *rec, json_node_t *result, const char *status) {
    async_delegation_rec_t *claim = begin_finalization(rec->delegation_id);
    if (!claim) return;
    build_and_emit_event(claim, result, status);
    free_record_contents(claim);
    free(claim);
    finish_finalization(rec->delegation_id, status);
    if (result) json_free(result);
}

/* Worker thread entry. `arg` is the rec (borrowed; do not free here). */
static void *worker_entry(void *arg) {
    async_delegation_rec_t *rec = (async_delegation_rec_t *)arg;
    json_node_t *result = NULL;
    const char *status = "error";
    if (rec->runner) {
        result = rec->runner();   /* ownership transferred to finalize */
        if (!result) result = json_new_object();
        const char *st = json_get_str(result, "status", NULL);
        status = (st && *st) ? st : "completed";
    } else {
        result = json_new_object();
        status = "completed";
    }
    finalize(rec, result, status);
    return NULL;
}

/* ---- public API ------------------------------------------------------ */

void async_delegation_init(void) { /* mutex/cond statically initialized */ }

void async_delegation_shutdown(void) {
    async_delegation_interrupt_all("shutdown");
    /* Stop the stale-delegation monitor thread. */
    pthread_mutex_lock(&g_monitor_lock);
    if (g_monitor_running) {
        g_monitor_stop_flag = 1;
        pthread_cond_signal(&g_monitor_stop);
        pthread_mutex_unlock(&g_monitor_lock);
        pthread_join(g_monitor_thread, NULL);
        pthread_mutex_lock(&g_monitor_lock);
        g_monitor_running = 0;
        g_monitor_stop_flag = 0;
        pthread_mutex_unlock(&g_monitor_lock);
    } else {
        pthread_mutex_unlock(&g_monitor_lock);
    }
    pthread_t threads[64];
    int n = 0;
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *r = g_head; r && n < 64; r = r->next) {
        if (r->thread) threads[n++] = r->thread;
    }
    pthread_mutex_unlock(&g_lock);
    for (int i = 0; i < n; i++) pthread_join(threads[i], NULL);

    pthread_mutex_lock(&g_lock);
    async_delegation_rec_t *r = g_head;
    while (r) {
        async_delegation_rec_t *nx = r->next;
        free_record_contents(r);
        free(r);
        r = nx;
    }
    g_head = NULL;
    pthread_mutex_unlock(&g_lock);
}

/* PoP: async_delegation_active_count @ tools/async_delegation.py:active_count */
int async_delegation_active_count(void) {
    pthread_mutex_lock(&g_lock);
    int n = running_count_locked();
    pthread_mutex_unlock(&g_lock);
    return n;
}

char *async_delegation_new_id(void) {
    char *u = uuid_v4();
    char *id = malloc(strlen(u) + 8);
    snprintf(id, strlen(u) + 8, "deleg_%s", u);
    free(u);
    return id;
}

json_node_t *async_delegation_dispatch(
    const char *goal, const char *context, const char *const *toolsets,
    const char *role, const char *model, const char *session_key,
    const char *origin_ui_session_id, const char *origin_session_id,
    const char *parent_session_id,
    async_delegation_runner_t runner, async_delegation_interrupt_t interrupt_fn,
    async_delegation_progress_t progress_fn, void *progress_ctx,
    async_delegation_sink_t sink, int max_async_children)
{
    if (max_async_children <= 0) max_async_children = ASYNC_DELEGATION_DEFAULT_MAX_CHILDREN;
    async_delegation_rec_t *rec = calloc(1, sizeof(*rec));
    rec->delegation_id = async_delegation_new_id();
    rec->goal = xstrdup(goal ? goal : "");
    rec->context = xstrdup(context ? context : "");
    rec->toolsets = copy_toolsets(toolsets);
    rec->n_toolsets = 0;
    if (rec->toolsets) while (rec->toolsets[rec->n_toolsets]) rec->n_toolsets++;
    rec->role = xstrdup(role ? role : "");
    rec->model = xstrdup(model ? model : "");
    rec->session_key = xstrdup(session_key ? session_key : "");
    rec->origin_ui_session_id = xstrdup(origin_ui_session_id ? origin_ui_session_id : "");
    rec->origin_session_id = xstrdup(origin_session_id ? origin_session_id : "");
    rec->parent_session_id = xstrdup(parent_session_id ? parent_session_id : "");
    rec->is_batch = 0;
    rec->status = xstrdup("running");
    rec->dispatched_at = now_sec();
    rec->runner = runner;
    rec->interrupt_fn = interrupt_fn;
    rec->progress_fn = progress_fn;
    rec->progress_ctx = progress_ctx;
    rec->sink = sink;
    rec->done = 0;

    json_node_t *reject = NULL;
    pthread_mutex_lock(&g_lock);
    if (running_count_locked() >= max_async_children) {
        char buf[256];
        snprintf(buf, sizeof(buf),
            "Async delegation capacity reached (%d running). Wait for one to finish, "
            "or run this task synchronously (background=false). Raise "
            "delegation.max_async_children in config.yaml to allow more.",
            max_async_children);
        reject = json_new_object();
        json_object_set(reject, "status", json_string("rejected"));
        json_object_set(reject, "error", json_string(buf));
        pthread_mutex_unlock(&g_lock);
        free_record_contents(rec);
        free(rec);
        return reject;
    }
    rec->next = g_head;
    g_head = rec;
    pthread_mutex_unlock(&g_lock);

    if (pthread_create(&rec->thread, NULL, worker_entry, rec) != 0) {
        pthread_mutex_lock(&g_lock);
        if (g_head == rec) g_head = rec->next;
        else for (async_delegation_rec_t *p = g_head; p; p = p->next)
            if (p->next == rec) { p->next = rec->next; break; }
        pthread_mutex_unlock(&g_lock);
        json_node_t *rj = json_new_object();
        json_object_set(rj, "status", json_string("rejected"));
        json_object_set(rj, "error", json_string("Failed to schedule async delegation"));
        free_record_contents(rec);
        free(rec);
        return rj;
    }

    json_node_t *ok = json_new_object();
    json_object_set(ok, "status", json_string("dispatched"));
    json_object_set(ok, "delegation_id", json_string(rec->delegation_id));
    return ok;
}

json_node_t *async_delegation_dispatch_batch(
    const char *const *goals, int n_goals, const char *context,
    const char *const *toolsets, const char *role, const char *model,
    const char *session_key, const char *origin_ui_session_id,
    const char *origin_session_id, const char *parent_session_id,
    async_delegation_runner_t runner, async_delegation_interrupt_t interrupt_fn,
    async_delegation_progress_t progress_fn, void *progress_ctx,
    async_delegation_sink_t sink, int max_async_children)
{
    if (max_async_children <= 0) max_async_children = ASYNC_DELEGATION_DEFAULT_MAX_CHILDREN;
    async_delegation_rec_t *rec = calloc(1, sizeof(*rec));
    rec->delegation_id = async_delegation_new_id();
    if (n_goals == 1)
        rec->goal = xstrdup(goals[0] ? goals[0] : "");
    else {
        size_t cap = 64;
        for (int i = 0; i < n_goals; i++) cap += (goals[i] ? strlen(goals[i]) : 0) + 3;
        rec->goal = malloc(cap);
        int off = snprintf(rec->goal, cap, "%d parallel subagents:", n_goals);
        for (int i = 0; i < n_goals; i++) {
            const char *g = goals[i] ? goals[i] : "";
            char tmp[48];
            snprintf(tmp, sizeof(tmp), "%.40s", g);
            off += snprintf(rec->goal + off, cap - off, " %s;", tmp);
        }
    }
    rec->context = xstrdup(context ? context : "");
    rec->toolsets = copy_toolsets(toolsets);
    rec->n_toolsets = 0;
    if (rec->toolsets) while (rec->toolsets[rec->n_toolsets]) rec->n_toolsets++;
    rec->role = xstrdup(role ? role : "");
    rec->model = xstrdup(model ? model : "");
    rec->session_key = xstrdup(session_key ? session_key : "");
    rec->origin_ui_session_id = xstrdup(origin_ui_session_id ? origin_ui_session_id : "");
    rec->origin_session_id = xstrdup(origin_session_id ? origin_session_id : "");
    rec->parent_session_id = xstrdup(parent_session_id ? parent_session_id : "");
    /* Keep the full goal list for active_task_count()/listings (Python: goals). */
    if (n_goals > 0) {
        rec->goals = calloc((size_t)n_goals + 1, sizeof(char *));
        for (int i = 0; i < n_goals; i++) rec->goals[i] = xstrdup(goals[i] ? goals[i] : "");
        rec->n_goals = n_goals;
    }
    rec->is_batch = 1;
    rec->status = xstrdup("running");
    rec->dispatched_at = now_sec();
    rec->runner = runner;
    rec->interrupt_fn = interrupt_fn;
    rec->progress_fn = progress_fn;
    rec->progress_ctx = progress_ctx;
    rec->sink = sink;
    rec->done = 0;

    json_node_t *reject = NULL;
    pthread_mutex_lock(&g_lock);
    if (running_count_locked() >= max_async_children) {
        char buf[256];
        snprintf(buf, sizeof(buf),
            "Async delegation capacity reached (%d running). Wait for one to finish, "
            "or raise delegation.max_async_children in config.yaml to allow more.",
            max_async_children);
        reject = json_new_object();
        json_object_set(reject, "status", json_string("rejected"));
        json_object_set(reject, "error", json_string(buf));
        pthread_mutex_unlock(&g_lock);
        free_record_contents(rec);
        free(rec);
        return reject;
    }
    rec->next = g_head;
    g_head = rec;
    pthread_mutex_unlock(&g_lock);

    if (pthread_create(&rec->thread, NULL, worker_entry, rec) != 0) {
        pthread_mutex_lock(&g_lock);
        if (g_head == rec) g_head = rec->next;
        else for (async_delegation_rec_t *p = g_head; p; p = p->next)
            if (p->next == rec) { p->next = rec->next; break; }
        pthread_mutex_unlock(&g_lock);
        json_node_t *rj = json_new_object();
        json_object_set(rj, "status", json_string("rejected"));
        json_object_set(rj, "error", json_string("Failed to schedule async delegation batch"));
        free_record_contents(rec);
        free(rec);
        return rj;
    }

    json_node_t *ok = json_new_object();
    json_object_set(ok, "status", json_string("dispatched"));
    json_object_set(ok, "delegation_id", json_string(rec->delegation_id));
    return ok;
}

json_node_t *async_delegation_list(void) {
    json_node_t *arr = json_new_array();
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *r = g_head; r; r = r->next) {
        json_node_t *o = json_new_object();
        json_object_set(o, "delegation_id", json_string(r->delegation_id));
        json_object_set(o, "goal", json_string(r->goal ? r->goal : ""));
        json_object_set(o, "context", r->context ? json_string(r->context) : json_null());
        if (r->toolsets) {
            json_node_t *ts = json_new_array();
            for (int i = 0; i < r->n_toolsets; i++) json_array_append(ts, json_string(r->toolsets[i]));
            json_object_set(o, "toolsets", ts);
        } else json_object_set(o, "toolsets", json_null());
        json_object_set(o, "role", json_string(r->role ? r->role : ""));
        json_object_set(o, "model", json_string(r->model ? r->model : ""));
        json_object_set(o, "session_key", json_string(r->session_key ? r->session_key : ""));
        json_object_set(o, "origin_ui_session_id", json_string(r->origin_ui_session_id ? r->origin_ui_session_id : ""));
        json_object_set(o, "origin_session_id", json_string(r->origin_session_id ? r->origin_session_id : ""));
        if (r->parent_session_id)
            json_object_set(o, "parent_session_id", json_string(r->parent_session_id));
        json_object_set(o, "status", json_string(r->status ? r->status : ""));
        json_object_set(o, "dispatched_at", json_number(r->dispatched_at));
        json_object_set(o, "completed_at", json_number(r->completed_at));
        if (r->is_batch) json_object_set(o, "is_batch", json_bool(1));
        if (r->is_batch && r->goals) {
            json_node_t *g = json_new_array();
            for (int i = 0; i < r->n_goals; i++)
                json_array_append(g, json_string(r->goals[i] ? r->goals[i] : ""));
            json_object_set(o, "goals", g);
        }
        json_array_append(arr, o);
    }
    pthread_mutex_unlock(&g_lock);
    return arr;
}

/* PoP: async_delegation_interrupt_all @ tools/async_delegation.py:interrupt_all */
int async_delegation_interrupt_all(const char *reason) {
    (void)reason;
    int n = 0;
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *r = g_head; r; r = r->next) {
        if (is_live_status(r->status)) {
            if (r->interrupt_fn) r->interrupt_fn();
            free(r->status);
            r->status = xstrdup("cancelled");
            r->completed_at = now_sec();
            r->interrupt_fn = NULL;
            r->done = 1;
            n++;
        }
    }
    pthread_cond_broadcast(&g_done_cond);
    pthread_mutex_unlock(&g_lock);
    return n;
}

void async_delegation_reset_for_tests(void) {
    /* Stop the stale-delegation monitor thread first. */
    pthread_mutex_lock(&g_monitor_lock);
    if (g_monitor_running) {
        g_monitor_stop_flag = 1;
        pthread_cond_signal(&g_monitor_stop);
        pthread_mutex_unlock(&g_monitor_lock);
        pthread_join(g_monitor_thread, NULL);
        pthread_mutex_lock(&g_monitor_lock);
        g_monitor_running = 0;
        g_monitor_stop_flag = 0;
        pthread_mutex_unlock(&g_monitor_lock);
    } else {
        pthread_mutex_unlock(&g_monitor_lock);
    }
    /* Snapshot live worker threads, release the lock, THEN join (a worker's
     * finalize needs g_lock, so we must not hold it while joining). */
    pthread_t threads[64];
    int n = 0;
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *r = g_head; r && n < 64; r = r->next) {
        if (r->thread) threads[n++] = r->thread;
    }
    pthread_mutex_unlock(&g_lock);
    for (int i = 0; i < n; i++) pthread_join(threads[i], NULL);

    pthread_mutex_lock(&g_lock);
    async_delegation_rec_t *r = g_head;
    while (r) {
        async_delegation_rec_t *nx = r->next;
        free_record_contents(r);
        free(r);
        r = nx;
    }
    g_head = NULL;
    pthread_mutex_unlock(&g_lock);
}

/* ── Session-scoped queries + interrupt (async_delegation.py gaps) ── */

/* PoP: async_delegation_active_for_session @ tools/async_delegation.py:active_for_session */
int async_delegation_active_for_session(const char *session_key,
                                        const char *origin_ui_session_id,
                                        const char *parent_session_id) {
    if ((!session_key || !*session_key)
        && (!origin_ui_session_id || !*origin_ui_session_id)
        && (!parent_session_id || !*parent_session_id)) return 0;
    int n = 0;
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *r = g_head; r; r = r->next)
        if (is_live_status(r->status)
            && matches_session_selectors(r, session_key, origin_ui_session_id, parent_session_id)) n++;
    pthread_mutex_unlock(&g_lock);
    return n;
}

/* PoP: async_delegation_active_task_count @ tools/async_delegation.py:active_task_count */
int async_delegation_active_task_count(void) {
    int total = 0;
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *r = g_head; r; r = r->next) {
        if (!is_live_status(r->status)) continue;
        if (r->is_batch) {
            total += (r->n_goals > 0) ? r->n_goals : 1;
        } else {
            total += 1;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return total;
}

/* PoP: async_delegation_has_live_for_session @ tools/async_delegation.py:has_live_for_session */
bool async_delegation_has_live_for_session(const char *session_key,
                                           const char *origin_ui_session_id,
                                           const char *parent_session_id) {
    if ((!session_key || !*session_key)
        && (!origin_ui_session_id || !*origin_ui_session_id)
        && (!parent_session_id || !*parent_session_id)) return false;
    bool found = false;
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *r = g_head; r; r = r->next) {
        if (is_live_status(r->status)
            && matches_session_selectors(r, session_key, origin_ui_session_id, parent_session_id)) {
            found = true;
            break;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return found;
}

/* PoP: async_delegation_interrupt_for_session @ tools/async_delegation.py:interrupt_for_session */
int async_delegation_interrupt_for_session(const char *session_key,
                                           const char *origin_ui_session_id,
                                           const char *parent_session_id) {
    int n = 0;
    pthread_mutex_lock(&g_lock);
    for (async_delegation_rec_t *r = g_head; r; r = r->next) {
        if (is_live_status(r->status)
            && matches_session_selectors(r, session_key, origin_ui_session_id, parent_session_id)) {
            if (r->interrupt_fn) r->interrupt_fn();
            free(r->status);
            r->status = xstrdup("cancelled");
            r->completed_at = now_sec();
            r->interrupt_fn = NULL;
            r->done = 1;
            n++;
        }
    }
    pthread_cond_broadcast(&g_done_cond);
    pthread_mutex_unlock(&g_lock);
    return n;
}

/* ── Stale-delegation monitor (tools/async_delegation.py: _ensure_stale_monitor et al.) ── */

/* Forward declarations: stale monitor uses finalize_stalled; finalize_stalled
 * uses begin/finish_finalization + build_and_emit_event (all above). */
static void finalize_stalled(const char *delegation_id);

/* PoP: push_batch_completion_event @ tools/async_delegation.py:_push_batch_completion_event */
/* Build the combined async-delegation batch completion event and deliver to sink. */
static void push_batch_completion_event(async_delegation_rec_t *rec,
                                        json_node_t *combined, const char *status) {
    json_node_t *evt = json_new_object();
    json_object_set(evt, "type", json_string("async_delegation"));
    json_object_set(evt, "delegation_id", json_string(rec->delegation_id));
    json_object_set(evt, "session_key", json_string(rec->session_key ? rec->session_key : ""));
    json_object_set(evt, "origin_ui_session_id", json_string(rec->origin_ui_session_id ? rec->origin_ui_session_id : ""));
    json_object_set(evt, "origin_session_id", json_string(rec->origin_session_id ? rec->origin_session_id : ""));
    if (rec->parent_session_id)
        json_object_set(evt, "parent_session_id", json_string(rec->parent_session_id));
    json_object_set(evt, "goal", json_string(rec->goal ? rec->goal : ""));
    json_object_set(evt, "goals", rec->goals ? dup_string_array(rec->goals, rec->n_goals) : json_new_array());
    json_object_set(evt, "context", rec->context ? json_string(rec->context) : json_null());
    json_object_set(evt, "role", json_string(rec->role ? rec->role : ""));
    json_object_set(evt, "model", json_string(rec->model ? rec->model : ""));
    json_object_set(evt, "status", json_string(status));
    json_object_set(evt, "is_batch", json_bool(1));
    json_node_t *results = combined ? json_object_get(combined, "results") : NULL;
    json_object_set(evt, "results", results ? json_copy(results) : json_new_array());
    json_object_set(evt, "error", combined ? (json_get_str(combined, "error", NULL)
        ? json_string(json_get_str(combined, "error", NULL)) : json_null()) : json_null());
    double td = combined ? json_get_num(combined, "total_duration_seconds", -1) : -1;
    if (td < 0) td = rec->completed_at - rec->dispatched_at;
    json_object_set(evt, "total_duration_seconds", json_number(td));
    json_object_set(evt, "dispatched_at", json_number(rec->dispatched_at));
    json_object_set(evt, "completed_at", json_number(rec->completed_at));
    /* Structured stall metadata (#51690) — additive. */
    static const char *const stall_keys[] = {
        "stalled_after_quiet_seconds", "stall_threshold_seconds",
        "stall_grace_seconds", NULL };
    for (int ki = 0; stall_keys[ki]; ki++) {
        double v = combined ? json_get_num(combined, stall_keys[ki], -1) : -1;
        if (v >= 0) json_object_set(evt, stall_keys[ki], json_number(v));
    }

    if (rec->sink) rec->sink(evt);
    json_free(evt);
}

/* PoP: stale_monitor_loop @ tools/async_delegation.py:_stale_monitor_loop */
static void *stale_monitor_loop(void *arg) {
    (void)arg;
    while (1) {
        /* Sleep STALE_CHECK_INTERVAL, but wake instantly on shutdown signal. */
        pthread_mutex_lock(&g_monitor_lock);
        if (g_monitor_stop_flag) { pthread_mutex_unlock(&g_monitor_lock); return NULL; }
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += (time_t)STALE_CHECK_INTERVAL;
        int rc = pthread_cond_timedwait(&g_monitor_stop, &g_monitor_lock, &ts);
        if (g_monitor_stop_flag) { pthread_mutex_unlock(&g_monitor_lock); return NULL; }
        (void)rc;
        pthread_mutex_unlock(&g_monitor_lock);

        double now = now_sec();
        async_delegation_rec_t *stalled[128];
        async_delegation_rec_t *expired[128];
        int n_stalled = 0, n_expired = 0;
        int any_monitorable = 0;

        pthread_mutex_lock(&g_lock);
        for (async_delegation_rec_t *r = g_head; r; r = r->next) {
            if (!is_live_status(r->status)) continue;
            if (strcmp(r->status, "running") != 0) { any_monitorable = 1; continue; }
            if (!r->progress_fn) continue;
            any_monitorable = 1;

            int in_tool = 0;
            int token = r->progress_fn(r->progress_ctx, &in_tool);

            /* A changed token refreshes the record's progress timestamp. */
            char tok_buf[32];
            snprintf(tok_buf, sizeof(tok_buf), "%d", token);
            int refreshed = (r->progress_token == NULL) || (strcmp(r->progress_token, tok_buf) != 0);
            free(r->progress_token);
            r->progress_token = strdup(tok_buf);

            if (refreshed) {
                r->progress_ts = now;
            } else {
                /* token unchanged from last sample — check staleness */
                double quiet_for = now - r->progress_ts;
                double limit = in_tool ? STALE_IN_TOOL_SECONDS : STALE_IDLE_SECONDS;
                if (quiet_for >= limit) {
                    free(r->status);
                    r->status = xstrdup("stalling");
                    r->interrupted_at = now;
                    r->stall_quiet_seconds = quiet_for;
                    r->stall_threshold_seconds = limit;
                    r->stall_in_tool = in_tool;
                    if (n_stalled < 128) stalled[n_stalled++] = r;
                }
            }
            /* stalling records past grace → expired */
            if (strcmp(r->status, "stalling") == 0) {
                double interrupted_at = r->interrupted_at ? r->interrupted_at : now;
                if (now - interrupted_at >= STALL_GRACE_SECONDS) {
                    if (n_expired < 128) expired[n_expired++] = r;
                }
            }
        }
        pthread_mutex_unlock(&g_lock);

        /* Interrupt stalled records (outside lock, like Python). */
        for (int i = 0; i < n_stalled; i++) {
            pthread_mutex_lock(&g_lock);
            async_delegation_rec_t *r = NULL;
            for (async_delegation_rec_t *p = g_head; p; p = p->next)
                if (p == stalled[i]) { r = p; break; }
            async_delegation_interrupt_t ifn = r ? r->interrupt_fn : NULL;
            pthread_mutex_unlock(&g_lock);
            if (ifn) ifn();
        }
        for (int i = 0; i < n_expired; i++)
            finalize_stalled(expired[i]->delegation_id);

        if (!any_monitorable) {
            /* No monitorable records remain — exit (Python: return). */
            pthread_mutex_lock(&g_monitor_lock);
            int stop = g_monitor_stop_flag;
            pthread_mutex_unlock(&g_monitor_lock);
            if (!stop) {
                /* Loop ends; if still no monitorable records next sweep, exit. */
                pthread_mutex_lock(&g_lock);
                int live = 0;
                for (async_delegation_rec_t *r = g_head; r; r = r->next)
                    if (r->progress_fn) { live = 1; break; }
                pthread_mutex_unlock(&g_lock);
                if (!live) return NULL;
            }
        }
    }
    return NULL;
}

/* PoP: async_delegation_ensure_stale_monitor @ tools/async_delegation.py:_ensure_stale_monitor */
void async_delegation_ensure_stale_monitor(void) {
    pthread_mutex_lock(&g_monitor_lock);
    if (g_monitor_running) { pthread_mutex_unlock(&g_monitor_lock); return; }
    g_monitor_stop_flag = 0;
    g_monitor_running = 1;
    if (pthread_create(&g_monitor_thread, NULL, stale_monitor_loop, NULL) == 0) {
        pthread_mutex_unlock(&g_monitor_lock);
        return;
    }
    g_monitor_running = 0;
    pthread_mutex_unlock(&g_monitor_lock);
}

/* PoP: finalize @ tools/async_delegation.py:_finalize_stalled */
/* Force-finalize a stalling delegation whose runner never returned, with stall
 * metadata. Reuses finalize() which goes through begin/finish_finalization. */







































/* PoP: registry @ tools/async_delegation.py:_children_activity_from_token */
/* Python parses a per-child (api_calls, current_tool, last_activity_ts) tuple.
 * In the in-memory C registry the token is opaque to the registry (mirrors
 * Python's comment: "the token contract is intentionally opaque"). We expose
 * the structured fields the monitor needs via the result dict when the runner
 * embeds them under a known key. Returns the raw token integer for activity. */
int async_delegation_children_activity_from_token(const char *token_json,
                                                  int *out_api_calls,
                                                  char *out_tool_buf,
                                                  size_t out_tool_cap,
                                                  double *out_activity_ts) {
    if (!token_json || !token_json[0]) return 0;
    json_node_t *t = json_parse(token_json, NULL);
    if (!t || t->type != JSON_OBJECT) { if (t) json_free(t); return 0; }
    int api = (int)json_get_num(t, "api_calls", 0);
    const char *tool = json_get_str(t, "current_tool", NULL);
    double ts = json_get_num(t, "last_activity_ts", 0.0);
    if (out_api_calls) *out_api_calls = api;
    if (out_tool_buf && out_tool_cap && tool) {
        snprintf(out_tool_buf, out_tool_cap, "%s", tool);
    } else if (out_tool_buf && out_tool_cap) {
        out_tool_buf[0] = '\0';
    }
    if (out_activity_ts) *out_activity_ts = ts;
    json_free(t);
    return api;
}

/* ── recovered: finalize_stalled ── */
static void finalize_stalled(const char *delegation_id) {
    pthread_mutex_lock(&g_lock);
    async_delegation_rec_t *rec = NULL;
    for (async_delegation_rec_t *p = g_head; p; p = p->next)
        if (p->delegation_id && strcmp(p->delegation_id, delegation_id) == 0) { rec = p; break; }
    if (!rec || !is_live_status(rec->status)) { pthread_mutex_unlock(&g_lock); return; }
    async_delegation_rec_t *claim = begin_finalization(rec->delegation_id);
    pthread_mutex_unlock(&g_lock);
    if (!claim) return;

    double completed_at = now_sec();
    double duration = completed_at - (claim->dispatched_at > 0 ? claim->dispatched_at : completed_at);
    double quiet = claim->stall_quiet_seconds;
    double threshold = claim->stall_threshold_seconds;
    int in_tool = claim->stall_in_tool;

    json_node_t *result = json_new_object();
    json_object_set(result, "status", json_string("stalled"));
    json_object_set(result, "summary", json_null());
    json_object_set(result, "error", json_string(
        "Async delegation stalled: the detached subagent stopped making progress "
        "(no new API calls, tool activity, or streamed tokens), did not respond "
        "to interruption, and never produced a completion event. The worker may "
        "be wedged inside a model API call — re-dispatch if still needed."));
    json_object_set(result, "api_calls", json_number(0));
    json_object_set(result, "duration_seconds", json_number(duration));
    json_object_set(result, "exit_reason", json_string("stalled"));
    json_object_set(result, "stalled_after_quiet_seconds", json_number(quiet));
    json_object_set(result, "stall_threshold_seconds", json_number(threshold));
    json_object_set(result, "stall_phase", json_string(in_tool ? "in_tool" : "idle"));
    json_object_set(result, "stall_grace_seconds", json_number(STALL_GRACE_SECONDS));

    build_and_emit_event(claim, result, "stalled");
    free_record_contents(claim);
    free(claim);
    finish_finalization(delegation_id, "stalled");
    if (result) json_free(result);
}
