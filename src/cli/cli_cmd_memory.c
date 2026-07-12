/*
 * cli_cmd_memory.c — Memory slash-command handlers extracted from commands.c.
 * Self-contained command-category module.
 */

#include "cli_cmd_memory.h"
#include "commands_shared.h"
#include "hermes.h"

/* /memory: Memory setup, status, and provider management */
/* AG26: Port of Python hermes_cli/main.py:cmd_memory(). */
void cmd_memory(const char *args, agent_state_t *state) {
    if (!args || !args[0]) {
        printf("Memory configuration management.\n");
        printf("Usage: /memory status           — Show current memory config\n");
        printf("       /memory providers         — List known memory providers\n");
        printf("       /memory setup <provider>  — Set active memory provider\n");
        return;
    }
    const char *sub = args;
    while (*sub == ' ') sub++;

    if (strcmp(sub, "status") == 0) {
        hermes_config_t cfg;
        /* Resolve home directory */
        char home_dir[1024];
        memset(home_dir, 0, sizeof(home_dir));
        const char *home_env = NULL;

        if (state->hermes_home[0]) {
            snprintf(home_dir, sizeof(home_dir), "%s", state->hermes_home);
        } else {
            home_env = getenv("SLERMES_HOME");
            if (!home_env) home_env = getenv("HOME");
            if (!home_env) { printf("Error: Cannot determine Hermes home\n"); return; }

            snprintf(home_dir, sizeof(home_dir), "%s/.slermes", home_env);
            if (access(home_dir, F_OK) != 0) {
                snprintf(home_dir, sizeof(home_dir), "%s/.hermes", home_env);
            }
        }

        if (access(home_dir, F_OK) == 0) {
            if (hermes_config_load(&cfg, home_dir)) {
                show_section_memory(&cfg);
            } else {
                printf("Error: Could not load config from %s\n", home_dir);
            }
        } else {
            printf("No Hermes home directory found at %s\n", home_dir);
        }
        return;
    }

    if (strcmp(sub, "providers") == 0) {
        printf("Memory providers:\n");
        printf("  (built-in)  MEMORY.md / USER.md (always active, default)\n");
        printf("  honcho      Cloud-based memory with semantic search\n");
        printf("  mem0        Personalized AI memory with vector store\n");
        printf("  supermemory  Persistent memory with semantic retrieval\n");
        printf("  hindsight   Task-aware episodic memory\n");
        printf("\nTo activate: /memory setup <provider>\n");
        printf("C memory plugins: .so files in plugins/memory/ are auto-detected.\n");
        return;
    }

    if (strncmp(sub, "setup ", 6) == 0) {
        const char *provider = sub + 6;
        while (*provider == ' ') provider++;
        if (!*provider) {
            printf("Usage: /memory setup <provider>\n");
            printf("Example: /memory setup honcho\n");
            return;
        }
        printf("To activate '%s' memory provider:\n", provider);
        printf("  1. Edit ~/.slermes/config.yaml with:\n");
        printf("     memory:\n");
        printf("       provider: %s\n", provider);
        printf("  2. Set required API keys in ~/.slermes/.env\n");
        printf("  3. Start a new session to activate.\n");
        return;
    }

    printf("Unknown subcommand: '%s'\n", sub);
    printf("Usage: /memory status | providers | setup <provider>\n");
}

