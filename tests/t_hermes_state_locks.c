/* t_hermes_state_locks.c — behavioral test for the compression-lock +
 * child-publication surface (hermes_state_locks.c), mirroring the Python
 * suite invariants for try_acquire/release/refresh/get_holder/
 * find_live_compression_child/publish_compression_child.
 */
#include "hermes_state_db.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fails = 0;
#define CHECK(cond, name) do { \
    if (cond) printf("PASS %s\n", name); \
    else { printf("FAIL %s\n", name); fails++; } \
} while (0)

int main(void) {
    char path[] = "/tmp/t_hs_locks_XXXXXX";
    int fd = mkstemp(path);
    if (fd >= 0) close(fd);
    unlink(path);

    hermes_state_db_t *db = hermes_state_db_open(path);
    CHECK(db != NULL, "open");

    /* ── lock protocol ───────────────────────────────────────────── */
    char holder1[128], holder2[128];
    snprintf(holder1, sizeof(holder1), "pid=%d:tid=1:agent=a:nonce=1", getpid());
    snprintf(holder2, sizeof(holder2), "pid=%d:tid=2:agent=b:nonce=2", getpid());

    CHECK(hermes_state_try_acquire_compression_lock(db, "s1", holder1, 300.0),
          "acquire fresh");
    CHECK(!hermes_state_try_acquire_compression_lock(db, "s1", holder2, 300.0),
          "second holder blocked");
    char *h = hermes_state_get_compression_lock_holder(db, "s1");
    CHECK(h && strcmp(h, holder1) == 0, "get_holder returns owner");
    free(h);

    /* re-acquire by same holder is idempotent-true (row already ours) */
    CHECK(hermes_state_try_acquire_compression_lock(db, "s1", holder1, 300.0),
          "same holder reacquire");

    /* refresh by owner works; by non-owner fails */
    CHECK(hermes_state_refresh_compression_lock(db, "s1", holder1, 600.0),
          "refresh by owner");
    CHECK(!hermes_state_refresh_compression_lock(db, "s1", holder2, 600.0),
          "refresh by non-owner rejected");

    /* release by non-owner is a no-op; owner still holds */
    hermes_state_release_compression_lock(db, "s1", holder2);
    h = hermes_state_get_compression_lock_holder(db, "s1");
    CHECK(h && strcmp(h, holder1) == 0, "non-owner release no-op");
    free(h);

    /* owner release frees the lock */
    hermes_state_release_compression_lock(db, "s1", holder1);
    h = hermes_state_get_compression_lock_holder(db, "s1");
    CHECK(h == NULL, "owner release clears");
    CHECK(hermes_state_try_acquire_compression_lock(db, "s1", holder2, 300.0),
          "reacquire after release");
    hermes_state_release_compression_lock(db, "s1", holder2);

    /* expired lock reclaim: acquire with tiny negative-effective TTL */
    CHECK(hermes_state_try_acquire_compression_lock(db, "s2", holder1, 0.001),
          "acquire short ttl");
    usleep(20000);
    CHECK(hermes_state_try_acquire_compression_lock(db, "s2", holder2, 300.0),
          "expired lock reclaimed");
    hermes_state_release_compression_lock(db, "s2", holder2);

    /* dead-PID reclaim: holder with a PID that can't exist */
    CHECK(hermes_state_try_acquire_compression_lock(
              db, "s3", "pid=999999999:tid=1:agent=x:nonce=z", 300.0),
          "acquire dead-pid holder");
    /* our probe: 999999999 is beyond pid_max ⇒ ESRCH ⇒ dead */
    CHECK(hermes_state_lock_holder_process_is_dead(
              "pid=999999999:tid=1:agent=x:nonce=z"),
          "dead pid detected");
    CHECK(!hermes_state_lock_holder_process_is_dead(holder1),
          "same-process holder never self-reclaimed");
    CHECK(!hermes_state_lock_holder_process_is_dead("legacy-holder"),
          "unstructured holder protected");
    CHECK(hermes_state_try_acquire_compression_lock(db, "s3", holder1, 300.0),
          "dead-pid lock reclaimed");
    hermes_state_release_compression_lock(db, "s3", holder1);

    /* ── publication protocol ────────────────────────────────────── */
    CHECK(hermes_state_create_session(db, "parent1", "cli"),
          "create parent session");

    const char *handoff =
        "[{\"role\": \"user\", \"content\": \"hi\"},"
        " {\"role\": \"assistant\", \"content\": \"summary of earlier\"}]";

    /* lease required but absent → -2 */
    CHECK(hermes_state_publish_compression_child(db, "parent1", "child1",
              "cli", handoff, NULL, NULL, NULL, NULL, NULL,
              holder1, true) == -2,
          "publish without lease rejected");

    /* with lease → success */
    CHECK(hermes_state_try_acquire_compression_lock(db, "parent1", holder1, 300.0),
          "acquire for publish");
    CHECK(hermes_state_publish_compression_child(db, "parent1", "child1",
              "cli", handoff, NULL, NULL, NULL, NULL, NULL,
              holder1, true) == 0,
          "publish with lease");

    /* parent now ended with compression; child is the unique live child */
    char *child = hermes_state_find_live_compression_child(db, "parent1");
    CHECK(child && strcmp(child, "child1") == 0, "find_live child");
    free(child);

    /* live parent → NULL */
    CHECK(hermes_state_create_session(db, "parent2", "cli"),
          "create parent2");
    CHECK(hermes_state_find_live_compression_child(db, "parent2") == NULL,
          "live parent has no child");

    /* double publish rejected (parent already ended → -4) */
    CHECK(hermes_state_publish_compression_child(db, "parent1", "child2",
              "cli", handoff, NULL, NULL, NULL, NULL, NULL,
              holder1, true) == -4,
          "double publish rejected");

    /* empty handoff rejected */
    CHECK(hermes_state_try_acquire_compression_lock(db, "parent2", holder1, 300.0),
          "acquire parent2");
    CHECK(hermes_state_publish_compression_child(db, "parent2", "child3",
              "cli", "[]", NULL, NULL, NULL, NULL, NULL,
              holder1, true) == -5,
          "empty handoff rejected");

    /* no-lease mode works */
    CHECK(hermes_state_publish_compression_child(db, "parent2", "child3",
              "cli", handoff, "modelX", NULL, "sys prompt", NULL, NULL,
              NULL, false) == 0,
          "publish without lease requirement");
    child = hermes_state_find_live_compression_child(db, "parent2");
    CHECK(child && strcmp(child, "child3") == 0, "find child3");
    free(child);

    /* ambiguity fails closed: add a second live child of parent2 via a raw
     * side-connection (create_session has no parent param; link_child would
     * also end the row, so set parent_session_id directly) */
    CHECK(hermes_state_create_session(db, "child3b", "cli"),
          "create competing child");
    {
        sqlite3 *raw = NULL;
        CHECK(sqlite3_open(path, &raw) == SQLITE_OK, "raw open");
        CHECK(sqlite3_exec(raw,
                  "UPDATE sessions SET parent_session_id = 'parent2' "
                  "WHERE id = 'child3b'", NULL, NULL, NULL) == SQLITE_OK,
              "link competing child");
        sqlite3_close(raw);
    }
    child = hermes_state_find_live_compression_child(db, "parent2");
    CHECK(child == NULL, "ambiguous children fail closed");
    free(child);

    hermes_state_release_compression_lock(db, "parent1", holder1);
    hermes_state_release_compression_lock(db, "parent2", holder1);
    hermes_state_db_close(db);
    unlink(path);

    printf(fails ? "\n%d FAILURES\n" : "\nALL PASS\n", fails);
    return fails ? 1 : 0;
}
