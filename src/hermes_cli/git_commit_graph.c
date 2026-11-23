/*
 * git_commit_graph.c — C port of upstream Hermes hermes_cli/git_commit_graph.py.
 * Gate: w15_gitcommit_f305b1
 *
 * PoP: w15_gitcommit_f305b1_commit_graph_stale @ hermes_cli/git_commit_graph.py:commit_graph_stale
 * PoP: w15_gitcommit_f305b1_graph_on_disk @ hermes_cli/git_commit_graph.py:_graph_on_disk
 *
 * Filesystem state is injected as a path->mtime mapping; `now` and `chain_text`
 * replace time.time() and the chain file read. The oracle uses a matching fake
 * Path tree, so no real files or git subprocesses are involved.
 */
#include "noc_w15_gitcommit_f305b1.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_AGE (24.0 * 3600.0)
#define GRAPH1 "objects/info/commit-graph"
#define GRAPH2 "objects/info/commit-graphs/commit-graph-chain"
#define STAMP  "hermes-commit-graph.fresh"

static double file_mtime(const json_t *files, const char *path, int *present) {
    const json_t *v = json_obj_get(files, path);
    if (v && v->type == JSON_NUMBER) { *present = 1; return v->num_val; }
    *present = 0;
    return 0.0;
}

json_t *w15_gitcommit_f305b1_commit_graph_stale(const struct json_t *files,
                                                double now) {
    double newest = 0.0;
    int have = 0;
    int present = 0;
    double m = file_mtime(files, GRAPH1, &present);
    if (present) { newest = have ? (newest > m ? newest : m) : m; have = 1; }
    m = file_mtime(files, GRAPH2, &present);
    if (present) { newest = have ? (newest > m ? newest : m) : m; have = 1; }
    if (!have) return json_bool(1);
    m = file_mtime(files, STAMP, &present);
    if (present) newest = newest > m ? newest : m;
    return json_bool((now - newest) > MAX_AGE);
}

json_t *w15_gitcommit_f305b1_graph_on_disk(const struct json_t *files,
                                           const char *chain_text) {
    if (!chain_text) {
        const json_t *mono = json_obj_get(files, GRAPH1);
        return json_bool(mono && mono->type == JSON_NUMBER);
    }
    /* str.split(): split on whitespace, drop empties. */
    char *buf = (char *)malloc(strlen(chain_text) + 1);
    if (!buf) return json_bool(0);
    strcpy(buf, chain_text);
    char **layers = NULL;
    size_t nl = 0, cap = 0;
    char *p = buf;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        char *start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p) *p++ = '\0';
        if (nl == cap) { cap = cap ? cap * 2 : 8; char **t = realloc(layers, cap * sizeof(char *)); if (t) layers = t; }
        if (nl < cap) layers[nl++] = start;
    }
    int ok = nl > 0;
    for (size_t i = 0; i < nl && ok; i++) {
        char path[512];
        snprintf(path, sizeof(path), "objects/info/commit-graphs/graph-%s.graph", layers[i]);
        const json_t *v = json_obj_get(files, path);
        if (!v || v->type != JSON_NUMBER) ok = 0;
    }
    free(layers);
    free(buf);
    return json_bool(ok);
}

