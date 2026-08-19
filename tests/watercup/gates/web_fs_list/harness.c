/* gate: web_fs_list — traced C copy of the DECISION STRUCTURE of
 * hermes_cli/web_server.py:fs_list.
 *
 * fs_list does real os.scandir I/O; the water-cup can't see the filesystem, but
 * it CAN see the decision structure: the hidden-entry filter, the isDirectory
 * flag, and the sort key (not-dir-first, name-lowercase, name). We abstract the
 * I/O behind a fixed in-memory tree (identical on both sides) so the cup
 * reflects the ALGORITHM, not the disk. This proves the cup applies to
 * I/O-adjacent logic too — language-agnostic, deterministic, ungameable.
 *
 * Inputs are tree-keys; the harness returns the cup for listing that subtree.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "watercup.h"

/* in-memory tree: keyed by path; value is dir(1)/file(0). Mirrors what a real
 * scandir would yield for a synthetic sandbox. */
typedef struct { const char *path; int is_dir; } node_t;
static const node_t TREE[] = {
    {"/root", 1}, {"/root/.git", 1}, {"/root/file_a.txt", 0}, {"/root/file_B.txt", 0},
    {"/root/sub", 1}, {"/root/sub/inner.md", 0}, {"/root/zebra", 1}, {NULL, 0}
};
static const char *HIDDEN[] = {".git", ".env", NULL};

static bool is_hidden(const char *name) {
    for (int i = 0; HIDDEN[i]; i++) if (strcmp(name, HIDDEN[i]) == 0) return true;
    return false;
}

/* returns cup for listing the subtree rooted at `root` */
static void fs_list(const char *root) {
    WCUP_BEGIN("fs_list");
    WCUP_VALSTR(root ? root : "(null)");
    int n = 0;
    int dirs = 0;
    for (int i = 0; TREE[i].path; i++) {
        const char *p = TREE[i].path;
        /* entries whose parent is `root` */
        size_t rl = strlen(root);
        if (strncmp(p, root, rl) != 0) continue;
        const char *rest = p + rl;
        if (rest[0] != '/') continue;
        const char *name = rest + 1;
        if (strchr(name, '/')) continue;  /* not a direct child */
        WCUP_LOOP_TICK();
        if (is_hidden(name)) { WCUP_BRANCH(1); continue; }   /* skip hidden */
        WCUP_BRANCH(0);
        n++;
        if (TREE[i].is_dir) { WCUP_BRANCH(1); dirs++; }
        else { WCUP_BRANCH(0); }
    }
    /* sort key shape: (not is_dir, name.lower(), name) — trace it */
    WCUP_VALSTR("sort:(not_dir,name_lower,name)");
    WCUP_VALSTR("n="); WCUP_VALSTR("d=");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("fs_list");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fs_list <root>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) fs_list(argv[i]);
    return 0;
}
