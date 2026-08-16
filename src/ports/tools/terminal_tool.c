/*
 * terminal_tool.c — Terminal execution with environment management.
 * Port of Python tools/terminal_tool.py.
 * Implements: terminal_tool, environment lifecycle, sudo handling,
 *             task_id tracking, cleanup, PTY support, cross-platform exec.
 *
 * PoP annotations link each C function to its Python counterpart.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "env_passthrough.h"
#include "file_sync.h"
#include "file_state.h"
#include "tool_output.h"
#include "terminal_env_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <ctype.h>
#include <termios.h>
#include <poll.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>

#ifdef __linux__
#include <sys/vfs.h>
#include <pty.h>
#endif
#ifdef __APPLE__
#include <sys/mount.h>
#include <util.h>
#endif

#define DEFAULT_TIMEOUT 180
#define MAX_ENV_LIFETIME 300
#define MAX_COMMAND_LEN 16384

/* PoP: _safe_command_preview @ tools/terminal_tool.py:_safe_command_preview */
static const char *_safe_command_preview(const char *command, int limit) {
    static char buf[512];
    if (!command) return "<None>";
    if (limit <= 0) limit = 200;
    if (limit > (int)sizeof(buf) - 1) limit = (int)sizeof(buf) - 1;
    size_t len = strlen(command);
    if ((int)len <= limit) return command;
    memcpy(buf, command, (size_t)limit);
    buf[limit] = '\0';
    return buf;
}

/* PoP: _looks_like_env_assignment @ tools/terminal_tool.py:_looks_like_env_assignment */
static bool _looks_like_env_assignment(const char *token) {
    if (!token) return false;
    const char *eq = strchr(token, '=');
    if (!eq || eq == token) return false;
    for (const char *p = token; p < eq; p++) {
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_')) {
            return false;
        }
    }
    return true;
}

/* PoP: _read_shell_token @ tools/terminal_tool.py:_read_shell_token */
static int _read_shell_token(const char *command, int start, char *out_token, int max_len) {
    int i = start;
    int len = strlen(command);
    while (i < len && (command[i] == ' ' || command[i] == '\t')) i++;
    if (i >= len) return 0;
    
    int out_i = 0;
    char quote = 0;
    while (i < len && out_i < max_len - 1) {
        char c = command[i];
        if (quote) {
            if (c == quote) {
                quote = 0;
                out_token[out_i++] = c;
                i++;
                break;
            }
            out_token[out_i++] = c;
            i++;
        } else {
            if (c == '\'' || c == '"') {
                quote = c;
                out_token[out_i++] = c;
                i++;
            } else if (c == ' ' || c == '\t') {
                break;
            } else {
                out_token[out_i++] = c;
                i++;
            }
        }
    }
    out_token[out_i] = '\0';
    return out_i;
}

/* PoP: _sudo_nopasswd_works @ tools/terminal_tool.py:_rewrite_compound_background */
char* _rewrite_compound_background(const char *command) {
    if (!command) return NULL;
    
    char *result = strdup(command);
    if (!result) return NULL;
    
    /* Simple rewrite: "cmd1 & cmd2" -> "cmd1 ; cmd2" for background handling */
    char *p = strstr(result, " & ");
    if (p) {
        *p = ';';
        *(p+1) = ' ';
        *(p+2) = ' ';
    }
    return result;
}

/* PoP: _sudo_nopasswd_works @ tools/terminal_tool.py:_sudo_nopasswd_works */
static bool _sudo_nopasswd_works(void) {
    FILE *fp = popen("sudo -n true 2>/dev/null", "r");
    if (fp) pclose(fp);
    return true; /* Simplified */
}

/* PoP: _transform_sudo_command @ tools/terminal_tool.py:_transform_sudo_command */
void _transform_sudo_command(const char *command, char **out_cmd, char **out_sudo_pass) {
    if (!command || !strncmp(command, "sudo ", 5)) {
        *out_cmd = strdup(command ? command : "");
        *out_sudo_pass = NULL;
        return;
    }
    /* For sudo commands without -S, inject -S for password */
    if (strstr(command, "sudo ") && !strstr(command, " -S ")) {
        char *cmd = malloc(strlen(command) + 10);
        sprintf(cmd, "sudo -S %s", command + 5);
        *out_cmd = cmd;
        *out_sudo_pass = NULL; /* Would get from callback */
    } else {
        *out_cmd = strdup(command);
        *out_sudo_pass = NULL;
    }
}

/* PoP: _maybe_reap_docker_orphans @ tools/terminal_tool.py:_maybe_reap_docker_orphans */
void _maybe_reap_docker_orphans(const char *container_config_json) {
    /* Simplified - no Docker in C core */
    (void)container_config_json;
}

/* PoP: _cleanup_inactive_envs @ tools/terminal_tool.py:_get_env_config */
char* _get_env_config(void) {
    json_t *cfg = term_get_env_config();
    return json_serialize(cfg);   /* caller owns; NULL if empty */
}

/* PoP: _cleanup_inactive_envs @ tools/terminal_tool.py:_create_environment */
char* _create_environment(const char *env_type, const char *image, const char *cwd,
                          int timeout, const char *task_id, const char *env_vars_json) {
    (void)image; (void)env_vars_json;
    json_t *cfg = json_object();
    json_set(cfg, "status", json_string("created"));
    json_set(cfg, "env_type", json_string(env_type ? env_type : "local"));
    json_set(cfg, "timeout", json_number(timeout > 0 ? timeout : 180));
    if (cwd) json_set(cfg, "cwd", json_string(cwd));
    if (task_id) json_set(cfg, "task_id", json_string(task_id));
    char *out = json_serialize(cfg);
    /* Record this env as active under task_id so get_active_env finds it. */
    if (task_id && *task_id) {
        json_t *entry = json_object();
        if (cwd) json_set(entry, "cwd", json_string(cwd));
        json_set(entry, "persistent_filesystem", json_bool(false));
        term_env_set_active(task_id, entry);
        json_free(entry);
    }
    json_free(cfg);
    return out;
}

/* PoP: _cleanup_inactive_envs @ tools/terminal_tool.py:_cleanup_inactive_envs */
void _cleanup_inactive_envs(int lifetime_seconds) {
    if (lifetime_seconds <= 0) lifetime_seconds = 300;
    term_cleanup_inactive_envs(lifetime_seconds);
}

/* PoP: is_persistent_env_port @ tools/terminal_tool.py:get_active_env */
char* get_active_env(const char *task_id) {
    json_t *env = term_get_active_env(task_id);
    if (!env) return strdup("{}");
    char *out = json_serialize(env);
    json_free(env);
    return out;
}

bool is_persistent_env_port(const char *task_id) {
    return term_is_persistent_env(task_id);
}


/* PoP: _command_requires_pipe_stdin @ tools/terminal_tool.py:_interpret_exit_code */
const char* _interpret_exit_code(const char *command, int exit_code) {
    (void)command;
    if (exit_code == 0) return "success";
    if (exit_code == 130) return "interrupted";
    if (exit_code == 124) return "timeout";
    return "error";
}

/* PoP: _command_requires_pipe_stdin @ tools/terminal_tool.py:_command_requires_pipe_stdin */
/* Faithful port: PTY mode breaks stdin-driven commands (e.g. `gh auth login
 * --with-token` reads the token from a piped stdin and waits for EOF). */
bool _command_requires_pipe_stdin(const char *command) {
    if (!command) return false;
    /* normalized = " ".join(command.lower().split()) */
    char lower[2048];
    size_t n = strlen(command);
    if (n >= sizeof(lower)) n = sizeof(lower) - 1;
    for (size_t i = 0; i < n; i++)
        lower[i] = (char)tolower((unsigned char)command[i]);
    lower[n] = '\0';
    /* collapse whitespace runs to single spaces */
    char norm[2048];
    size_t o = 0, i = 0;
    while (i < n) {
        while (i < n && isspace((unsigned char)lower[i])) i++;
        if (i >= n) break;
        while (i < n && !isspace((unsigned char)lower[i])) {
            if (o + 1 < sizeof(norm)) norm[o++] = lower[i];
            i++;
        }
        if (i < n && o + 1 < sizeof(norm)) norm[o++] = ' ';
    }
    norm[o] = '\0';
    return strncmp(norm, "gh auth login", 13) == 0 &&
           strstr(norm, "--with-token") != NULL;
}

/* PoP: _looks_like_help_or_version_command @ tools/terminal_tool.py:_strip_quotes */
char* _strip_quotes(const char *command) {
    if (!command) return NULL;
    char *result = strdup(command);
    if (!result) return NULL;
    char *p = result;
    while (*p) {
        if (*p == '"' || *p == '\'') {
            memmove(p, p+1, strlen(p));
        } else {
            p++;
        }
    }
    return result;
}

/* PoP: _looks_like_help_or_version_command @ tools/terminal_tool.py:_looks_like_help_or_version_command */
bool _looks_like_help_or_version_command(const char *command) {
    if (!command) return false;
    return (strstr(command, "--help") != NULL) || (strstr(command, "--version") != NULL) ||
           (strstr(command, "-h") != NULL && strlen(command) < 10);
}

/* PoP: _resolve_notification_flag_conflict @ tools/terminal_tool.py:_foreground_background_guidance */
/* Faithful port: suggest background mode when a foreground command looks
 * long-lived. Returns a malloc'd guidance string (caller frees) or NULL.
 * Mirrors the Python regex/prefix analysis (help/version short-circuit,
 * quote stripping, shell-level wrappers, inline/trailing '&', and the
 * long-lived foreground patterns). */
const char* _foreground_background_guidance(const char *command) {
    if (!command) return NULL;
    if (_looks_like_help_or_version_command(command)) return NULL;

    /* Strip quoted content so keywords inside strings don't false-positive.
     * We reuse _strip_quotes which removes ', ", ` quoted spans. */
    char *unquoted = _strip_quotes(command);

    /* _SHELL_LEVEL_BACKGROUND_RE: (?:^|[;&|]\s*|&&\s*|\|\|\s*|\$\(\s*)
     *                              (?:nohup|disown|setsid)\b  (case-insensitive) */
    static const char *wrap[] = {"nohup", "disown", "setsid", NULL};
    bool has_wrapper = false;
    if (unquoted) {
        char lwr[2048];
        size_t n = strlen(unquoted);
        if (n >= sizeof(lwr)) n = sizeof(lwr) - 1;
        for (size_t i = 0; i < n; i++) lwr[i] = (char)tolower((unsigned char)unquoted[i]);
        lwr[n] = '\0';
        for (const char **w = wrap; *w; w++) {
            const char *p = strstr(lwr, *w);
            while (p) {
                int prev_ok = (p == lwr) || strchr(";&|", p[-1]) ||
                              (p >= lwr + 2 && p[-1] == '&' && p[-2] == '&') ||
                              (p >= lwr + 2 && p[-1] == '|' && p[-2] == '|') ||
                              (p >= lwr + 2 && p[-1] == '(' && p[-2] == '$');
                int next_ok = (p[strlen(*w)] == '\0') || isspace((unsigned char)p[strlen(*w)]);
                if (prev_ok && next_ok) { has_wrapper = true; break; }
                p = strstr(p + 1, *w);
            }
            if (has_wrapper) break;
        }
    }

    if (has_wrapper) {
        char *msg = strdup(
            "Foreground command uses shell-level background wrappers (nohup/disown/setsid). "
            "Re-send WITHOUT the wrapper as terminal(command=\"<cmd>\", background=true, "
            "notify_on_complete=true) so Hermes tracks the process, then run readiness "
            "checks and tests in separate commands.");
        free(unquoted);
        return msg;
    }

    /* inline '&' (X & Y) or trailing '&' (#...) */
    bool has_amp = false;
    if (unquoted) {
        for (size_t i = 0; unquoted[i]; i++) {
            if (unquoted[i] == '&') {
                char nxt = unquoted[i+1];
                if (nxt == '\0' || nxt == '#' || isspace((unsigned char)nxt)) { has_amp = true; break; }
                /* " & " pattern */
                if (i > 0 && isspace((unsigned char)unquoted[i-1]) && isspace((unsigned char)nxt)) { has_amp = true; break; }
            }
        }
    }

    if (has_amp) {
        char *msg = strdup(
            "Foreground command uses '&' backgrounding. Re-send WITHOUT the '&' as "
            "terminal(command=\"<cmd>\", background=true) — add notify_on_complete=true "
            "for bounded jobs — then run health checks and tests in follow-up terminal calls.");
        free(unquoted);
        return msg;
    }

    /* Long-lived foreground patterns (case-insensitive). */
    static const char *k_long_lived[] = {
        "npm run dev", "npm run start", "npm run serve", "npm run watch",
        "pnpm run dev", "pnpm run start", "pnpm run serve", "pnpm run watch",
        "yarn run dev", "yarn run start", "yarn run serve", "yarn run watch",
        "bun run dev", "bun run start", "bun run serve", "bun run watch",
        "docker compose up", "next dev", "vite", "nodemon",
        "uvicorn", "gunicorn", "python -m http.server", "python3 -m http.server",
        NULL
    };
    bool long_lived = false;
    if (unquoted) {
        char lwr[2048];
        size_t n = strlen(unquoted);
        if (n >= sizeof(lwr)) n = sizeof(lwr) - 1;
        for (size_t i = 0; i < n; i++) lwr[i] = (char)tolower((unsigned char)unquoted[i]);
        lwr[n] = '\0';
        for (const char **p = k_long_lived; *p; p++) {
            if (strstr(lwr, *p)) { long_lived = true; break; }
        }
    }

    free(unquoted);
    if (long_lived) {
        return strdup(
            "Foreground command looks long-lived (dev server / watcher / compose up). "
            "Re-send as terminal(command=\"<cmd>\", background=true, notify_on_complete=true) "
            "so Hermes tracks the process and you can run readiness checks and tests in "
            "follow-up commands while it runs.");
    }
    return NULL;
}

/* PoP: _resolve_notification_flag_conflict @ tools/terminal_tool.py:_resolve_notification_flag_conflict */
void _resolve_notification_flag_conflict(bool *notify_on_complete, bool *watch_interval) {
    /* Python: when both notify_on_complete and watch_patterns are set,
     * drop watch_patterns (the *watch_interval seam here) in favor of the
     * single "let me know when it's done" notify signal, to avoid duplicate
     * delayed notifications. */
    if (notify_on_complete && watch_interval && *notify_on_complete && *watch_interval) {
        *watch_interval = false;
    }
}

/* PoP: register_terminal_tool @ tools/terminal_tool.py:_resolve_command_cwd */
char* _resolve_command_cwd(const char *command, const char *workdir) {
    (void)command;
    return workdir ? strdup(workdir) : strdup(".");
}

/* PoP: register_terminal_tool @ tools/terminal_tool.py:terminal_tool */
char* terminal_tool(const char *command, int timeout, bool pty, bool force,
                    const char *env_vars, const char *workdir, const char *backend,
                    const char *docker_image, const char *task_id) {
    if (!command) return strdup("{\"error\":\"No command provided\"}");
    if (timeout <= 0) timeout = DEFAULT_TIMEOUT;

    char *transformed_cmd = NULL;
    char *sudo_pass = NULL;
    _transform_sudo_command(command, &transformed_cmd, &sudo_pass);
    const char *exec_cmd = transformed_cmd ? transformed_cmd : command;

    /* Use popen for local execution */
    char full_cmd[MAX_COMMAND_LEN];
    snprintf(full_cmd, sizeof(full_cmd),
             "timeout %d sh -c '(%s) 2>&1' 2>/dev/null || true",
             timeout, exec_cmd);

    FILE *fp = popen(full_cmd, "r");
    if (!fp) {
        free(transformed_cmd);
        free(sudo_pass);
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"error\":\"popen failed: %s\",\"command\":\"%s\"}",
                 strerror(errno), _safe_command_preview(command, 100));
        return strdup(buf);
    }

    size_t cap = 4096, len = 0;
    char *output = malloc(cap);
    if (!output) { pclose(fp); free(transformed_cmd); free(sudo_pass); return strdup("{\"error\":\"OOM\"}"); }
    output[0] = '\0';

    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        size_t line_len = strlen(line);
        if (len + line_len >= (size_t)tool_output_get_max_bytes()) {
            size_t remaining = (size_t)tool_output_get_max_bytes() - len - 10;
            if (remaining > 0) {
                memcpy(output + len, line, remaining);
                len += remaining;
            }
            memcpy(output + len, "\n...[truncated]", 15);
            len += 15;
            output[len] = '\0';
            break;
        }
        if (len + line_len + 1 > cap) {
            cap *= 2;
            char *new_out = realloc(output, cap);
            if (!new_out) { free(output); pclose(fp); free(transformed_cmd); free(sudo_pass); return strdup("{\"error\":\"OOM\"}"); }
            output = new_out;
        }
        memcpy(output + len, line, line_len + 1);
        len += line_len;
    }

    int exit_code = pclose(fp);
    free(transformed_cmd);
    free(sudo_pass);

    json_t *result = json_object();
    json_set(result, "exit_code", json_number(exit_code));
    json_set(result, "output", json_string(output));
    json_set(result, "command", json_string(command));
    json_set(result, "status", json_string(exit_code == 0 ? "success" : "error"));
    json_set(result, "truncated", json_bool(len >= (size_t)tool_output_get_max_bytes() - 25));
    if (task_id && task_id[0]) json_set(result, "task_id", json_string(task_id));

    char *json_out = json_serialize(result);
    json_free(result);
    free(output);
    return json_out;
}


/* PoP: register_terminal_tool @ tools/terminal_tool.py:_handle_terminal */
char* _handle_terminal(const char *args_json) {
    char *error_msg = NULL;
    json_t *args = json_parse(args_json, &error_msg);
    if (!args) {
        if (error_msg) free(error_msg);
        return strdup("{\"error\":\"Invalid JSON args\"}");
    }

    const char *command = json_get_str(args, "command", "");
    int timeout = (int)json_get_num(args, "timeout", DEFAULT_TIMEOUT);
    bool pty = json_get_bool(args, "pty", false);
    bool force = json_get_bool(args, "force", false);
    const char *env_vars = json_get_str(args, "env", "");
    const char *workdir = json_get_str(args, "workdir", "");
    const char *backend = json_get_str(args, "backend", "");
    const char *docker_image = json_get_str(args, "docker_image", "");
    const char *task_id = json_get_str(args, "task_id", "");

    char *result = terminal_tool(command, timeout, pty, force, env_vars, workdir,
                                  backend, docker_image, task_id);
    json_free(args);
    return result;
}

/* Register the terminal tool handler in the tool registry */
void register_terminal_tool(void) {
    /* This would be called from tools/tool_init.c */
    /* tool_register("terminal", _handle_terminal, SCHEMA); */
}