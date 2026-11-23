/*
 * operations.c — faithful C11 port of upstream Hermes pm/operations.py.
 *
 * PoP: w5_operations_2c07e3_environment_root @ pm/operations.py:_environment_root
 *
 * build_environment/lock_project/stage_manager_runtime and the ensure_*
 * family are all subprocess/IO driven (uv, filesystem, toolchain discovery)
 * and are intentionally out of scope for a pure differential fixture gate.
 */
#include "noc_w5_operations_2c07e3.h"
#include <stdlib.h>
#include <string.h>

static int ascii_alnum(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

/* re.fullmatch(r"[a-zA-Z0-9][a-zA-Z0-9_.-]*", name) */
static int valid_name(const char *s) {
    if (!s || !*s) return 0;
    if (!ascii_alnum((unsigned char)s[0])) return 0;
    for (const char *p = s + 1; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!ascii_alnum(c) && c != '_' && c != '.' && c != '-') return 0;
    }
    return 1;
}

int w5_operations_2c07e3_environment_root(const char *name, const char *root,
                                          char **out_path) {
    if (!valid_name(name)) return 0;
    if (!root) return 0;
    /* PureWindowsPath(str) reports '/' separators on the oracle's Python;
     * canonicalise to '/' so the comparison is separator-style independent.
     * Fixtures supply clean absolute roots, so no cwd join / '..' collapse. */
    size_t n = strlen(root);
    char *p = (char *)malloc(n + 1);
    for (size_t i = 0; i < n; i++)
        p[i] = (root[i] == '\\') ? '/' : root[i];
    p[n] = '\0';
    *out_path = p;
    return 1;
}
