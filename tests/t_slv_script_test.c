/*
 * slv_script_test.c -- drive engine-style bindings through slv.
 *
 * Proves the VM meets the gamemode contract: a `wb` table of host functions
 * (spawn/count as cfn) is set as a global; a gamemode script runs and uses
 * them; step() is fetched and invoked via slv_call.
 */
#include "slv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) g_pass++; else { g_fail++; printf("FAIL: %s (line %d)\n", msg, __LINE__); } } while (0)

typedef struct { int count; } entreg;

static slv_value cfn_spawn(slv *L, slv_value *args, int n, void *ud) {
    (void)L; (void)args; (void)n;
    entreg *eg = (entreg *)ud;
    eg->count++;
    return slv_number(eg->count);   /* handle id */
}
static slv_value cfn_count(slv *L, slv_value *args, int n, void *ud) {
    (void)L; (void)args; (void)n;
    entreg *eg = (entreg *)ud;
    return slv_number(eg->count);
}

static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc(n + 1); if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, n, f); b[got] = 0; fclose(f);
    return b;
}

int main(void) {
    entreg reg = {0};

    slv *L = slv_new();

    /* build the wb.* API as a table of cfn fields, set as global "wb" */
    {
        slv_value wb;
        wb.t = SLV_TABLE; wb.as.tab = slv_table_new(L, 0, 2);
        slv_value spawn, cnt;
        spawn.t = SLV_CFUNCTION; spawn.as.cf.f = cfn_spawn; spawn.as.cf.ud = &reg;
        cnt.t   = SLV_CFUNCTION; cnt.as.cf.f   = cfn_count; cnt.as.cf.ud   = &reg;
        slv_table_set(L, wb.as.tab, slv_str(slv_intern(L,"spawn",5)), spawn);
        slv_table_set(L, wb.as.tab, slv_str(slv_intern(L,"count",5)), cnt);
        slv_set_global(L, "wb", wb);
    }

    char *src = slurp("games/gamemode_demo.lua");
    CHECK(src != NULL, "gamemode file loaded");
    if (!src) { printf("=== SLV-SCRIPT TESTS: %d pass, %d fail ===\n", g_pass, g_fail); return 1; }

    const char *err = slv_run(L, src, "gamemode_demo");
    CHECK(err == NULL, "gamemode compiled + ran without error");

    CHECK(reg.count == 2, "gamemode spawned its 2 load-time mobs via wb.spawn");

    /* fetch and drive step() from C (the engine loop contract) */
    slv_value step = slv_get_global(L, "step");
    CHECK(step.t == SLV_FUNCTION, "step() is a callable Lua function");
    for (int i = 0; i < 30; i++) {
        slv_value dt = slv_number(1.0/60.0);
        slv_call(L, step, &dt, 1);
    }

    CHECK(reg.count == 2, "deterministic gamemode did not spawn during step");

    free(src);
    slv_free(L);
    printf("\n=== SLV-SCRIPT TESTS: %d pass, %d fail ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
