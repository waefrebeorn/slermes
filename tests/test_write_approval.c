/*
 * test_write_approval.c — Write-approval store test suite (faithful port of
 * tools/write_approval.py pending-store + enabled resolution).
 *
 * Proves the previously-stubbed functions in port_tools_write_approval.c now
 * do REAL file-backed work (red->green: pending_count was hardcoded 0,
 * stage_write/get_pending/discard_pending were no-ops).
 *
 * Build/run via `make test-write-approval`.
 */

#include "hermes_json.h"
#include "hermes_core_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Functions under test (mirror the linked port_tools_write_approval.c API). */
extern json_node_t *cli_tools_write_approval_stage_write(const char *subsystem,
        const json_node_t *payload, const char *summary, const char *origin);
extern int   cli_tools_write_approval_pending_count(const char *session_key);
extern json_node_t *cli_tools_write_approval_list_pending(const char *subsystem);
extern json_node_t *cli_tools_write_approval_get_pending(const char *session_key);
extern int   cli_tools_write_approval_discard_pending(const char *session_key, const char *request_id);
extern int   cli_tools_write_approval_write_approval_enabled(const char *subsystem);
extern char *cli_tools_write_approval_skill_gist(const char *action, const char *name,
        const char *content, const char *file_path, const char *old_string, const char *new_string);
extern char *uuid_v4(void);

static int passed = 0, failed = 0;
#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while (0)
#define TEST_STR_EQ(name, a, b) TEST(name, (a) && (b) && strcmp((a), (b)) == 0)
#define TEST_INT_EQ(name, a, b) TEST(name, (a) == (b))

static char HOME[4096];

static void rmrf(const char *path) {
    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
    system(cmd);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Write-Approval Store Test Suite ===\n");

    /* Isolated HERMES_HOME so we don't touch the real user store. */
    snprintf(HOME, sizeof(HOME), "/tmp/wa_test_%d", (int)getpid());
    rmrf(HOME);
    mkdir(HOME, 0700);
    setenv("HERMES_HOME", HOME, 1);

    /* ----------------------------------------------------------------
     * 1. pending_count starts at 0, returns REAL count after staging
     *    (this is the red->green: the old stub always returned 0)
     * ---------------------------------------------------------------- */
    TEST_INT_EQ("pending_count empty = 0", cli_tools_write_approval_pending_count("s"), 0);

    json_node_t *payload = json_new_object();
    json_object_set(payload, "action", json_string("add"));
    json_object_set(payload, "target", json_string("user"));
    json_object_set(payload, "content", json_string("user loves cats"));

    json_node_t *rec = cli_tools_write_approval_stage_write("memory", payload,
            "user loves cats", "foreground");
    TEST("stage_write returns a record", rec != NULL);
    char id[64] = "";
    if (rec) {
        const char *rid = json_get_str(rec, "id", "");
        snprintf(id, sizeof(id), "%s", rid ? rid : "");
    }
    json_free(rec);
    json_free(payload);

    /* The record must now exist on disk under <HOME>/pending/memory/<id>.json */
    char recpath[8192];
    snprintf(recpath, sizeof(recpath), "%s/pending/memory/%s.json", HOME, id);
    struct stat st;
    TEST("record file exists on disk", stat(recpath, &st) == 0);

    TEST_INT_EQ("pending_count after stage = 1",
                 cli_tools_write_approval_pending_count("s"), 1);

    /* ----------------------------------------------------------------
     * 2. list_pending returns the staged record
     * ---------------------------------------------------------------- */
    json_node_t *list = cli_tools_write_approval_list_pending("memory");
    TEST("list_pending returns 1 entry", list && json_len(list) == 1);
    if (list && json_len(list) == 1) {
        json_node_t *r0 = json_get(list, 0);
        TEST_STR_EQ("list entry id matches",
                    r0 ? json_get_str(r0, "id", "") : "",
                    id);
        TEST_STR_EQ("list entry subsystem = memory",
                    r0 ? json_get_str(r0, "subsystem", "") : "", "memory");
    }
    json_free(list);

    /* ----------------------------------------------------------------
     * 3. get_pending (aggregated across subsystems)
     * ---------------------------------------------------------------- */
    json_node_t *all = cli_tools_write_approval_get_pending("sess");
    TEST("get_pending aggregated >= 1", all && json_len(all) >= 1);
    json_free(all);

    /* ----------------------------------------------------------------
     * 4. discard_pending removes the record and count returns to 0
     * ---------------------------------------------------------------- */
    int dr = cli_tools_write_approval_discard_pending("sess", id);
    TEST("discard_pending returns 0 (ok)", dr == 0);
    TEST("record file gone after discard", stat(recpath, &st) != 0);
    TEST_INT_EQ("pending_count after discard = 0",
                 cli_tools_write_approval_pending_count("s"), 0);

    /* discard of a non-existent id returns -1 (not found) */
    TEST_INT_EQ("discard unknown id = -1",
                 cli_tools_write_approval_discard_pending("sess", "nope"), -1);

    /* ----------------------------------------------------------------
     * 5. write_approval_enabled reads <HOME>/config.yaml
     *    (old code: function did not exist / bare comment)
     * ---------------------------------------------------------------- */
    /* No config.yaml -> defaults to false */
    TEST_INT_EQ("enabled = 0 when no config",
                 cli_tools_write_approval_write_approval_enabled("memory"), 0);
    TEST_INT_EQ("enabled = 0 for invalid subsystem",
                 cli_tools_write_approval_write_approval_enabled("bogus"), 0);

    /* Write a config.yaml enabling memory.write_approval */
    char cfg[8192];
    snprintf(cfg, sizeof(cfg),
        "%s/config.yaml", HOME);
    FILE *f = fopen(cfg, "w");
    if (f) {
        fputs("memory:\n  write_approval: true\n", f);
        fclose(f);
    }
    TEST_INT_EQ("enabled = 1 after config set",
                 cli_tools_write_approval_write_approval_enabled("memory"), 1);
    TEST_INT_EQ("skills still disabled",
                 cli_tools_write_approval_write_approval_enabled("skills"), 0);

    /* yaml truthy string coercion */
    f = fopen(cfg, "w");
    if (f) {
        fputs("memory:\n  write_approval: \"on\"\n", f);
        fclose(f);
    }
    TEST_INT_EQ("enabled = 1 for yaml 'on' string",
                 cli_tools_write_approval_write_approval_enabled("memory"), 1);

    /* ----------------------------------------------------------------
     * 6. skill_gist heuristic (old code: not implemented / bare comment)
     * ---------------------------------------------------------------- */
    char *g = cli_tools_write_approval_skill_gist("create", "my-skill",
            "---\ndescription: Summarizes logs\n---\nbody", NULL, NULL, NULL);
    TEST("skill_gist create pulls description", g && strstr(g, "Summarizes logs"));
    TEST("skill_gist create verb 'create'",
         g && (strstr(g, "create 'my-skill'") != NULL));
    free(g);

    char *g2 = cli_tools_write_approval_skill_gist("patch", "my-skill",
            NULL, "SKILL.md", "old line\n", "new line\n");
    TEST("skill_gist patch shows +/- lines",
         g2 && strstr(g2, "+2/-2 lines"));
    free(g2);

    /* ----------------------------------------------------------------
     * cleanup
     * ---------------------------------------------------------------- */
    rmrf(HOME);

    printf("\n%sWRITE-APPROVAL TESTS: %d passed, %d failed%s\n",
           failed ? "FAIL " : "", passed, failed,
           failed ? "" : " — ALL PASSED");
    return failed ? 1 : 0;
}
