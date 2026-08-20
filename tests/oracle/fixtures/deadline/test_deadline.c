/*
 * test_deadline.c — behavioral oracle for the deadline port.
 * Covers clamp_timeout, resolve_timeout (config > env > default), and
 * run_bounded_sync timeout/completion. Build:
 *   cc -D_POSIX_C_SOURCE=200809L -I../include -I../lib/libjson -I../lib/libhash \
 *      -I../lib/libuuid ../src/agent/deadline.c test_deadline.c \
 *      ../lib/libjson/json.o -lpthread -lm -o t && ./t
 */
#include "deadline.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <math.h>
#include <errno.h>

#define CK(cond) do { if(!(cond)){ printf("FAIL: %s (line %d)\n", #cond, __LINE__); fails++; } else { printf("ok: %s\n", #cond); } } while(0)
static int fails = 0;

/* config stub: config_py_load_config_readonly -> returns a json with timeouts: */
#include "hermes_json.h"
json_t *config_py_load_config_readonly(void) {
    json_t *cfg = json_object();
    json_t *timeouts = json_object();
    json_set(timeouts, "tool_a", json_number(7.0));
    json_t *tools = json_object();
    json_set(tools, "concurrent_batch", json_number(42.0));
    json_set(timeouts, "tools", tools);
    json_set(cfg, "timeouts", timeouts);
    return cfg;
}

/* test work fn: sleep then set a value */
static void slow_work(void *ctx, void **out) {
    (void)ctx;
    usleep(50000); /* 50ms */
    static int v = 123;
    *out = &v;
}
static void hang_work(void *ctx, void **out) {
    (void)ctx; (void)out;
    usleep(500000); /* 500ms -> exceeds 100ms deadline */
}
static void timeout_cb(void *ctx) {
    (void)ctx;
    /* best-effort */
}

int main(void) {
    /* clamp_timeout */
    CK(clamp_timeout(-1.0) < 0);                 /* non-positive -> unbounded */
    CK(clamp_timeout(0.0) < 0);
    CK(clamp_timeout(NAN) < 0);                  /* NaN -> unbounded */
    CK(clamp_timeout(5.0) == 5.0);               /* passthrough */
    CK(clamp_timeout(1e12) == DEADLINE_MAX_SAFE_TIMEOUT_S); /* cap */

    /* resolve_timeout: config wins over default */
    double r = resolve_timeout("tool_a", 30.0, NULL);
    CK(r == 7.0);
    /* dotted key */
    CK(resolve_timeout("tools.concurrent_batch", 30.0, NULL) == 42.0);
    /* missing key -> default (clamped) */
    CK(resolve_timeout("does.not.exist", 15.0, NULL) == 15.0);
    /* env override: not set -> default */
    unsetenv("HERMES_TEST_TIMEOUT");
    CK(resolve_timeout("tool_a", 30.0, "HERMES_TEST_TIMEOUT") == 7.0); /* config still wins */
    /* env override when config absent and env set */
    CK(resolve_timeout("nope.key", 30.0, "HERMES_TEST_TIMEOUT") == 30.0);

    /* run_bounded_sync: completes within deadline */
    deadline_bounded_result_t ok = run_bounded_sync(slow_work, NULL, 2.0, "slow", NULL);
    CK(!ok.timed_out);
    CK(ok.value != NULL);
    CK(ok.error_code == 0);

    /* run_bounded_sync: times out, abandons worker */
    deadline_bounded_result_t to = run_bounded_sync(hang_work, NULL, 0.1, "hang", timeout_cb);
    CK(to.timed_out);
    CK(to.value == NULL);

    /* run_bounded_sync unbounded: completes */
    deadline_bounded_result_t ub = run_bounded_sync(slow_work, NULL, -1.0, "unbounded", NULL);
    CK(!ub.timed_out);

    /* run_bounded_async shares contract */
    deadline_bounded_result_t ao = run_bounded_async(hang_work, NULL, 0.1, "hang_async", timeout_cb);
    CK(ao.timed_out);

    /* kill_process_tree: spawn a child that outlives, kill the tree, confirm
     * the child is gone. */
    pid_t child = fork();
    if (child == 0) {
        /* child: sleep long enough to be killed */
        usleep(3000000);
        _exit(0);
    }
    usleep(50000); /* let child start */
    CK(kill_process_tree(child, 0) == true);  /* default SIGKILL */
    int status = 0;
    /* child should be reaped quickly (killed); poll briefly since the kernel
     * may not have reaped the zombie at the first WNOHANG. */
    pid_t got = 0;
    for (int i = 0; i < 50 && got == 0; i++) {
        got = waitpid(child, &status, WNOHANG);
        if (got == 0) usleep(20000);
    }
    if (got != child) { printf("  [diag] waitpid got=%d errno=%d\n", (int)got, errno); }
    CK(got == child);  /* child terminated by the tree kill */

    printf("\n%s (%d failures)\n", fails ? "ORACLE FAILED" : "ORACLE PASSED", fails);
    return fails ? 1 : 0;
}
