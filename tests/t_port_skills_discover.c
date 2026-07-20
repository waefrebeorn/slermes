/*
 * t_port_skills_discover.c — faithful verification harness for
 * _discover_skills_in_dir() in lib/libskillsync/skills_sync.c.
 *
 * The fixture (argv[1]) describes a skill tree; one directive per line:
 *   skill   <name>            -> dir <root>/<name>/SKILL.md  (a skill)
 *   nested  <parent> <name>   -> dir <root>/<parent>/<name>/SKILL.md
 *   noskill <name>            -> empty dir <root>/<name> (no SKILL.md)
 * The harness builds the tree under a temp root, runs the discovery from
 * <root>, and emits the sorted list of discovered skill basenames as JSON
 * {"discovered":[...]}. The Python oracle (tests/sta_oracle_skills_discover.py)
 * recomputes the same discovery from the fixture; the runner diffs them.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "skills_sync.h"

static void make_dir(const char *p) {
    /* mkdir -p: create parent components as needed */
    char tmp[1536];
    size_t n = strlen(p);
    if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
    memcpy(tmp, p, n); tmp[n] = '\0';
    for (size_t i = 1; i < n; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            mkdir(tmp, 0755);
            tmp[i] = '/';
        }
    }
    mkdir(tmp, 0755);
}
static void write_file(const char *p, const char *content) {
    FILE *f = fopen(p, "a");
    if (f) { fputs(content, f); fclose(f); }
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <tree.txt>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

    char root[1024];
    snprintf(root, sizeof(root), "/tmp/skills_discover_%d", (int)getpid());
    mkdir(root, 0755);

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        if (n == 0) continue;

        char *op = line;
        char *a = strchr(op, ' '); if (!a) continue; *a++ = '\0';
        char *b = strchr(a, ' ');
        if (b) *b++ = '\0';

        if (strcmp(op, "skill") == 0) {
            char d[1536]; snprintf(d, sizeof(d), "%s/%s", root, a);
            make_dir(d);
            char sf[2048]; snprintf(sf, sizeof(sf), "%s/SKILL.md", d);
            write_file(sf, "---\nname: "); write_file(sf, a); write_file(sf, "\n---\n");
        } else if (strcmp(op, "nested") == 0 && b) {
            char d[1536]; snprintf(d, sizeof(d), "%s/%s/%s", root, a, b);
            make_dir(d);
            char pd[1536]; snprintf(pd, sizeof(pd), "%s/%s", root, a); make_dir(pd);
            char sf[2048]; snprintf(sf, sizeof(sf), "%s/SKILL.md", d);
            write_file(sf, "---\nname: "); write_file(sf, b); write_file(sf, "\n---\n");
        } else if (strcmp(op, "noskill") == 0) {
            char d[1536]; snprintf(d, sizeof(d), "%s/%s", root, a);
            make_dir(d);
        }
    }
    fclose(f);

    discover_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    _discover_skills_in_dir(root, &ctx);

    /* emit sorted names */
    /* simple insertion sort by name */
    for (int i = 1; i < ctx.count; i++) {
        char nm[SKILLS_SYNC_MAX_NAME];
        char dr[SKILLS_SYNC_MAX_PATH];
        memcpy(nm, ctx.names[i], sizeof(nm));
        memcpy(dr, ctx.dirs[i], sizeof(dr));
        int j = i - 1;
        while (j >= 0 && strcmp(ctx.names[j], nm) > 0) {
            memcpy(ctx.names[j+1], ctx.names[j], sizeof(ctx.names[j]));
            memcpy(ctx.dirs[j+1], ctx.dirs[j], sizeof(ctx.dirs[j]));
            j--;
        }
        memcpy(ctx.names[j+1], nm, sizeof(nm));
        memcpy(ctx.dirs[j+1], dr, sizeof(dr));
    }

    printf("{\"discovered\":[");
    for (int i = 0; i < ctx.count; i++) {
        if (i) printf(",");
        printf("\"%s\"", ctx.names[i]);
    }
    printf("]}\n");

    /* cleanup */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", root);
    system(cmd);
    return 0;
}
