/*
 * test_close_terminal_tool.c — Faithful port of tools/close_terminal_tool.py
 * plus the process_registry.request_close_terminal sink it routes through.
 *
 * Verifies: gating on HERMES_DESKTOP, the no-sink error path, the wired-sink
 * happy path (incl. a missing session, which is NOT an error), and the live
 * registry registration of the close_terminal tool.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "hermes_json.h"
#include "registry.h"
#include "process_registry.h"
#include "port_close_terminal_tool.h"

/* stub for tool_error (defined in agent core, not linked here; only hit on
 * error paths / registry dispatch this unit test does not exercise). */
char *tool_error(const char *m, ...) { (void)m; return strdup("{\"success\":false}"); }

static int g_fail = 0;
#define TEST(c,m) do { if(!(c)){ fprintf(stderr,"FAIL: %s\n",m); g_fail++; } } while(0)

/* Track sink invocations. */
static int g_sink_calls = 0;
static char g_sink_id[64];
static void test_sink(ProcessSession *s, const char *id) {
    (void)s;
    g_sink_calls++;
    snprintf(g_sink_id, sizeof(g_sink_id), "%s", id ? id : "");
}

static void reset_sink(void) { g_sink_calls = 0; g_sink_id[0] = 0; }

int main(void) {
    process_registry_init();

    /* 1. No sink wired -> desktop-only error (faithful). */
    process_registry_set_close_sink(NULL);
    reset_sink();
    char *r1 = process_registry_request_close_terminal("proc_abc123");
    json_node_t *j1 = json_parse(r1, NULL);
    TEST(strcmp(json_object_get_string(j1,"status",""), "error")==0, "no sink -> status error");
    TEST(strstr(json_object_get_string(j1,"error",""), "desktop") != NULL, "no sink -> desktop-only message");
    TEST(g_sink_calls == 0, "no sink -> sink not called");
    json_free(j1); free(r1);

    /* 2. Sink wired -> calls sink, returns ok/closed/note. */
    process_registry_set_close_sink(test_sink);
    reset_sink();
    char *r2 = process_registry_request_close_terminal("proc_xyz789");
    json_node_t *j2 = json_parse(r2, NULL);
    TEST(strcmp(json_object_get_string(j2,"status",""), "ok")==0, "sink wired -> status ok");
    TEST(strcmp(json_object_get_string(j2,"closed",""), "proc_xyz789")==0, "sink wired -> closed id echoed");
    TEST(strlen(json_object_get_string(j2,"note","")) > 0, "sink wired -> note present");
    TEST(g_sink_calls == 1, "sink wired -> sink called once");
    TEST(strcmp(g_sink_id, "proc_xyz789")==0, "sink wired -> correct id passed");
    json_free(j2); free(r2);

    /* 3. Missing session is NOT an error (tab can linger after prune). */
    reset_sink();
    char *r3 = process_registry_request_close_terminal("proc_does_not_exist");
    json_node_t *j3 = json_parse(r3, NULL);
    TEST(strcmp(json_object_get_string(j3,"status",""), "ok")==0, "missing session -> still ok");
    TEST(g_sink_calls == 1, "missing session -> sink still called");
    json_free(j3); free(r3);
    process_registry_set_close_sink(NULL);

    /* 4. check_close_terminal_requirements gated on HERMES_DESKTOP. */
    unsetenv("HERMES_DESKTOP");
    TEST(check_close_terminal_requirements() == false, "no HERMES_DESKTOP -> unavailable");
    setenv("HERMES_DESKTOP", "1", 1);
    TEST(check_close_terminal_requirements() == true, "HERMES_DESKTOP=1 -> available");
    setenv("HERMES_DESKTOP", "true", 1);
    TEST(check_close_terminal_requirements() == true, "HERMES_DESKTOP=true -> available");
    setenv("HERMES_DESKTOP", "yes", 1);
    TEST(check_close_terminal_requirements() == true, "HERMES_DESKTOP=yes -> available");
    setenv("HERMES_DESKTOP", "0", 1);
    TEST(check_close_terminal_requirements() == false, "HERMES_DESKTOP=0 -> unavailable");
    unsetenv("HERMES_DESKTOP");

    /* 5. Live tool registration + gating. */
    registry_init_close_terminal();
    /* The tool is desktop-gated: it only becomes findable once HERMES_DESKTOP
     * is set and availability is refreshed (registry_find filters on available). */
    setenv("HERMES_DESKTOP", "1", 1);
    registry_set_check_fn("close_terminal", check_close_terminal_requirements); /* re-eval now */
    tool_t *t = registry_find("close_terminal");
    TEST(t != NULL, "close_terminal registered in registry");
    if (t) {
        TEST(t->handler != NULL, "close_terminal has handler");
        TEST(strcmp(t->toolset, "terminal")==0, "close_terminal in 'terminal' toolset");
        /* Without HERMES_DESKTOP it must be unavailable. Re-setting the check_fn
         * re-evaluates immediately (bypassing the 30s availability cache). */
        unsetenv("HERMES_DESKTOP");
        registry_set_check_fn("close_terminal", check_close_terminal_requirements);
        TEST(registry_is_available("close_terminal") == false, "close_terminal unavailable w/o HERMES_DESKTOP");
        setenv("HERMES_DESKTOP", "1", 1);
        registry_set_check_fn("close_terminal", check_close_terminal_requirements);
        TEST(registry_is_available("close_terminal") == true, "close_terminal available with HERMES_DESKTOP");
        unsetenv("HERMES_DESKTOP");

        /* Handler with empty process_id -> required error. */
        char *rh = t->handler("{}", NULL);
        json_node_t *jh = json_parse(rh, NULL);
        TEST(json_get_bool(jh, "success", 1) == 0, "handler empty id -> success false");
        json_free(jh); free(rh);
    }

    if (g_fail==0) printf("ALL PASSED\n"); else printf("%d FAIL\n", g_fail);
    return g_fail?1:0;
}
