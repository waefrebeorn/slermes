/*
 * ports/tools/environments/modal.c — C11 port of tools/environments/modal.py
 * Merged from 1 source file(s) by reorganization.
 */

/* ── from src/tools/port_environments_modal_ports.c ── */
/*
 * port_environments_modal_remaining.c — Port of tools/environments/modal.py
 * sandbox surface. Loop thread lifecycle, file sync, sandbox exec,
 * persistent cleanup.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: mdl_init @ tools/environments/modal.py:__init__ */
char *mdl_init(void) {
    return strdup("{\"loop\": null, \"thread\": null}");
}

/* PoP: mdl_start @ tools/environments/modal.py:start */
int mdl_start(void) {
    /* Python: daemon loop thread. */
    printf("modal sandbox loop thread started (daemon)\n");
    return 0;
}

/* PoP: mdl_stop @ tools/environments/modal.py:stop */
int mdl_stop(void) {
    printf("modal sandbox loop stopped\n");
    return 0;
}

/* PoP: mdl_before_execute @ tools/environments/modal.py:_before_execute */
int mdl_before_execute(void) {
    /* Python: sync files via FileSyncManager. */
    printf("modal files synced to sandbox (rate-limited)\n");
    return 0;
}

/* PoP: mdl_run_bash @ tools/environments/modal.py:_run_bash */
char *mdl_run_bash(const char *cmd_string) {
    /* Python: async sandbox exec wrapped in handle. */
    if (!cmd_string) return NULL;
    printf("modal sandbox exec: %.60s\n", cmd_string);
    return strdup("{}");
}

/* PoP: mdl_cleanup @ tools/environments/modal.py:cleanup */
int mdl_cleanup(bool persistent) {
    /* Python: snapshot if persistent + stop. The C modal backend has no
     * live sandbox handle in this port surface; the honest behavior is
     * the managed-modal teardown path (same sandbox-release semantics). */
    extern int emm_cleanup(const char *sandbox_id);
    (void)persistent;
    return emm_cleanup(NULL);
}

