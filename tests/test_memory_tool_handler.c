/*
 * test_memory_tool_handler.c — E2E + unit tests for the memory_tool handler
 * (memory_tool_run / memory_tool_apply_pending / memory_tool_available /
 *  write-gate integration), faithful port of tools/memory_tool.py.
 *
 * Each test uses its OWN isolated memories dir so writes don't leak across
 * tests. Verifies handler dispatch, gate (block/stage/allow), missing-old_text,
 * apply_pending, and E2E parity vs the real Python module via popen.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "memory_store.h"
#include "hermes_json.h"

/* stub for tool_error (defined in agent core, not linked here; only hit on
 * error paths this unit test does not exercise). */
char *tool_error(const char *m, ...) { (void)m; return strdup("{\"success\":false}"); }

#define HOME "mth_handler"

static int g_fail = 0;
#define TEST(cond, msg) do { if(!(cond)){ fprintf(stderr,"FAIL: %s\n", msg); g_fail++; } } while(0)

/* Create a fresh isolated memories dir; returns it in membuf. Sets HERMES_HOME. */
static void fresh_mem(char *membuf, int tag) {
    char home[1100];
    snprintf(home,sizeof(home),"/tmp/%s_%d_%d", HOME, (int)getpid(), tag);
    snprintf(membuf,1100,"%s/memories", home);
    char cmd[1300]; snprintf(cmd,sizeof(cmd),"rm -rf %s && mkdir -p %s", home, membuf);
    system(cmd);
    setenv("HERMES_HOME", home, 1);
}

static char g_home[1024];
static char g_mem[1100];

static void mk_home(void) {
    snprintf(g_home,sizeof(g_home),"/tmp/%s_%d", HOME, (int)getpid());
    snprintf(g_mem,sizeof(g_mem),"%s/memories", g_home);
    char cmd[1200]; snprintf(cmd,sizeof(cmd),"rm -rf %s && mkdir -p %s", g_home, g_mem);
    system(cmd);
    setenv("HERMES_HOME", g_home, 1);
}

static char *py_eval_home(const char *mem, const char *expr) {
    char home[1100]; snprintf(home,sizeof(home),"%s/..", mem); /* mem = home/memories */
    char tmpl[] = "import sys,os,json\nsys.path.insert(0,'/home/wubu/hermes-agent-dev')\nos.environ['HERMES_HOME']='%s'\nimport tools.memory_tool as M\ns=M.load_on_disk_store()\n%s\nprint(json.dumps(r) if not isinstance(r,str) else r)\n";
    char script[1200]; snprintf(script,sizeof(script),tmpl,home,expr);
    char path[256]; snprintf(path,sizeof(path),"/tmp/mth_py_XXXXXX");
    int fd = mkstemp(path); dprintf(fd,"%s",script); close(fd);
    char cmd[1400]; snprintf(cmd,sizeof(cmd),"python3 %s",path);
    FILE *f = popen(cmd,"r");
    static char out[8192]; out[0]=0;
    size_t got=0; char buf[512];
    while (fgets(buf,sizeof(buf),f) && got<sizeof(out)-1) { size_t l=strlen(buf); memcpy(out+got,buf,l); got+=l; }
    out[got]=0; pclose(f); unlink(path);
    return out;
}
static void py_exec_home(const char *mem, const char *stmt) {
    char home[1100]; snprintf(home,sizeof(home),"%s/..", mem);
    char tmpl[] = "import sys,os\nsys.path.insert(0,'/home/wubu/hermes-agent-dev')\nos.environ['HERMES_HOME']='%s'\nimport tools.memory_tool as M\ns=M.load_on_disk_store()\n%s\n";
    char script[1200]; snprintf(script,sizeof(script),tmpl,home,stmt);
    char path[256]; snprintf(path,sizeof(path),"/tmp/mth_py_XXXXXX");
    int fd = mkstemp(path); dprintf(fd,"%s",script); close(fd);
    char cmd[1400]; snprintf(cmd,sizeof(cmd),"python3 %s",path);
    FILE *f = popen(cmd,"r"); char buf[512]; while(fgets(buf,sizeof(buf),f)){}
    pclose(f); unlink(path);
}

static json_node_t *parse_r(const char *s) { return json_parse(s, NULL); }

/* ---- write gate stubs ---- */
static memory_write_gate_decision_t g_gate_block(const char *t, const char *d) {
    (void)t; (void)d; memory_write_gate_decision_t x={0}; x.blocked=1; x.message=strdup("blocked"); return x;
}
static memory_write_gate_decision_t g_gate_stage(const char *t, const char *d) {
    (void)t; (void)d; memory_write_gate_decision_t x={0}; x.staged=1; x.message=strdup("staged"); x.pending_id=strdup("pend-1"); return x;
}

int main(void) {
    mk_home();

    /* 1. available */
    TEST(memory_tool_available()==1, "memory_tool_available true");

    /* 2. NULL store */
    {
        char *r = memory_tool_run(NULL,"add","memory","x",NULL,NULL);
        json_node_t *j = parse_r(r);
        TEST(json_get_bool(j,"success",1)==0, "null store -> success false");
        json_free(j); free(r);
    }

    /* 3. invalid target */
    {
        char m[1100]; fresh_mem(m,3);
        memory_store_t *s = memory_store_new(0,0); memory_store_load(s,m);
        char *r = memory_tool_run(s,"add","bogus","x",NULL,NULL);
        json_node_t *j = parse_r(r);
        TEST(json_get_bool(j,"success",1)==0, "invalid target -> success false");
        json_free(j); free(r); memory_store_free(s);
    }

    /* 4. single add -> persists */
    {
        char m[1100]; fresh_mem(m,4);
        memory_store_t *s = memory_store_new(0,0); memory_store_load(s,m);
        char *r = memory_tool_run(s,"add","memory","handler add entry",NULL,NULL);
        json_node_t *j = parse_r(r);
        TEST(json_get_bool(j,"success",0)==1, "single add success");
        json_free(j); free(r);
        TEST(memory_store_entry_count(s,"memory")==1, "single add -> 1 entry");
        memory_store_free(s);
        memory_store_t *s2 = memory_store_new(0,0); memory_store_load(s2,m);
        TEST(memory_store_entry_count(s2,"memory")==1, "handler add persisted");
        memory_store_free(s2);
    }

    /* 5. replace w/o old_text -> missing_old_text + current_entries */
    {
        char m[1100]; fresh_mem(m,5);
        memory_store_t *s = memory_store_new(0,0); memory_store_load(s,m);
        char *seed = memory_tool_run(s,"add","memory","seed entry",NULL,NULL); free(seed);
        char *r = memory_tool_run(s,"replace","memory","new",NULL,NULL);
        json_node_t *j = parse_r(r);
        TEST(json_get_bool(j,"success",1)==0, "replace w/o old_text -> false");
        TEST(json_array_size(json_object_get(j,"current_entries"))>=1, "missing-old_text has current_entries");
        json_free(j); free(r); memory_store_free(s);
    }

    /* 6. batch add -> 2 entries */
    {
        char m[1100]; fresh_mem(m,6);
        memory_store_t *s = memory_store_new(0,0); memory_store_load(s,m);
        json_node_t *ops = json_parse("[{\"action\":\"add\",\"content\":\"batch one\"},{\"action\":\"add\",\"content\":\"batch two\"}]", NULL);
        char *r = memory_tool_run(s,NULL,"memory",NULL,NULL,ops);
        json_node_t *j = parse_r(r);
        TEST(json_get_bool(j,"success",0)==1, "batch add success");
        TEST(memory_store_entry_count(s,"memory")==2, "batch -> 2 entries");
        json_free(j); json_free(ops); free(r); memory_store_free(s);
    }

    /* 7. gate BLOCKED -> store unchanged */
    {
        char m[1100]; fresh_mem(m,7);
        memory_store_t *s = memory_store_new(0,0); memory_store_load(s,m);
        memory_store_set_write_gate(s, g_gate_block);
        char *r = memory_tool_run(s,"add","memory","blocked",NULL,NULL);
        json_node_t *j = parse_r(r);
        TEST(json_get_bool(j,"success",1)==0, "gate blocked -> false");
        TEST(memory_store_entry_count(s,"memory")==0, "gate blocked -> store unchanged");
        json_free(j); free(r); memory_store_free(s);
    }

    /* 8. gate STAGED -> staged true, store unchanged */
    {
        char m[1100]; fresh_mem(m,8);
        memory_store_t *s = memory_store_new(0,0); memory_store_load(s,m);
        memory_store_set_write_gate(s, g_gate_stage);
        char *r = memory_tool_run(s,"add","memory","staged",NULL,NULL);
        json_node_t *j = parse_r(r);
        TEST(json_get_bool(j,"staged",0)==1, "gate staged -> staged true");
        TEST(memory_store_entry_count(s,"memory")==0, "gate staged -> store unchanged");
        json_free(j); free(r); memory_store_free(s);
    }

    /* 9. apply_pending replays a staged add */
    {
        char m[1100]; fresh_mem(m,9);
        memory_store_t *s = memory_store_new(0,0); memory_store_load(s,m);
        json_node_t *p = json_parse("{\"action\":\"add\",\"target\":\"memory\",\"content\":\"replayed\"}", NULL);
        char *r = memory_tool_apply_pending(s,p);
        json_node_t *j = parse_r(r);
        TEST(json_get_bool(j,"success",0)==1, "apply_pending add success");
        TEST(memory_store_entry_count(s,"memory")==1, "apply_pending -> 1 entry");
        json_free(j); json_free(p); free(r); memory_store_free(s);
    }

    /* 10. E2E parity: handler batch add == Python, independent homes */
    {
        char mC[1100]; fresh_mem(mC,10);
        memory_store_t *s = memory_store_new(0,0); memory_store_load(s,mC);
        json_node_t *ops = json_parse("[{\"action\":\"add\",\"content\":\"parity A\"},{\"action\":\"add\",\"content\":\"parity B\"}]", NULL);
        char *r = memory_tool_run(s,NULL,"memory",NULL,NULL,ops); (void)r; json_free(ops); free(r);
        int c_n = memory_store_entry_count(s,"memory");
        memory_store_free(s);

        char mP[1100]; fresh_mem(mP,11);
        py_exec_home(mP, "s.add('memory','parity A'); s.add('memory','parity B')");
        char *p = py_eval_home(mP, "r=len(s._entries_for('memory'))");
        int p_n = atoi(p);
        TEST(c_n==p_n, "handler batch add parity vs Python");
    }

    if (g_fail==0) printf("ALL PASSED\n"); else printf("%d FAIL\n", g_fail);
    return g_fail?1:0;
}
