/*
 * learning_graph_test.c — behavioral + E2E tests for the learning_graph.py
 * port (agent/learning_graph.py gaps: frontmatter / iter_skill_files /
 * load_usage / build_skill_nodes / memory_cards / skill_roots /
 * build_learning_graph).
 *
 * Exercises the real path against a temp HERMES_HOME (SKILL.md frontmatter
 * parse via libyaml, .usage.json load via libskillusage, MEMORY.md split).
 */
#include "port_learning_graph.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); g_fail++; } \
    else { printf("ok: %s\n", msg); } \
} while (0)

static char g_home[1024];

static void write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (f) { fputs(content, f); fclose(f); }
}
static void mkdirs(const char *path) { mkdir(path, 0755); }

/* ---- unit: frontmatter -> JSON ---- */
static void test_frontmatter(void) {
    const char *md =
        "---\n"
        "name: my-skill\n"
        "category: coding\n"
        "related_skills:\n"
        "  - other-skill\n"
        "  - third\n"
        "metadata:\n"
        "  hermes:\n"
        "    category: coding\n"
        "    related_skills: [other-skill]\n"
        "---\n"
        "# Body text here\n";
    char *fm = learning_graph_frontmatter(md);
    CHECK(fm != NULL, "frontmatter returns non-NULL");
    json_t *j = json_parse(fm, NULL);
    CHECK(j && j->type == JSON_OBJECT, "frontmatter parses to JSON object");
    if (j) {
        CHECK(strcmp(json_object_get_string(j, "name", ""), "my-skill") == 0,
              "frontmatter name parsed");
        CHECK(strcmp(json_object_get_string(j, "category", ""), "coding") == 0,
              "frontmatter category parsed");
        json_t *rel = json_object_get(j, "related_skills");
        CHECK(rel && rel->type == JSON_ARRAY && json_array_size(rel) == 2,
              "frontmatter related_skills list parsed");
        json_free(j);
    }
    free(fm);

    /* no frontmatter -> {} */
    char *empty = learning_graph_frontmatter("just text no dashes");
    CHECK(strcmp(empty, "{}") == 0, "no frontmatter yields {}");
    free(empty);
}

/* ---- unit: load_usage ---- */
static void test_load_usage(void) {
    /* .usage.json shape matches skill_usage_load's expectation */
    char path[1100];
    snprintf(path, sizeof(path), "%s/skills/.usage.json", g_home);
    write_file(path,
        "{\"my-skill\":{\"name\":\"my-skill\",\"use_count\":7,\"state\":\"active\","
        "\"created_by\":\"agent\",\"pinned\":true,"
        "\"last_used_at\":\"2024-01-02T03:04:05+00:00\"}}");
    char *u = learning_graph_load_usage(g_home);
    CHECK(u != NULL, "load_usage returns non-NULL");
    json_t *j = json_parse(u, NULL);
    CHECK(j && j->type == JSON_OBJECT, "load_usage parses to object");
    if (j) {
        json_t *rec = json_object_get(j, "my-skill");
        CHECK(rec != NULL, "load_usage has my-skill record");
        if (rec) {
            CHECK((int)json_object_get_number(rec, "use_count", 0) == 7,
                  "load_usage use_count=7");
            CHECK(strcmp(json_object_get_string(rec, "created_by", ""), "agent") == 0,
                  "load_usage created_by=agent");
            CHECK(json_object_get_bool(rec, "pinned", 0) == 1,
                  "load_usage pinned=true");
        }
        json_free(j);
    }
    free(u);
}

/* ---- unit: skill_roots + iter_skill_files ---- */
static void test_skill_files(void) {
    char roots[1200];
    snprintf(roots, sizeof(roots),
             "[{\"source\":\"profile\",\"path\":\"%s/skills\"}]", g_home);
    char *files = learning_graph_iter_skill_files(roots);
    json_t *j = json_parse(files, NULL);
    CHECK(j && j->type == JSON_ARRAY, "iter_skill_files returns array");
    int found = 0;
    if (j) {
        for (size_t i = 0; i < json_array_size(j); i++) {
            json_t *e = json_array_get(j, i);
            const char *nm = json_object_get_string(e, "path", "");
            if (strstr(nm ? nm : "", "my-skill/SKILL.md")) found = 1;
        }
    }
    CHECK(found, "iter_skill_files found my-skill/SKILL.md");
    if (j) json_free(j);
    free(files);
}

/* ---- E2E: build_learning_graph ---- */
static void test_build(void) {
    char *graph = learning_graph_build(g_home, "/nonexistent/repo/skills");
    json_t *j = json_parse(graph, NULL);
    CHECK(j && j->type == JSON_OBJECT, "build returns object");
    if (!j) { free(graph); return; }

    json_t *nodes = json_object_get(j, "nodes");
    json_t *edges = json_object_get(j, "edges");
    json_t *clusters = json_object_get(j, "clusters");
    json_t *memory = json_object_get(j, "memory");
    json_t *stats = json_object_get(j, "stats");
    CHECK(nodes && nodes->type == JSON_ARRAY, "build has nodes array");
    CHECK(edges && edges->type == JSON_ARRAY, "build has edges array");
    CHECK(clusters && clusters->type == JSON_ARRAY, "build has clusters array");
    CHECK(memory && memory->type == JSON_ARRAY, "build has memory array");
    CHECK(stats && stats->type == JSON_OBJECT, "build has stats object");

    /* my-skill is learned (created_by=agent, use_count>0) -> should appear */
    int has_my = 0;
    if (nodes) {
        for (size_t i = 0; i < json_array_size(nodes); i++) {
            json_t *n = json_array_get(nodes, i);
            if (strcmp(json_object_get_string(n, "id", ""), "my-skill") == 0) {
                has_my = 1;
                CHECK(strcmp(json_object_get_string(n, "category", ""), "coding") == 0,
                      "node my-skill category=coding");
                CHECK((int)json_object_get_number(n, "useCount", 0) == 7,
                      "node my-skill useCount=7");
            }
        }
    }
    CHECK(has_my, "learned skill my-skill present in graph nodes");

    /* memory card surfaced */
    CHECK(memory && json_array_size(memory) >= 1, "memory card surfaced from MEMORY.md");

    /* clusters include coding + memory */
    int has_mem_cluster = 0;
    if (clusters) {
        for (size_t i = 0; i < json_array_size(clusters); i++) {
            json_t *p = json_array_get(clusters, i);
            const char *c = (p && json_array_size(p) >= 1)
                ? json_string_value(json_array_get(p, 0)) : "";
            if (strcmp(c, "memory") == 0) has_mem_cluster = 1;
        }
    }
    CHECK(has_mem_cluster, "clusters includes memory bucket");

    /* stats sanity */
    if (stats) {
        CHECK((int)json_object_get_number(stats, "learned_skills", -1) >= 1,
              "stats.learned_skills >= 1");
        CHECK((int)json_object_get_number(stats, "memory_nodes", -1) >= 1,
              "stats.memory_nodes >= 1");
    }

    json_free(j);
    free(graph);
}

int main(void) {
    printf("=== learning_graph_test ===\n");

    /* Build a temp HERMES_HOME with a profile skill + memory. */
    char tmpl[] = "/tmp/lg_home_XXXXXX";
    char *d = mkdtemp(tmpl);
    CHECK(d != NULL, "mkdtemp home");
    snprintf(g_home, sizeof(g_home), "%s", d);

    char skills_dir[1200], mem_dir[1200];
    snprintf(skills_dir, sizeof(skills_dir), "%s/skills", g_home);
    snprintf(mem_dir, sizeof(mem_dir), "%s/memories", g_home);
    mkdirs(skills_dir);
    mkdirs(mem_dir);
    char skill_dir[1200];
    snprintf(skill_dir, sizeof(skill_dir), "%s/my-skill", skills_dir);
    mkdirs(skill_dir);
    {
        char p[1300];
        snprintf(p, sizeof(p), "%s/skills/my-skill/SKILL.md", g_home);
        write_file(p,
            "---\n"
            "name: my-skill\n"
            "category: coding\n"
            "related_skills:\n"
            "  - other-skill\n"
            "---\n"
            "# My Skill\nBody.\n");
    }
    char memdir[1200];
    snprintf(memdir, sizeof(memdir), "%s/memories", g_home);
    mkdirs(memdir);
    {
        char p[1300];
        snprintf(p, sizeof(p), "%s/memories/MEMORY.md", g_home);
        write_file(p, "# First memory\nDetails about the first thing.\n\n§\n\n# Second memory\nAnother note.\n");
    }

    test_frontmatter();
    test_load_usage();
    test_skill_files();
    test_build();

    /* cleanup */
    char cmd[1400];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", g_home);
    system(cmd);

    if (g_fail == 0) { printf("ALL LEARNING_GRAPH TESTS PASSED\n"); return 0; }
    printf("%d LEARNING_GRAPH CHECK(S) FAILED\n", g_fail);
    return 1;
}
