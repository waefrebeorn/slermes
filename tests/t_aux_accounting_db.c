/*
 * t_aux_accounting_db.c — behavioral test for the aux-accounting stack:
 * src/cli/port_state_usage_db.c (SessionDB usage surface over sqlite) +
 * src/agent/port_aux_accounting.c (agent/aux_accounting.py context).
 * Self-verifying; exit 0 = all assertions pass.
 */

#include "aux_accounting.h"
#include "state_usage_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
    else { printf("ok: %s\n", msg); } \
} while (0)

int main(void) {
    char path[] = "/tmp/t_aux_acct_XXXXXX.db";
    /* mkstemps keeps the .db suffix */
    int fd = mkstemps(path, 3);
    if (fd < 0) { fprintf(stderr, "mkstemps failed\n"); return 2; }
    close(fd);
    unlink(path);   /* sqlite recreates it */

    state_usage_db_t *db = state_usage_db_open(path);
    CHECK(db != NULL, "state_usage_db_open");
    if (!db) return 2;

    /* --- no context published -> no-op --- */
    CHECK(!aux_record_usage("vision", "gemini-2.5", "google", NULL,
                            10, 5, 0, 0, 0, false, 0.0),
          "record without context -> no-op");

    /* --- publish context; record accumulates --- */
    aux_accounting_token_t tok = aux_set_accounting_context(db, "sess-1");
    CHECK(aux_record_usage("vision", "gemini-2.5", "google", NULL,
                           10, 5, 0, 0, 2, true, 0.001),
          "record with context");
    CHECK(aux_record_usage("vision", "gemini-2.5", "google", NULL,
                           7, 3, 0, 0, 1, true, 0.002),
          "second record accumulates");

    int calls = 0; long long in_t = 0, out_t = 0, rzn = 0; double cost = 0;
    CHECK(state_usage_get_row(db, "sess-1", "gemini-2.5", "vision",
                              &calls, &in_t, &out_t, &rzn, &cost),
          "row exists");
    CHECK(calls == 2, "api_call_count == 2");
    CHECK(in_t == 17 && out_t == 8 && rzn == 3, "token sums 17/8/3");
    CHECK(cost > 0.0029 && cost < 0.0031, "estimated cost sums to 0.003");

    /* --- excluded MoA tasks are never recorded --- */
    CHECK(!aux_record_usage("moa_reference", "m", NULL, NULL,
                            100, 100, 0, 0, 0, false, 0.0),
          "moa_reference excluded");
    CHECK(!aux_record_usage("moa_aggregator", "m", NULL, NULL,
                            100, 100, 0, 0, 0, false, 0.0),
          "moa_aggregator excluded");
    CHECK(!state_usage_get_row(db, "sess-1", "m", "moa_reference",
                               NULL, NULL, NULL, NULL, NULL),
          "no MoA row written");

    /* --- all-zero usage is skipped --- */
    CHECK(!aux_record_usage("compression", "m2", NULL, NULL,
                            0, 0, 0, 0, 0, false, 0.0),
          "zero usage skipped");

    /* --- empty task skipped --- */
    CHECK(!aux_record_usage("", "m3", NULL, NULL, 1, 1, 0, 0, 0, false, 0.0),
          "empty task skipped");

    /* --- NULL model falls back to 'unknown' --- */
    CHECK(aux_record_usage("title_generation", NULL, NULL, NULL,
                           3, 1, 0, 0, 0, false, 0.0),
          "NULL model recorded");
    CHECK(state_usage_get_row(db, "sess-1", "unknown", "title_generation",
                              &calls, &in_t, &out_t, NULL, NULL),
          "unknown-model row exists");
    CHECK(calls == 1 && in_t == 3 && out_t == 1, "unknown-model row counts");

    /* --- reset restores previous (empty) context --- */
    aux_reset_accounting_context(tok);
    CHECK(!aux_record_usage("vision", "gemini-2.5", "google", NULL,
                            1, 1, 0, 0, 0, false, 0.0),
          "record after reset -> no-op");

    /* --- publishing NULL clears --- */
    aux_set_accounting_context(db, "sess-2");
    aux_set_accounting_context(NULL, NULL);
    CHECK(!aux_record_usage("vision", "x", NULL, NULL, 1, 1, 0, 0, 0, false, 0.0),
          "publish None clears context");

    /* --- direct record_auxiliary_usage: empty session/task guard --- */
    CHECK(!state_usage_record_auxiliary_usage(db, "", "vision", "m", NULL, NULL,
                                              1, 1, 0, 0, 0, false, 0.0),
          "empty session_id guard");
    CHECK(!state_usage_record_auxiliary_usage(db, "s", "", "m", NULL, NULL,
                                              1, 1, 0, 0, 0, false, 0.0),
          "empty task guard");

    /* --- FK guard created the bare session row --- */
    state_usage_db_close(db);
    unlink(path);

    printf(failures ? "FAILURES: %d\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
