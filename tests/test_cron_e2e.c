/* test_cron_e2e.c — Cron job E2E integration tests.
 *
 * Tests the full cron pipeline: persistent store, schedule matching,
 * field persistence, update, deletion, and edge cases.
 *
 * S12 Cron E2E (P2) — v573.
 */
#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ── Internal cron functions (from scheduler.h / scheduler.c) ── */

typedef struct {
    char name[128];
    char schedule[128];
    char command[2048];
    bool active;
    int  retry_count;
    int  max_retries;
    int  backoff_sec;
    char chain_from[128];
    char template_name[128];
    char script_type[32];
    char interpreter[64];
    char notify_channel[128];
    bool notify_on_complete;
    bool notify_on_failure;
    time_t last_run;
    time_t created_at;
} cron_job_entry_t;

/* Opaque store type */
struct cron_sqlite_store_t;

/* Store functions */
extern struct cron_sqlite_store_t *cron_sqlite_open(const char *path);
extern void cron_sqlite_close(struct cron_sqlite_store_t *store);
extern bool cron_sqlite_save_job(struct cron_sqlite_store_t *store,
    const char *name, const char *schedule, const char *command,
    bool active, int retry_count, int max_retries,
    const char *chain_from, const char *template_name,
    const char *script_type);
extern bool cron_sqlite_load_jobs(struct cron_sqlite_store_t *store);
extern bool cron_sqlite_delete_job(struct cron_sqlite_store_t *store, const char *name);
extern bool cron_sqlite_update_job(struct cron_sqlite_store_t *store,
    const char *name, const char *field, const char *value);
extern int  cron_sqlite_count(struct cron_sqlite_store_t *store);
extern cron_job_entry_t *cron_sqlite_get(struct cron_sqlite_store_t *store, int index);
extern cron_job_entry_t *cron_sqlite_find(struct cron_sqlite_store_t *store, const char *name);

/* Schedule matching */
typedef enum { MINUTE, HOUR, DAY, MONTH, WEEKDAY } crontab_field_t;
typedef struct {
    int minute, hour, day, month, weekday;
    int interval_minutes;
} cron_schedule_t;

extern bool parse_schedule(const char *expr, cron_schedule_t *sched);
extern bool should_run(cron_schedule_t *sched, time_t now, time_t last_run);
extern bool cron_should_run(const char *schedule_expr, time_t now, time_t last_run);

/* ── Test framework ── */
static int passed = 0, failed = 0;

#define TEST(name, expr) do {                                    \
    if (expr) { passed++; printf("  PASS: %s\n", name); }         \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

static char *make_store_path(void) {
    static char path[128];
    snprintf(path, sizeof(path), "/tmp/hermes_cron_e2e_%d.json", (int)getpid());
    return path;
}

int main(void) {
    printf("=== Cron E2E Integration Tests ===\n\n");

    char *store_path = make_store_path();
    unlink(store_path); /* clean slate */

    /* ============================================================
     * Phase 1: Store lifecycle — open, save, close
     * ============================================================ */
    printf("--- Phase 1: Store lifecycle ---\n");

    struct cron_sqlite_store_t *store = cron_sqlite_open(store_path);
    TEST("store open", store != NULL);

    bool saved = cron_sqlite_save_job(store, "test_job1", "*/5 * * * *",
        "echo hello", true, 0, 3, "", "", "bash");
    TEST("save job1", saved == true);

    saved = cron_sqlite_save_job(store, "test_job2", "@daily",
        "/usr/bin/backup.sh", true, 1, 5, "", "", "bash");
    TEST("save job2", saved == true);

    saved = cron_sqlite_save_job(store, "test_job3", "0 9 * * 1-5",
        "echo workday", false, 0, 0, "", "", "bash");
    TEST("save disabled job", saved == true);

    int count = cron_sqlite_count(store);
    TEST("store count == 3", count == 3);

    cron_sqlite_close(store);

    /* ============================================================
     * Phase 2: Persistence — re-open and verify saved data
     * ============================================================ */
    printf("\n--- Phase 2: Persistence ---\n");

    store = cron_sqlite_open(store_path);
    TEST("re-open store", store != NULL);

    cron_sqlite_load_jobs(store);
    count = cron_sqlite_count(store);
    TEST("persisted count == 3", count == 3);

    cron_job_entry_t *j = cron_sqlite_find(store, "test_job1");
    TEST("find job1", j != NULL);
    if (j) {
        TEST("  name matches", strcmp(j->name, "test_job1") == 0);
        TEST("  command matches", strcmp(j->command, "echo hello") == 0);
        TEST("  schedule matches", strcmp(j->schedule, "*/5 * * * *") == 0);
        TEST("  active == true", j->active == true);
        TEST("  max_retries == 3", j->max_retries == 3);
    }

    j = cron_sqlite_find(store, "test_job2");
    TEST("find job2", j != NULL);
    if (j) {
        TEST("  retry_count == 1", j->retry_count == 1);
        TEST("  max_retries == 5", j->max_retries == 5);
    }

    j = cron_sqlite_find(store, "test_job3");
    TEST("find disabled job3", j != NULL);
    if (j) {
        TEST("  active == false", j->active == false);
        TEST("  schedule == 0 9 * * 1-5", strcmp(j->schedule, "0 9 * * 1-5") == 0);
    }

    /* ============================================================
     * Phase 3: Job update — modify fields
     * ============================================================ */
    printf("\n--- Phase 3: Job update ---\n");

    bool updated = cron_sqlite_update_job(store, "test_job1", "schedule", "0 0 * * *");
    TEST("update schedule", updated == true);

    updated = cron_sqlite_update_job(store, "test_job1", "command", "echo updated");
    TEST("update command", updated == true);

    updated = cron_sqlite_update_job(store, "test_job1", "active", "false");
    TEST("deactivate job", updated == true);

    /* Reload and verify */
    cron_sqlite_load_jobs(store);
    j = cron_sqlite_find(store, "test_job1");
    TEST("verify update: find", j != NULL);
    if (j) {
        TEST("  new schedule", strcmp(j->schedule, "0 0 * * *") == 0);
        TEST("  new command", strcmp(j->command, "echo updated") == 0);
        TEST("  now inactive", j->active == false);
    }

    /* ============================================================
     * Phase 4: Job deletion
     * ============================================================ */
    printf("\n--- Phase 4: Job deletion ---\n");

    bool deleted = cron_sqlite_delete_job(store, "test_job3");
    TEST("delete job3", deleted == true);

    count = cron_sqlite_count(store);
    TEST("count == 2 after delete", count == 2);

    j = cron_sqlite_find(store, "test_job3");
    TEST("job3 not found after delete", j == NULL);

    /* ============================================================
     * Phase 5: Schedule matching
     * ============================================================ */
    printf("\n--- Phase 5: Schedule matching ---\n");

    /* Parse standard cron expressions */
    cron_schedule_t sched;
    bool parsed = parse_schedule("*/5 * * * *", &sched);
    TEST("parse */5 * * * *", parsed == true);
    if (parsed) {
        TEST("  interval_minutes == 5", sched.interval_minutes == 5);
        TEST("  minute == -1 (every)", sched.minute == -1);
    }

    parsed = parse_schedule("0 9 * * 1-5", &sched);
    TEST("parse 0 9 * * 1-5", parsed == true);
    if (parsed) {
        TEST("  hour == 9", sched.hour == 9);
        TEST("  minute == 0", sched.minute == 0);
        TEST("  weekday == 1-5 pattern", sched.weekday != -1);
    }

    /* should_run: schedule says run every 5 min, last run was 310s ago (5 min 10s) */
    parsed = parse_schedule("*/5 * * * *", &sched);
    TEST("parse for should_run", parsed == true);
    if (parsed) {
        /* interval mode: difftime(now, last_run) >= 300 */
        bool run = should_run(&sched, 400, 90);  /* 310s > 300s interval */
        TEST("should_run (310s > 5min interval)", run == true);

        run = should_run(&sched, 400, 350);  /* 50s < 300s interval */
        TEST("should NOT run (50s < 5min interval)", run == false);

        run = should_run(&sched, 1000, 100);  /* 900s > 300s interval */
        TEST("should_run (900s > 5min interval)", run == true);
    }

    /* cron_should_run convenience wrapper with standard expressions */
    bool should = cron_should_run("*/5 * * * *", 400, 90);
    TEST("cron_should_run */5 every, 310s gap", should == true);

    should = cron_should_run("*/5 * * * *", 400, 350);
    TEST("cron_should_run */5 every, 50s gap", should == false);

    /* cron_should_run with hourly schedule */
    should = cron_should_run("0 * * * *", 3600, 3500);
    TEST("cron_should_run 0 * * * *, 100s into hour", should == true);

    should = cron_should_run("0 * * * *", 0, 0);
    TEST("cron_should_run 0 * * * *, epoch 0 matches", should == true);

    /* ============================================================
     * Phase 6: Edge cases
     * ============================================================ */
    printf("\n--- Phase 6: Edge cases ---\n");

    /* Save with NULL fields */
    saved = cron_sqlite_save_job(store, "edge_null", NULL, "echo null_sched",
        true, 0, 0, NULL, NULL, NULL);
    TEST("save with NULL schedule rejected", saved == false);

    saved = cron_sqlite_save_job(store, "edge_empty", "", "",
        true, 0, 0, "", "", "");
    TEST("save with empty fields", saved == true);

    /* Delete non-existent */
    deleted = cron_sqlite_delete_job(store, "nonexistent");
    TEST("delete nonexistent returns false", deleted == false);

    /* Find non-existent */
    j = cron_sqlite_find(store, "ghost");
    TEST("find nonexistent", j == NULL);

    /* Update non-existent */
    updated = cron_sqlite_update_job(store, "ghost", "command", "nope");
    TEST("update nonexistent returns false", updated == false);

    /* Invalid schedule parsing */
    cron_schedule_t bad;
    parsed = parse_schedule("not-a-cron-expr", &bad);
    TEST("parse invalid expr fails", parsed == false);

    /* Duplicate name — should overwrite */
    saved = cron_sqlite_save_job(store, "test_job1", "@weekly",
        "echo weekly", true, 0, 0, "", "", "");
    TEST("save duplicate name succeeds (overwrite)", saved == true);
    cron_sqlite_load_jobs(store);
    j = cron_sqlite_find(store, "test_job1");
    if (j) {
        TEST("  schedule overwritten", strcmp(j->schedule, "@weekly") == 0);
        TEST("  command overwritten", strcmp(j->command, "echo weekly") == 0);
    }

    /* Max jobs — store up to a reasonable number */
    int initial_count = cron_sqlite_count(store);
    for (int i = 0; i < 10; i++) {
        char name[64], cmd[64];
        snprintf(name, sizeof(name), "mass_job_%d", i);
        snprintf(cmd, sizeof(cmd), "echo job%d", i);
        cron_sqlite_save_job(store, name, "*/1 * * * *", cmd, true, 0, 0, "", "", "");
    }
    int new_count = cron_sqlite_count(store);
    TEST("store accepts 10 more jobs", new_count == initial_count + 10);

    /* ============================================================
     * Cleanup
     * ============================================================ */
    cron_sqlite_close(store);
    unlink(store_path);

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    printf("%s\n", failed ? "SOME TESTS FAILED" : "All Cron E2E tests PASSED");
    return failed > 0 ? 1 : 0;
}
