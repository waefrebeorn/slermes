/*
 * mcp_tool_node_abi.c — C home for upstream Hermes tools/mcp_tool_node_abi.py.
 * PoP: w10_mcptoolno_477ad2_package_of @ tools/mcp_tool_node_abi.py:_package_of
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w10_mcptoolno_477ad2.h"

/* Path(module_path).parts on Windows splits on both '/' and '\'. Odd path
   strings (drives, '..', anchors) are outside the fixture domain. */
static size_t split_parts(const char *path, char parts[64][256]) {
    size_t n = 0;
    if (!path) return 0;
    const char *p = path;
    while (*p) {
        while (*p == '/' || *p == '\\') p++;
        if (!*p) break;
        const char *s = p;
        while (*p && *p != '/' && *p != '\\') p++;
        size_t len = (size_t)(p - s);
        if (n >= 64 || len >= 256) break;
        memcpy(parts[n], s, len); parts[n][len] = '\0'; n++;
    }
    return n;
}

json_t *w10_mcptoolno_477ad2_package_of(const char *module_path) {
    char parts[64][256];
    size_t n = split_parts(module_path, parts);
    long at = -1;
    for (size_t i = 0; i < n; i++)
        if (!strcmp(parts[i], "node_modules")) at = (long)i;
    if (at < 0 || (size_t)(at + 1) >= n) {
        json_t *res = json_object();
        json_object_set(res, "root", json_null());
        json_object_set(res, "package", json_string(""));
        return res;
    }

    json_t *res = json_object();
    const char *package = parts[at + 1];
    char scoped[512];
    if (package[0] == '@' && (size_t)(at + 2) < n) {
        snprintf(scoped, sizeof(scoped), "%s/%s", parts[at + 1], parts[at + 2]);
        package = scoped;
    }
    char root[4096];
    if (at == 0) {
        snprintf(root, sizeof(root), ".");
    } else {
        root[0] = '\0';
        for (long i = 0; i < at; i++) {
            if (i) strcat(root, "\\");
            strcat(root, parts[i]);
        }
    }
    json_object_set(res, "root", json_string(root));
    json_object_set(res, "package", json_string(package));
    return res;
}
