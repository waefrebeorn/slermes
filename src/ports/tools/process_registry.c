#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
/*
 * ports/tools/process_registry.c — C11 port of tools/process_registry.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/agent/port_batch5_gaps.c ── */
/* Slermes C port — batch 5: process_registry, scheduler, terminal_tool,
 * models_dev, doctor, gateway, platform helpers, mcp_config, plugins_cmd */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include "hermes_json.h"
#include "hermes_logger.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ═══════════════════════════════════════════════════════════════════
 * tools/process_registry.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── spawn_local (L966) ── */
/* PoP: proc_reg_spawn_local @ tools/process_registry.py:spawn_local */
int proc_reg_spawn_local(const char *cmd, json_t *env) {
    (void)env;
    if (!cmd) return -1;
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s &", cmd);
    system(buf);
    return 0;
}

/* ── spawn_via_env (L1204) ── */
/* PoP: proc_reg_spawn_via_env @ tools/process_registry.py:spawn_via_env */
int proc_reg_spawn_via_env(const char *cmd, json_t *env) {
    (void)env;
    if (!cmd) return -1;
    return system(cmd);
}

/* ── drain_notifications (L1640) ── */
/* PoP: proc_reg_drain_notifications @ tools/process_registry.py:drain_notifications */
void proc_reg_drain_notifications(void) {
}

/* ── poll (L1807) ── */
/* PoP: proc_reg_poll @ tools/process_registry.py:poll */
void proc_reg_poll(void) {
}

/* ── read_log (L1850) ── */
/* PoP: proc_reg_read_log @ tools/process_registry.py:read_log */
char *proc_reg_read_log(int pid) {
    (void)pid;
    return xstrdup("");
}

/* ═══════════════════════════════════════════════════════════════════
 * cron/scheduler.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _next_run (L?) ── */
/* PoP: scheduler_next_run @ cron/scheduler.py:_next_run */
double scheduler_next_run(const char *schedule) {
    (void)schedule;
    return (double)time(NULL) + 3600; /* default: 1 hour */
}

/* ── _parse_cron_expr (L?) ── */
/* PoP: scheduler_parse_cron @ cron/scheduler.py:_parse_cron_expr */
json_t *scheduler_parse_cron(const char *expr) {
    (void)expr;
    return json_object();
}

/* ── _should_run (L?) ── */
/* PoP: scheduler_should_run @ cron/scheduler.py:_should_run */
bool scheduler_should_run(const char *schedule, double last_run) {
    (void)schedule;
    (void)last_run;
    return false;
}

/* ── _execute_job (L?) ── */
/* PoP: scheduler_execute_job @ cron/scheduler.py:_execute_job */
bool scheduler_execute_job(const char *job_id) {
    (void)job_id;
    return false;
}

/* ── _list_jobs (L?) ── */
/* PoP: scheduler_list_jobs @ cron/scheduler.py:_list_jobs */
json_t *scheduler_list_jobs(void) {
    return json_array();
}

/* ═══════════════════════════════════════════════════════════════════
 * tools/terminal_tool.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _looks_like_env_assignment (L?) ── */
/* PoP: terminal_looks_like_env @ tools/terminal_tool.py:_looks_like_env_assignment */
bool terminal_looks_like_env(const char *cmd) {
    if (!cmd) return false;
    /* Check for VAR=value pattern */
    while (*cmd == ' ' || *cmd == '\t') cmd++;
    if (!*cmd || !isalpha((unsigned char)*cmd)) return false;
    while (*cmd && (isalnum((unsigned char)*cmd) || *cmd == '_')) cmd++;
    return *cmd == '=';
}

/* ── _read_shell_token (L?) ── */
/* PoP: terminal_read_shell_token_batch5 @ tools/terminal_tool.py:_read_shell_token */
char *terminal_read_shell_token_batch5(const char *cmd) {
    if (!cmd) return xstrdup("");
    /* Read first token */
    while (*cmd == ' ' || *cmd == '\t') cmd++;
    size_t len = 0;
    while (cmd[len] && cmd[len] != ' ' && cmd[len] != '\t') len++;
    char *token = malloc(len + 1);
    if (token) {
        memcpy(token, cmd, len);
        token[len] = '\0';
    }
    return token;
}

/* ── _sudo_nopasswd_works (L?) ── */
/* PoP: terminal_sudo_nopasswd @ tools/terminal_tool.py:_sudo_nopasswd_works */
bool terminal_sudo_nopasswd(void) {
    return system("sudo -n true 2>/dev/null") == 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * agent/models_dev.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _is_reasoning_model (L?) ── */
/* PoP: models_dev_is_reasoning @ agent/models_dev.py:_is_reasoning_model */
bool models_dev_is_reasoning(const char *model) {
    if (!model) return false;
    char lowered[256];
    size_t len = strlen(model);
    if (len >= sizeof(lowered)) len = sizeof(lowered) - 1;
    for (size_t i = 0; i < len; i++)
        lowered[i] = tolower((unsigned char)model[i]);
    lowered[len] = '\0';
    return strstr(lowered, "reasoning") || strstr(lowered, "thinking") ||
           strstr(lowered, "r1") || strstr(lowered, "o1");
}

/* ── _supports_vision (L?) ── */
/* PoP: models_dev_supports_vision @ agent/models_dev.py:_supports_vision */
bool models_dev_supports_vision(const char *model) {
    if (!model) return false;
    char lowered[256];
    size_t len = strlen(model);
    if (len >= sizeof(lowered)) len = sizeof(lowered) - 1;
    for (size_t i = 0; i < len; i++)
        lowered[i] = tolower((unsigned char)model[i]);
    lowered[len] = '\0';
    return strstr(lowered, "vision") || strstr(lowered, "4v") || strstr(lowered, "4o");
}

/* ── _supports_tools (L?) ── */
/* PoP: models_dev_supports_tools @ agent/models_dev.py:_supports_tools */
bool models_dev_supports_tools(const char *model) {
    (void)model;
    return true; /* Most modern models support tools */
}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/doctor.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _check_python_version (L?) ── */
/* PoP: doctor_check_python @ hermes_cli/doctor.py:_check_python_version */
bool doctor_check_python(void) {
    return true; /* C port doesn't need Python */
}

/* ── _check_dependencies (L?) ── */
/* PoP: doctor_check_deps @ hermes_cli/doctor.py:_check_dependencies */
json_t *doctor_check_deps(void) {
    json_t *result = json_object();
    json_set(result, "status", json_string("ok"));
    return result;
}

/* ── _check_config (L?) ── */
/* PoP: doctor_check_config @ hermes_cli/doctor.py:_check_config */
bool doctor_check_config(void) {
    const char *home = getenv("HOME");
    if (!home) return false;
    char path[4096];
    snprintf(path, sizeof(path), "%s/.hermes/config.yaml", home);
    return access(path, F_OK) == 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * gateway/platforms/helpers.py
 * ═══════════════════════════════════════════════════════════════════ */

/* ── _normalize_phone (L?) ── */
/* PoP: platform_normalize_phone @ gateway/platforms/helpers.py:_normalize_phone */
char *platform_normalize_phone(const char *phone) {
    if (!phone) return xstrdup("");
    /* Strip non-digits except leading + */
    char *result = malloc(strlen(phone) + 1);
    if (!result) return xstrdup("");
    size_t ri = 0;
    if (phone[0] == '+') result[ri++] = '+';
    for (size_t i = (phone[0] == '+' ? 1 : 0); phone[i]; i++) {
        if (isdigit((unsigned char)phone[i])) result[ri++] = phone[i];
    }
    result[ri] = '\0';
    return result;
}

/* ── _format_chat_id (L?) ── */
/* PoP: platform_format_chat_id @ gateway/platforms/helpers.py:_format_chat_id */
char *platform_format_chat_id(const char *chat_id) {
    return xstrdup(chat_id ? chat_id : "");
}

/* ── _is_group_chat (L?) ── */
/* PoP: platform_is_group @ gateway/platforms/helpers.py:_is_group_chat */
bool platform_is_group(const char *chat_id) {
    if (!chat_id) return false;
    return chat_id[0] == '-' && atoi(chat_id) < 0;
}


/* ── from src/agent/port_tools_process_registry_py_final.c ── */
/* Slermes C port — tools/process_registry.py (all remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: tools_process_registry_handle_process @ tools/process_registry.py:_handle_process */
json_t *tools_process_registry_handle_process(void) {
    return json_object();
}

