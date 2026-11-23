#ifndef NOC_W13_BROWSERRUN_9220BD_H
#define NOC_W13_BROWSERRUN_9220BD_H

/* C port of the Chromium-selection logic in hermes_cli/browser_runtime.py
 * (gate w13_browserrun_9220bd). The four lookups (config override, pm build,
 * Docker baked path, system command) are injected as args, matching the
 * live module's seams; NULL / "" means Python None / falsy.
 * Returned char* is malloc'd; NULL represents Python None (caller frees). */

char *w13_browserrun_9220bd_chromium_executable(
    int allow_override,
    const char *configured,      /* browser.executable_path override */
    const char *pm_exec,         /* pm.installed_package("chromium").binary */
    const char *baked_exec,      /* /etc/hermes/chromium-executable-path */
    const char *system_exec,     /* system chromium / chromium-browser on PATH */
    int pm_has_no_chromium);     /* pm publishes no build for this target */

char *w13_browserrun_9220bd_chromium_missing_hint(
    const char *pinned,          /* configured override, or NULL */
    int is_termux,
    int baked_exists,            /* BAKED_EXECUTABLE_FILE.exists() */
    int pm_has_no_chromium);

#endif /* NOC_W13_BROWSERRUN_9220BD_H */
