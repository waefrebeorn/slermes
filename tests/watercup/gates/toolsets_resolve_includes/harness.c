/* gate: toolsets_resolve_includes — traced copy of
 * src/tools/toolsets.c::toolsets_resolve / resolve_into ladder
 * (LIVE twin: toolsets.py::resolve_toolset, include_registry=False).
 *
 * The harness uses the REAL port's lookup surface (toolsets_get_static,
 * toolset_def_t fields) so table drift is visible; the recursion ladder is a
 * traced mirror of resolve_into's decision points.
 *
 * Rung order (must match twin.py EXACTLY):
 *   A1: name is "all"/"*" alias        A2: already visited (diamond/cycle)
 *   A3: toolset not found              A4: direct tools present
 *   A5: includes present
 *   V:  result class "empty"/"nonempty"
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"
#include "toolsets.h"

#define MAXV 256

static char *visited[MAXV];
static int n_visited;
static wcup_t *g_wc;
static void TB(int taken) { wcup_branch(g_wc, taken); }

/* registry stubs: this gate pins the STATIC (include_registry=False) ladder */
size_t registry_get_tool_names_for_toolset(const char *name, char ***out) {
    (void)name; if (out) *out = NULL; return 0;
}
char **registry_get_registered_toolset_names(size_t *n) {
    if (n) *n = 0; return NULL;
}

static char *names_out[4096];
static int n_names;

static void add_name(const char *s) {
    for (int i = 0; i < n_names; i++)
        if (strcmp(names_out[i], s) == 0) return;
    if (n_names < 4096) names_out[n_names++] = strdup(s);
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void resolve_trace(const char *name) {
    /* A1 */
    if (name && (strcmp(name, "all") == 0 || strcmp(name, "*") == 0)) {
        TB(1);
        size_t nn = 0;
        char **names = toolsets_get_names(&nn);
        for (size_t i = 0; i < nn; i++) {
            int saved = n_visited;              /* fresh visited per branch */
            resolve_trace(names[i]);
            n_visited = saved;
        }
        toolsets_free_list(names, nn);
        return;
    }
    TB(0);

    /* A2 */
    int seen = 0;
    for (int i = 0; i < n_visited; i++)
        if (visited[i] && strcmp(visited[i], name) == 0) { seen = 1; break; }
    if (!name || seen) {
        TB(1);
        return;
    }
    TB(0);
    if (n_visited < MAXV) visited[n_visited++] = strdup(name ? name : "");

    const toolset_def_t *d = toolsets_get_static(name);
    /* A3 */
    if (!d) {
        TB(1);
        return;
    }
    TB(0);

    /* A4 — direct tools, including the real port's core-bundle expansion
     * rule (hermes-* bundles with tools==NULL resolve HERMES_CORE_TOOLS,
     * except gateway/acp/api-server which are includes-unions). */
    int has_tools = (d->tools && d->tools[0]) ||
        (d->tools == NULL && strncmp(d->name, "hermes-", 7) == 0 &&
         strcmp(d->name, "hermes-gateway") != 0 &&
         strcmp(d->name, "hermes-acp") != 0 &&
         strcmp(d->name, "hermes-api-server") != 0);
    if (has_tools) {
        TB(1);
        if (d->tools && d->tools[0])
            for (size_t i = 0; d->tools[i]; i++) add_name(d->tools[i]);
        else
            for (const char *const *ct = toolsets_core_tools(); *ct; ct++)
                add_name(*ct);
    } else {
        TB(0);
    }

    /* A5 */
    if (d->includes && d->includes[0]) {
        TB(1);
        for (size_t i = 0; d->includes[i]; i++)
            resolve_trace(d->includes[i]);
    } else {
        TB(0);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fn <name>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("toolsets_resolve_includes"); g_wc = &_wc;
        n_names = 0;
        for (int k = 0; k < MAXV; k++) visited[k] = NULL;
        n_visited = 0;
        resolve_trace(argv[i]);
        for (int k = 0; k < n_visited; k++) free(visited[k]);
        qsort(names_out, n_names, sizeof(char *), cmp_str);
        char cls[32];
        snprintf(cls, sizeof cls, "%s", n_names ? "nonempty" : "empty");
        WCUP_VALSTR(cls);
        for (int k = 0; k < n_names; k++) free(names_out[k]);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("toolsets_resolve_includes");
    }
    return 0;
}
