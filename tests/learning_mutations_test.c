/*
 * learning_mutations_test.c
 *
 * E2E parity test for agent/learning_mutations.py (ported to
 * src/cli/port_learning_mutations.c). Mirrors the Python module against the
 * C port on a temp HERMES_HOME with a skill + memory cards, exercising:
 *   - parse_node_kind / _parse_memory_id
 *   - node_detail (skill + memory)
 *   - delete_node (skill archive + memory delete)
 *   - edit_node (skill rewrite + memory edit)
 *
 * Python reference runs in an independent fresh home (via popen with an
 * inline driver that sets HERMES_HOME), so C and Python never share mutated
 * state — each operates on an identical starting fixture and we compare the
 * returned JSON. This is the faithful E2E contract.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "hermes_json.h"
#include "port_learning_mutations.h"

/* clear_skills_system_prompt_cache lives in agent/prompt_builder.c (heavy
 * agent graph). The Python _clear_skill_cache is a best-effort no-op on
 * import failure, so a no-op stub preserves equivalent test behavior without
 * dragging the full agent link set into this unit test. */
void clear_skills_system_prompt_cache(const char *hermes_home, int clear_snapshot) { (void)hermes_home; (void)clear_snapshot; }

static int g_fail = 0;
#define CHECK(c, m) do { if (c) printf("ok: %s\n", (m)); else { printf("FAIL: %s\n", (m)); g_fail++; } } while (0)

static void mkdirs(const char *path) {
    char tmp[2048];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
}

static void write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}

/* Build a fresh HERMES_HOME fixture (skill + memory + usage) and return its
 * path (caller frees). Each mutation test gets its own so C and Python never
 * share mutated state. */
static char *mk_home(void) {
    char tmpl[] = "/tmp/lm_home_XXXXXX";
    char *home = mkdtemp(tmpl);
    if (!home) return NULL;
    char skills_dir[2300], sk_dir[2300], mem_dir[2300], p[2300];
    snprintf(skills_dir, sizeof(skills_dir), "%s/skills", home);
    snprintf(sk_dir,    sizeof(sk_dir),    "%s/my-skill", skills_dir);
    snprintf(mem_dir,   sizeof(mem_dir),   "%s/memories", home);
    mkdirs(sk_dir); mkdirs(mem_dir);
    snprintf(p, sizeof(p), "%s/skills/my-skill/SKILL.md", home);
    write_file(p, "---\nname: my-skill\ncategory: coding\n---\n# My Skill\nBody.\n");
    snprintf(p, sizeof(p), "%s/MEMORY.md", mem_dir);
    write_file(p, "# First memory\nDetails about the first thing.\n\n§\n\n# Second memory\nAnother note.\n");
    snprintf(p, sizeof(p), "%s/skills/.usage.json", home);
    write_file(p, "{\"my-skill\":{\"name\":\"my-skill\",\"created_by\":\"agent\",\"use_count\":7,\"state\":\"active\",\"pinned\":false}}");
    return strdup(home);
}

static void rm_home(char *home) {
    char cmd[2400];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", home);
    system(cmd);
    free(home);
}

/* Python reference: run expr under an independent fresh home copy. */
static char *py_eval_home(const char *home, const char *expr) {
    char tmpl[] = "/tmp/lm_py_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) return NULL;
    dprintf(fd,
        "import sys,json,os\n"
        "sys.path.insert(0,'/home/wubu/hermes-agent-dev')\n"
        "os.environ['HERMES_HOME']='%s'\n"
        "import hermes_constants\n"
        "from pathlib import Path\n"
        "import importlib, agent.learning_mutations as M\n"
        "importlib.reload(M)\n"
        "v=(%s)\n"
        "print(json.dumps(v))\n",
        home, expr);
    close(fd);
    char cmd[2300];
    snprintf(cmd, sizeof(cmd), "python3 %s", tmpl);
    FILE *f = popen(cmd, "r");
    static char buf[8192];
    buf[0] = '\0';
    if (f) { if (!fgets(buf, sizeof(buf), f)) buf[0]='\0'; pclose(f); }
    unlink(tmpl);
    return buf[0] ? strdup(buf) : NULL;
}

/* Compare two result JSONs on the "ok" boolean + "kind"/"content" where present. */
static int results_match(const char *cjson, const char *pjson) {
    json_t *jc = json_parse(cjson ? cjson : "{}", NULL);
    json_t *jp = json_parse(pjson ? pjson : "{}", NULL);
    int ok = 0;
    if (jc && jp) {
        int cok = (int)json_object_get_bool(jc, "ok", 0);
        int pok = (int)json_object_get_bool(jp, "ok", 0);
        ok = (cok == pok);
        if (ok) {
            const char *ck = json_object_get_string(jc, "kind", "");
            const char *pk = json_object_get_string(jp, "kind", "");
            if (*ck || *pk) ok = (strcmp(ck, pk) == 0);
        }
        if (ok) {
            const char *cc = json_object_get_string(jc, "content", "");
            const char *pc = json_object_get_string(jp, "content", "");
            if (*cc || *pc) ok = (strcmp(cc, pc) == 0);
        }
    }
    if (jc) json_free(jc);
    if (jp) json_free(jp);
    return ok;
}

/* ── tests ───────────────────────────────────────────────────────────────── */
static void test_parse(void) {
    CHECK(learning_mutations_parse_node_kind("memory:memory:0") == 1, "parse_node_kind memory");
    CHECK(learning_mutations_parse_node_kind("my-skill") == 0, "parse_node_kind skill");
    char src[64]; int gidx; char err[128];
    CHECK(learning_mutations_parse_memory_id("memory:profile:2", src, &gidx, err, sizeof(err)) == 0, "parse_memory_id ok");
    CHECK(strcmp(src, "profile") == 0, "parse_memory_id source");
    CHECK(gidx == 2, "parse_memory_id gidx");
    CHECK(learning_mutations_parse_memory_id("bogus", src, &gidx, err, sizeof(err)) != 0, "parse_memory_id invalid");
}

static void test_node_detail(void) {
    char *home = mk_home();
    setenv("HERMES_HOME", home, 1);
    char *c = learning_mutations_node_detail("my-skill");
    char *p = py_eval_home(home, "M.node_detail('my-skill')");
    CHECK(results_match(c, p), "node_detail skill matches python");
    free(c); free(p);

    char *m = learning_mutations_node_detail("memory:memory:0");
    char *mp = py_eval_home(home, "M.node_detail('memory:memory:0')");
    CHECK(results_match(m, mp), "node_detail memory matches python (content)");
    free(m); free(mp);
    rm_home(home);
}

static void test_edit_skill(void) {
    const char *new_md = "---\nname: my-skill\ndescription: A demonstration skill used by the learning-mutations test harness.\ncategory: coding\n---\n# My Skill\nEDITED BODY.\n";
    char *home = mk_home();
    setenv("HERMES_HOME", home, 1);
    char *homeP = mk_home();
    char *c = learning_mutations_edit_node("my-skill", new_md);
    char *p = py_eval_home(homeP, "M.edit_node('my-skill', '''---\nname: my-skill\ndescription: A demonstration skill used by the learning-mutations test harness.\ncategory: coding\n---\n# My Skill\nEDITED BODY.\n''')");
    CHECK(results_match(c, p), "edit_node skill matches python (ok)");
    free(c); free(p);
    /* verify the file was actually rewritten (C side) */
    char path[2300]; snprintf(path, sizeof(path), "%s/skills/my-skill/SKILL.md", home);
    char *rd = py_eval_home(home, "open(__import__('os').path.join(os.environ['HERMES_HOME'],'skills','my-skill','SKILL.md')).read()");
    CHECK(rd && strstr(rd, "EDITED BODY.") != NULL, "edit_node skill file rewritten");
    free(rd);
    rm_home(home); rm_home(homeP);
}

static void test_edit_memory(void) {
    const char *new_body = "Edited first memory entry.";
    char *home = mk_home();
    setenv("HERMES_HOME", home, 1);
    char *homeP = mk_home();
    char *c = learning_mutations_edit_node("memory:memory:0", new_body);
    char *p = py_eval_home(homeP, "M.edit_node('memory:memory:0', 'Edited first memory entry.')");
    CHECK(results_match(c, p), "edit_node memory matches python (ok)");
    free(c); free(p);
    char *rd = py_eval_home(home, "open(__import__('os').path.join(os.environ['HERMES_HOME'],'memories','MEMORY.md')).read()");
    CHECK(rd && strstr(rd, "Edited first memory entry.") != NULL, "edit_node memory file rewritten");
    free(rd);
    rm_home(home); rm_home(homeP);
}

static void test_delete_memory(void) {
    char *home = mk_home();
    setenv("HERMES_HOME", home, 1);
    char *homeP = mk_home();
    char *c = learning_mutations_delete_node("memory:memory:1");
    char *p = py_eval_home(homeP, "M.delete_node('memory:memory:1')");
    CHECK(results_match(c, p), "delete_node memory matches python (ok)");
    free(c); free(p);
    rm_home(home); rm_home(homeP);
}

static void test_delete_skill(void) {
    char *home = mk_home();
    setenv("HERMES_HOME", home, 1);
    char *homeP = mk_home();
    char *c = learning_mutations_delete_node("my-skill");
    char *p = py_eval_home(homeP, "M.delete_node('my-skill')");
    CHECK(results_match(c, p), "delete_node skill matches python (ok)");
    free(c); free(p);
    rm_home(home); rm_home(homeP);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== learning_mutations_test ===\n");

    test_parse();
    test_node_detail();
    test_edit_skill();
    test_edit_memory();
    test_delete_memory();
    test_delete_skill();

    if (g_fail == 0) { printf("ALL LEARNING_MUTATIONS TESTS PASSED\n"); return 0; }
    printf("%d LEARNING_MUTATIONS CHECK(S) FAILED\n", g_fail);
    return 1;
}
