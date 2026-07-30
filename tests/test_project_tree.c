/*
 * test_project_tree.c — unit tests for the pure tui_gateway/project_tree.py
 * path/lane-id helpers. Invariants derived from a Python oracle.
 */

#include "project_tree_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;
#define CHK(cond, lbl) do { \
    if (!(cond)) { printf("FAIL: %s\n", lbl); g_fail++; } \
    else printf("ok: %s\n", lbl); \
} while (0)

int main(void)
{
    /* _segments */
    int n; char **s;
    s = project_tree_segments("", &n);
    CHK(n == 0, "segments empty -> 0");
    project_tree_free_segments(s, n);

    s = project_tree_segments("/", &n);
    CHK(n == 0, "segments slash -> 0");
    project_tree_free_segments(s, n);

    s = project_tree_segments("/home/user/repo/", &n);
    CHK(n == 3 && strcmp(s[0],"home")==0 && strcmp(s[1],"user")==0 && strcmp(s[2],"repo")==0,
         "segments unix trailing-slash");
    project_tree_free_segments(s, n);

    s = project_tree_segments("a/b\\c//d", &n);
    CHK(n == 4 && strcmp(s[0],"a")==0 && strcmp(s[1],"b")==0 &&
         strcmp(s[2],"c")==0 && strcmp(s[3],"d")==0, "segments mixed separators");
    project_tree_free_segments(s, n);

    /* base_name */
    char buf[256];
    project_tree_base_name("", buf, sizeof buf);
    CHK(buf[0] == '\0', "base_name empty");
    project_tree_base_name("/a/b/c/", buf, sizeof buf);
    CHK(strcmp(buf, "c") == 0, "base_name trailing slash");
    project_tree_base_name("C:\\Users\\x", buf, sizeof buf);
    CHK(strcmp(buf, "x") == 0, "base_name windows");

    /* kanban_worktree_dir */
    char *k;
    k = project_tree_kanban_worktree_dir("/repo/.worktrees/t_abc123");
    CHK(k && strcmp(k, "/repo/.worktrees") == 0, "kwd match");
    free(k);
    k = project_tree_kanban_worktree_dir("/repo/.worktrees/t_abc123/");
    CHK(k && strcmp(k, "/repo/.worktrees") == 0, "kwd match trailing sep");
    free(k);
    k = project_tree_kanban_worktree_dir("/repo/.worktrees/notask");
    CHK(k == NULL, "kwd no t_ prefix");
    k = project_tree_kanban_worktree_dir("/repo/branches/t_abc123");
    CHK(k == NULL, "kwd wrong dir name");
    k = project_tree_kanban_worktree_dir(".worktrees/t_abc123");
    CHK(k == NULL, "kwd no parent repo");

    /* _is_path_under */
    CHK(project_tree_is_path_under("/a/b", "/a/b") == 1, "is_under equal");
    CHK(project_tree_is_path_under("/a/b", "/a/b/c/d") == 1, "is_under nested");
    CHK(project_tree_is_path_under("/a/b", "/a/c") == 0, "is_under sibling");
    CHK(project_tree_is_path_under("/a", "/a/b/c") == 1, "is_under shallow parent");
    CHK(project_tree_is_path_under("/a/b/c", "/a/b") == 0, "is_under deeper-not-parent");

    /* _with_base_name */
    char *w = project_tree_with_base_name("/repo/foo", "bar");
    CHK(w && strcmp(w, "/repo/bar") == 0, "with_base_name basic");
    free(w);
    w = project_tree_with_base_name("/repo/foo/", "bar");
    CHK(w && strcmp(w, "/repo/bar") == 0, "with_base_name trailing slash");
    free(w);
    w = project_tree_with_base_name("foo", "bar");
    CHK(w && strcmp(w, "bar") == 0, "with_base_name single segment");
    free(w);

    /* _branch_lane_id */
    char *bl = project_tree_branch_lane_id("/repo", "main");
    CHK(bl && strcmp(bl, "/repo::branch::main") == 0, "branch_lane_id plain");
    free(bl);
    bl = project_tree_branch_lane_id("/repo", "");
    CHK(bl && strcmp(bl, "/repo::branch::") == 0, "branch_lane_id empty branch");
    free(bl);
    bl = project_tree_branch_lane_id("/repo", "  dev  ");
    CHK(bl && strcmp(bl, "/repo::branch::dev") == 0, "branch_lane_id stripped");
    free(bl);

    /* _kanban_lane_id */
    char *kl = project_tree_kanban_lane_id("/repo");
    CHK(kl && strcmp(kl, "/repo::kanban") == 0, "kanban_lane_id");
    free(kl);

    if (g_fail) { printf("\n%d FAIL\n", g_fail); return 1; }
    printf("\nALL PASSED\n");
    return 0;
}
