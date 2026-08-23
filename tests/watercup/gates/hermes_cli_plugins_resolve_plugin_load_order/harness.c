/* gate: hermes_cli_plugins_resolve_plugin_load_order — drives the REAL
 * src/hermes_cli/plugins_core.c::plugins_resolve_plugin_load_order (linked
 * via extra_src). LIVE twin: hermes_cli/plugins.py::resolve_plugin_load_order.
 *
 * Input: one graph per cup, entries '|' separated, each "key:name:dep,dep"
 * ('-' name = NULL display name; empty dep list allowed).
 * Rung order (must match twin.py EXACTLY):
 *   B1  resolution succeeded (non-NULL order)
 *   V   comma-joined load-order keys ("" for empty manifest set)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

typedef struct {
    const char *key;
    const char *name;
    const char *const *requires;
} plugins_dep_node_t;

const char **plugins_resolve_plugin_load_order(const plugins_dep_node_t *nodes,
                                               size_t n_nodes,
                                               size_t *out_n);

/* Shims: plugins_core.c's manifest-scanner symbols are outside this gate's
 * pure rung (no YAML scanning happens in load-order resolution). */
void *json_parse_yaml_real(const char *text, char **err) {
    (void)text; if (err) *err = NULL; return NULL;
}
char *plugins_portable_skill_namespace(const char *plugin_name) {
    return plugin_name ? strdup(plugin_name) : NULL;
}

#define MAX_NODES 16
#define MAX_DEPS 8

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s resolve <graph>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        static plugins_dep_node_t nodes[MAX_NODES];
        static const char *dep_store[MAX_NODES][MAX_DEPS + 1];
        memset(nodes, 0, sizeof nodes);
        size_t n = 0;

        /* parse "key:name:dep,dep|key:name:..." */
        char buf[2048];
        snprintf(buf, sizeof buf, "%s", argv[i]);
        if (strcmp(buf, "-") == 0) buf[0] = '\0';   /* "-" encodes empty set */
        char *save = NULL;
        for (char *ent = strtok_r(buf, "|", &save); ent && n < MAX_NODES;
             ent = strtok_r(NULL, "|", &save)) {
            char *c1 = strchr(ent, ':');
            if (c1) *c1 = '\0';
            char *name = c1 ? c1 + 1 : NULL;
            const char *depspec = NULL;
            if (name) {
                char *c2 = strchr(name, ':');
                if (c2) { *c2 = '\0'; depspec = c2 + 1; }
            }
            int nd = 0;
            if (depspec && *depspec) {
                char *dsave = NULL;
                for (char *d = strtok_r((char *)depspec, ",", &dsave);
                     d && nd < MAX_DEPS; d = strtok_r(NULL, ",", &dsave))
                    dep_store[n][nd++] = d;
            }
            dep_store[n][nd] = NULL;
            nodes[n].key = ent;
            nodes[n].name = (name && strcmp(name, "-") != 0) ? name : NULL;
            nodes[n].requires = dep_store[n];
            n++;
        }

        WCUP_BEGIN("resolve_plugin_load_order");
        size_t out_n = 0;
        const char **order =
            n ? plugins_resolve_plugin_load_order(nodes, n, &out_n) : NULL;
        WCUP_BRANCH(order != NULL);                    /* B1 */
        char val[1024];
        val[0] = '\0';
        if (order) {
            for (size_t k = 0; k < out_n; k++) {
                if (k) strcat(val, ",");
                strcat(val, order[k]);
            }
            free(order);
        }
        WCUP_VALSTR(val);                              /* V */
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("resolve_plugin_load_order");
    }
    return 0;
}
