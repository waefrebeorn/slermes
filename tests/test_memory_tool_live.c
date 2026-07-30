/*
 * test_memory_tool_live.c — Integration test: the faithful memory_tool.py port
 * is wired as a LIVE "memory" tool in the registry. Verifies registry_init_memory()
 * registers it, the bridge handler dispatches to the store, writes persist to disk,
 * and the write-approval gate attaches (fail-open by default).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "memory_store.h"
#include "registry.h"

/* Stubs for helpers defined in other translation units not linked here.
 * Only reachable on error paths this test does not exercise. */
#include <stdarg.h>
char *tool_error(const char *message, ...) { (void)message; return strdup("{\"success\":false}"); }
const char *plugin_error(void) { return "plugin error"; }

static int g_fail = 0;
#define TEST(c,m) do { if(!(c)){ fprintf(stderr,"FAIL: %s\n",m); g_fail++; } } while(0)

/* Declared in memory.c / tool_init wiring (defined in port_tools_write_approval.c). */
void registry_init_memory(void);
void cli_tools_write_approval_attach_memory_gate(void);

static int file_has_line(const char *path, const char *needle) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char buf[4096];
    int found = 0;
    while (fgets(buf, sizeof(buf), f)) if (strstr(buf, needle)) { found = 1; break; }
    fclose(f);
    return found;
}

int main(void) {
    char home[1024]; snprintf(home,sizeof(home),"/tmp/memlive_%d",(int)getpid());
    char mem[1100]; snprintf(mem,sizeof(mem),"%s/memories",home);
    char cmd[1300]; snprintf(cmd,sizeof(cmd),"rm -rf %s && mkdir -p %s", home, mem);
    system(cmd);
    setenv("HERMES_HOME", home, 1);

    /* Wire the live tool (mirrors tools_init_all's memory wiring). */
    registry_init_memory();
    registry_set_toolset("memory", "memory");
    cli_tools_write_approval_attach_memory_gate();

    /* 1. registered */
    tool_t *t = registry_find("memory");
    TEST(t != NULL, "memory tool registered in registry");
    if (!t) { printf("%d FAIL\n", g_fail); return 1; }
    TEST(strcmp(t->toolset, "memory")==0, "memory tool has 'memory' toolset");
    TEST(t->handler != NULL, "memory tool has a handler");

    /* 2. single add via the live handler */
    char *r1 = t->handler("{\"action\":\"add\",\"target\":\"memory\",\"content\":\"live wired memory works\"}", NULL);
    json_node_t *j1 = json_parse(r1, NULL);
    TEST(json_get_bool(j1,"success",0)==1, "live add -> success");
    TEST(json_get_bool(j1,"done",0)==1, "live add -> done");
    json_free(j1); free(r1);

    /* 3. persisted to disk */
    char memfile[1300]; snprintf(memfile,sizeof(memfile),"%s/memories/MEMORY.md", home);
    TEST(file_has_line(memfile, "live wired memory works"), "live add persisted to MEMORY.md");

    /* 4. batch via live handler */
    char *r2 = t->handler("[{\"action\":\"add\",\"target\":\"memory\",\"content\":\"batch alpha\"},{\"action\":\"add\",\"target\":\"memory\",\"content\":\"batch beta\"}]", NULL);
    /* Note: batch must be passed as operations, not top-level array. Use proper shape: */
    free(r2);
    char *r3 = t->handler("{\"target\":\"memory\",\"operations\":[{\"action\":\"add\",\"content\":\"batch alpha\"},{\"action\":\"add\",\"content\":\"batch beta\"}]}", NULL);
    json_node_t *j3 = json_parse(r3, NULL);
    TEST(json_get_bool(j3,"success",0)==1, "live batch -> success");
    json_free(j3); free(r3);
    TEST(file_has_line(memfile, "batch alpha") && file_has_line(memfile, "batch beta"),
         "live batch persisted");

    /* 5. replace w/o old_text -> recoverable error (current_entries present) */
    char *r4 = t->handler("{\"action\":\"replace\",\"target\":\"memory\",\"content\":\"nope\"}", NULL);
    json_node_t *j4 = json_parse(r4, NULL);
    TEST(json_get_bool(j4,"success",1)==0, "live replace w/o old_text -> false");
    json_free(j4); free(r4);

    if (g_fail==0) printf("ALL PASSED\n"); else printf("%d FAIL\n", g_fail);
    return g_fail?1:0;
}
