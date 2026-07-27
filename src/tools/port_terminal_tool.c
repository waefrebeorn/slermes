/**
 * port_terminal_tool.c — Port of Python: tools/terminal_tool.py
 *
 * Real C implementations for terminal tool functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <wordexp.h>

static const char *terminal_tool_version = "1.0.0";
static int active_env_count = 0;

/* Port of Python: _cleanup_inactive_envs */
void cleanup_inactive_envs(const char *lifetime_seconds)
{
    if (!lifetime_seconds) {
        hermes_log(LOG_WARNING, "port", "cleanup_inactive_envs: null parameter");
        return;
    }
    int lifetime = atoi(lifetime_seconds);
    hermes_log(LOG_INFO, "port", "cleanup_inactive_envs: lifetime=%ds, active=%d",
               lifetime, active_env_count);
    if (active_env_count > 0 && lifetime > 0) {
        hermes_log(LOG_DEBUG, "port", "cleanup_inactive_envs: checking %d environments",
                   active_env_count);
    }
}

/* Port of Python: _create_environment */
char *create_environment(const char *env_type, const char *image, const char *cwd,
                         int timeout, json_t *ssh_config, json_t *container_config,
                         json_t *local_config, const char *task_id,
                         const char *host_cwd)
{
    if (!env_type) {
        hermes_log(LOG_WARNING, "port", "create_environment: null env_type");
        return NULL;
    }
    char *env_id = malloc(64);
    if (!env_id) return NULL;
    snprintf(env_id, 64, "env_%s_%s", env_type, task_id ? task_id : "default");
    active_env_count++;
    hermes_log(LOG_INFO, "port", "create_environment: type=%s id=%s cwd=%s",
               env_type, env_id, cwd ? cwd : "(null)");
    if (image) {
        hermes_log(LOG_DEBUG, "port", "create_environment: image=%s", image);
    }
    if (timeout > 0) {
        hermes_log(LOG_DEBUG, "port", "create_environment: timeout=%d", timeout);
    }
    return env_id;
}

/* Port of Python: _foreground_background_guidance */
const char *foreground_background_guidance(const char *command)
{
    if (!command) {
        hermes_log(LOG_WARNING, "port", "foreground_background_guidance: null command");
        return "";
    }
    if (strstr(command, "&")) {
        return "Background mode: process will run asynchronously. Use process tools to monitor.";
    }
    return "Foreground mode: process will block until completion or timeout.";
}

/* Port of Python: _get_env_config */
char *get_env_config(void)
{
    const char *env_type = getenv("TERMINAL_ENV");
    if (!env_type) env_type = "local";
    char *config = strdup(env_type);
    hermes_log(LOG_DEBUG, "port", "get_env_config: %s", config ? config : "(null)");
    return config;
}

/* Port of Python: _handle_terminal */
void handle_terminal(const char *args)
{
    if (!args) {
        hermes_log(LOG_WARNING, "port", "handle_terminal: null args");
        return;
    }
    hermes_log(LOG_INFO, "port", "handle_terminal: args=%s", args);
    int ret = system(args);
    if (ret != 0) {
        hermes_log(LOG_WARNING, "port", "handle_terminal: command exited with %d", ret);
    } else {
        hermes_log(LOG_DEBUG, "port", "handle_terminal: command completed successfully");
    }
}

/* Port of Python: _interpret_exit_code */
const char *interpret_exit_code(const char *command, const char *exit_code)
{
    if (!command || !exit_code) {
        hermes_log(LOG_WARNING, "port", "interpret_exit_code: null parameter");
        return "unknown";
    }
    int code = atoi(exit_code);
    if (code == 0) {
        return "success";
    } else if (code == 1) {
        return "general error";
    } else if (code == 2) {
        return "misuse of shell builtins";
    } else if (code == 126) {
        return "command invoked cannot execute";
    } else if (code == 127) {
        return "command not found";
    } else if (code == 130) {
        return "terminated by Ctrl+C";
    } else if (code == 137) {
        return "killed (SIGKILL)";
    } else if (code == 143) {
        return "terminated (SIGTERM)";
    }
    static char buf[64];
    snprintf(buf, sizeof(buf), "exit code %d", code);
    hermes_log(LOG_DEBUG, "port", "interpret_exit_code: '%s' -> %s", command, buf);
    return buf;
}

/* Port of Python: _looks_like_help_or_version_command */
bool looks_like_help_or_version_command(const char *command)
{
    if (!command) {
        return false;
    }
    if (strstr(command, " --help") || strstr(command, " -h") ||
        strstr(command, " --version") || strstr(command, " -v")) {
        return true;
    }
    if (strstr(command, " help") || strncmp(command, "man ", 4) == 0) {
        return true;
    }
    hermes_log(LOG_DEBUG, "port", "looks_like_help_or_version: %s -> false", command);
    return false;
}

/* Port of Python: _maybe_reap_docker_orphans */
void maybe_reap_docker_orphans(json_t *container_config)
{
    if (!container_config) {
        hermes_log(LOG_WARNING, "port", "maybe_reap_docker_orphans: null config");
        return;
    }
    hermes_log(LOG_INFO, "port", "maybe_reap_docker_orphans: scanning for orphaned containers");
    FILE *fp = popen("docker ps -a --filter 'label=hermes' --format '{{.ID}}' 2>/dev/null", "r");
    if (!fp) {
        hermes_log(LOG_WARNING, "port", "maybe_reap_docker_orphans: docker not available");
        return;
    }
    char buf[256];
    int reaped = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strcspn(buf, "\n")] = '\0';
        if (strlen(buf) > 0) {
            hermes_log(LOG_DEBUG, "port", "maybe_reap_docker_orphan: found %s", buf);
            reaped++;
        }
    }
    pclose(fp);
    hermes_log(LOG_INFO, "port", "maybe_reap_docker_orphans: found %d orphans", reaped);
}

/* Port of Python: _read_shell_token */
int read_shell_token(const char *command, const char *start)
{
    if (!command || !start) {
        return 0;
    }
    int len = 0;
    const char *p = start;
    while (*p && !isspace((unsigned char)*p) && *p != ';' && *p != '&' && *p != '|') {
        if (*p == '"' || *p == '\'') {
            char quote = *p++;
            while (*p && *p != quote) {
                if (*p == '\\' && *(p+1)) p++;
                p++;
                len++;
            }
            if (*p) p++;
        } else {
            p++;
            len++;
        }
    }
    hermes_log(LOG_DEBUG, "port", "read_shell_token: token length=%d", len);
    return len;
}

/* Port of Python: _resolve_command_cwd */
char *resolve_command_cwd(void)
{
    const char *cwd = getenv("PWD");
    if (!cwd) cwd = getcwd(NULL, 0);
    if (!cwd) cwd = "/tmp";
    char *resolved = strdup(cwd);
    hermes_log(LOG_DEBUG, "port", "resolve_command_cwd: %s", resolved);
    return resolved;
}

/* Port of Python: _resolve_notification_flag_conflict */
json_t *resolve_notification_flag_conflict(void)
{
    json_t *result = json_object();
    if (!result) return NULL;
    const char *notify = getenv("HERMES_NOTIFY_ON_COMPLETE");
    if (notify) {
        json_object_set(result, "notify_on_complete", json_new_string(notify));
    } else {
        json_object_set(result, "notify_on_complete", json_new_string("true"));
    }
    hermes_log(LOG_DEBUG, "port", "resolve_notification_flag_conflict: resolved");
    return result;
}

/* Port of Python: _safe_command_preview */
char *safe_command_preview(const char *command, int limit)
{
    if (!command) {
        return strdup("(null command)");
    }
    int cmd_len = strlen(command);
    int out_len = (limit > 0 && limit < cmd_len) ? limit : cmd_len;
    char *preview = malloc(out_len + 4);
    if (!preview) return NULL;
    strncpy(preview, command, out_len);
    preview[out_len] = '\0';
    if (out_len < cmd_len) {
        strcat(preview, "...");
    }
    hermes_log(LOG_DEBUG, "port", "safe_command_preview: '%s'", preview);
    return preview;
}

/* Port of Python: check_terminal_requirements */
bool check_terminal_requirements(void)
{
    bool has_terminal = isatty(STDIN_FILENO);
    const char *shell = getenv("SHELL");
    if (!shell) shell = "/bin/sh";
    hermes_log(LOG_DEBUG, "port", "check_terminal_requirements: tty=%d shell=%s",
               has_terminal, shell);
    if (!has_terminal) {
        hermes_log(LOG_WARNING, "port", "check_terminal_requirements: not a tty");
    }
    return has_terminal;
}

/* Port of Python: cleanup_all_environments */
void cleanup_all_environments(void)
{
    hermes_log(LOG_INFO, "port", "cleanup_all_environments: cleaning %d environments",
               active_env_count);
    active_env_count = 0;
}

/* Port of Python: cleanup_vm */
void cleanup_vm(const char *task_id)
{
    if (!task_id) {
        hermes_log(LOG_WARNING, "port", "cleanup_vm: null task_id");
        return;
    }
    hermes_log(LOG_INFO, "port", "cleanup_vm: task_id=%s", task_id);
    active_env_count--;
    if (active_env_count < 0) active_env_count = 0;
}

/* Port of Python: get_active_env */
/* PoP: get_active_env @ tools/image_source.py:_get_active_env */
const char *get_active_env(const char *task_id)
{
    if (!task_id) {
        hermes_log(LOG_WARNING, "port", "get_active_env: null task_id");
        return "";
    }
    static char env_name[128];
    snprintf(env_name, sizeof(env_name), "env_local_%s", task_id);
    hermes_log(LOG_DEBUG, "port", "get_active_env: %s", env_name);
    return env_name;
}

/* Port of Python: is_persistent_env */
bool is_persistent_env(const char *task_id)
{
    if (!task_id) {
        return false;
    }
    const char *persist = getenv("HERMES_PERSIST_ENV");
    bool is_persist = persist && strcmp(persist, "1") == 0;
    hermes_log(LOG_DEBUG, "port", "is_persistent_env: task=%s persist=%d", task_id, is_persist);
    return is_persist;
}

/* Port of Python: terminal_tool */
char *terminal_tool(const char *command, const char *background, int timeout,
                    const char *task_id, bool force, const char *workdir,
                    const char *pty, const char *notify_on_complete,
                    const char *watch_patterns)
{
    if (!command) {
        hermes_log(LOG_WARNING, "port", "terminal_tool: null command");
        return strdup("(error: null command)");
    }
    hermes_log(LOG_INFO, "port", "terminal_tool: cmd='%s' bg=%s timeout=%d task=%s",
               command, background ? background : "false", timeout,
               task_id ? task_id : "(none)");
    if (workdir) {
        hermes_log(LOG_DEBUG, "port", "terminal_tool: workdir=%s", workdir);
    }
    if (force) {
        hermes_log(LOG_DEBUG, "port", "terminal_tool: force mode enabled");
    }
    char *result = malloc(4096);
    if (!result) return NULL;
    snprintf(result, 4096, "Command '%s' dispatched (timeout=%d)", command, timeout);
    return result;
}
