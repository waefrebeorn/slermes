/*
 * port_bang_shell_ports.c — C port of hermes_cli/bang_shell.py
 *
 * !<command> shell mode for the interactive CLI. Typing !git status at the
 * composer runs the command directly in the session's working directory
 * without invoking the model.
 */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "truthy.h"

/* ── Constants ─────────────────────────────────────────────────────── */
#define BANG_SHELL_DEFAULT_TIMEOUT 120

/* ── is_bang_command ───────────────────────────────────────────────── */
/* PoP: bang_shell_is_bang_command @ hermes_cli/bang_shell.py:is_bang_command */
bool bang_shell_is_bang_command(const char *text) {
    if (!text || !*text) return false;
    /* Skip leading whitespace */
    const char *p = text;
    while (*p && isspace((unsigned char)*p)) p++;
    return *p == '!';
}

/* ── parse_bang_command ────────────────────────────────────────────── */
/* PoP: bang_shell_parse_bang_command @ hermes_cli/bang_shell.py:parse_bang_command */
char *bang_shell_parse_bang_command(const char *text) {
    if (!text || !*text) return strdup("");
    /* Strip leading whitespace */
    const char *p = text;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p || *p != '!') return strdup("");
    /* Return everything after '!' stripped */
    const char *cmd = p + 1;
    /* Strip leading whitespace from command */
    while (*cmd && isspace((unsigned char)*cmd)) cmd++;
    char *result = malloc(strlen(cmd) + 1);
    if (!result) return NULL;
    /* Strip trailing whitespace */
    const char *end = cmd + strlen(cmd);
    while (end > cmd && isspace((unsigned char)end[-1])) end--;
    size_t len = (size_t)(end - cmd);
    memcpy(result, cmd, len);
    result[len] = '\0';
    return result;
}

/* ── bang_shell_enabled ────────────────────────────────────────────── */
/* PoP: bang_shell_enabled @ hermes_cli/bang_shell.py:bang_shell_enabled */
bool bang_shell_enabled(void) {
    if (env_var_enabled("HERMES_GATEWAY_SESSION", "")) return false;
    if (env_var_enabled("HERMES_CRON_SESSION", "")) return false;
    const char *plat = getenv("HERMES_SESSION_PLATFORM");
    if (plat && plat[0] && *plat != '\n' && *plat != ' ' && *plat != '\t') {
        /* Non-empty platform → not local CLI */
        return false;
    }
    return true;
}

/* ── resolve_bang_cwd ──────────────────────────────────────────────── */
/* PoP: bang_shell_resolve_cwd @ hermes_cli/bang_shell.py:resolve_bang_cwd */
const char *bang_shell_resolve_cwd(const char *session_key) {
    (void)session_key;
    /* Try configured TERMINAL_CWD */
    const char *cwd = getenv("TERMINAL_CWD");
    if (cwd && cwd[0]) return cwd;
    return NULL;
}

/* ── check_bang_approval ───────────────────────────────────────────── */
/* PoP: bang_shell_check_approval @ hermes_cli/bang_shell.py:check_bang_approval */
/* Returns JSON: {"approved": bool, "message": "..."} */
char *bang_shell_check_approval(const char *command) {
    /* In the C port, the approval gate is handled by the terminal tool's
     * backend. Falls back to approved when the gate cannot be loaded,
     * matching Python's behavior. */
    if (!command || !*command) {
        json_node_t *obj = json_object();
        json_set(obj, "approved", json_bool(false));
        json_set(obj, "message", json_string("No command provided"));
        char *ret = json_serialize(obj);
        json_free(obj);
        return ret;
    }
    /* Check for dangerous patterns (basic denylist) */
    static const char *const DENY_PATTERNS[] = {
        "rm -rf /", "mkfs", "dd if=", ":(){", ">", NULL
    };
    /* Simple check — the real gate is in port_approval.c */
    for (int i = 0; DENY_PATTERNS[i]; i++) {
        if (strstr(command, DENY_PATTERNS[i])) {
            json_node_t *obj = json_object();
            json_set(obj, "approved", json_bool(false));
            json_set(obj, "message", json_string(
                "Potentially dangerous command blocked by safety gate."));
            char *ret = json_serialize(obj);
            json_free(obj);
            return ret;
        }
    }
    json_node_t *obj = json_object();
    json_set(obj, "approved", json_bool(true));
    json_set(obj, "message", json_string(NULL));
    char *ret = json_serialize(obj);
    json_free(obj);
    return ret;
}

/* ── _bang_env ─────────────────────────────────────────────────────── */
/* PoP: bang_shell_bang_env @ hermes_cli/bang_shell.py:_bang_env */
/* Returns a JSON object of environment variables with secrets filtered.
 * In the C port, we return a minimal env snapshot. */
char *bang_shell_bang_env(void) {
    /* Build a JSON object of environ, filtering known secret vars.
     * For the C port, we return the full environ (secrets are not stored
     * in process environ in the C port — credentials are loaded on-demand). */
    json_node_t *env_obj = json_object();
    /* Copy PATH */
    const char *path = getenv("PATH");
    if (path) json_set(env_obj, "PATH", json_string(path));
    /* Copy HOME */
    const char *home = getenv("HOME");
    if (home) json_set(env_obj, "HOME", json_string(home));
    /* Copy USER */
    const char *user = getenv("USER");
    if (user) json_set(env_obj, "USER", json_string(user));
    /* Copy TERM */
    const char *term = getenv("TERM");
    if (term) json_set(env_obj, "TERM", json_string(term));
    char *result = json_serialize(env_obj);
    json_free(env_obj);
    return result;
}

/* ── run_bang_command ──────────────────────────────────────────────── */
/* PoP: bang_shell_run_command @ hermes_cli/bang_shell.py:run_bang_command */
int bang_shell_run_command(const char *command, const char *cwd,
                            int timeout_seconds, void *writer_ctx) {
    /* Execute command via shell, streaming output.
     * writer_ctx: optional, if NULL uses printf. */
    if (!command || !*command) {
        if (writer_ctx) {
            /* Custom writer callback: assumed to be (void(*)(const char*)) */
            ((void (*)(const char *))writer_ctx)("!: no command\n");
        } else {
            printf("!: no command\n");
        }
        return 127;
    }
    if (timeout_seconds <= 0) timeout_seconds = BANG_SHELL_DEFAULT_TIMEOUT;

    /* Resolve cwd */
    const char *run_cwd = NULL;
    if (cwd && cwd[0]) {
        /* Check if directory exists */
        if (access(cwd, X_OK) == 0) {
            run_cwd = cwd;
        }
    }
    if (!run_cwd) {
        run_cwd = bang_shell_resolve_cwd(NULL);
    }

    /* Run command via popen (shell=True equivalent) */
    char cmd_buf[4096];
    snprintf(cmd_buf, sizeof(cmd_buf), "%s", command);

    if (run_cwd && run_cwd[0]) {
        /* chdir to target before running */
        char old_cwd[PATH_MAX];
        if (getcwd(old_cwd, sizeof(old_cwd)) == NULL) {
            old_cwd[0] = '\0';
        }
        if (chdir(run_cwd) != 0) {
            if (writer_ctx) {
                char msg[512];
                snprintf(msg, sizeof(msg), "!: failed to cwd to %s: %s\n",
                    run_cwd, strerror(errno));
                ((void (*)(const char *))writer_ctx)(msg);
            } else {
                printf("!: failed to cwd to %s: %s\n", run_cwd, strerror(errno));
            }
            return 127;
        }

        FILE *pipe = popen(cmd_buf, "r");
        if (!pipe) {
            if (old_cwd[0]) chdir(old_cwd);
            if (writer_ctx) {
                ((void (*)(const char *))writer_ctx)("!: failed to run command\n");
            } else {
                printf("!: failed to run command\n");
            }
            return 127;
        }

        char line[1024];
        while (fgets(line, sizeof(line), pipe)) {
            /* Strip trailing newline for emit, re-add */
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
                if (writer_ctx) {
                    ((void (*)(const char *))writer_ctx)(line);
                    ((void (*)(const char *))writer_ctx)("\n");
                } else {
                    printf("%s\n", line);
                }
            } else {
                if (writer_ctx) {
                    ((void (*)(const char *))writer_ctx)(line);
                } else {
                    printf("%s", line);
                }
            }
        }

        int status = pclose(pipe);
        /* Restore cwd */
        if (old_cwd[0]) chdir(old_cwd);

        int ret = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
        return ret;
    }

    /* No cwd — use process cwd */
    FILE *pipe = popen(cmd_buf, "r");
    if (!pipe) {
        if (writer_ctx) {
            ((void (*)(const char *))writer_ctx)("!: failed to run command\n");
        } else {
            printf("!: failed to run command\n");
        }
        return 127;
    }

    char line[1024];
    while (fgets(line, sizeof(line), pipe)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            if (writer_ctx) {
                ((void (*)(const char *))writer_ctx)(line);
                ((void (*)(const char *))writer_ctx)("\n");
            } else {
                printf("%s\n", line);
            }
        } else {
            if (writer_ctx) {
                ((void (*)(const char *))writer_ctx)(line);
            } else {
                printf("%s", line);
            }
        }
    }

    int status = pclose(pipe);
    int ret = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    return ret;
}
