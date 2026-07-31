#ifndef HERMES_CLI_H
#define HERMES_CLI_H

/* Focused CLI command-system header.
 * Extracted from the hermes.h umbrella so command-table consumers
 * (cli/commands.c, cli/cli.c, cli_cmd_*.c) no longer pull in the god header.
 * Requires agent_state_t + hermes_config_t from the slim core types. */

#include "hermes_core_types.h"

/* Command system */
typedef struct command_def_t {
    const char *name;
    const char *alias;
    const char *description;
    const char *category;   /* Grouping category for help display (e.g. "Session", "Config") */
    const char *args_hint;  /* Usage hint for args (e.g. "[key] [val]" or "<session_id>") */
    void (*handler)(const char *args, agent_state_t *state);
    const char *subcommands;/* Comma-separated subcommand list, e.g. "list,show,set", NULL if none */
    bool cli_only;          /* Only available in CLI mode */
    bool gateway_only;      /* Only available in gateway/messaging mode */
    const char *gateway_config_gate; /* If set, command is available in gateway when this config dotpath is truthy */
} command_def_t;
bool commands_dispatch(const char *input, agent_state_t *state);
bool commands_try_skill(const char *input, agent_state_t *state);
bool commands_try_quick(const char *input, agent_state_t *state);
void commands_set_quick_config(const hermes_config_t *cfg);
const command_def_t *commands_resolve(const char *input);
const command_def_t *commands_get_all(void);
int commands_count(void);
const char *commands_list_json(void);

#endif /* HERMES_CLI_H */

/* /command runtime toggles */
void commands_set_verbose(int level);
void commands_set_yolo(bool enabled);
void commands_set_fast(bool enabled);
int  commands_get_verbose(void);
bool commands_get_yolo(void);
bool commands_get_fast(void);

/* CLI entry point */
int cli_main(int argc, char **argv);
