/*
 * webapp.c - Browser-hosted Desktop renderer helpers (C11 port).
 *
 * PoP: w6_webapp_879137_dist_dir @ hermes_cli/webapp.py:webapp_dist_dir
 * PoP: w6_webapp_879137_mark_stale @ hermes_cli/webapp.py:mark_webapp_stale
 * PoP: w6_webapp_879137_bundle_stale @ hermes_cli/webapp.py:webapp_bundle_stale
 *
 * Filesystem effects are supplied as pre-resolved scalars from the fixture
 * (dist env value, index.html presence, stale-marker presence), so no process
 * environment mutation or real file I/O is needed on the C side.
 */
#include "noc_w6_webapp_879137.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *w6_webapp_879137_dist_dir(const char *project_root, const char *dist_env) {
    if (dist_env && *dist_env) return strdup(dist_env);   /* packaged_webapp_dist() wins */
    const char *root = project_root ? project_root : "";
    size_t len = strlen(root) + strlen("/apps/desktop/dist-webapp") + 1;
    char *out = (char *)malloc(len);
    if (!out) return NULL;
    snprintf(out, len, "%s/apps/desktop/dist-webapp", root);
    return out;
}

int w6_webapp_879137_mark_stale(int packaged, int index_exists) {
    if (packaged) return 0;       /* packager owns the bundle */
    if (!index_exists) return 0;  /* nothing serves a bundle */
    return 1;
}

int w6_webapp_879137_bundle_stale(int marker_exists) { return marker_exists ? 1 : 0; }
void w6_webapp_879137_free_str(char *s) { free(s); }

