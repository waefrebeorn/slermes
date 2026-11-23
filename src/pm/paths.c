/*
 * paths.c — real C11 port of pm/paths.py (gate w5_paths_f8daf3).
 *
 * PoP: pmpaths_install_root @ pm/paths.py:install_root
 * PoP: pmpaths_install_stamp_path @ pm/paths.py:install_stamp_path
 * PoP: pmpaths_partials_root @ pm/paths.py:partials_root
 * PoP: pmpaths_facts_path @ pm/paths.py:facts_path
 *
 * __file__-derived roots and HERMES_INSTALL_ROOT are injected at the gate;
 * the selection logic and path joins are reproduced exactly (POSIX spelling).
 */
#include "noc_w5_paths_f8daf3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Path / operator: one separator, whatever the left side ends with. */
static void path_join2(const char *a, const char *b, char *out, size_t outsz) {
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a);
    if (la == 0) { snprintf(out, outsz, "%s", b); return; }
    if (a[la - 1] == '/') snprintf(out, outsz, "%s%s", a, b);
    else snprintf(out, outsz, "%s/%s", a, b);
}

int pmpaths_install_root(const char *repo_root, const char *env_value, char **out) {
    /* env = os.environ.get("HERMES_INSTALL_ROOT"); return Path(env) if env else repo_root() */
    if (env_value && *env_value) *out = strdup(env_value);
    else *out = strdup(repo_root ? repo_root : "");
    return 1;
}

int pmpaths_install_stamp_path(const char *project_root, int resolves_to_repo,
                               const char *install_root, char **out) {
    const char *root = resolves_to_repo ? (install_root ? install_root : "") : (project_root ? project_root : "");
    char buf[4096];
    path_join2(root, "install-stamp.json", buf, sizeof(buf));
    *out = strdup(buf);
    return 1;
}

int pmpaths_partials_root(const char *default_root, char **out) {
    char first[4096], buf[4096];
    path_join2(default_root ? default_root : "", "cache", first, sizeof(first));
    path_join2(first, "partials", buf, sizeof(buf));
    *out = strdup(buf);
    return 1;
}

int pmpaths_facts_path(const char *store_root, char **out) {
    char buf[4096];
    path_join2(store_root ? store_root : "", "facts.json", buf, sizeof(buf));
    *out = strdup(buf);
    return 1;
}
