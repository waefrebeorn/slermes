/*
 * _old_updater.c — C port of upstream Hermes hermes_cli/_old_updater.py.
 *
 * PoP: w9_oldupdater_42fb74_is_historical_update_frame @ hermes_cli/_old_updater.py:_is_historical_update_frame
 *
 * Only the frame-recognition predicate is deterministic and portable; the
 * frame walk, subprocess custody and takeover child are intentionally excluded.
 */
#include "noc_w9_oldupdater_42fb74.h"
#include <string.h>

static const char *const _NAMES[] = { "_cmd_update_impl", "cmd_update", NULL };
static const char *const _MODULES[] = { "hermes_cli.update_cmd", "hermes_cli.main", NULL };

static int in_set(const char *const *set, const char *s) {
    if (!s) return 0;
    for (int i = 0; set[i]; i++) if (!strcmp(set[i], s)) return 1;
    return 0;
}

/* Normalise a Windows/POSIX path for comparison: backslashes -> slashes. */
static int same_path(const char *a, const char *b) {
    char na[4096], nb[4096];
    if (!a || !b) return 0;
    size_t i = 0;
    for (; a[i] && i + 1 < sizeof(na); i++) na[i] = a[i] == '\\' ? '/' : a[i];
    na[i] = '\0';
    size_t j = 0;
    for (; b[j] && j + 1 < sizeof(nb); j++) nb[j] = b[j] == '\\' ? '/' : b[j];
    nb[j] = '\0';
    /* A missing path resolves to nothing -> treat as non-match. */
    if (!na[0] || !nb[0]) return 0;
    return !strcmp(na, nb);
}

int w9_oldupdater_42fb74_is_historical_update_frame(const char *co_name,
                                                    const char *module,
                                                    const char *co_filename,
                                                    const char *main_py_path) {
    if (!in_set(_NAMES, co_name)) return 0;
    if (in_set(_MODULES, module)) return 1;
    if (!module || strcmp(module, "__main__")) return 0;
    return same_path(co_filename, main_py_path) ? 1 : 0;
}
