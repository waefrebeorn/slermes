/*
 * test_skill_manager_val.c — Faithful port of tools/skill_manager_tool.py
 * validation + delete-guard core.
 */

#include "skill_manager_val.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static int g_fail = 0;

#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); g_fail++; } } while (0)

int main(void) {
    char *e;

    e = skill_val_validate_name("my-skill");
    CHECK(e == NULL, "valid name ok"); free(e);
    e = skill_val_validate_name("");
    CHECK(e != NULL, "empty name rejected"); free(e);
    e = skill_val_validate_name("MySkill");
    CHECK(e != NULL, "uppercase rejected"); free(e);
    e = skill_val_validate_name("-bad");
    CHECK(e != NULL, "leading hyphen rejected"); free(e);
    e = skill_val_validate_name("bad name");
    CHECK(e != NULL, "space rejected"); free(e);
    e = skill_val_validate_name("a/b");
    CHECK(e != NULL, "slash rejected"); free(e);

    e = skill_val_validate_category(NULL);
    CHECK(e == NULL, "null category ok"); free(e);
    e = skill_val_validate_category("cat-name");
    CHECK(e == NULL, "valid category ok"); free(e);
    e = skill_val_validate_category("a/b");
    CHECK(e != NULL, "category slash rejected"); free(e);

    e = skill_val_validate_content_size("hello", "SKILL.md");
    CHECK(e == NULL, "small content ok"); free(e);
    char *big = (char*)malloc(200000); memset(big,'x',199999); big[199999]='\0';
    e = skill_val_validate_content_size(big, "SKILL.md");
    CHECK(e != NULL, "huge content rejected"); free(e); free(big);

    e = skill_val_validate_frontmatter("");
    CHECK(e != NULL, "empty rejected"); free(e);
    e = skill_val_validate_frontmatter("no frontmatter here");
    CHECK(e != NULL, "no dashdash rejected"); free(e);
    e = skill_val_validate_frontmatter("---\nname: x\n---\nbody");
    CHECK(e != NULL, "missing description rejected"); free(e);
    e = skill_val_validate_frontmatter("---\nname: x\ndescription: y\n");
    CHECK(e != NULL, "unclosed rejected"); free(e);
    e = skill_val_validate_frontmatter("---\nname: my-skill\ndescription: does a thing\n---\n\nBody text here.");
    CHECK(e == NULL, "valid frontmatter ok"); free(e);
    e = skill_val_validate_frontmatter("---\nname: my-skill\ndescription: y\n---\n");
    CHECK(e != NULL, "no body rejected"); free(e);

    e = skill_val_validate_file_path("SKILL.md");
    CHECK(e == NULL, "SKILL.md ok"); free(e);
    e = skill_val_validate_file_path("my-skill/SKILL.md");
    CHECK(e == NULL, "<name>/SKILL.md ok"); free(e);
    e = skill_val_validate_file_path("../escape");
    CHECK(e != NULL, "traversal rejected"); free(e);
    e = skill_val_validate_file_path("references/foo.md");
    CHECK(e == NULL, "allowed subdir ok"); free(e);
    e = skill_val_validate_file_path("evil/foo.md");
    CHECK(e != NULL, "disallowed subdir rejected"); free(e);
    e = skill_val_validate_file_path("references");
    CHECK(e != NULL, "no filename rejected"); free(e);

    char *err = NULL;
    char *t = skill_val_resolve_skill_target("/skills/foo", "references/x.md", &err);
    CHECK(t != NULL && strcmp(t, "/skills/foo/references/x.md")==0 && err==NULL, "resolve within dir");
    free(t); free(err);
    err = NULL;
    t = skill_val_resolve_skill_target("/skills/foo", "../escape.md", &err);
    CHECK(t == NULL && err != NULL, "resolve escapes dir -> error");
    free(t); free(err);

    char tmpl[] = "/tmp/sm_val_XXXXXX";
    char *root = mkdtemp(tmpl);
    char sub[512]; snprintf(sub, sizeof(sub), "%s/myskill", root);
    mkdir(sub, 0755);
    char *roots[] = { root };
    e = skill_val_validate_delete_target(sub, roots, 1);
    CHECK(e == NULL, "delete inside root ok"); free(e);
    e = skill_val_validate_delete_target(root, roots, 1);
    CHECK(e != NULL, "delete root itself refused"); free(e);
    char outside[512]; snprintf(outside, sizeof(outside), "/tmp/not_under_root_x");
    e = skill_val_validate_delete_target(outside, roots, 1);
    CHECK(e != NULL, "delete outside root refused"); free(e);
    rmdir(sub); rmdir(root);

    char *rroots[] = { "/home/u/.hermes/skills", "/mnt/ext/skills" };
    char *cr = skill_val_containing_skills_root("/home/u/.hermes/skills/cat/s", rroots, 2);
    CHECK(cr != NULL && strcmp(cr, "/home/u/.hermes/skills")==0, "containing root found");
    free(cr);
    cr = skill_val_containing_skills_root("/elsewhere/x", rroots, 2);
    CHECK(cr == NULL, "no containing root -> NULL");

    char rt[] = "/tmp/sm_redir_XXXXXX";
    char *d = mkdtemp(rt);
    char link[512]; snprintf(link, sizeof(link), "%s/lnk", d);
    symlink("/tmp", link);

    CHECK(skill_val_is_path_redirect(link), "symlink detected");
    CHECK(!skill_val_is_path_redirect(d), "real dir not a redirect");
    unlink(link); rmdir(d);

    if (g_fail==0) printf("ALL PASSED\n"); else printf("%d FAIL\n", g_fail);
    return g_fail ? 1 : 0;
}
