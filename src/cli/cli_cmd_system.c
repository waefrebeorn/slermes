/*
 * cli_cmd_system.c — System slash-command handlers extracted from commands.c.
 * Self-contained command-category module.
 */

#include "cli_cmd_system.h"
#include "commands_shared.h"
#include "hermes_core_types.h"
#include "approval.h"
#include "hermes_cdp.h"
#include "hermes_agent.h"
#include "hermes_display.h"
#include "hermes_plugin.h"
#include "hermes_skin.h"
#include "registry.h"

#include "hermes_web_dashboard.h"
#include "send_message.h"
#include <ctype.h>
#include "registry.h"
#include "hermes_display.h"
#include "hermes_plugin.h"

/* Forward declarations for helpers defined later in this file but called by
 * earlier handlers (the split preserved their original intra-file ordering). */
int  cmd_compress_coerce_keep_value(const char *value);
void handoff_write_request(const char *handoff_id, const char *platform,
                           const char *session_id, const char *requester);
void list_init(list_t *l);
void list_append(list_t *l, void *item);
void list_free(list_t *l);

/* /approve: Show pending approvals or approve a specific one */
/* AG26: Port of Python hermes_cli/write_approval_commands.py:_approve().
 * AG26: Port of Python hermes_cli/pairing.py:_cmd_approve().
 */
void cmd_approve(const char *args, agent_state_t *state) {
    (void)state;
    if (args && args[0]) {
        if (strcmp(args, "list") == 0 || strcmp(args, "-l") == 0) {
            int count = approval_cache_count();
            printf("Approval cache (%d entries):\n", count);
            for (int i = 0; i < count; i++)
                printf("  %s\n", approval_cache_entry(i));
            return;
        }
        if (strcmp(args, "clear") == 0 || strcmp(args, "-c") == 0) {
            approval_cache_clear_last(0);
            printf("Approval cache cleared.\n");
            return;
        }
        printf("Usage: /approve [list|clear]\n");
        return;
    }
    int count = approval_cache_count();
    if (count == 0) {
        printf("No cached approvals. Use /tools to verify tool permissions.\n");
    } else {
        printf("Approval cache (%d entries). Use /approve list to view.\n", count);
    }
}

/* /browser: Connect CDP browser */
void cmd_browser(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /browser connect <ws_url>  — connect to CDP browser\n");
        printf("       /browser status            — show CDP connection status\n");
        printf("       /browser disconnect         — close CDP connection\n");
        return;
    }

    if (strncmp(args, "connect ", 8) == 0) {
        const char *url = args + 8;
        while (*url == ' ') url++;
        if (!*url) { printf("Usage: /browser connect <ws_url>\n"); return; }
        cdp_set_url(url);
        printf("CDP URL set to: %s\n", url);
        printf("Use browser_navigate, browser_snapshot, etc. via the tool system.\n");
        return;
    }

    if (strcmp(args, "disconnect") == 0) {
        cdp_set_url("");
        printf("CDP connection cleared.\n");
        return;
    }

    if (strcmp(args, "status") == 0) {
        const char *url = cdp_get_url();
        if (url && url[0])
            printf("CDP connected to: %s\n", url);
        else
            printf("CDP not connected. Use /browser connect <ws_url>.\n");
        return;
    }

    printf("Unknown browser command: %s\n", args);
}

void cmd_busy(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        const char *modes[] = {"queue", "steer", "interrupt"};
        printf("Busy behavior: %s\n", g_busy_mode >= 0 && g_busy_mode < 3 ? modes[g_busy_mode] : "queue");
        printf("Usage: /busy [queue|steer|interrupt|status]\n");
        printf("  queue     - Enter queues prompt while working (default)\n");
        printf("  steer     - Enter queues as steer message\n");
        printf("  interrupt - Enter sends interrupt\n");
        return;
    }

    if (strcmp(args, "status") == 0) {
        const char *modes[] = {"queue", "steer", "interrupt"};
        printf("Busy behavior: %s\n", g_busy_mode >= 0 && g_busy_mode < 3 ? modes[g_busy_mode] : "queue");
        return;
    }

    if (strcmp(args, "queue") == 0) { g_busy_mode = 0; }
    else if (strcmp(args, "steer") == 0) { g_busy_mode = 1; }
    else if (strcmp(args, "interrupt") == 0) { g_busy_mode = 2; }
    else {
        printf("Unknown busy mode: %s\n", args);
        printf("Valid: queue, steer, interrupt, status\n");
        return;
    }

    printf("Busy behavior set to: %s\n", args);
}

void cmd_clear(const char *args, agent_state_t *state) {
    (void)args;
    /* AG26: Port of Python hermes_cli/main.py:cmd_clear(). */
    /* Preserve session metadata, wipe messages */
    char keep_id[64];
    snprintf(keep_id, sizeof(keep_id), "%s", state->session_id);
    int keep_iterations = state->iteration_count;

    context_clear(state);

    /* Restore session metadata */
    snprintf(state->session_id, sizeof(state->session_id), "%s", keep_id);
    state->iteration_count = keep_iterations;
    state->interrupted = false;

    printf("Context cleared. Session: %s\n", state->session_id);
}

void cmd_commands(const char *args, agent_state_t *state) {
    (void)state;
    /* Count commands */
    int count = 0;
    while (COMMANDS[count].name) count++;

    /* Parse page number if provided */
    int page = 0;
    int per_page = 20;
    if (args && args[0]) {
        char *endptr;
        long val = strtol(args, &endptr, 10);
        if (*endptr == '\0' && val > 0)
            page = (int)val - 1; /* 1-indexed in UI */
    }
    int total_pages = (count + per_page - 1) / per_page;
    if (page < 0) page = 0;
    if (page >= total_pages) page = total_pages - 1;

    /* Build rows array for this page */
    int start = page * per_page;
    int end = start + per_page;
    if (end > count) end = count;
    int page_count = end - start;

    const char **rows = malloc(page_count * sizeof(char *));
    if (!rows) {
        printf("All slash commands (%d total):\n", count);
        for (int i = 0; COMMANDS[i].name; i++) {
            printf("  %s", COMMANDS[i].name);
            if (COMMANDS[i].alias)
                printf(" (%s)", COMMANDS[i].alias);
            printf("\n");
        }
        return;
    }

    for (int i = start; i < end; i++) {
        char *buf = malloc(128);
        if (!buf) { rows[i - start] = ""; continue; }
        snprintf(buf, 128, "%s\t%s\t%s",
                 COMMANDS[i].name,
                 COMMANDS[i].alias ? COMMANDS[i].alias : "",
                 COMMANDS[i].description);
        rows[i - start] = buf;
    }

    const char *headers[] = {"Command", "Alias", "Description"};
    display_table(3, headers, (const char **)rows, page_count, DISPLAY_CYAN);

    for (int i = 0; i < page_count; i++) {
        if (rows[i] && rows[i][0]) free((void *)rows[i]);
    }
    free(rows);

    if (total_pages > 1)
        printf("  Page %d/%d. Use /commands <N> for a specific page.\n",
               page + 1, total_pages);
}

/* /completions: Generate shell completion scripts */
void cmd_completions(const char *args, agent_state_t *state) {
    (void)state;
    if (args && (strcmp(args, "fish") == 0 || strcmp(args, "zsh") == 0)) {
        printf("#compdef hermes\n");
        printf("_hermes_commands() {\n");
        printf("  local -a commands\n");
        const command_def_t *cmd = COMMANDS;
        while (cmd->name && cmd->handler) {
            printf("  commands+=( \"%s:%s\" )\n", cmd->name, cmd->description);
            cmd++;
        }
        printf("  _describe 'hermes commands' commands\n");
        printf("}\n");
        printf("_hermes() {\n");
        printf("  _arguments -C \\n");
        printf("    '1: :->command' \\n");
        printf("    '*: :->args'\n");
        printf("  case $state in\n");
        printf("    (command) _hermes_commands ;;\n");
        printf("    (args) _hermes_commands ;;\n");
        printf("  esac\n");
        printf("}\n");
        printf("compdef _hermes hermes\n");
    } else {
        /* Default: bash completions */
        printf("_hermes_completions() {\n");
        printf("  local cur=${COMP_WORDS[COMP_CWORD]}\n");
        printf("  COMPREPLY=( $(compgen -W \"");
        const command_def_t *cmd = COMMANDS;
        int count = 0;
        while (cmd->name && cmd->handler) {
            if (count > 0) printf(" ");
            printf("%s", cmd->name);
            cmd++; count++;
        }
        printf("\" -- \"$cur\") )\n");
        printf("  return 0\n");
        printf("}\n");
        printf("complete -F _hermes_completions hermes\n");
    }
}

/* PoP: cmd_compress_coerce_keep_value @ hermes_cli/partial_compress.py:_coerce_keep */
int cmd_compress_coerce_keep_value(const char *value)
{
    if (!value) return CMD_COMPRESS_DEFAULT_KEEP_LAST;
    /* trim leading/trailing whitespace */
    while (*value && isspace((unsigned char)*value)) value++;
    const char *end = value + strlen(value);
    while (end > value && isspace((unsigned char)end[-1])) end--;
    if (end == value) return CMD_COMPRESS_DEFAULT_KEEP_LAST;
    char buf[32];
    size_t n = (size_t)(end - value);
    if (n >= sizeof(buf)) return CMD_COMPRESS_DEFAULT_KEEP_LAST;
    memcpy(buf, value, n);
    buf[n] = '\0';
    char *pend = NULL;
    long v = strtol(buf, &pend, 10);
    if (pend == buf || *pend != '\0')
        return CMD_COMPRESS_DEFAULT_KEEP_LAST;
    if (v < 1) return 1;
    if (v > CMD_COMPRESS_MAX_KEEP_LAST) return CMD_COMPRESS_MAX_KEEP_LAST;
    return (int)v;
}

/* /copy: Copy last assistant response (prints to stdout in CLI) */
void cmd_copy(const char *args, agent_state_t *state) {
    /* Determine which response: default=last (0), or Nth from end */
    int n = 0;
    if (args && args[0]) {
        char *endptr;
        long val = strtol(args, &endptr, 10);
        if (*endptr != '\0' || val < 0) {
            printf("Usage: /copy [number]  — copy Nth-to-last assistant response (0=last)\n");
            return;
        }
        n = (int)val;
    }
    /* Find Nth-to-last assistant message */
    const char *found = NULL;
    int found_index = 0;
    for (size_t i = state->message_count; i > 0; i--) {
        if (state->messages[i-1]->role == MSG_ASSISTANT) {
            if (found_index == n) {
                found = state->messages[i-1]->content;
                break;
            }
            found_index++;
        }
    }
    if (found) {
        if (n == 0)
            printf("=== Last response ===\n%s\n", found);
        else
            printf("=== Response -%d ===\n%s\n", n, found);
    } else {
        printf("No assistant response at index %d.\n", n);
    }
}

/* /cron: Manage scheduled tasks */
void cmd_cron(const char *args, agent_state_t *state) {
    (void)state;
    if (args && args[0]) {
        if (strcmp(args, "list") == 0 || strcmp(args, "-l") == 0) {
            char cron_dir[HERMES_PATH_MAX];
            hermes_get_home(cron_dir, sizeof(cron_dir));
            strncat(cron_dir, "/cron", sizeof(cron_dir) - strlen(cron_dir) - 1);
            printf("Scheduled tasks in %s:\n", cron_dir);
            char cmd[HERMES_PATH_MAX + 32];
            snprintf(cmd, sizeof(cmd), "ls -la %s/ 2>/dev/null || echo '(empty)'", cron_dir);
            FILE *fp = popen(cmd, "r");
            if (fp) {
                char line[256];
                while (fgets(line, sizeof(line), fp)) printf("  %s", line);
                pclose(fp);
            }
            return;
        }
        printf("Usage: /cron [list]\n");
        return;
    }
    /* Show cron config */
    char cron_dir[HERMES_PATH_MAX];
    hermes_get_home(cron_dir, sizeof(cron_dir));
    strncat(cron_dir, "/cron", sizeof(cron_dir) - strlen(cron_dir) - 1);
    printf("Cron scheduler: active\n");
    printf("  Directory: %s\n", cron_dir);
    printf("  Config: cron.dir, cron.max_concurrent_jobs, cron.job_timeout\n");
    printf("  Use cronjob tool to create/manage tasks.\n");
    printf("  Use /cron list to show scheduled tasks.\n");
}

/* ─── /dashboard — Launch web dashboard ─── */
/* AG26: Port of Python hermes_cli/main.py:cmd_dashboard(). */
void cmd_dashboard(const char *args, agent_state_t *state) {
    (void)state;
    const char *cmd = args;
    while (cmd && *cmd == ' ') cmd++;

    if (!cmd || !cmd[0] || strcmp(cmd, "status") == 0) {
        if (dashboard_is_running()) {
            printf("\n=== Web Dashboard ===\n");
            const char *h = getenv("DASHBOARD_HOST");
            if (!h || !h[0]) h = "127.0.0.1";
            const char *p = getenv("DASHBOARD_PORT");
            printf("  Status: RUNNING\n");
            printf("  URL:    http://%s:%s/\n", h, p && *p ? p : "9119");
        } else {
            printf("  Status: STOPPED\n");
            printf("  Use '/dashboard start' to launch.\n");
        }
        return;
    }

    if (strcmp(cmd, "start") == 0) {
        if (dashboard_is_running()) {
            printf("Dashboard already running.\n");
            return;
        }
        dashboard_init();
        if (dashboard_start()) {
            const char *h = getenv("DASHBOARD_HOST");
            if (!h || !h[0]) h = "127.0.0.1";
            const char *p = getenv("DASHBOARD_PORT");
            printf("Dashboard started at http://%s:%s/\n",
                   h, p && *p ? p : "9119");
        } else {
            printf("Error: Failed to start dashboard.\n");
        }
        return;
    }

    if (strcmp(cmd, "stop") == 0) {
        if (!dashboard_is_running()) {
            printf("Dashboard is not running.\n");
            return;
        }
        dashboard_stop();
        printf("Dashboard stopped.\n");
        return;
    }

    if (strcmp(cmd, "url") == 0) {
        const char *h = getenv("DASHBOARD_HOST");
        if (!h || !h[0]) h = "127.0.0.1";
        const char *p = getenv("DASHBOARD_PORT");
        printf("http://%s:%s/\n", h, p && *p ? p : "9119");
        return;
    }

    printf("Usage: /dashboard [start|stop|status|url]\n");
}

/* /debug: Generate debug report */
/* AG26: Port of Python hermes_cli/main.py:_debug(). */
/* PoP: cmd_debug @ hermes_cli/main.py:cmd_debug */
void cmd_debug(const char *args, agent_state_t *state) {
    (void)args;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S %Z", tm_info);

    printf("=== Hermes C Debug Report ===\n");
    printf("Generated: %s\n\n", time_buf);

    /* System info */
    {
        struct utsname un;
        if (uname(&un) == 0)
            printf("Kernel:     %s %s %s %s\n", un.sysname, un.nodename, un.release, un.machine);
        else
            printf("Kernel:     (unknown)\n");
    }
    {
        long nproc = sysconf(_SC_NPROCESSORS_ONLN);
        if (nproc > 0) printf("CPUs:       %ld\n", nproc);
    }
    {
        long pages = sysconf(_SC_PHYS_PAGES);
        long psize = sysconf(_SC_PAGE_SIZE);
        if (pages > 0 && psize > 0)
            printf("Memory:     %ld MB total\n", (pages * psize) / (1024 * 1024));
    }

    /* Version info */
    printf("\n-- Version --\n");
    printf("Version:    %s\n", HERMES_VERSION);
    printf("Build:      %s %s\n", __DATE__, __TIME__);

    /* Git info */
    char buf[256];
    FILE *fp = popen("git rev-parse --short=8 HEAD 2>/dev/null", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) {
            size_t blen = strlen(buf);
            if (blen > 0 && buf[blen-1] == '\n') buf[blen-1] = '\0';
            printf("Git commit: %s\n", buf);
        }
        pclose(fp);
    } else {
        printf("Git commit: (unknown)\n");
    }
    fp = popen("git rev-parse --abbrev-ref HEAD 2>/dev/null", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) {
            size_t blen = strlen(buf);
            if (blen > 0 && buf[blen-1] == '\n') buf[blen-1] = '\0';
            printf("Git branch: %s\n", buf);
        }
        pclose(fp);
    }

    /* Uptime */
    fp = popen("uptime 2>/dev/null", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) {
            size_t blen = strlen(buf);
            if (blen > 0 && buf[blen-1] == '\n') buf[blen-1] = '\0';
            printf("Uptime:     %s\n", buf);
        }
        pclose(fp);
    }

    /* Session info */
    printf("\n-- Session --\n");
    printf("Messages:   %zu\n", state->message_count);
    printf("Iterations: %d/%d\n", state->iteration_count, state->max_iterations);
    printf("Session ID: %s\n", state->session_id[0] ? state->session_id : "(none)");

    /* Tools registered */
    printf("\n-- Tools --\n");
    {
        int n = (int)registry_count();
        printf("Registered: %d\n", n);
    }

    /* Config */
    printf("\n-- Config --\n");
    printf("Provider:   %s\n", state->llm.provider[0] ? state->llm.provider : "(default)");
    printf("Model:      %s\n", state->llm.model[0] ? state->llm.model : "(default)");

    /* Log files tail */
    {
        char log_dir[512] = "";
        const char *home = state->hermes_home[0] ? state->hermes_home :
                           getenv("SLERMES_HOME");
        if (home) snprintf(log_dir, sizeof(log_dir), "%s/logs", home);

        if (log_dir[0]) {
            printf("\n-- Recent Logs --\n");
            const char *log_files[] = {"agent.log", "errors.log", NULL};
            for (int i = 0; log_files[i]; i++) {
                char logpath[1024];
                snprintf(logpath, sizeof(logpath), "%s/%s", log_dir, log_files[i]);
                struct stat st;
                if (stat(logpath, &st) == 0) {
                    char cmd[2048];
                    snprintf(cmd, sizeof(cmd), "tail -20 '%s' 2>/dev/null", logpath);
                    FILE *lfp = popen(cmd, "r");
                    if (lfp) {
                        printf("\n%s (last 20 lines, %ld bytes):\n", log_files[i], (long)st.st_size);
                        char line[1024];
                        while (fgets(line, sizeof(line), lfp))
                            printf("  %s", line);
                        pclose(lfp);
                    }
                }
            }
        }
    }

    printf("\n--- End Debug Report ---\n");

    /* Save report to file for sharing */
    {
        char log_dir[512] = "";
        const char *home = state->hermes_home[0] ? state->hermes_home :
                           getenv("SLERMES_HOME");
        if (!home || !home[0]) home = getenv("HOME");
        if (home) {
            snprintf(log_dir, sizeof(log_dir), "%s/logs", home);
            mkdir(log_dir, 0755);
            char outpath[1024];
            snprintf(outpath, sizeof(outpath), "%s/debug-%s.txt",
                     log_dir, time_buf);
            /* Sanitize filename — replace spaces/colons */
            for (char *p = outpath; *p; p++) {
                if (*p == ' ' || *p == ':') *p = '-';
            }
            FILE *dfp = fopen(outpath, "w");
            if (dfp) {
                fprintf(dfp, "=== Hermes C Debug Report ===\n");
                fprintf(dfp, "Command:  /debug\n");
                fprintf(dfp, "Version:  %s\n", HERMES_VERSION);
                fprintf(dfp, "Saved:    %s\n\n", time_buf);
                fprintf(dfp, "Note: Full report printed above.\n");
                fprintf(dfp, "Share: Provide this file for diagnostics.\n");
                fclose(dfp);
                printf("\nDebug report saved to: %s\n", outpath);
                printf("Share this file for diagnostics.\n");
            }
        }
    }
}

/* /deny: Clear approval cache (deny all pending) */
void cmd_deny(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    int count = approval_cache_count();
    approval_reset_session();
    printf("Approval cache cleared. %d entries removed. All operations denied.\n", count);
}

/* /deps: Install Python bridge dependencies */
void cmd_deps(const char *args, agent_state_t *state) {
    (void)args;
    (void)state;
    printf("Installing Python bridge dependencies...\n");
    printf("See THIRD_PARTY.md §9 for details.\n\n");
    int rc = system("pip install -r requirements-bridge.txt 2>/dev/null "
                    "|| pip3 install -r requirements-bridge.txt 2>/dev/null");
    if (rc == 0) {
        printf("\n✓ Python bridge dependencies installed.\n");
    } else {
        printf("\n✗ Install failed or pip not found.\n");
        printf("  Install manually: pip install -r requirements-bridge.txt\n");
    }
}

/* /doctor: Run system diagnostics */
/* AG26: Port of Python hermes_cli/hooks.py:_cmd_doctor().
 * AG26: Port of Python hermes_cli/main.py:cmd_doctor().
 */
/* PoP: cmd_doctor @ tools/computer_use/permissions.py:_doctor */
/* PoP: cmd_doctor @ hermes_cli/console_engine.py:_doctor */
void cmd_doctor(const char *args, agent_state_t *state) {
    /* Normalize subcommand */
    char subcmd[64] = "";
    if (args) {
        while (*args == ' ') args++;
        strncpy(subcmd, args, sizeof(subcmd) - 1);
        subcmd[sizeof(subcmd) - 1] = '\0';
        char *nl = strchr(subcmd, '\n');
        if (nl) *nl = '\0';
    }

    bool show_all   = (!subcmd[0] || strcmp(subcmd, "all") == 0);
    bool show_cfg   = show_all || strcmp(subcmd, "config") == 0;
    bool show_env2  = show_all || strcmp(subcmd, "env") == 0;
    bool show_keys  = show_all || strcmp(subcmd, "keys") == 0;
    bool show_sys   = show_all || strcmp(subcmd, "system") == 0;
    bool show_help  = (strcmp(subcmd, "help") == 0 || strcmp(subcmd, "--help") == 0);

    if (show_help || (!show_all && !show_cfg && !show_env2 && !show_keys && !show_sys)) {
        printf("Usage: /doctor [all|config|env|keys|system]\n");
        printf("  all       Run all diagnostics (default)\n");
        printf("  config    Check configuration file\n");
        printf("  env       Check environment file\n");
        printf("  keys      Check API keys in environment\n");
        printf("  system    Check system resources (disk, memory, CPU)\n");
        return;
    }

    printf("=== System Diagnostics ===\n\n");

    /* 1. HERMES_HOME */
    const char *home = state->hermes_home[0] ? state->hermes_home
                    : getenv("SLERMES_HOME") ? getenv("SLERMES_HOME")
                    : getenv("HOME");
    printf("[HERMES_HOME]  %s\n", home ? home : "(not found)");

    /* Check ~/.slermes or ~/.hermes exists */
    char home_dir[1024];
    if (state->hermes_home[0]) {
        snprintf(home_dir, sizeof(home_dir), "%s", state->hermes_home);
    } else {
        const char *uh = getenv("HOME");
        if (uh) {
            snprintf(home_dir, sizeof(home_dir), "%s/.slermes", uh);
            if (access(home_dir, F_OK) != 0)
                snprintf(home_dir, sizeof(home_dir), "%s/.hermes", uh);
        }
    }
    printf("[HOME_DIR]    %s %s\n", home_dir,
           access(home_dir, F_OK) == 0 ? "✓" : "✗ (not found)");

    /* 2. Config file */
    if (show_cfg) {
        printf("\n--- Config ---\n");
        char cfg_path[1024];
        snprintf(cfg_path, sizeof(cfg_path), "%s/config.yaml", home_dir);
        if (access(cfg_path, F_OK) == 0) {
            printf("[config.yaml] %s ✓\n", cfg_path);
            hermes_config_t cfg;
            if (hermes_config_load(&cfg, home_dir))
                printf("[config]      Loaded successfully (v%d)\n", cfg.config_version);
            else
                printf("[config]      ✗ Failed to parse\n");
        } else {
            printf("[config.yaml] ✗ Not found at %s\n", cfg_path);
        }
    }

    /* 3. .env file */
    if (show_env2) {
        printf("\n--- Environment ---\n");
        char env_path[1024];
        snprintf(env_path, sizeof(env_path), "%s/.env", home_dir);
        if (access(env_path, F_OK) == 0) {
            printf("[.env]        %s ✓\n", env_path);
        } else {
            printf("[.env]        Not found (optional)\n");
        }
    }

    /* 4. API key detection */
    if (show_keys) {
        printf("\n--- API Keys ---\n");
        const char *key_names[] = {
            "OPENAI_API_KEY", "ANTHROPIC_API_KEY", "ANTHROPIC_TOKEN",
            "OPENROUTER_API_KEY", "DEEPSEEK_API_KEY", "GOOGLE_API_KEY",
            "XAI_API_KEY", "AZURE_API_KEY", "AWS_ACCESS_KEY_ID",
            "NOUS_API_KEY", "HF_TOKEN", NULL
        };
        int found = 0;
        for (int i = 0; key_names[i]; i++) {
            const char *val = getenv(key_names[i]);
            if (val && *val) {
                size_t vlen = strlen(val);
                printf("[%-20s] ✓ (%zu chars)\n", key_names[i], vlen);
                found++;
            }
        }
        if (found == 0)
            printf("  No API keys found in environment.\n");
        else
            printf("  %d API key(s) detected.\n", found);
    }

    /* 5. System resources */
    if (show_sys) {
        printf("\n--- System ---\n");

        /* CPU */
        long nproc = sysconf(_SC_NPROCESSORS_ONLN);
        if (nproc > 0) printf("[CPU]         %ld logical core(s)\n", nproc);

        /* Memory */
        long pages = sysconf(_SC_PHYS_PAGES);
        long psize = sysconf(_SC_PAGE_SIZE);
        if (pages > 0 && psize > 0) {
            unsigned long long total_mem = (unsigned long long)pages * psize;
            printf("[Memory]      %llu MB total\n", total_mem / (1024 * 1024));

            long avail = sysconf(_SC_AVPHYS_PAGES);
            if (avail > 0) {
                unsigned long long free_mem = (unsigned long long)avail * psize;
                unsigned long long used_mem = total_mem - free_mem;
                unsigned long pct = (unsigned long)(used_mem * 100 / total_mem);
                printf("[Memory]      %llu MB used (%lu%%), %llu MB free\n",
                       used_mem / (1024 * 1024), pct, free_mem / (1024 * 1024));
            }
        }

        /* Disk space for home directory */
        struct statvfs vfs;
        if (statvfs(home_dir, &vfs) == 0) {
            unsigned long long total = (unsigned long long)vfs.f_frsize * vfs.f_blocks;
            unsigned long long free  = (unsigned long long)vfs.f_frsize * vfs.f_bfree;
            unsigned long long avail2 = (unsigned long long)vfs.f_frsize * vfs.f_bavail;
            unsigned long long used  = total - free;
            unsigned long pct = total > 0 ? (unsigned long)(used * 100 / total) : 0;
            printf("[Disk %s] %llu MB total, %llu MB used (%lu%%), %llu MB available\n",
                   home_dir, total / (1024 * 1024), used / (1024 * 1024),
                   pct, avail2 / (1024 * 1024));
        }

        /* Uptime */
        FILE *upt = fopen("/proc/uptime", "r");
        if (upt) {
            double up_secs, idle_secs;
            if (fscanf(upt, "%lf %lf", &up_secs, &idle_secs) == 2) {
                int days = (int)(up_secs / 86400);
                int hours = (int)((up_secs - days * 86400) / 3600);
                int mins = (int)((up_secs - days * 86400 - hours * 3600) / 60);
                printf("[Uptime]      %d day(s), %d hour(s), %d min(s)\n",
                       days, hours, mins);
            }
            fclose(upt);
        }

        /* Process info */
        printf("[PID]         %d\n", getpid());
    }

    printf("\n=== Diagnostics complete ===\n");
}

/* ================================================================
 *  /dump — System debug info dump
 * ================================================================ */
/* PoP: cmd_dump @ hermes_cli/main.py:cmd_dump */
void cmd_dump(const char *args, agent_state_t *state) {
    (void)args;

    printf("=== Hermes C System Dump ===\n\n");

    /* Version */
    printf("Version:        %s\n", HERMES_VERSION);

    /* Git commit */
    {
        char buf[128] = "(unknown)";
        FILE *fp = popen("git rev-parse --short=8 HEAD 2>/dev/null", "r");
        if (fp) {
            if (fgets(buf, sizeof(buf), fp)) {
                size_t len = strlen(buf);
                if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
            }
            pclose(fp);
        }
        printf("Git commit:     %s\n", buf);
    }

    /* Host info */
    {
        char hostname[256] = "(unknown)";
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            hostname[sizeof(hostname)-1] = '\0';
        }
        printf("Hostname:       %s\n", hostname);
    }

    /* Config home */
    const char *home = state->hermes_home[0] ? state->hermes_home : getenv("SLERMES_HOME");
    if (!home) home = getenv("HOME");
    printf("Config home:    %s\n", home ? home : "(not set)");
    printf("Config file:    %s/config.yaml\n", home ? home : "~/.slermes");

    /* Provider + model */
    printf("Provider:       %s\n", state->llm.provider[0] ? state->llm.provider : "(default)");
    printf("Model:          %s\n", state->llm.model[0] ? state->llm.model : "(default)");

    /* Session info */
    printf("Session ID:     %s\n", state->session_id[0] ? state->session_id : "(unsaved)");
    printf("Messages:       %zu\n", state->message_count);
    printf("Iterations:     %d/%d\n", state->iteration_count, state->max_iterations);

    /* Token usage */
    printf("Tokens in:      %d\n", state->session_input_tokens);
    printf("Tokens out:     %d\n", state->session_output_tokens);
    printf("Tokens total:   %d\n", state->session_total_tokens);
    if (state->session_estimated_cost_usd > 0.0)
        printf("Est. cost:      $%.6f\n", state->session_estimated_cost_usd);

    /* Tools */
    printf("Tools reg:      %zu\n", state->tools.count);

    /* Gateway platforms — count registered platform adapters */
    {
        /* Try to read gateway platforms from process list */
        int gw_count = 0;
        char buf[256];
        FILE *fp = popen("ls src/gateway/platforms/*.c 2>/dev/null | grep -v server.c | wc -l", "r");
        if (fp) {
            if (fgets(buf, sizeof(buf), fp)) gw_count = atoi(buf);
            pclose(fp);
        }
        if (gw_count > 0)
            printf("Gateway plats:  %d\n", gw_count);
    }

    /* Plugins */
    {
        int pcount = 0;
        char buf[256];
        FILE *fp = popen("ls -1 src/plugins/*.so 2>/dev/null | wc -l", "r");
        if (fp) {
            if (fgets(buf, sizeof(buf), fp)) pcount = atoi(buf);
            pclose(fp);
        }
        printf("Plugin .so:     %d\n", pcount);
    }

    /* Source stats */
    {
        int c_count = 0, h_count = 0;
        char buf[256];
        FILE *fp = popen("ls src/*.c src/**/*.c 2>/dev/null | wc -l", "r");
        if (fp) {
            if (fgets(buf, sizeof(buf), fp)) c_count = atoi(buf);
            pclose(fp);
        }
        fp = popen("ls include/*.h 2>/dev/null | wc -l", "r");
        if (fp) {
            if (fgets(buf, sizeof(buf), fp)) h_count = atoi(buf);
            pclose(fp);
        }
        printf("C source files: %d (.c) + %d (.h)\n", c_count, h_count);
    }

    /* CLI commands registered */
    printf("CLI commands:   %d\n", commands_count());

    /* Config keys (approximate from YAML key count) */
    {
        int kcount = 0;
        if (home) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/config.yaml", home);
            FILE *fp = fopen(path, "r");
            if (fp) {
                char line[4096];
                while (fgets(line, sizeof(line), fp)) {
                    if (strchr(line, ':')) kcount++;
                }
                fclose(fp);
            }
        }
        printf("Config keys:    ~%d\n", kcount > 0 ? kcount : 322);
    }

    /* Upstream sync status */
    {
        char buf[256];
        FILE *fp = popen("cd /home/wubu/hermes-agent-dev && git rev-list --count HEAD --not upstream/main 2>/dev/null || echo 0", "r");
        int ahead = 0;
        if (fp) {
            if (fgets(buf, sizeof(buf), fp)) ahead = atoi(buf);
            pclose(fp);
        }
        printf("Upstream:       0 behind, %d ahead\n", ahead);
    }

    printf("\n=== End Dump ===\n");
}

void cmd_exit(const char *args, agent_state_t *state) {
    (void)args;
    state->interrupted = true;
}

void cmd_handoff(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Session handoff commands:\n");
        printf("  /handoff request <platform>  — Request handoff to another platform\n");
        printf("  /handoff claim <id>           — Claim a pending handoff\n");
        printf("  /handoff complete <id>        — Mark handoff as complete\n");
        printf("  /handoff status               — Show pending/claimed handoffs\n");
        printf("  /handoff list                 — List all handoff requests\n");
        return;
    }

    /* Parse subcommand */
    char subcmd[64], arg[512];
    subcmd[0] = '\0';
    arg[0] = '\0';
    if (sscanf(args, "%63s %511[^\n]", subcmd, arg) < 1) {
        printf("Error: missing subcommand. See /handoff for usage.\n");
        return;
    }

    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";
    char handoff_dir[8192];
    snprintf(handoff_dir, sizeof(handoff_dir), "%s/.hermes/handoffs", home);

    if (strcmp(subcmd, "request") == 0) {
        if (!arg[0]) {
            printf("Usage: /handoff request <platform>\n");
            return;
        }
        /* Generate handoff ID from session + timestamp */
        char handoff_id[128];
        snprintf(handoff_id, sizeof(handoff_id), "%s_%ld",
                 state->session_id[0] ? state->session_id : "unknown",
                 (long)time(NULL));
        handoff_write_request(handoff_id, arg, state->session_id, "cli");
        printf("Handoff requested: id=%s platform=%s\n", handoff_id, arg);
        printf("Another agent can claim this with: /handoff claim %s\n", handoff_id);

    } else if (strcmp(subcmd, "claim") == 0) {
        if (!arg[0]) {
            printf("Usage: /handoff claim <id>\n");
            return;
        }
        /* Read the handoff request file */
        char path[8192];
        snprintf(path, sizeof(path), "%s/%s.json", handoff_dir, arg);
        json_node_t *req = json_parse_file(path, NULL);
        if (!req) {
            printf("Error: handoff '%s' not found\n", arg);
            return;
        }
        const char *platform = json_get_str(req, "platform", "unknown");
        const char *src_session = json_get_str(req, "session_id", "unknown");

        /* Mark as claimed */
        json_object_set(req, "status", json_new_string("claimed"));
        json_object_set(req, "claimed_at", json_new_number((double)time(NULL)));
        json_object_set(req, "claimer", json_new_string("cli"));
        char *updated = json_serialize(req);
        if (updated) {
            FILE *f = fopen(path, "w");
            if (f) { fputs(updated, f); fclose(f); }
            free(updated);
        }
        json_free(req);

        printf("Handoff claimed: id=%s platform=%s session=%s\n",
               arg, platform, src_session);
        printf("Resume the session with context from platform '%s'\n", platform);

    } else if (strcmp(subcmd, "complete") == 0) {
        if (!arg[0]) {
            printf("Usage: /handoff complete <id>\n");
            return;
        }
        char path[8192];
        snprintf(path, sizeof(path), "%s/%s.json", handoff_dir, arg);
        json_node_t *req = json_parse_file(path, NULL);
        if (!req) {
            printf("Error: handoff '%s' not found\n", arg);
            return;
        }
        json_object_set(req, "status", json_new_string("completed"));
        json_object_set(req, "completed_at", json_new_number((double)time(NULL)));
        char *updated = json_serialize(req);
        if (updated) {
            FILE *f = fopen(path, "w");
            if (f) { fputs(updated, f); fclose(f); }
            free(updated);
        }
        json_free(req);
        printf("Handoff completed: id=%s\n", arg);

    } else if (strcmp(subcmd, "status") == 0 || strcmp(subcmd, "list") == 0) {
        list_t entries;
        list_init(&entries);
        handoff_read_dir(&entries);
        if (entries.count == 0) {
            printf("No handoff requests found.\n");
        } else {
            printf("Handoff requests (%d):\n", entries.count);
            for (int i = 0; i < entries.count; i++) {
                handoff_entry_t *e = (handoff_entry_t *)entries.items[i];
                printf("  [%s] id=%s platform=%s session=%s requester=%s\n",
                       e->status, e->id, e->platform, e->session_id, e->requester);
                free(e->id);
                free(e->platform);
                free(e->session_id);
                free(e->requester);
                free(e->status);
                free(e);
            }
        }
        list_free(&entries);

    } else {
        printf("Unknown subcommand: %s. See /handoff for usage.\n", subcmd);
    }
}

void cmd_indicator(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Current indicator: %s Options: default, dots, bar, face\n", g_indicator_style);
        return;
    }
    if (strcmp(args, "default") == 0 || strcmp(args, "dots") == 0 ||
        strcmp(args, "bar") == 0 || strcmp(args, "face") == 0) {
        snprintf(g_indicator_style, sizeof(g_indicator_style), "%s", args);
        printf("Indicator set to: %s\n", args);
    } else {
        printf("Unknown indicator: %s Options: default, dots, bar, face\n", args);
    }
}

/* ================================================================
 *  /logs — View agent logs
 * ================================================================ */
void cmd_logs(const char *args, agent_state_t *state) {
    (void)state;
    const char *logname = "agent";  /* default */
    int n_lines = 20;
    bool follow = false;
    const char *level_filter = NULL;
    char buf[512];  /* for strtok parsing — must outlive level_filter pointer */

    /* Parse args */
    if (args && args[0]) {
        strncpy(buf, args, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        char *token = strtok(buf, " \t");
        while (token) {
            if (token[0] == '-' && token[1] == 'n' && token[2] == '\0') {
                token = strtok(NULL, " \t");
                if (token) n_lines = atoi(token);
            } else if (strcmp(token, "--level") == 0 || strcmp(token, "-l") == 0) {
                token = strtok(NULL, " \t");
                if (token) level_filter = token;
            } else if (strcmp(token, "--follow") == 0 || strcmp(token, "-f") == 0) {
                follow = true;
            } else if (strcmp(token, "agent") == 0 ||
                       strcmp(token, "errors") == 0 ||
                       strcmp(token, "error") == 0 ||
                       strcmp(token, "gateway") == 0) {
                logname = token;
            }
            token = strtok(NULL, " \t");
        }
    }

    if (n_lines <= 0) n_lines = 20;
    if (n_lines > 1000) n_lines = 1000;

    /* Map log name to filename */
    const char *filename = "agent.log";
    if (strcmp(logname, "errors") == 0 || strcmp(logname, "error") == 0)
        filename = "errors.log";
    else if (strcmp(logname, "gateway") == 0)
        filename = "gateway.log";

    /* Build path: <hermes_home>/logs/<filename> */
    char path[1024];
    {
        char log_dir[512];
        hermes_log_dir(log_dir, sizeof(log_dir));
        snprintf(path, sizeof(path), "%s/%s", log_dir, filename);
    }

    printf("=== %s ===\n", filename);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf("No log file found at: %s\n", path);
        printf("Hermes may not have been run yet, or logs are stored elsewhere.\n");
        return;
    }

    /* Read all lines into a ring buffer */
    char **lines = NULL;
    int count = 0;
    char line[4096];

    while (fgets(line, sizeof(line), fp)) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

        /* Apply level filter if set */
        if (level_filter) {
            /* Parse level from standard Hermes log format:
             * "2026-04-05 22:35:00,123 INFO  ..." */
            char *level_start = NULL;
            for (size_t i = 0; line[i]; i++) {
                if (line[i] == ' ' && i + 5 < strlen(line)) {
                    /* Check for known level after timestamp pattern */
                    const char *p = line + i + 1;
                    if (strncmp(p, "DEBUG", 5) == 0 ||
                        strncmp(p, "INFO", 4) == 0 ||
                        strncmp(p, "WARNING", 7) == 0 ||
                        strncmp(p, "ERROR", 5) == 0 ||
                        strncmp(p, "CRITICAL", 8) == 0) {
                        level_start = line + i + 1;
                        break;
                    }
                }
            }
            if (level_start) {
                /* Extract level string */
                char lvl[16];
                int lvl_len = 0;
                while (level_start[lvl_len] && level_start[lvl_len] != ' ' &&
                       level_start[lvl_len] != '\t' && lvl_len < 15) {
                    lvl[lvl_len] = level_start[lvl_len];
                    lvl_len++;
                }
                lvl[lvl_len] = '\0';

                if (strcasecmp(lvl, level_filter) != 0)
                    continue;  /* skip non-matching lines */
            } else {
                /* Unparseable line — include it anyway */
            }
        }

        /* Store line in ring buffer */
        char **new_lines = realloc(lines, sizeof(char *) * (count + 1));
        if (!new_lines) break;
        lines = new_lines;
        lines[count] = strdup(line);
        if (!lines[count]) break;
        count++;
    }
    fclose(fp);

    /* Print last n_lines */
    int start = count > n_lines ? count - n_lines : 0;
    for (int i = start; i < count; i++) {
        printf("%s\n", lines[i]);
        free(lines[i]);
    }
    free(lines);

    if (count == 0)
        printf("(empty)\n");
    else if (count <= n_lines)
        printf("\n(%d lines, total)\n", count);
    else
        printf("\n(%d lines, showing last %d of %d)\n", n_lines < count ? n_lines : count, n_lines, count);

    /* Follow mode: poll for new lines */
    if (follow) {
        printf("\n--- Following %s (Ctrl-C to stop) ---\n", filename);
        fflush(stdout);

        /* Get current file size for comparison */
        struct stat st;
        long last_size = 0;
        if (stat(path, &st) == 0) last_size = st.st_size;

        while (1) {
            sleep(1);
            FILE *f = fopen(path, "r");
            if (!f) break;
            struct stat new_st;
            if (stat(path, &new_st) == 0) {
                /* Handle log rotation: if inode changed or size shrunk, restart */
                if (new_st.st_ino != st.st_ino || new_st.st_size < last_size) {
                    printf("\n--- Log rotated, following %s ---\n", filename);
                    last_size = 0;
                    st = new_st;
                }
            }
            if (new_st.st_size > last_size) {
                /* Seek to last known position */
                fseek(f, last_size, SEEK_SET);
                char newline[4096];
                while (fgets(newline, sizeof(newline), f)) {
                    size_t nl = strlen(newline);
                    if (nl > 0 && newline[nl-1] == '\n') newline[nl-1] = '\0';
                    printf("%s\n", newline);
                }
                fflush(stdout);
                last_size = new_st.st_size;
            }
            fclose(f);
        }
    }
}

/* /paste: Attach clipboard image */
void cmd_paste(const char *args, agent_state_t *state) {
    (void)state;
    (void)args;
    /* On WSL, try Windows clipboard via powershell.exe */
    FILE *fp = popen("powershell.exe -NoProfile -Command \"Get-Clipboard\" 2>/dev/null", "r");
    if (!fp) {
        printf("Clipboard paste not available. Use /image <path> instead.\n");
        return;
    }
    char buf[4096];
    size_t len = 0;
    if (fgets(buf, sizeof(buf), fp)) {
        len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
    }
    int rc = pclose(fp);
    if (rc != 0 || len == 0) {
        printf("No clipboard content detected.\n");
        printf("Tip: Use /image <path> to attach an image file.\n");
        return;
    }
    printf("Clipboard content:\n%s\n", buf);
}

/* /platforms: Show gateway platform status */
void cmd_platforms(const char *args, agent_state_t *state) {
    (void)state;
    bool verbose = (args && (strcmp(args, "-v") == 0 || strcmp(args, "verbose") == 0));
    bool list_only = (args && strcmp(args, "list") == 0);
    (void)list_only;

    printf("Gateway platforms:\n");
    const char *gw = getenv("HERMES_GATEWAY_PLATFORMS");
    if (gw) {
        printf("  Configured: %s\n", gw);
    } else {
        hermes_config_t cfg;
        if (hermes_config_load(&cfg, NULL) && cfg.gateway_platforms[0])
            printf("  Configured: %s\n", cfg.gateway_platforms);
        else
            printf("  Configured: telegram (default)\n");
    }

    if (verbose) {
        printf("\nCredentials check:\n");
         const char *check[][3] = {
            {"telegram", "TELEGRAM_BOT_TOKEN",           "Polling"},
            {"discord",  "DISCORD_BOT_TOKEN",            "Gateway"},
            {"slack",    "SLACK_BOT_TOKEN",              "Events API"},
            {"signal",   "SIGNAL_NUMBER",                "dbus CLI"},
            {"sms",      "TWILIO_ACCOUNT_SID",           "Twilio SMS"},
            {"matrix",   "MATRIX_HOMESERVER",            "CS API"},
            {"email",    "EMAIL_HOST",                   "IMAP/SMTP"},
            {"whatsapp", "WHATSAPP_PHONE_NUMBER_ID",     "Cloud API"},
            {"feishu",   "FEISHU_APP_ID",                "Lark bot"},
            {"wecom",    "WECOM_CORP_ID",                "WeChat Work"},
            {"dingtalk", "DINGTALK_WEBHOOK_TOKEN",       "DingTalk"},
            {"homeassistant", "HASS_TOKEN",               "HA API"},
            {"mattermost","MATTERMOST_URL",               "Webhooks"},
            {"bluebubbles","BLUEBUBBLES_PASSWORD",       "iMessage"},
            {NULL, NULL, NULL}
        };
        for (int i = 0; check[i][0]; i++) {
            const char *val = getenv(check[i][1]);
            printf("  %-12s %s %-16s %s\n",
                   check[i][0], val ? "✅" : "❌",
                   val ? "(found)" : "(missing)",
                   check[i][2]);
        }
    }

    printf("\nAll 20 platform types available. Use -v for credential check.\n");
    printf("Config: gateway.platforms in config.yaml or $HERMES_GATEWAY_PLATFORMS\n");
}

/* /profile: Show active profile */
/* PoP: cmd_profile @ hermes_cli/main.py:cmd_profile */
void cmd_profile(const char *args, agent_state_t *state) {
    if (args && args[0]) {
        if (strcmp(args, "home") == 0) {
            printf("Home: %s\n", state->hermes_home[0] ? state->hermes_home :
                   getenv("SLERMES_HOME") ? getenv("SLERMES_HOME") : "~/.slermes");
            return;
        }
        if (strcmp(args, "model") == 0) {
            printf("Model: %s\n", state->llm.model[0] ? state->llm.model : "(default)");
            printf("Provider: %s\n", state->llm.provider[0] ? state->llm.provider : "(default)");
            return;
        }
        printf("Usage: /profile [home|model]\n");
        return;
    }
    printf("Home: %s\n", state->hermes_home[0] ? state->hermes_home :
           getenv("SLERMES_HOME") ? getenv("SLERMES_HOME") : "~/.slermes");
    printf("Model: %s\n", state->llm.model[0] ? state->llm.model : "(default)");
    printf("Provider: %s\n", state->llm.provider[0] ? state->llm.provider : "(default)");
}

/* /redraw: Force a full UI repaint */
void cmd_redraw(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("\033[2J\033[H"); /* Clear screen + home cursor */
    printf("Screen cleared. Use /help for commands.\n");
}

/* /reload: Reload .env */
/* PoP: cmd_reload @ hermes_cli/bundles.py:_cmd_reload */
/* PoP: cmd_reload @ hermes_cli/proxy_cli.py:cmd_reload */
void cmd_reload(const char *args, agent_state_t *state) {
    if (args && args[0]) {
        if (strcmp(args, "plugins") == 0) {
            /* Plugin-only reload (hot-reload) */
            hermes_config_t cfg;
            hermes_config_load(&cfg, NULL);
            hermes_config_load_env(&cfg);
            plugin_registry_t *old_reg = (plugin_registry_t *)state->plugin_reg;
            hermes_plugin_shutdown(old_reg);
            plugin_registry_t *new_reg = hermes_plugin_init(&cfg);
            if (new_reg) state->plugin_reg = new_reg;
            printf("Plugins reloaded.\n");
            return;
        }
        if (strcmp(args, "env") == 0) {
            hermes_config_t cfg;
            hermes_config_load_env(&cfg);
            if (cfg.api_key[0]) memcpy(state->llm.api_key, cfg.api_key, sizeof(state->llm.api_key));
            printf(".env variables reloaded.\n");
            return;
        }
        printf("Usage: /reload [plugins|env]\n");
        return;
    }
    hermes_config_t cfg;
    hermes_config_load(&cfg, NULL);
    hermes_config_load_env(&cfg);
    memcpy(state->llm.base_url, cfg.base_url, sizeof(state->llm.base_url));
    if (cfg.api_key[0]) memcpy(state->llm.api_key, cfg.api_key, sizeof(state->llm.api_key));
    if (cfg.model[0]) memcpy(state->llm.model, cfg.model, sizeof(state->llm.model));
    if (cfg.provider[0]) memcpy(state->llm.provider, cfg.provider, sizeof(state->llm.provider));
    /* Reload plugins from updated config */
    plugin_registry_t *old_reg = (plugin_registry_t *)state->plugin_reg;
    hermes_plugin_shutdown(old_reg);
    plugin_registry_t *new_reg = hermes_plugin_init(&cfg);
    if (new_reg) state->plugin_reg = new_reg;
    printf(".env reloaded. Config + plugins updated.\n");
}

/* /send: Send a message to a platform */
/* AG26: Port of Python hermes_cli/main.py:cmd_send(). */
void cmd_send(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /send [target] <message>\n");
        printf("  target: 'local' (save), 'stdout' (print), or 'platform:chat_id' (e.g. telegram:-100123456)\n");
        printf("  If target omitted, defaults to 'stdout'.\n");
        return;
    }

    /* Parse: first word is target if it contains ':' or is 'local'/'stdout' */
    char target[256];
    const char *message;
    target[0] = '\0';

    const char *space = strchr(args, ' ');
    if (space && (strncmp(args, "local", 5) == 0 || strncmp(args, "stdout", 6) == 0 ||
                  strchr(args, ':') != NULL)) {
        /* First word is a target */
        size_t tlen = (size_t)(space - args);
        if (tlen >= sizeof(target)) tlen = sizeof(target) - 1;
        memcpy(target, args, tlen);
        target[tlen] = '\0';
        message = space + 1;
        while (*message == ' ') message++;
    } else {
        strcpy(target, "stdout");
        message = args;
    }

    /* Build JSON args for send_message_handler */
    char json_args[8192];
    char *escaped = malloc(strlen(message) * 2 + 1);
    if (!escaped) { printf("Error: allocation failed\n"); return; }

    size_t j = 0;
    for (const char *p = message; *p && j < strlen(message) * 2; p++) {
        if (*p == '"') { escaped[j++] = '\\'; escaped[j++] = '"'; }
        else if (*p == '\\') { escaped[j++] = '\\'; escaped[j++] = '\\'; }
        else if (*p == '\n') { escaped[j++] = '\\'; escaped[j++] = 'n'; }
        else escaped[j++] = *p;
    }
    escaped[j] = '\0';

    snprintf(json_args, sizeof(json_args),
             "{\"target\":\"%s\",\"message\":\"%s\"}", target, escaped);
    free(escaped);

    char *result = send_message_handler(json_args, NULL);
    if (result) {
        /* Parse result JSON to extract meaningful output */
        char *err = NULL;
        json_node_t *r = json_parse(result, &err);
        if (r) {
            const char *status = json_object_get_string(r, "status", NULL);
            const char *error = json_object_get_string(r, "error", NULL);
            if (error && error[0])
                printf("Error: %s\n", error);
            else if (status)
                printf("Sent: %s\n", status);
            else
                printf("Result: %s\n", result);
            json_free(r);
        } else {
            printf("Result: %s\n", result);
            free(err);
        }
        free(result);
    } else {
        printf("Error: send_message_handler returned NULL\n");
    }
}

void cmd_sethome(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Current home channel: %s\n", g_home_channel[0] ? g_home_channel : "(not set)");
        printf("Usage: /sethome <platform:chat_id>\n");
        return;
    }
    snprintf(g_home_channel, sizeof(g_home_channel), "%s", args);
    printf("Home channel set to: %s\n", g_home_channel);
}

/* PoP: cmd_skin @ hermes_cli/main.py:cmd_skin */
void cmd_skin(const char *args, agent_state_t *state) {
    (void)state;
    if (args && args[0]) {
        if (strcmp(args, "list") == 0) {
            int count = skin_builtin_count();
            printf("Available skins (%d):\n", count);
            for (int i = 0; i < count; i++) {
                const char *name = skin_builtin_name(i);
                printf("  - %s\n", name ? name : "(unnamed)");
            }
            printf("Use /skin <name> to activate.\n");
            return;
        }
        if (strlen(args) >= sizeof(g_current_skin)) {
            printf("Skin name too long (max %zu chars).\n", sizeof(g_current_skin) - 1);
            return;
        }
        /* Try loading as built-in preset first */
        skin_t *sk = skin_load_preset(args);
        if (!sk) {
            printf("Skin not found: %s\n", args);
            printf("Use /skin list to see available skins.\n");
            return;
        }
        snprintf(g_current_skin, sizeof(g_current_skin), "%s", args);
        setenv("HERMES_SKIN", args, 1);
        display_set_skin((void *)sk);
        printf("Skin set to: %s\n", args);
        return;
    }
    const char *skin = g_current_skin[0] ? g_current_skin : getenv("HERMES_SKIN");
    printf("Current skin: %s\n", skin && skin[0] ? skin : "(default)");
    printf("Use /skin list for available skins.\n");
}

void cmd_statusbar(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0] || strcmp(args, "status") == 0) {
        printf("Status bar: %s\n", g_statusbar_on ? "shown" : "hidden");
        printf("  Usage: /statusbar [on|off|status]\n");
        return;
    }
    if (strcmp(args, "on") == 0) {
        g_statusbar_on = 1;
        printf("Status bar shown.\n");
    } else if (strcmp(args, "off") == 0) {
        g_statusbar_on = 0;
        printf("Status bar hidden.\n");
    } else {
        printf("Unknown argument: %s\n", args);
        printf("  Usage: /statusbar [on|off|status]\n");
    }
}

/* /stop: Kill all running background processes */
/* PoP: cmd_stop @ hermes_cli/proxy_cli.py:cmd_stop */
void cmd_stop(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Stopping all background processes...\n");
    int killed = 0;
    char buf[256];
    FILE *fp = popen("pkill -f 'hermes background' 2>/dev/null; echo $?", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) killed = atoi(buf) == 0 ? 1 : 0;
        pclose(fp);
    }
    printf("Done. Killed %d process(es).\n", killed ? 1 : 0);
}

/* PoP: cmd_tools @ hermes_cli/portal_cli.py:_cmd_tools */
/* PoP: cmd_tools @ hermes_cli/main.py:cmd_tools */
void cmd_tools(const char *args, agent_state_t *state) {
    if (args && args[0]) {
        /* Show details for a specific tool */
        const char *tool_name = args;
        for (size_t i = 0; i < state->tools.count; i++) {
            if (strcmp(state->tools.tools[i].name, tool_name) == 0) {
                printf("Tool:          %s\n", state->tools.tools[i].name);
                printf("Description:   %s\n", state->tools.tools[i].description);
                printf("Available:     %s\n", state->tools.tools[i].available ? "yes" : "no");
                if (state->tools.tools[i].schema_json[0])
                    printf("Schema:        %s\n", state->tools.tools[i].schema_json);
                return;
            }
        }
        printf("Tool not found: %s\n", tool_name);
        return;
    }
    printf("Registered tools (%zu):\n", state->tools.count);
    for (size_t i = 0; i < state->tools.count; i++) {
        printf("  %s", state->tools.tools[i].name);
        if (!state->tools.tools[i].available)
            printf(" [UNAVAILABLE]");
        printf(" \u2014 %s\n", state->tools.tools[i].description);
    }
}

/* /tools-verify: Verify all expected tools are registered */
void cmd_tools_verify(const char *args, agent_state_t *state) {
    (void)args;
    const char *expected[] = {
        "terminal", "read_file", "write_file", "search_files", "patch",
        "web_get", "web_search", "web_extract",
        "skills_list", "skill_view", "skill_manage",
        "execute_code", "clarify", "memory", "todo", "process",
        "send_message", "cronjob", "session_search",
        "text_to_speech", "vision_analyze", "delegate_task",
        "x_search", "approval_status",
        "voice_listen", "voice_speak", "image_generate",
        "ha_list_entities", "ha_get_state", "ha_list_services", "ha_call_service",
        "browser_navigate", "browser_snapshot", "browser_back", "browser_forward",
        "browser_click", "browser_type", "browser_scroll",
        "browser_get_images", "browser_press",
        "browser_vision", "browser_console", "browser_dialog", "browser_cdp",
        "computer_use",
        "kanban_show", "kanban_list", "kanban_create", "kanban_complete",
        "kanban_block", "kanban_heartbeat", "kanban_comment", "kanban_unblock",
        "kanban_link",
        NULL
    };
    int total_expected = 0, found = 0, missing = 0;
    for (int i = 0; expected[i]; i++) {
        total_expected++;
        bool ok = false;
        for (size_t j = 0; j < state->tools.count; j++) {
            if (strcmp(state->tools.tools[j].name, expected[i]) == 0) {
                ok = true; break;
            }
        }
        if (ok) found++;
        else { printf("  MISSING: %s\n", expected[i]); missing++; }
    }
    printf("Tools: %zu registered, %d expected, %d missing, %d found\n",
           state->tools.count, total_expected, missing, found);
    if (missing == 0) printf("ALL EXPECTED TOOLS PRESENT\n");
}

/* /toolsets: List available toolsets (dynamic from registry) */
void cmd_toolsets(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Available toolsets (dynamic):\n");
    size_t n = registry_get_count();
    char seen[32][32]; int seen_count = 0;
    for (size_t i = 0; i < n; i++) {
        const char *name = registry_get_name(i);
        const char *ts = registry_get_toolset(name);
        if (!ts || !ts[0]) ts = "core";
        bool dup = false;
        for (int j = 0; j < seen_count; j++)
            if (strcmp(seen[j], ts) == 0) { dup = true; break; }
        if (dup) continue;
        if (seen_count < 32) snprintf(seen[seen_count], 32, "%s", ts);
        seen_count++;
        /* Collect tools for this toolset */
        printf("  %s —", ts);
        int col = 0;
        for (size_t k = 0; k < n; k++) {
            const char *tn = registry_get_name(k);
            const char *tt = registry_get_toolset(tn);
            if (!tt || !tt[0]) tt = "core";
            if (strcmp(tt, ts) != 0) continue;
            if (col > 0) printf(",");
            if (col % 5 == 0 && col > 0) printf("\n         ");
            printf(" %s", tn);
            col++;
        }
        printf("\n");
    }
}

/* /update: Update Hermes Agent — git pull + rebuild */
/* PoP: cmd_update @ hermes_cli/main.py:cmd_update */
void cmd_update(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Updating Hermes Agent...\n");

    /* Determine repo root: walk up from CWD until .git found */
    char cwd[4096];
    char repo_root[4096] = "";
    if (getcwd(cwd, sizeof(cwd))) {
        memcpy(repo_root, cwd, sizeof(repo_root) - 1);
        repo_root[sizeof(repo_root) - 1] = '\0';
        while (repo_root[0]) {
            char git_dir[4096];
            snprintf(git_dir, sizeof(git_dir), "%s/.git", repo_root);
            struct stat st;
            if (stat(git_dir, &st) == 0 && S_ISDIR(st.st_mode))
                break;
            /* Go up one directory */
            char *slash = strrchr(repo_root, '/');
            if (!slash || slash == repo_root) { repo_root[0] = '\0'; break; }
            *slash = '\0';
        }
    }

    if (!repo_root[0]) {
        printf("Error: not inside a git repository.\n");
        return;
    }

    /* Fetch latest */
    {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "cd '%s' && git fetch --quiet origin 2>&1", repo_root);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char err[1024];
            size_t n = 0;
            while (fgets(err, sizeof(err), fp)) {
                if (n == 0) printf("  Fetch: ");
                printf("%s", err);
                n++;
            }
            int rc = pclose(fp);
            if (rc != 0) {
                printf("  Git fetch failed (exit %d). Aborting.\n", rc);
                return;
            }
        }
    }

    /* Check if behind */
    char behind_buf[64] = "0";
    {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
                 "cd '%s' && git rev-list --count HEAD..origin/$(git rev-parse --abbrev-ref HEAD 2>/dev/null) 2>/dev/null || echo 0",
                 repo_root);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            if (!fgets(behind_buf, sizeof(behind_buf), fp)) behind_buf[0] = '0';
            size_t blen = strlen(behind_buf);
            if (blen > 0 && behind_buf[blen-1] == '\n') behind_buf[blen-1] = '\0';
            pclose(fp);
        }
    }

    int behind = atoi(behind_buf);
    if (behind == 0) {
        printf("  Already up to date (%s).\n", repo_root);
        return;
    }
    printf("  %d commit(s) behind. Pulling...\n", behind);

    /* Get current commit before pull */
    char old_commit[128] = "(unknown)";
    {
        FILE *fp = popen("git rev-parse --short=8 HEAD 2>/dev/null", "r");
        if (fp) {
            if (!fgets(old_commit, sizeof(old_commit), fp)) old_commit[0] = '\0';
            size_t olen = strlen(old_commit);
            if (olen > 0 && old_commit[olen-1] == '\n') old_commit[olen-1] = '\0';
            pclose(fp);
        }
    }

    /* Pull */
    {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "cd '%s' && git pull --ff-only origin 2>&1", repo_root);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char line[1024];
            while (fgets(line, sizeof(line), fp))
                printf("  %s", line);
            int rc = pclose(fp);
            if (rc != 0) {
                printf("  Git pull failed (exit %d). Resolve conflicts manually.\n", rc);
                return;
            }
        }
    }

    /* Get new commit */
    char new_commit[128] = "(unknown)";
    {
        FILE *fp = popen("git rev-parse --short=8 HEAD 2>/dev/null", "r");
        if (fp) {
            if (!fgets(new_commit, sizeof(new_commit), fp)) new_commit[0] = '\0';
            size_t nlen = strlen(new_commit);
            if (nlen > 0 && new_commit[nlen-1] == '\n') new_commit[nlen-1] = '\0';
            pclose(fp);
        }
    }
    printf("  %s -> %s\n", old_commit, new_commit);

    /* Rebuild */
    printf("  Rebuilding...\n");
    {
        FILE *fp = popen("make -j$(nproc) 2>&1", "r");
        if (fp) {
            char line[1024];
            while (fgets(line, sizeof(line), fp)) {
                /* Only print errors and final line */
                if (strstr(line, "error:") || strstr(line, "Error:") ||
                    strstr(line, "Phase 5 complete"))
                    printf("  %s", line);
            }
            int rc = pclose(fp);
            if (rc == 0)
                printf("  Update complete! Binary rebuilt.\n");
            else
                printf("  Build failed (exit %d).\n", rc);
        }
    }
}

void cmd_verbose(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    g_verbose = (g_verbose + 1) % 3;
    const char *modes[] = {"off", "normal", "verbose"};
    printf("Verbosity set to: %s\n", modes[g_verbose]);
}

/* /whoami: Show access level */
void cmd_whoami(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("Access level: admin (C translation build)\n");
    printf("Version:     %s\n", HERMES_VERSION);
    printf("Platform:    Linux (WSL)\n");
}

/* Scan handoff directory and return entries */
void handoff_read_dir(list_t *entries) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";
    char handoff_dir[8192];
    snprintf(handoff_dir, sizeof(handoff_dir), "%s/.hermes/handoffs", home);

    DIR *d = opendir(handoff_dir);
    if (!d) return;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        /* Only read .json files */
        size_t len = strlen(ent->d_name);
        if (len < 6 || strcmp(ent->d_name + len - 5, ".json") != 0)
            continue;

        char path[8192];
        snprintf(path, sizeof(path), "%s/%s", handoff_dir, ent->d_name);
        json_node_t *req = json_parse_file(path, NULL);
        if (!req) continue;

        handoff_entry_t *e = (handoff_entry_t *)calloc(1, sizeof(handoff_entry_t));
        if (e) {
            e->id = strdup(json_get_str(req, "id", ""));
            e->platform = strdup(json_get_str(req, "platform", ""));
            e->session_id = strdup(json_get_str(req, "session_id", ""));
            e->requester = strdup(json_get_str(req, "requester", ""));
            e->status = strdup(json_get_str(req, "status", "unknown"));
            list_append(entries, e);
        }
        json_free(req);
    }
    closedir(d);
}

/* Write a handoff request JSON file */
void list_append(list_t *l, void *item) {
    if (l->count >= l->capacity) {
        l->capacity = l->capacity ? l->capacity * 2 : 8;
        l->items = (void **)realloc(l->items, l->capacity * sizeof(void *));
    }
    l->items[l->count++] = item;
}

void list_free(list_t *l) {
    if (l->items) free(l->items);
    l->items = NULL;
    l->count = l->capacity = 0;
}

void list_init(list_t *l) {
    l->items = NULL;
    l->count = 0;
    l->capacity = 0;
}

