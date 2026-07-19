/*
 * test_projects_db.c — Faithful port of hermes_cli/projects_db.py store layer.
 */

#include "projects_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); g_fail++; } } while (0)

int main(void) {
    char tmpl[] = "/tmp/proj_db_XXXXXX";
    char *dir = mkdtemp(tmpl);
    projects_db_t *db = projects_db_connect(dir);
    CHECK(db != NULL, "connect ok");

    /* slugify + normalize_slug (pure helpers) */
    char *sl = projects_db_slugify("My Cool Project!!");
    CHECK(strcmp(sl, "my-cool-project") == 0, "slugify My Cool Project!!");
    free(sl);
    sl = projects_db_slugify("   ");
    CHECK(strcmp(sl, "project") == 0, "slugify blank -> project");
    free(sl);
    char *ns = projects_db_normalize_slug("Good-Slug_1");
    CHECK(ns && strcmp(ns, "good-slug_1") == 0, "normalize_slug ok");
    free(ns);
    CHECK(projects_db_normalize_slug("Bad Slug") == NULL, "normalize_slug rejects space");
    CHECK(projects_db_normalize_slug("-leading") == NULL, "normalize_slug rejects leading sep");

    /* create */
    char *folders[] = { "/tmp/proj_db_a", "/tmp/proj_db_b" };
    mkdir(folders[0], 0755); mkdir(folders[1], 0755);
    char *pid = projects_db_create_project(db, "Alpha", NULL, folders, 2, folders[0],
                                            "desc", "icon", "blue", "board1");
    CHECK(pid != NULL, "create_project returns id");
    CHECK(strncmp(pid, "p_", 2) == 0, "id has p_ prefix");

    /* get + fields */
    project_t *p = projects_db_get_project(db, pid);
    CHECK(p != NULL, "get_project by id");
    CHECK(strcmp(p->name, "Alpha") == 0, "name preserved");
    CHECK(strcmp(p->slug, "alpha") == 0, "slug derived from name");
    CHECK(p->n_folders == 2, "two folders recorded");
    CHECK(p->primary_path && strcmp(p->primary_path, folders[0]) == 0, "primary_path set");
    CHECK(p->description && strcmp(p->description, "desc") == 0, "description stored");
    projects_db_free_project(p);

    /* get by slug */
    project_t *p2 = projects_db_get_project(db, "ALPHA");
    CHECK(p2 != NULL, "get_project by slug (case-insensitive)");
    projects_db_free_project(p2);

    /* unique slug on collision (no folders so ownership is unambiguous) */
    char *pid2 = projects_db_create_project(db, "Alpha", NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL);
    project_t *p3 = projects_db_get_project(db, pid2);
    CHECK(p3 && strcmp(p3->slug, "alpha-2") == 0, "duplicate slug -> alpha-2");
    projects_db_free_project(p3);

    /* list */
    int n; project_t *all = projects_db_list_projects(db, false, &n);
    CHECK(all != NULL && n == 2, "list_projects -> 2 non-archived");
    projects_db_free_projects(all, n);

    /* update */
    CHECK(projects_db_update_project(db, pid, "Alpha Renamed", "new desc", NULL, "red", NULL),
          "update_project returns true");
    p = projects_db_get_project(db, pid);
    CHECK(strcmp(p->name, "Alpha Renamed") == 0, "name updated");
    CHECK(strcmp(p->description, "new desc") == 0, "description updated");
    CHECK(p->color && strcmp(p->color, "red") == 0, "color updated");
    projects_db_free_project(p);

    /* add folder + primary swap */
    char *added = projects_db_add_folder(db, pid, "/tmp/proj_db_c", "extra", true);
    CHECK(added != NULL, "add_folder returns path");
    p = projects_db_get_project(db, pid);
    CHECK(p->n_folders == 3, "folder count now 3");
    CHECK(p->primary_path && strcmp(p->primary_path, added) == 0, "new folder is primary");
    projects_db_free_project(p);
    free(added);

    /* remove primary -> repoints */
    CHECK(projects_db_remove_folder(db, pid, folders[0]), "remove_folder ok");
    p = projects_db_get_project(db, pid);
    CHECK(p->n_folders == 2, "folder count now 2");
    projects_db_free_project(p);

    /* archive / restore */
    CHECK(projects_db_archive_project(db, pid2), "archive ok");
    all = projects_db_list_projects(db, false, &n);
    CHECK(n == 1, "archived hidden from default list");
    projects_db_free_projects(all, n);
    all = projects_db_list_projects(db, true, &n);
    CHECK(n == 2, "include_archived -> 2");
    projects_db_free_projects(all, n);
    CHECK(projects_db_restore_project(db, pid2), "restore ok");

    /* active pointer */
    projects_db_set_active(db, pid);
    char *act = projects_db_get_active_id(db);
    CHECK(act && strcmp(act, pid) == 0, "active id set");
    free(act);
    projects_db_set_active(db, NULL);
    CHECK(projects_db_get_active_id(db) == NULL, "active cleared");

    /* discovered repos */
    const char *roots[] = { "/repo/x", "/repo/y" };
    const char *labels[] = { "X", "" };
    int w = projects_db_record_discovered_repos(db, roots, labels, 2, false);
    CHECK(w == 2, "record_discovered_repos wrote 2");
    int rn; discovered_repo_t *repos = projects_db_list_discovered_repos(db, &rn);
    CHECK(rn == 2, "list_discovered_repos -> 2");
    /* y has empty label -> falls back to basename */
    for (int i=0;i<rn;i++) {
        if (strcmp(repos[i].root, "/repo/y")==0)
            CHECK(repos[i].label && strcmp(repos[i].label, "y")==0, "empty label -> basename");
    }
    projects_db_free_repos(repos, rn);

    /* project_for_path: longest-prefix match (folders /tmp/proj_db_a was removed above) */
    mkdir("/tmp/proj_db_b/sub", 0755);
    project_t *owner = projects_db_project_for_path(db, "/tmp/proj_db_b/sub/file.c", false);
    CHECK(owner != NULL && strcmp(owner->id, pid) == 0, "project_for_path resolves owner");
    projects_db_free_project(owner);
    CHECK(projects_db_project_for_path(db, "/nonexistent/elsewhere", false) == NULL, "no owner -> NULL");

    /* branch_name_for */
    p = projects_db_get_project(db, pid);
    char *bn = projects_db_branch_name_for(p, "task-123", "Fix the Bug!!");
    CHECK(strcmp(bn, "alpha/task-123-fix-the-bug") == 0, "branch_name_for deterministic");
    free(bn);
    projects_db_free_project(p);

    /* delete */
    CHECK(projects_db_delete_project(db, pid2), "delete ok");
    all = projects_db_list_projects(db, true, &n);
    CHECK(n == 1, "after delete -> 1");
    projects_db_free_projects(all, n);

    projects_db_close(db);
    rmdir(folders[0]); rmdir(folders[1]); rmdir("/tmp/proj_db_b/sub"); rmdir(dir);

    if (g_fail==0) printf("ALL PASSED\n"); else printf("%d FAIL\n", g_fail);
    return g_fail ? 1 : 0;
}
