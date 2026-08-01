/*
 * cli_cmd_kanban.c — Kanban slash-command handlers extracted from commands.c.
 * Self-contained command-category module.
 */

#include "hermes_agent.h"
#include "cli_cmd_kanban.h"
#include "commands_shared.h"
#include "hermes_core_types.h"

#include "cli.h"
#include "port_hermes_cli_kanban_helpers.h"

/* cli_display_error is defined in cli/display.c (centralized styled error
 * printer); route kanban's error prints through it instead of raw printf. */
void cli_display_error(const char *msg);

/* /kanban: Kanban board management */
/* PoP: cmd_kanban @ hermes_cli/main.py:cmd_kanban */
void cmd_kanban(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /kanban <subcommand> [args]\n");
        printf("  /kanban list [--all]        — List kanban tasks\n");
        printf("  /kanban show <task_id>      — Show kanban task details\n");
        printf("  /kanban create <title>      — Create a new kanban task\n");
        printf("  /kanban complete <task_id>  — Mark a task complete\n");
        printf("  /kanban block <task_id>     — Mark a task blocked\n");
        printf("  /kanban unblock <task_id>   — Move blocked task to ready\n");
        printf("  /kanban link <parent> <child> — Add parent->child link\n");
        return;
    }
    const char *sub = args;
    while (*sub == ' ') sub++;

    if (strncmp(sub, "list", 4) == 0) {
        const char *rest = sub + 4;
        while (*rest == ' ') rest++;
        bool show_all = (strcmp(rest, "--all") == 0);
        char json_args[256];
        snprintf(json_args, sizeof(json_args),
                 "{\"limit\":%d,\"include_archived\":%s}",
                 show_all ? 200 : 50,
                 show_all ? "true" : "false");
        char *result = registry_dispatch("kanban_list", json_args, "");
        if (result) {
            /* Try to pretty-print the JSON result */
            json_t *jr = json_parse(result, NULL);
            if (jr) {
                json_t *tasks = json_obj_get(jr, "tasks");
                if (tasks && tasks->type == JSON_ARRAY) {
                    printf("Kanban tasks (%zu):\n", tasks->c.count);
                    for (size_t i = 0; i < tasks->c.count; i++) {
                        json_t *t = tasks->c.items[i];
                        /* Bridge JSON task -> kb_task_t and render via the
                         * ported formatter (was orphaned; now assembled). */
                        kb_task_t kt;
                        memset(&kt, 0, sizeof(kt));
                        kt.id        = json_get_str(t, "id", "");
                        kt.title     = json_get_str(t, "title", "");
                        kt.assignee  = json_get_str(t, "assignee", "");
                        kt.status    = json_get_str(t, "status", "");
                        kt.tenant    = json_get_str(t, "tenant", "");
                        kt.priority  = (int)json_get_num(t, "priority", 0);
                        kt.created_at = (long)json_get_num(t, "created_at", 0);
                        char *line = fmt_task_line(&kt);
                        printf("  %s\n", line ? line : "(unable to format)");
                        free(line);
                    }
                } else {
                    printf("Result: %s\n", result);
                }
                json_free(jr);
            } else {
                printf("Result: %s\n", result);
            }
            free(result);
        } else {
            cli_display_error("kanban_list returned NULL");
        }
        return;
    }

    if (strncmp(sub, "show", 4) == 0) {
        const char *id = sub + 4;
        while (*id == ' ') id++;
        if (!*id) { printf("Usage: /kanban show <task_id>\n"); return; }
        char json_args[512];
        snprintf(json_args, sizeof(json_args), "{\"task_id\":\"%s\"}", id);
        char *result = registry_dispatch("kanban_show", json_args, "");
        if (result) {
            printf("Task %s:\n%s\n", id, result);
            free(result);
        } else {
            cli_display_error("kanban_show returned NULL (task not found?)");
        }
        return;
    }

    if (strncmp(sub, "create", 6) == 0) {
        const char *title = sub + 6;
        while (*title == ' ') title++;
        if (!*title) { printf("Usage: /kanban create <title>\n"); return; }
        char title_esc[768];
        json_escape_arg(title, title_esc, sizeof(title_esc));
        char json_args[1024];
        snprintf(json_args, sizeof(json_args),
                 "{\"title\":\"%s\",\"assignee\":\"cli\"}", title_esc);
        char *result = registry_dispatch("kanban_create", json_args, "");
        if (result) {
            json_t *jr = json_parse(result, NULL);
            if (jr) {
                const char *tid = json_get_str(jr, "task_id", result);
                printf("Created task: %s\n", tid);
                json_free(jr);
            } else {
                printf("Result: %s\n", result);
            }
            free(result);
        } else {
            cli_display_error("kanban_create returned NULL");
        }
        return;
    }

    if (strncmp(sub, "complete", 8) == 0) {
        const char *id = sub + 8;
        while (*id == ' ') id++;
        if (!*id) { printf("Usage: /kanban complete <task_id>\n"); return; }
        char json_args[512];
        snprintf(json_args, sizeof(json_args),
                 "{\"task_id\":\"%s\",\"summary\":\"completed via CLI\"}", id);
        char *result = registry_dispatch("kanban_complete", json_args, "");
        if (result) {
            printf("Result: %s\n", result);
            free(result);
        } else {
            cli_display_error("kanban_complete returned NULL");
        }
        return;
    }

    if (strncmp(sub, "block", 5) == 0) {
        const char *id = sub + 5;
        while (*id == ' ') id++;
        if (!*id) { printf("Usage: /kanban block <task_id>\n"); return; }
        char json_args[512];
        snprintf(json_args, sizeof(json_args),
                 "{\"task_id\":\"%s\",\"reason\":\"blocked via CLI\"}", id);
        char *result = registry_dispatch("kanban_block", json_args, "");
        if (result) {
            printf("Result: %s\n", result);
            free(result);
        } else {
            cli_display_error("kanban_block returned NULL");
        }
        return;
    }

    if (strncmp(sub, "unblock", 7) == 0) {
        const char *id = sub + 7;
        while (*id == ' ') id++;
        if (!*id) { printf("Usage: /kanban unblock <task_id>\n"); return; }
        char json_args[256];
        snprintf(json_args, sizeof(json_args), "{\"task_id\":\"%s\"}", id);
        char *result = registry_dispatch("kanban_unblock", json_args, "");
        if (result) {
            printf("Result: %s\n", result);
            free(result);
        } else {
            cli_display_error("kanban_unblock returned NULL");
        }
        return;
    }

    if (strncmp(sub, "link", 4) == 0) {
        const char *rest = sub + 4;
        while (*rest == ' ') rest++;
        char parent[256], child[256];
        if (sscanf(rest, "%255s %255s", parent, child) != 2) {
            printf("Usage: /kanban link <parent_id> <child_id>\n");
            return;
        }
        char json_args[1024];
        snprintf(json_args, sizeof(json_args),
                 "{\"parent_id\":\"%s\",\"child_id\":\"%s\"}", parent, child);
        char *result = registry_dispatch("kanban_link", json_args, "");
        if (result) {
            printf("Result: %s\n", result);
            free(result);
        } else {
            cli_display_error("kanban_link returned NULL");
        }
        return;
    }

    printf("Unknown kanban subcommand: %s\n  Use: list, show <id>, create <title>, complete <id>, block <id>, unblock <id>, link <parent> <child>\n", sub);
}

/* JSON-escape a string into a fixed-size buffer (for safe JSON injection) */
void json_escape_arg(const char *src, char *dst, size_t dst_sz) {
    size_t pos = 0;
    for (const char *s = src; *s && pos < dst_sz - 2; s++) {
        switch (*s) {
            case '"':  if (pos + 2 < dst_sz) { dst[pos++] = '\\'; dst[pos++] = '"'; } break;
            case '\\': if (pos + 2 < dst_sz) { dst[pos++] = '\\'; dst[pos++] = '\\'; } break;
            case '\n': if (pos + 2 < dst_sz) { dst[pos++] = '\\'; dst[pos++] = 'n'; } break;
            case '\t': if (pos + 2 < dst_sz) { dst[pos++] = '\\'; dst[pos++] = 't'; } break;
            case '\r': if (pos + 2 < dst_sz) { dst[pos++] = '\\'; dst[pos++] = 'r'; } break;
            default:   dst[pos++] = *s; break;
        }
    }
    dst[pos] = '\0';
}

