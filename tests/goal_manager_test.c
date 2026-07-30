/*
 * goal_manager_test.c — behavioral test for port_goals_manager.c
 *
 * Exercises the pure GoalManager surface + judge-prompt helpers against the
 * documented semantics of hermes_cli/goals.py. Persistence is in-memory
 * (no vtab), and liveness callbacks are injected for the wait-barrier tests.
 */

#include "goal_contract.h"
#include "tools/process_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

static int checks = 0, failures = 0;
#define CHECK(cond, msg) do { checks++; if (!(cond)) { failures++; printf("FAIL: %s\n", msg); } } while (0)
#define EQ_STR(a, b, msg) do { checks++; const char *_a=(a), *_b=(b); \
    if (!_a || !_b || strcmp(_a,_b)!=0) { failures++; printf("FAIL: %s\n  got=%s\n  exp=%s\n", msg, _a?_a:"(null)", _b?_b:"(null)"); } } while (0)
#define HAS(hay, needle, msg) do { checks++; const char *_h=(hay), *_n=(needle); \
    if (!_h || !_n || strstr(_h,_n)==NULL) { failures++; printf("FAIL: %s\n  missing=%s\n", msg, _n?_n:"(null)"); } } while (0)

/* in-memory persistence */
static char *g_store = NULL;
static char *mem_load(const char *sid) { (void)sid; return g_store ? strdup(g_store) : NULL; }
static int mem_save(const char *sid, const char *json) { (void)sid; free(g_store); g_store = strdup(json); return 0; }

static goal_manager_vtab_t mem_vtab = { mem_load, mem_save, NULL, NULL };

/* liveness for wait tests */
static int g_pid_alive;
static int fake_pid_alive(long pid) { (void)pid; return g_pid_alive; }
static int g_session_waiting;
static int fake_session_waiting(const char *sid) { (void)sid; return g_session_waiting; }

static goal_manager_vtab_t live_vtab = { mem_load, mem_save, fake_pid_alive, fake_session_waiting };

static void test_manager_basic(void) {
    goal_manager_t *m = goal_manager_new("sess1", &mem_vtab, 0);
    CHECK(!goal_manager_has_goal(m), "no goal initially");
    CHECK(!goal_manager_is_active(m), "not active initially");

    goal_state_t *s = goal_manager_set(m, "Write the report", 0, NULL);
    CHECK(s != NULL, "set returns state");
    CHECK(goal_manager_is_active(m), "active after set");
    CHECK(goal_manager_has_goal(m), "has_goal after set");
    EQ_STR(goal_state_status_str(s), "active", "status active");
    CHECK(goal_state_max_turns(s) == 20, "default max_turns 20");

    /* persistence: reload in a new manager */
    goal_manager_free(m);
    m = goal_manager_new("sess1", &mem_vtab, 0);
    CHECK(goal_manager_has_goal(m), "goal reloaded from store");
    EQ_STR(goal_state_goal(goal_manager_state(m)), "Write the report", "reloaded goal text");

    /* status_line */
    char *sl = goal_manager_status_line(m);
    HAS(sl, "active", "status_line marks active");
    HAS(sl, "Write the report", "status_line has goal");
    free(sl);

    goal_manager_free(m);
}

static void test_subgoals(void) {
    goal_manager_t *m = goal_manager_new("sess2", &mem_vtab, 0);
    goal_manager_set(m, "Build it", 0, NULL);
    char *a = goal_manager_add_subgoal(m, "  add tests  ");
    CHECK(a != NULL, "add_subgoal returns text");
    EQ_STR(a, "add tests", "subgoal trimmed");
    free(a);
    char *b = goal_manager_add_subgoal(m, "add tests"); /* dedupe */
    CHECK(b == NULL, "duplicate subgoal rejected");
    char *c = goal_manager_add_subgoal(m, "ship it");
    EQ_STR(c, "ship it", "second subgoal");
    free(c);

    CHECK(goal_state_subgoal_count(goal_manager_state(m)) == 2, "two subgoals");
    char *rend = goal_manager_render_subgoals(m);
    HAS(rend, "- 1. add tests", "render subgoal 1");
    HAS(rend, "- 2. ship it", "render subgoal 2");
    free(rend);

    char *removed = goal_manager_remove_subgoal(m, 1);
    EQ_STR(removed, "add tests", "removed first");
    free(removed);
    CHECK(goal_state_subgoal_count(goal_manager_state(m)) == 1, "one left");

    int prev = goal_manager_clear_subgoals(m);
    CHECK(prev == 1, "clear_subgoals returns prev count");
    CHECK(goal_state_subgoal_count(goal_manager_state(m)) == 0, "none left");

    goal_manager_free(m);
}

static void test_pause_resume_clear(void) {
    goal_manager_t *m = goal_manager_new("sess3", &mem_vtab, 0);
    goal_manager_set(m, "Do thing", 0, NULL);
    goal_manager_pause(m, "user-paused");
    CHECK(strcmp(goal_state_status_str(goal_manager_state(m)), "paused") == 0, "paused");
    char *sl = goal_manager_status_line(m);
    HAS(sl, "paused", "status_line paused");
    free(sl);
    goal_manager_resume(m, true);
    CHECK(goal_manager_is_active(m), "resumed active");
    goal_manager_mark_done(m, "finished");
    CHECK(strcmp(goal_state_status_str(goal_manager_state(m)), "done") == 0, "done");
    sl = goal_manager_status_line(m);
    HAS(sl, "done", "status_line done");
    free(sl);
    goal_manager_clear(m);
    CHECK(!goal_manager_has_goal(m), "cleared -> no goal");
    sl = goal_manager_status_line(m);
    EQ_STR(sl, "No active goal. Set one with /goal <text>.", "status_line after clear");
    free(sl);
    goal_manager_free(m);
}

static void test_contract(void) {
    goal_manager_t *m = goal_manager_new("sess4", &mem_vtab, 0);
    goal_contract_t *c = goal_contract_new();
    goal_contract_set(c, "verification", "tests pass");
    goal_contract_set(c, "constraints", "keep API shape");
    goal_manager_set(m, "Ship", 0, c);
    goal_contract_free(c);
    CHECK(goal_manager_has_contract(m), "has contract");
    char *rc = goal_manager_render_contract(m);
    HAS(rc, "Verification: tests pass", "contract render verification");
    HAS(rc, "Constraints: keep API shape", "contract render constraints");
    free(rc);
    char *cp = goal_manager_next_continuation_prompt(m);
    HAS(cp, "Completion contract:", "continuation prompt has contract");
    HAS(cp, "tests pass", "continuation includes verification");
    free(cp);
    goal_manager_free(m);
}

static void test_wait_barrier(void) {
    goal_manager_t *m = goal_manager_new("sess5", &live_vtab, 0);
    goal_manager_set(m, "Wait job", 0, NULL);

    /* pid barrier */
    g_pid_alive = 1;
    goal_manager_wait_on(m, 1234, "building", (double)time(NULL));
    CHECK(goal_manager_is_waiting(m, (double)time(NULL)), "waiting while pid alive");
    char *sl = goal_manager_status_line(m);
    HAS(sl, "parked on pid 1234", "status shows pid park");
    free(sl);
    g_pid_alive = 0;
    CHECK(!goal_manager_is_waiting(m, (double)time(NULL)), "not waiting when pid dead (auto-cleared)");
    sl = goal_manager_status_line(m);
    HAS(sl, "active", "status active after barrier cleared");
    free(sl);

    /* time barrier */
    goal_manager_wait_for_seconds(m, 100, "cooldown", (double)time(NULL));
    CHECK(goal_manager_is_waiting(m, (double)time(NULL)), "waiting within deadline");
    CHECK(goal_manager_stop_waiting(m), "stop_waiting clears");
    CHECK(!goal_manager_stop_waiting(m), "stop_waiting idempotent");
    CHECK(!goal_manager_is_waiting(m, (double)time(NULL)), "not waiting after stop");
    CHECK(!goal_manager_is_waiting(m, (double)time(NULL) + 200.0), "not waiting after deadline");

    /* session barrier */
    g_session_waiting = 1;
    goal_manager_wait_on_session(m, "task-9", "", (double)time(NULL));
    CHECK(goal_manager_is_waiting(m, (double)time(NULL)), "waiting on session");
    g_session_waiting = 0;
    CHECK(!goal_manager_is_waiting(m, (double)time(NULL)), "session barrier auto-clears");

    goal_manager_free(m);
}

static void test_judge_parse(void) {
    char *verdict = NULL, *reason = NULL, *wait = NULL;
    bool failed = false;

    goal_judge_parse_response("{\"verdict\":\"done\",\"reason\":\"shipped\"}", &verdict, &reason, &failed, &wait);
    EQ_STR(verdict, "done", "verdict done");
    EQ_STR(reason, "shipped", "reason shipped");
    CHECK(!failed, "not parse failed");
    free(verdict); free(reason); free(wait);

    goal_judge_parse_response("{\"done\":true}", &verdict, &reason, &failed, &wait);
    EQ_STR(verdict, "done", "legacy done true");
    free(verdict); free(reason); free(wait);

    goal_judge_parse_response("{\"verdict\":\"wait\",\"wait_on_pid\":55,\"reason\":\"CI\"}", &verdict, &reason, &failed, &wait);
    EQ_STR(verdict, "wait", "verdict wait");
    EQ_STR(wait, "{\"pid\":55}", "wait directive pid");
    free(verdict); free(reason); free(wait);

    goal_judge_parse_response("{\"verdict\":\"wait\",\"seconds\":30}", &verdict, &reason, &failed, &wait);
    EQ_STR(wait, "{\"seconds\":30}", "wait directive seconds");
    free(verdict); free(reason); free(wait);

    /* unparseable */
    goal_judge_parse_response("just prose", &verdict, &reason, &failed, &wait);
    CHECK(failed, "prose => parse_failed");
    EQ_STR(verdict, "continue", "prose downgraded to continue");
    free(verdict); free(reason); free(wait);

    /* wait with no target => continue */
    goal_judge_parse_response("{\"verdict\":\"wait\"}", &verdict, &reason, &failed, &wait);
    EQ_STR(verdict, "continue", "wait no target => continue");
    free(verdict); free(reason); free(wait);
}

static void test_judge_extract_and_bg(void) {
    char *j = goal_judge_extract_json_object("```json\n{\"verdict\":\"done\"}\n```");
    CHECK(j != NULL, "extract strips fences");
    HAS(j, "\"verdict\"", "extracted object has verdict");
    free(j);

    j = goal_judge_extract_json_object("here is {\"a\":1} and more");
    EQ_STR(j, "{\"a\":1}", "first json object");
    free(j);

    const char *procs[] = {
        "{\"status\":\"running\",\"pid\":7,\"command\":\"make test\",\"uptime_seconds\":12,\"output_preview\":\"ok\"}",
        "{\"status\":\"exited\",\"pid\":8,\"command\":\"old\"}",
        NULL
    };
    char *bg = goal_judge_render_background_block(procs);
    HAS(bg, "- pid 7", "running pid shown");
    HAS(bg, "make test", "command shown");
    HAS(bg, "running 12s", "uptime shown");
    CHECK(strstr(bg, "exited") == NULL, "exited process omitted");
    free(bg);

    CHECK(goal_judge_max_tokens_resolved(NULL) == 4096, "default max tokens (resolved)");
}

static int fake_always_waiting(const char *id) {
    (void)id; return 1;
}
static int fake_never_waiting(const char *id) {
    (void)id; return 0;
}

static void test_os_liveness(void) {
    /* _pid_alive: fail-safe on non-positive; real POSIX probe for self. */
    CHECK(!goal_pid_alive(0), "pid 0 dead");
    CHECK(!goal_pid_alive(-1), "negative pid dead");
    CHECK(goal_pid_alive((long)getpid()), "own pid alive");
    CHECK(!goal_pid_alive(99999999L), "absent pid dead");

    /* _session_waiting: injected callback, fail-safe on NULL/empty. */
    CHECK(!goal_session_waiting(NULL, "sessX"), "NULL cb => not waiting");
    CHECK(!goal_session_waiting(fake_always_waiting, ""), "empty id => not waiting");
    CHECK(goal_session_waiting(fake_always_waiting, "sessX"), "always-waiting cb");
    CHECK(!goal_session_waiting(fake_never_waiting, "sessX"), "never-waiting cb");

    /* integration: a session barrier actually parks while the callback says so */
    goal_manager_t *m = goal_manager_new("lv", &mem_vtab, 0);
    goal_manager_set(m, "wait on session", 0, NULL);
    goal_manager_wait_on_session(m, "sessX", "waiting for it", 0.0);
    /* mem_vtab has no session_waiting callback => default fail-safe => not waiting */
    CHECK(!goal_manager_is_waiting(m, 0.0), "session barrier fails safe w/o callback");
    goal_manager_free(m);
}

static void test_gather_background_processes(void) {
    /* registry auto-inits on first spawn/list. Spawn a real long-running
     * process tagged to a task. */
    ProcessSession *s = process_registry_spawn_local(
        "sleep 5", "/tmp", "gather_task_1", "gather_sess_key", NULL);
    CHECK(s != NULL, "spawned sleep session");
    if (!s) return;

    /* Gather for the task -> should include the running process. */
    char *running = goal_gather_background_processes("gather_task_1");
    CHECK(running && strstr(running, s->id) != NULL, "running process appears in gather");
    free(running);

    /* Gather with no filter -> still sees it among all. */
    char *all = goal_gather_background_processes(NULL);
    CHECK(all && strstr(all, s->id) != NULL, "no-filter gather includes process");
    free(all);

    /* Fail-safe: unknown task -> "[]" (no entries for that task). */
    char *none = goal_gather_background_processes("no_such_task");
    CHECK(none && strcmp(none, "[]") == 0, "unknown task yields []");
    free(none);

    /* Kill it, then it should be dropped (status becomes exited). */
    char *killres = process_registry_kill(s->id);
    free(killres);
    char *after = goal_gather_background_processes("gather_task_1");
    CHECK(after && strstr(after, s->id) == NULL, "killed process dropped from gather");
    free(after);
}

int main(void) {
    test_manager_basic();
    test_subgoals();
    test_pause_resume_clear();
    test_contract();
    test_wait_barrier();
    test_judge_parse();
    test_judge_extract_and_bg();
    test_os_liveness();
    test_gather_background_processes();
    printf("goal_manager_test: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
