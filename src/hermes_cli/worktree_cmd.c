/*
 * worktree_cmd.c — C11 port of hermes_cli/worktree_cmd.py
 *
 * `hermes worktree list|prune` over the worktree_gc port. Usage:
 *   hermes worktree list             # audit: verdict + reason per tree
 *   hermes worktree prune            # reap safe trees + merged branches
 *   hermes worktree prune --dry-run  # show the plan, change nothing
 *   hermes worktree prune --trees-only / --branches-only
 */
#define _POSIX_C_SOURCE 200809L

#include "worktree_cmd.h"
#include "worktree_gc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>

/* PoP: worktree_cmd_repo_root @hermes_cli/worktree_cmd.py:_repo_root */
static char *worktree_cmd_repo_root(void) { return wgc_git_repo_root(); }

/* PoP: _fmt_size @hermes_cli/worktree_cmd.py:_fmt_size */
char *worktree_cmd_fmt_size(int has_size, int size_mb)
{
    char buf[32];
    if (!has_size)
        snprintf(buf, sizeof(buf), "?");
    else if (size_mb >= 1024)
        snprintf(buf, sizeof(buf), "%.1fG", (double)size_mb / 1024.0);
    else
        snprintf(buf, sizeof(buf), "%dM", size_mb);
    return strdup(buf);
}

static void free_actions(char **actions, size_t n)
{
    for (size_t i = 0; i < n; i++) free(actions[i]);
    free(actions);
}

/* PoP: cmd_worktree @hermes_cli/worktree_cmd.py:cmd_worktree */
int worktree_cmd_cmd_worktree(const char *repo, const char *action,
                              int dry_run, int trees_only, int branches_only)
{
    char *repo_root = NULL;
    if (repo && repo[0]) {
        repo_root = strdup(repo);
    } else {
        repo_root = worktree_cmd_repo_root();
    }
    if (!repo_root || !repo_root[0]) {
        printf("Not inside a git repository (or pass --repo <path>).\n");
        free(repo_root);
        return 1;
    }
    const char *act = (action && action[0]) ? action : "list";

    if (strcmp(act, "list") == 0) {
        size_t rn = 0;
        wgc_tree_record_t *records =
            wgc_audit_worktrees(repo_root, 1, &rn);
        if (!records || rn == 0) {
            printf("No worktrees under .worktrees/ — nothing to reclaim.\n");
            wgc_free_tree_records(records, rn);
            free(repo_root);
            return 0;
        }
        long total_mb = 0, reapable_mb = 0;
        for (size_t i = 0; i < rn; i++) {
            long sz = records[i].has_size ? records[i].size_mb : 0;
            total_mb += sz;
            if (strncmp(records[i].verdict, "reap", 4) == 0)
                reapable_mb += sz;
        }
        printf("%-32s %6s %6s %-13s REASON\n", "TREE", "AGE", "SIZE",
               "VERDICT");
        /* sort by -(size_mb or 0): simple selection sort */
        for (size_t i = 0; i + 1 < rn; i++) {
            size_t best = i;
            for (size_t j = i + 1; j < rn; j++) {
                long sj = records[j].has_size ? records[j].size_mb : 0;
                long sb = records[best].has_size ? records[best].size_mb : 0;
                if (sj > sb) best = j;
            }
            if (best != i) {
                wgc_tree_record_t tmp = records[i];
                records[i] = records[best];
                records[best] = tmp;
            }
        }
        for (size_t i = 0; i < rn; i++) {
            wgc_tree_record_t *r = &records[i];
            char *sz = worktree_cmd_fmt_size(r->has_size, r->size_mb);
            printf("%-32.32s %5.1fd %6s %-13s %s\n", r->name, r->age_days,
                   sz, r->verdict, r->reason);
            free(sz);
        }
        char *tot = worktree_cmd_fmt_size(1, (int)total_mb);
        char *rep = worktree_cmd_fmt_size(1, (int)reapable_mb);
        printf("\n%zu tree(s), %s total — %s reclaimable now via `hermes "
               "worktree prune`.\n",
               rn, tot, rep);
        free(tot);
        free(rep);
        size_t bn = 0;
        wgc_branch_record_t *branch_records =
            wgc_audit_branches(repo_root, &bn);
        size_t deletable = 0;
        for (size_t i = 0; i < bn; i++)
            if (strcmp(branch_records[i].verdict, "delete") == 0)
                deletable++;
        if (deletable)
            printf("%zu local branch(es) fully merged/patch-equivalent "
                   "upstream would also be deleted.\n",
                   deletable);
        wgc_free_branch_records(branch_records, bn);
        wgc_free_tree_records(records, rn);
        free(repo_root);
        return 0;
    }

    if (strcmp(act, "prune") == 0) {
        char **actions = NULL;
        size_t an = 0;
        char **branch_actions = NULL;
        size_t branch_an = 0;

        if (!branches_only) {
            size_t rn = 0;
            wgc_tree_record_t *tree_records =
                wgc_audit_worktrees(repo_root, 0, &rn);
            actions = wgc_reclaim_worktrees(repo_root, dry_run, tree_records,
                                            rn, &an);
            /* Preserved trees with real work (excluding kanban/in-use). */
            size_t kept_n = 0;
            for (size_t i = 0; i < rn; i++) {
                wgc_tree_record_t *r = &tree_records[i];
                if (strcmp(r->verdict, "keep") == 0 &&
                    !strstr(r->reason, "kanban") &&
                    !strstr(r->reason, "in use"))
                    kept_n++;
            }
            if (kept_n) {
                printf("Preserved %zu tree(s) with real work:\n", kept_n);
                for (size_t i = 0; i < rn; i++) {
                    wgc_tree_record_t *r = &tree_records[i];
                    if (strcmp(r->verdict, "keep") == 0 &&
                        !strstr(r->reason, "kanban") &&
                        !strstr(r->reason, "in use"))
                        printf("  %s: %s\n", r->name, r->reason);
                }
            }
            wgc_free_tree_records(tree_records, rn);
        }
        if (!trees_only) {
            branch_actions =
                wgc_reclaim_branches(repo_root, dry_run, NULL, 0, &branch_an);
            if (branch_actions && branch_an) {
                char **na =
                    realloc(actions, (an + branch_an) * sizeof(char *));
                if (na) {
                    actions = na;
                    for (size_t i = 0; i < branch_an; i++)
                        actions[an++] = branch_actions[i];
                    free(branch_actions);
                    branch_actions = NULL;
                    branch_an = 0;
                }
            }
        }
        free_actions(branch_actions, branch_an);

        if (an) {
            for (size_t i = 0; i < an; i++) printf("  %s\n", actions[i]);
            printf("%zu action(s) %s.\n", an, dry_run ? "planned" : "done");
        } else {
            printf("Nothing to reclaim — all trees/branches carry real work "
                   "or are in use.\n");
        }
        free_actions(actions, an);
        free(repo_root);
        return 0;
    }

    printf("Unknown worktree action: %s\n", act);
    free(repo_root);
    return 1;
}
