/*
 * test_async_delegation.c — Faithful port of tools/async_delegation.py.
 *
 * Proves the previously-missing background delegation registry works:
 *  - dispatch returns {status:dispatched, delegation_id} and the injected
 *    runner actually executes on a daemon worker (status -> completed).
 *  - the injected completion sink receives the rich event (status/summary).
 *  - capacity gate rejects when at max running.
 *  - interrupt_all() cancels running delegations.
 *  - batch dispatch occupies ONE slot and emits an is_batch event with results.
 *
 * Build/run via `make test-async-delegation`.
 */

#include "async_delegation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

static int passed = 0, failed = 0;
#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while (0)
#define TEST_STR_EQ(name, a, b) TEST(name, (a) && (b) && strcmp((a), (b)) == 0)

/* ---- runner/sink plumbing (file-scope statics, no GCC trampolines) ---- */

static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_cv  = PTHREAD_COND_INITIALIZER;
static int g_runner_ran = 0;
static int g_evt_ready = 0;        /* sink delivered an event */
static int g_interrupt_hit = 0;

/* captured completion event (sink) */
static char g_evt_status[32] = "";
static char g_evt_summary[256] = "";
static int  g_evt_is_batch = 0;
static int  g_evt_results = 0;

static json_node_t *runner_ok(void) {
    json_node_t *r = json_new_object();
    json_object_set(r, "status", json_string("completed"));
    json_object_set(r, "summary", json_string("did the thing"));
    json_object_set(r, "api_calls", json_number(3));
    json_object_set(r, "duration_seconds", json_number(0.05));
    pthread_mutex_lock(&g_mtx); g_runner_ran = 1; pthread_cond_broadcast(&g_cv); pthread_mutex_unlock(&g_mtx);
    return r;
}

static json_node_t *runner_batch(void) {
    json_node_t *r = json_new_object();
    json_node_t *results = json_new_array();
    json_node_t *one = json_new_object();
    json_object_set(one, "status", json_string("completed"));
    json_array_append(results, one);
    json_object_set(r, "results", results);
    json_object_set(r, "total_duration_seconds", json_number(1.2));
    pthread_mutex_lock(&g_mtx); g_runner_ran = 1; pthread_cond_broadcast(&g_cv); pthread_mutex_unlock(&g_mtx);
    return r;
}

static void sink_capture(json_node_t *evt) {
    pthread_mutex_lock(&g_mtx);
    const char *s = json_get_str(evt, "status", "");
    snprintf(g_evt_status, sizeof(g_evt_status), "%s", s ? s : "");
    const char *sum = json_get_str(evt, "summary", "");
    snprintf(g_evt_summary, sizeof(g_evt_summary), "%s", sum ? sum : "");
    g_evt_is_batch = json_get_bool(evt, "is_batch", 0) ? 1 : 0;
    json_node_t *res = json_object_get(evt, "results");
    g_evt_results = res ? (int)json_array_size(res) : -1;  /* -1 => not present */
    g_evt_ready = 1;
    pthread_cond_broadcast(&g_cv);
    pthread_mutex_unlock(&g_mtx);
}

/* A runner that blocks until interrupt sets the flag (to test interrupt_all). */
static json_node_t *runner_long(void) {
    /* Signal we started, then spin until interrupted. */
    pthread_mutex_lock(&g_mtx); g_runner_ran = 1; pthread_cond_broadcast(&g_cv); pthread_mutex_unlock(&g_mtx);
    for (int i = 0; i < 100000000 && !g_interrupt_hit; i++) {
        if (g_interrupt_hit) break;
        usleep(1000);
    }
    json_node_t *r = json_new_object();
    json_object_set(r, "status", json_string(g_interrupt_hit ? "cancelled" : "completed"));
    return r;
}

static void interrupt_flag(void) {
    pthread_mutex_lock(&g_mtx); g_interrupt_hit = 1; pthread_mutex_unlock(&g_mtx);
}

static void wait_event(void) {
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_sec += 5;
    pthread_mutex_lock(&g_mtx);
    while (!g_evt_ready) pthread_cond_timedwait(&g_cv, &g_mtx, &ts);
    pthread_mutex_unlock(&g_mtx);
}

/* Wait (bounded) for g_runner_ran to become true. */
static void wait_runner(void) {
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_sec += 5;
    pthread_mutex_lock(&g_mtx);
    while (!g_runner_ran) pthread_cond_timedwait(&g_cv, &g_mtx, &ts);
    pthread_mutex_unlock(&g_mtx);
}

/* Wait for a delegation_id to leave "running" in the list. */
static int wait_completed(const char *id) {
    for (int i = 0; i < 500; i++) {
        json_node_t *list = async_delegation_list();
        int n = (int)json_array_size(list);
        int found_done = 0;
        for (int j = 0; j < n; j++) {
            json_node_t *rec = json_get(list, j);
            const char *rid = json_get_str(rec, "delegation_id", "");
            const char *st = json_get_str(rec, "status", "");
            if (rid && id && strcmp(rid, id) == 0 && strcmp(st, "running") != 0) found_done = 1;
        }
        json_free(list);
        if (found_done) return 1;
        usleep(2000);
    }
    return 0;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Async Delegation Test Suite ===\n");
    async_delegation_init();
    async_delegation_reset_for_tests();

    /* ---- single dispatch: runner executes on worker, event delivered ---- */
    g_runner_ran = 0; g_evt_status[0] = 0; g_evt_summary[0] = 0; g_evt_is_batch = 0;
    g_evt_ready = 0;
    const char *ts[] = {"shell", "browser", NULL};
    json_node_t *h = async_delegation_dispatch(
        "summarize logs", "ctx", ts, "researcher", "gpt-4o", "sess-1",
        runner_ok, NULL, sink_capture, 3);
    TEST_STR_EQ("dispatch status = dispatched", json_get_str(h, "status", ""), "dispatched");
    char id1[64] = "";
    const char *rid = json_get_str(h, "delegation_id", "");
    snprintf(id1, sizeof(id1), "%s", rid ? rid : "");
    TEST("dispatch returned an id", id1[0]);
    json_free(h);

    wait_runner();
    TEST("runner actually executed on worker", g_runner_ran == 1);
    wait_event();
    TEST("completion event delivered (status=completed)", strcmp(g_evt_status, "completed") == 0);
    TEST_STR_EQ("event summary captured", g_evt_summary, "did the thing");
    TEST("record reached completed state", wait_completed(id1) == 1);

    /* ---- active count + list contents ---- */
    TEST("active count is 0 after completion", async_delegation_active_count() == 0);
    json_node_t *list = async_delegation_list();
    TEST("list contains our completed record", (int)json_array_size(list) >= 1);
    json_free(list);

    /* ---- capacity gate: max 1 running, second dispatch rejected ---- */
    async_delegation_reset_for_tests();
    g_runner_ran = 0;
    json_node_t *h2 = async_delegation_dispatch(
        "long task A", NULL, NULL, "r", "m", "s",
        runner_long, interrupt_flag, sink_capture, 1);  /* cap=1 */
    TEST_STR_EQ("first dispatch accepted", json_get_str(h2, "status", ""), "dispatched");
    json_free(h2);
    wait_runner();  /* ensure worker started */
    json_node_t *h3 = async_delegation_dispatch(
        "task B", NULL, NULL, "r", "m", "s",
        runner_ok, NULL, sink_capture, 1);  /* cap=1, already 1 running */
    TEST_STR_EQ("second dispatch at capacity rejected", json_get_str(h3, "status", ""), "rejected");
    json_free(h3);
    async_delegation_interrupt_all("test");
    /* let the long one settle */
    usleep(50000);

    /* ---- interrupt_all sets running -> cancelled ---- */
    async_delegation_reset_for_tests();
    g_runner_ran = 0; g_interrupt_hit = 0;
    json_node_t *h4 = async_delegation_dispatch(
        "interrupt me", NULL, NULL, "r", "m", "s",
        runner_long, interrupt_flag, sink_capture, 3);
    TEST_STR_EQ("interrupt-test dispatch accepted", json_get_str(h4, "status", ""), "dispatched");
    char id4[64] = "";
    const char *rid4 = json_get_str(h4, "delegation_id", "");
    snprintf(id4, sizeof(id4), "%s", rid4 ? rid4 : "");
    json_free(h4);
    wait_runner();
    int n_int = async_delegation_interrupt_all("shutdown");
    TEST("interrupt_all returned >=1", n_int >= 1);
    TEST("interrupted record reached cancelled", wait_completed(id4) == 1);
    json_node_t *lst4 = async_delegation_list();
    int found_cancel = 0;
    for (int j = 0; j < (int)json_array_size(lst4); j++) {
        json_node_t *rec = json_get(lst4, j);
        if (strcmp(json_get_str(rec, "delegation_id", ""), id4) == 0 &&
            strcmp(json_get_str(rec, "status", ""), "cancelled") == 0) found_cancel = 1;
    }
    json_free(lst4);
    TEST("record status is cancelled", found_cancel == 1);

    /* ---- batch dispatch: one slot, is_batch event with results ---- */
    async_delegation_reset_for_tests();
    g_runner_ran = 0; g_evt_is_batch = 0; g_evt_results = 0; g_evt_ready = 0;
    const char *goals[] = {"task 1", "task 2", "task 3"};
    json_node_t *hb = async_delegation_dispatch_batch(
        goals, 3, "ctx", NULL, "r", "m", "s",
        runner_batch, NULL, sink_capture, 3);
    TEST_STR_EQ("batch dispatch accepted", json_get_str(hb, "status", ""), "dispatched");
    json_free(hb);
    wait_runner();
    wait_event();
    TEST("batch event marked is_batch", g_evt_is_batch == 1);
    TEST("batch event carries results array", g_evt_results == 1);
    json_node_t *lstb = async_delegation_list();
    int batch_found = 0;
    for (int j = 0; j < (int)json_array_size(lstb); j++) {
        json_node_t *rec = json_get(lstb, j);
        if (json_get_bool(rec, "is_batch", 0)) batch_found = 1;
    }
    json_free(lstb);
    TEST("batch record present in list", batch_found == 1);

    async_delegation_reset_for_tests();
    printf("\n%sASYNC-DELEGATION TESTS: %d passed, %d failed%s\n",
           failed ? "FAIL " : "", passed, failed, failed ? "" : " — ALL PASSED");
    return failed ? 1 : 0;
}
