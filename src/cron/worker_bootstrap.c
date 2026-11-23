/* worker_bootstrap.c — C home for upstream Hermes cron/worker_bootstrap.py.
 *
 * PoP: w7_workerboots_worker_bootstrap_ex @ cron/worker_bootstrap.py:worker_bootstrap
 * The marker gate is the whole decision: PM's activate_dependencies (with its lease)
 * runs only for a marked external worker; PM's other no-ops return normally. The
 * env value is injected through the _ex seam instead of mutating the CRT environment.
 */
#include "noc_w7_workerboots_fdb251.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

/* cron/worker_bootstrap.py: WORKER_MARKER = "_HERMES_CRON_WORKER_BOOT" */
static const char *const WORKER_MARKER = "_HERMES_CRON_WORKER_BOOT";

/* Python passes Path(project_root_str()) into activate_dependencies. Mirror
 * pathlib's Windows string form: "/" -> "\\", collapse separators, drop "."
 * components, strip trailing separators, and map "" -> ".". */
static void py_path_str(const char *in, char *out, size_t cap) {
    if (!in || !*in) { snprintf(out, cap, "%s", "."); return; }
    int unc = (in[0] == '/' || in[0] == '\\') && (in[1] == '/' || in[1] == '\\');
    char tmp[2048];
    size_t t = 0;
    int prev_sep = 0;
    for (const char *p = in; *p && t + 1 < sizeof(tmp); p++) {
        char c = (*p == '/') ? '\\' : *p;
        if (c == '\\') {
            if (prev_sep) continue;
            prev_sep = 1;
        } else {
            prev_sep = 0;
        }
        tmp[t++] = c;
    }
    tmp[t] = '\0';

    size_t o = 0;
    int first = 1;
    const char *p = tmp;
    if (unc) {
        out[o++] = '\\'; out[o++] = '\\';
        while (*p == '\\') p++;
        first = 0;
    }
    while (*p && o + 1 < cap) {
        const char *q = strchr(p, '\\');
        size_t len = q ? (size_t)(q - p) : strlen(p);
        if (!(len == 1 && p[0] == '.')) {
            if (!first && o + 1 < cap) out[o++] = '\\';
            for (size_t i = 0; i < len && o + 1 < cap; i++) out[o++] = p[i];
            first = 0;
        }
        if (!q) break;
        p = q + 1;
    }
    out[o] = '\0';
    if (o == 0 || !strcmp(out, ".")) snprintf(out, cap, "%s", ".");
}

json_t *w7_workerboots_worker_bootstrap_ex(bool has_marker, const char *marker_value,
                                           const char *project_root) {
    (void)WORKER_MARKER; /* documented provenance of the injected marker value */
    json_t *a = json_array();
    if (!has_marker || !marker_value || !marker_value[0]) {
        json_append(a, json_string("noop"));
        return a;
    }
    char norm[2048];
    py_path_str(project_root, norm, sizeof(norm));
    json_append(a, json_string("activate"));
    json_append(a, json_string(norm));
    return a;
}
