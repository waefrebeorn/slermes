/*
 * cli_cmd_help.c — Help slash-command handlers extracted from commands.c.
 * Self-contained command-category module.
 */

#include "hermes_display.h"
#include "cli_cmd_help.h"
#include "commands_shared.h"
#include "hermes_core_types.h"
#include "hermes_cli.h"

void cmd_help(const char *args, agent_state_t *state) {
    (void)state;
    if (args && args[0]) {
        /* Help for specific command */
        const command_def_t *cmd = commands_resolve(args);
        if (cmd) {
            /* Hide gateway-only commands from CLI help */
            if (cmd->gateway_only) {
                printf("  Unknown command: %s\n", args);
                return;
            }
            printf("  %s", cmd->name);
            if (cmd->alias) printf(" (%s)", cmd->alias);
            printf("\n    %s\n", cmd->description);
            if (cmd->args_hint)
                printf("    Usage: %s %s\n", cmd->name, cmd->args_hint);
            if (cmd->subcommands)
                printf("    Subcommands: %s\n", cmd->subcommands);
        } else {
            printf("  Unknown command: %s\n", args);
        }
        return;
    }

    /* Build help content as aligned lines */
    char content[8192];
    int pos = 0;
    int count = commands_count();
    const command_def_t **cmds = NULL;
    if (count > 0) {
        cmds = (const command_def_t **)calloc((size_t)count, sizeof(command_def_t *));
        for (int i = 0; i < count; i++)
            cmds[i] = commands_get_all() + i;
    }

    /* Find max command name width for alignment */
    int max_name = 0;
    for (int i = 0; i < count; i++) {
        int len = (int)strlen(cmds[i]->name);
        if (cmds[i]->alias) {
            int alen = (int)strlen(cmds[i]->alias) + 3; /* " (/x)" */
            if (alen > len) len = alen;
        }
        if (len > max_name) max_name = len;
    }
    if (max_name < 8) max_name = 8;
    if (max_name > 30) max_name = 30;

    /* Build dynamic categories from command category fields (CL06 parity).
     * Commands without a category field (NULL) are grouped under "Other".
     * Collect unique categories preserving order of first appearance. */
    const int MAX_CATS = 32;
    const char *categories[MAX_CATS];
    int cat_cmd_start[MAX_CATS];
    int cat_count = 0;

    for (int i = 0; i < count && cmds[i]->name; i++) {
        const char *cat = cmds[i]->category ? cmds[i]->category : "Other";
        bool found = false;
        for (int c = 0; c < cat_count; c++) {
            if (strcmp(categories[c], cat) == 0) {
                found = true;
                break;
            }
        }
        if (!found && cat_count < MAX_CATS) {
            categories[cat_count] = cat;
            cat_cmd_start[cat_count] = i;
            cat_count++;
        }
    }

    const char *text_color = "#FFF8DC";

    for (int c = 0; c < cat_count; c++) {
        /* Category header in accent color */
        if (pos > 0)
            pos += snprintf(content + pos, sizeof(content) - (size_t)pos, "\n");
        pos += snprintf(content + pos, sizeof(content) - (size_t)pos,
            "\x1B[1;38;2;255;191;0m  ── %s ──\x1B[0m\n",
            categories[c]);

        /* Commands in this category */
        for (int i = cat_cmd_start[c]; i < count && cmds[i]->name; i++) {
            /* Check if we've moved to a different category */
            if (c + 1 < cat_count && i >= cat_cmd_start[c + 1]) break;
            if (!cmds[i]->name) break;

            /* Skip gateway-only commands in CLI help */
            if (cmds[i]->gateway_only) continue;

            char cmd_line[128];
            if (cmds[i]->alias)
                snprintf(cmd_line, sizeof(cmd_line), "%s (%s)",
                         cmds[i]->name, cmds[i]->alias);
            else
                snprintf(cmd_line, sizeof(cmd_line), "%s", cmds[i]->name);

            int pad = max_name - (int)strlen(cmd_line) + 2;
            if (pad < 1) pad = 1;
            pos += snprintf(content + pos, sizeof(content) - (size_t)pos,
                "    %s%*s%s\n", cmd_line, pad, "", cmds[i]->description);
        }
    }
    free(cmds);

    /* Display in a panel */
    const char *border = "#CD7F32";
    display_panel_hex(" Commands ", content, border);
    display_printf_hex(text_color, DISPLAY_DIM,
        "  Type /help <command> for details on a specific command.\n");
}


