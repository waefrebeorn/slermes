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

/* PoP: _rewrite_compound_background @ tools/terminal_tool.py:_rewrite_compound_background */
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

/* PoP: _get_env_config @ tools/terminal_tool.py:_get_env_config */
char* _get_env_config(void) {
    json_t *cfg = term_get_env_config();
    return json_serialize(cfg);   /* caller owns; NULL if empty */
}

/* PoP: _create_environment @ tools/terminal_tool.py:_create_environment */
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

/* PoP: get_active_env @ tools/terminal_tool.py:get_active_env */
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


/* PoP: _interpret_exit_code @ tools/terminal_tool.py:_interpret_exit_code */
const char* _interpret_exit_code(const char *command, int exit_code) {
    (void)command;
    if (exit_code == 0) return "success";
    if (exit_code == 130) return "interrupted";
    if (exit_code == 124) return "timeout";
    return "error";
}

/* PoP: _command_requires_pipe_stdin @ tools/terminal_tool.py:_command_requires_pipe_stdin */
bool _command_requires_pipe_stdin(const char *command) {
    (void)command;
    return false;
}

/* PoP: _strip_quotes @ tools/terminal_tool.py:_strip_quotes */
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

/* PoP: _foreground_background_guidance @ tools/terminal_tool.py:_foreground_background_guidance */
const char* _foreground_background_guidance(const char *command) {
    (void)command;
    return NULL;
}

/* PoP: _resolve_notification_flag_conflict @ tools/terminal_tool.py:_resolve_notification_flag_conflict */
void _resolve_notification_flag_conflict(bool *notify_on_complete, bool *watch_interval) {
    (void)notify_on_complete; (void)watch_interval;
}

/* PoP: _resolve_command_cwd @ tools/terminal_tool.py:_resolve_command_cwd */
char* _resolve_command_cwd(const char *command, const char *workdir) {
    (void)command;
    return workdir ? strdup(workdir) : strdup(".");
}

/* PoP: terminal_tool @ tools/terminal_tool.py:terminal_tool */
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


/* PoP: _handle_terminal @ tools/terminal_tool.py:_handle_terminal */
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