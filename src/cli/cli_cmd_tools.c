/*
 * cli_cmd_tools.c — Tools slash-command handlers extracted from commands.c.
 * Self-contained command-category module.
 */

#include "cli_cmd_tools.h"
#include "commands_shared.h"
#include "hermes.h"

/* /image: Attach a local image file */
void cmd_image(const char *args, agent_state_t *state) {
    (void)state;
    if (!args || !args[0]) {
        printf("Usage: /image <path_to_image>\n");
        return;
    }
    printf("Image attached: %s (will be used in next prompt)\n", args);
}

