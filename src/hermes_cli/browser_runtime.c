/*
 * browser_runtime.c — faithful C11 port of the Chromium-selection logic in
 * hermes_cli/browser_runtime.py (gate w13_browserrun_9220bd).
 * PoP: w13_browserrun_9220bd_chromium_executable @ hermes_cli/browser_runtime.py:chromium_executable
 * PoP: w13_browserrun_9220bd_chromium_missing_hint @ hermes_cli/browser_runtime.py:chromium_missing_hint
 *
 * The live module resolves its four sources through helper calls / a Path
 * constant. Those are injected here so the ordering and hint copy are pure.
 */
#include "noc_w13_browserrun_9220bd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int nonempty(const char *s) { return s && s[0] != '\0'; }

char *w13_browserrun_9220bd_chromium_executable(
    int allow_override,
    const char *configured,
    const char *pm_exec,
    const char *baked_exec,
    const char *system_exec,
    int pm_has_no_chromium) {
    if (allow_override && nonempty(configured)) return strdup(configured);
    if (nonempty(pm_exec)) return strdup(pm_exec);
    if (nonempty(baked_exec)) return strdup(baked_exec);
    if (pm_has_no_chromium && nonempty(system_exec)) return strdup(system_exec);
    return NULL;  /* Python None */
}

char *w13_browserrun_9220bd_chromium_missing_hint(
    const char *pinned,
    int is_termux,
    int baked_exists,
    int pm_has_no_chromium) {
    if (nonempty(pinned)) {
        size_t cap = strlen(pinned) + 300;
        char *m = (char *)malloc(cap);
        snprintf(m, cap,
                 "browser.executable_path is set to %s, which is not a browser on this machine: "
                 "fix the path, or clear it to use Hermes's Chromium.", pinned);
        return m;
    }
    if (is_termux) {
        return strdup("No Chromium found: pm builds none for Termux. Install one with "
                      "`pkg install x11-repo && pkg install chromium`, or set browser.executable_path.");
    }
    if (baked_exists) {
        return strdup("The image's Chromium is missing: pull the current Hermes image and recreate "
                      "the container (a volume mounted over the image's tool store hides it).");
    }
    if (pm_has_no_chromium) {
        return strdup("No Chromium found: pm builds none for this platform. Install your "
                      "distribution's chromium package, or set browser.executable_path.");
    }
    return strdup("Chromium is not installed: run `hermes pm install chromium`.");
}

