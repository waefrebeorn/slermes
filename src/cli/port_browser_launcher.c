/*
 * port_browser_launcher.c
 *
 * Pure helpers ported from tools/browser_launcher.py (83 defs, previously 0
 * ported).
 *
 *   bl_has_display        <- _has_display
 *   bl_build_argv         <- build_chromium_argv
 *
 * The download / spawn / singleton half stays in Python. `build_chromium_argv`
 * is documented as "pure in its inputs + host sandbox facts +
 * browser.chromium_args" - the two externals are passed as parameters.
 */

#include "port_browser_launcher.h"

#include <stdlib.h>
#include <string.h>

/* browser_launcher.py:_BASE_FLAGS */
static const char *const BL_BASE_FLAGS[] = {
    "--no-first-run", "--no-default-browser-check", "--disable-background-networking",
    "--disable-component-update", "--disable-default-apps", "--disable-sync",
    "--disable-features=Translate",
};
#define BL_BASE_FLAGS_N (sizeof(BL_BASE_FLAGS) / sizeof(BL_BASE_FLAGS[0]))

/* browser_launcher.py:_PACKAGED_ONLY_FLAGS */
static const char *const BL_PACKAGED_FLAGS[] = { "--password-store=basic" };

/* browser_launcher.py:_MACOS_PACKAGED_ONLY_FLAGS */
static const char *const BL_MACOS_PACKAGED_FLAGS[] = { "--use-mock-keychain" };

/* browser_launcher.py:CHROMIUM_SANDBOX_BYPASS_ARGS */
static const char *const BL_BYPASS_ARGS[] = { "--no-sandbox", "--disable-dev-shm-usage" };
#define BL_BYPASS_ARGS_N (sizeof(BL_BYPASS_ARGS) / sizeof(BL_BYPASS_ARGS[0]))

/* ===========================================================================
 *  _has_display  (browser_launcher.py:_has_display)
 *
 *      return bool(env.get("DISPLAY") or env.get("WAYLAND_DISPLAY"))
 *
 *  `env` is a flat [key, value, ...] array; a missing or empty value is falsy.
 * =========================================================================== */

/* PoP: _has_display @ tools/browser_launcher.py:_has_display */
int bl_has_display(const char *const *env)
{
    for (int i = 0; env && env[i]; i += 2) {
        if ((strcmp(env[i], "DISPLAY") == 0 || strcmp(env[i], "WAYLAND_DISPLAY") == 0)) {
            const char *v = env[i + 1];
            if (v && v[0]) return 1;
        }
    }
    return 0;
}

/* ===========================================================================
 *  build_chromium_argv  (browser_launcher.py:build_chromium_argv)
 *
 *      argv = [executable, "--remote-debugging-port=0",
 *              f"--user-data-dir={user_data_dir}", *_BASE_FLAGS]
 *      if packaged:
 *          argv += _PACKAGED_ONLY_FLAGS
 *          if sys.platform == "darwin": argv += _MACOS_PACKAGED_ONLY_FLAGS
 *      if not headed: argv.append("--headless=new")
 *      configured = _configured_chromium_args()
 *      if sandbox_bypass and needs_chromium_sandbox_bypass():
 *          argv += [flag for flag in CHROMIUM_SANDBOX_BYPASS_ARGS
 *                   if flag not in configured]
 *      argv += [str(a) for a in extra_args]
 *      argv += configured
 *      argv.append("about:blank")
 *      return argv
 *
 *  Ordering is load-bearing: the sandbox-bypass flags are added only when the
 *  host needs them AND the operator did not already configure them, then the
 *  extra args, then the configured args, then the final URL. `configured`
 *  stands in for browser.chromium_args; `needs_bypass` for the host policy.
 *  Returns a malloc'd NULL-terminated array of malloc'd strings.
 * =========================================================================== */

/* PoP: build_chromium_argv @ tools/browser_launcher.py:build_chromium_argv */
char **bl_build_argv(const char *executable, const char *user_data_dir,
                     int packaged, int is_macos, int headed,
                     int sandbox_bypass, int needs_bypass,
                     const char *const *configured,
                     const char *const *extra_args, int *out_count)
{
    int nconf = 0, nextra = 0;
    if (configured) while (configured[nconf]) nconf++;
    if (extra_args) while (extra_args[nextra]) nextra++;

    size_t cap = 4 + BL_BASE_FLAGS_N + 2 + 1 + BL_BYPASS_ARGS_N
               + (size_t)nextra + (size_t)nconf + 1 + 1;
    char **argv = (char **)calloc(cap, sizeof(char *));
    if (!argv) { if (out_count) *out_count = 0; return NULL; }

    int k = 0;
    argv[k++] = strdup(executable ? executable : "");
    argv[k++] = strdup("--remote-debugging-port=0");
    size_t ul = strlen(user_data_dir ? user_data_dir : "") + 17;  /* "--user-data-dir=" is 16 + NUL */
    argv[k] = (char *)malloc(ul);
    if (argv[k]) { snprintf(argv[k], ul, "--user-data-dir=%s", user_data_dir ? user_data_dir : ""); k++; }

    for (size_t i = 0; i < BL_BASE_FLAGS_N; i++) argv[k++] = strdup(BL_BASE_FLAGS[i]);

    if (packaged) {
        for (size_t i = 0; i < sizeof(BL_PACKAGED_FLAGS) / sizeof(BL_PACKAGED_FLAGS[0]); i++)
            argv[k++] = strdup(BL_PACKAGED_FLAGS[i]);
        if (is_macos)
            for (size_t i = 0; i < sizeof(BL_MACOS_PACKAGED_FLAGS) / sizeof(BL_MACOS_PACKAGED_FLAGS[0]); i++)
                argv[k++] = strdup(BL_MACOS_PACKAGED_FLAGS[i]);
    }
    if (!headed) argv[k++] = strdup("--headless=new");

    if (sandbox_bypass && needs_bypass) {
        for (size_t i = 0; i < BL_BYPASS_ARGS_N; i++) {
            int already = 0;
            for (int j = 0; j < nconf; j++)
                if (strcmp(configured[j], BL_BYPASS_ARGS[i]) == 0) { already = 1; break; }
            if (!already) argv[k++] = strdup(BL_BYPASS_ARGS[i]);
        }
    }
    for (int i = 0; i < nextra; i++) argv[k++] = strdup(extra_args[i]);
    for (int i = 0; i < nconf; i++) argv[k++] = strdup(configured[i]);
    argv[k++] = strdup("about:blank");

    if (out_count) *out_count = k;
    return argv;
}
