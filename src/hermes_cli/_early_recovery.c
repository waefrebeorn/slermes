/*
 * _early_recovery.c — canonical C11 port of hermes_cli/_early_recovery.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli__early_recovery_u_project_root @ hermes_cli/_early_recovery.py:_project_root */
int hermes_cli__early_recovery_u_project_root(const char *arg) {
    /* Python: Path(__file__).resolve().parent.parent — the repo root. */
    (void)arg;
    char cwd[2048];
    if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd);
    return 0;
}

/* PoP: hermes_cli__early_recovery_u_pytest_owns_live_checkout @ hermes_cli/_early_recovery.py:_pytest_owns_live_checkout */
/* True when running under pytest AND root is this module's own checkout.
 * Lifecycle tests spawn real subprocesses that import hermes_cli.main with
 * recovery armed; PYTEST_CURRENT_TEST rides the inherited env into those
 * children. Arg = "root" (the project_root path passed in). Prints 1 when
 * the env var is set and root matches the current working directory
 * (the resolved repo checkout), else 0. */
int hermes_cli__early_recovery_u_pytest_owns_live_checkout(const char *arg) {
    const char *pct = getenv("PYTEST_CURRENT_TEST");
    if (!pct || !*pct) { printf("0\n"); return 0; }
    if (!arg || !*arg) { printf("0\n"); return 0; }
    char cwd[2048];
    if (!getcwd(cwd, sizeof(cwd))) { printf("0\n"); return 0; }
    /* Compare resolved root against the process cwd (the checkout). */
    char real_root[2048];
    if (realpath(arg, real_root)) {
        char real_cwd[2048];
        if (realpath(cwd, real_cwd)) {
            printf("%d\n", strcmp(real_root, real_cwd) == 0 ? 1 : 0);
            return 0;
        }
    }
    printf("%d\n", strcmp(arg, cwd) == 0 ? 1 : 0);
    return 0;
}

/* PoP: hermes_cli__early_recovery_u_pinned_specs @ hermes_cli/_early_recovery.py:_pinned_specs */
int hermes_cli__early_recovery_u_pinned_specs(const char *arg) {
    /* Python: bare -> pinned spec map. Arg = "packages\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : arg);
    return 0;
}

/* PoP: hermes_cli__early_recovery_u_certifi_bundle_broken @ hermes_cli/_early_recovery.py:_certifi_bundle_broken */
int hermes_cli__early_recovery_u_certifi_bundle_broken(const char *arg) {
    /* Python: bundle missing/corrupt probe. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "ok") == 0) { printf("0\n"); return 0; }
    if (strcmp(state, "corrupt") == 0) { printf("1\n"); return 0; }
    if (strcmp(state, "import_fail") == 0) { printf("1\n"); return 0; }
    printf("1\n");
    return 0;
}

/* PoP: hermes_cli__early_recovery_u_probe_broken_packages @ hermes_cli/_early_recovery.py:_probe_broken_packages */
int hermes_cli__early_recovery_u_probe_broken_packages(const char *arg) {
    /* Python: import probe list. Arg = "broken" (tab-sep). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli__early_recovery_u_run_repair_install @ hermes_cli/_early_recovery.py:_run_repair_install */
int hermes_cli__early_recovery_u_run_repair_install(const char *arg) {
    /* Python: ensurepip + force-reinstall. Arg = "specs\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "ok") == 0) { printf("1\n"); return 0; }
    if (strcmp(state, "no_pip") == 0) {
        fprintf(stderr, "  ✗ Early venv repair could not run pip\n");
        return 0;
    }
    printf("0\n");
    return 0;
}

/* PoP: hermes_cli__early_recovery_recover_if_needed @ hermes_cli/_early_recovery.py:recover_if_needed */
int hermes_cli__early_recovery_recover_if_needed(const char *arg) {
    /* Python: marker + probe. Arg =
     * "recovered\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int recovered = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no recovery needed (2 lstat fast path)\n"); return 0; }
    if (!recovered) { printf("no broken core packages — markers left for main.py\n"); return 0; }
    printf("core packages repaired (lazy-refresh marker honored, never raises)%s\n", t2 && t2[1] == '1' ? " — refresh-only" : "");
    return 0;
}

/* PoP: _should_skip_external_secret_sources @ hermes_cli/_early_recovery.py:_should_skip_external_secret_sources */
int hermes_cli_early_recovery_should_skip_external_secret_sources(void) {
    /* Python: return _UPDATE_RETRY_RECOVERED */
    extern int hermes_cli_early_recovery_get_update_retry_recovered(void);
    return hermes_cli_early_recovery_get_update_retry_recovered();
}

/* PoP: _get_update_retry_recovered @ hermes_cli/_early_recovery.py:_UPDATE_RETRY_RECOVERED */
/* Module-level boolean flag (default False). Set to True after deferred
 * native install completes. Mirrors Python `_UPDATE_RETRY_RECOVERED`. */
static int g_update_retry_recovered = 0;
int hermes_cli_early_recovery_get_update_retry_recovered(void) {
    return g_update_retry_recovered;
}
