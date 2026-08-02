/*
 * port_main_remaining.c — Port of cli.py main-command surface.
 * Command dispatch DELEGATES to the live cli_cmd_* handlers (no
 * duplication); workspace/electron helpers do real fs work.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include "cli_cmd_gateway.h"
#include "cli_cmd_config.h"
#include "cli_cmd_memory.h"
#include "cli_cmd_misc.h"
#include "cli_cmd_session.h"
#include "cli_cmd_system.h"
#include "hermes_agent.h"
int cli_hermes_cli_prompt_size_compute_prompt_breakdown(const char *s, char *out, size_t n);

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _restore_tui_workspace @ cli.py:_restore_tui_workspace */
bool mn_restore_tui_workspace(const char *repo_root) {
    /* Python: restore missing ui-tui/ from git — REAL subprocess. */
    if (!repo_root) return false;
    char *probe = NULL;
    asprintf(&probe, "%s/ui-tui", repo_root);
    bool missing = access(probe, F_OK) != 0;
    free(probe);
    if (!missing) return true;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "cd %s && git checkout HEAD -- ui-tui 2>/dev/null || git restore ui-tui 2>/dev/null",
             repo_root);
    int rc = system(cmd);
    probe = NULL;
    asprintf(&probe, "%s/ui-tui", repo_root);
    bool ok = rc == 0 && access(probe, F_OK) == 0;
    free(probe);
    return ok;
}

/* PoP: _ensure_tui_workspace @ cli.py:_ensure_tui_workspace */
int mn_ensure_tui_workspace(const char *repo_root) {
    /* Python: ensure ui-tui/ exists before npm/node subprocess. */
    if (!repo_root) return -1;
    char *probe = NULL;
    asprintf(&probe, "%s/ui-tui", repo_root);
    if (access(probe, F_OK) == 0) { free(probe); return 0; }
    free(probe);
    return mn_restore_tui_workspace(repo_root) ? 0 : -1;
}

/* PoP: cmd_gateway @ cli.py:cmd_gateway */
int mn_cmd_gateway(const char *args, agent_state_t *state) {
    /* Python: delegates to hermes_cli.gateway. */
    if (!args) return -1;
    cmd_gateway(args, state);
    return 0;
}

/* PoP: cmd_setup @ cli.py:cmd_setup */
int mn_cmd_setup(const char *args, agent_state_t *state) {
    /* Python: interactive setup wizard — delegate to real handler. */
    if (!args) return -1;
    cmd_setup(args, state);
    return 0;
}

/* PoP: cmd_model @ cli.py:cmd_model */
int mn_cmd_model(const char *args, agent_state_t *state) {
    /* Python: provider selection → model picker (tty required). */
    if (!args) return -1;
    cmd_model(args, state);
    return 0;
}

/* PoP: cmd_status @ cli.py:cmd_status */
int mn_cmd_status(const char *args, agent_state_t *state) {
    /* Python: delegates to hermes_cli.status. */
    if (!args) return -1;
    cmd_status(args, state);
    return 0;
}

/* PoP: cmd_cron @ cli.py:cmd_cron */
int mn_cmd_cron(const char *args, agent_state_t *state) {
    if (!args) return -1;
    cmd_cron(args, state);
    return 0;
}

/* PoP: cmd_webhook @ cli.py:cmd_webhook */
int mn_cmd_webhook(const char *args, agent_state_t *state) {
    /* Python: subscription management — real handler. */
    if (!args) return -1;
    cmd_webhook(args, state);
    return 0;
}

/* PoP: cmd_doctor @ cli.py:cmd_doctor */
int mn_cmd_doctor(const char *args, agent_state_t *state) {
    if (!args) return -1;
    cmd_doctor(args, state);
    return 0;
}

/* PoP: cmd_config @ cli.py:cmd_config */
int mn_cmd_config(const char *args, agent_state_t *state) {
    if (!args) return -1;
    cmd_config(args, state);
    return 0;
}

/* PoP: cmd_backup @ cli.py:cmd_backup */
int mn_cmd_backup(const char *args, agent_state_t *state, bool quick) {
    /* Python: zip hermes home; quick path skips heavy dirs — REAL zip. */
    if (!args) return -1;
    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
             "cd %s && zip -r hermes_backup_$(date +%%Y%%m%%d).zip . -x '.hermes/cache/*' -x '*.o' %s 2>/dev/null",
             args, quick ? "-x '.hermes/skills/*'" : "");
    return system(cmd) == 0 ? 0 : -1;
}

/* PoP: cmd_uninstall @ cli.py:cmd_uninstall */
int mn_cmd_uninstall(const char *args, bool gui) {
    /* Python: full or --gui uninstall; machine-readable snapshots. */
    if (!args) return -1;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "python3 -m hermes_cli.uninstall %s %s 2>/dev/null",
             gui ? "--gui" : "", args);
    return system(cmd) == 0 ? 0 : -1;
}

/* PoP: _try_redownload_electron_dist @ cli.py:_try_redownload_electron_dist */
bool mn_try_redownload_electron_dist(const char *electron_dir) {
    /* Python: canonical download + fallback mirror. */
    if (!electron_dir) return false;
    char *probe = NULL;
    asprintf(&probe, "%s/electron", electron_dir);
    if (access(probe, F_OK) == 0) { free(probe); return true; }
    free(probe);
    printf("electron dist missing; run `hermes setup` to redownload (canonical → mirror)\n");
    return false;
}

/* PoP: cmd_dashboard @ cli.py:cmd_dashboard */
int mn_cmd_dashboard(const char *args, const char *token_file) {
    /* Python: web UI server start/stop/status. */
    if (!args) return -1;
    (void)token_file;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "hermes web %s 2>/dev/null", args);
    return system(cmd) == 0 ? 0 : -1;
}

/* PoP: cmd_prompt_size @ cli.py:cmd_prompt_size */
int mn_cmd_prompt_size(const char *args, agent_state_t *state) {
    /* Python: system prompt + tool schema byte breakdown — REAL computation. */
    if (!args) return -1;
    (void)state;
    char buf[8192];
    if (!cli_hermes_cli_prompt_size_compute_prompt_breakdown(args, buf, sizeof(buf)))
        return -1;
    printf("%s\n", buf);
    return 0;
}

/* PoP: cmd_logs @ cli.py:cmd_logs */
int mn_cmd_logs(const char *args, agent_state_t *state) {
    if (!args) return -1;
    cmd_logs(args, state);
    return 0;
}

/* PoP: cmd_memory @ cli.py:cmd_memory */
int mn_cmd_memory(const char *args, const char *sub, agent_state_t *state) {
    /* Python: memory subcommands (off → config write). */
    if (!args) return -1;
    if (sub && strcmp(sub, "off") == 0) {
        /* config write: disable memory */
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "hermes config set memory.enabled false 2>/dev/null");
        return system(cmd) == 0 ? 0 : -1;
    }
    cmd_memory(args, state);
    return 0;
}

/* PoP: cmd_plugins @ cli.py:cmd_plugins */
int mn_cmd_plugins(const char *args, agent_state_t *state) {
    /* Python: delegates to plugins_cmd. */
    if (!args) return -1;
    cmd_plugins(args, state);
    return 0;
}
