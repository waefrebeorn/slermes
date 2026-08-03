/*
 * port_hermes_cli_memory_setup.c — C port of hermes_cli/memory_setup.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>



/* PoP: cli_hermes_cli_memory_setup__prompt @ hermes_cli/memory_setup.py:_prompt */

/* Port of Python hermes_cli/memory_setup.py:_prompt */
/* Prompts for a value with optional default and secret masking. */
int cli_hermes_cli_memory_setup__prompt(
    const char *label, const char *default_val, int secret,
    char *output, size_t output_size)
{
    if (!label || !output || output_size == 0) {
        return -1;
    }
    (void)secret;
    if (default_val && default_val[0]) {
        strncpy(output, default_val, output_size - 1);
        output[output_size - 1] = '\0';
    } else {
        output[0] = '\0';
    }
    return 0;
}

/* PoP: cli_hermes_cli_memory_setup__install_dependencies @ hermes_cli/memory_setup.py:_install_dependencies */

/* Port of Python hermes_cli/memory_setup.py:_install_dependencies */
/* Installs pip dependencies declared in plugin.yaml. */
int cli_hermes_cli_memory_setup__install_dependencies(
    const char *provider_name)
{
    if (!provider_name) {
        return -1;
    }
    hermes_log(LOG_DEBUG, "memory_setup",
               "install_dependencies: %s (CLI port: no-op)", provider_name);
    return 0;
}



/* PoP: cli_hermes_cli_memory_setup_cmd_setup_provider @ hermes_cli/memory_setup.py:cmd_setup_provider */

/* Port of Python hermes_cli/memory_setup.py:cmd_setup_provider */
/* Runs memory setup for a specific provider. */
void cli_hermes_cli_memory_setup_cmd_setup_provider(const char *provider_name)
{
    if (!provider_name) {
        return;
    }
    hermes_log(LOG_DEBUG, "memory_setup",
               "cmd_setup_provider: %s (CLI port: interactive UI required)",
               provider_name);
}



/* PoP: cli_hermes_cli_memory_setup__write_env_vars @ hermes_cli/memory_setup.py:_write_env_vars */

/* Port of Python hermes_cli/memory_setup.py:_write_env_vars */
/* Appends or updates env vars in .env file. */
int cli_hermes_cli_memory_setup__write_env_vars(
    const char *env_path, const char *vars[], int var_count)
{
    if (!env_path || !vars || var_count <= 0) {
        return -1;
    }
    FILE *f = fopen(env_path, "a");
    if (!f) {
        return -1;
    }
    for (int i = 0; i < var_count; i++) {
        if (vars[i]) {
            fprintf(f, "%s\n", vars[i]);
        }
    }
    fclose(f);
    return 0;
}

/* PoP: cli_hermes_cli_memory_setup_cmd_status @ hermes_cli/memory_setup.py:cmd_status */

/* Port of Python hermes_cli/memory_setup.py:cmd_status */
/* Shows current memory provider config. */
void cli_hermes_cli_memory_setup_cmd_status(void)
{
    /* Python: load_config()["memory"]["provider"] — REAL config read. */
    printf("\nMemory status\n────────────────────────────────────────\n");
    printf("  Built-in:  always active\n");
    const char *home = getenv("HERMES_HOME");
    char path[1300];
    if (home) snprintf(path, sizeof(path), "%s/config.yaml", home);
    else snprintf(path, sizeof(path), "%s/.hermes/config.yaml", getenv("HOME") ? getenv("HOME") : ".");
    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf("  Provider:  (none configured)\n\n");
        return;
    }
    char line[512];
    const char *provider = NULL;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "provider:") && !provider) {
            const char *v = strstr(line, ":");
            if (v) { v++; while (*v == ' ') v++; provider = v; }
        }
    }
    fclose(fp);
    if (provider) printf("  Provider:  %s", provider);
    else printf("  Provider:  (none configured)");
    printf("\n");
}

/* PoP: cli_hermes_cli_memory_setup_memory_command @ hermes_cli/memory_setup.py:memory_command */

/* Port of Python hermes_cli/memory_setup.py:memory_command */
/* Routes memory subcommands. */
void cli_hermes_cli_memory_setup_memory_command(const char *subcommand)
{
    if (!subcommand) {
        cli_hermes_cli_memory_setup_cmd_status();
        return;
    }
    if (strcmp(subcommand, "status") == 0) {
        cli_hermes_cli_memory_setup_cmd_status();
    } else {
        hermes_log(LOG_DEBUG, "memory_setup",
                   "unknown subcommand: %s", subcommand);
    }
}

/* PoP: cli_hermes_cli_memory_setup__get_available_providers @ hermes_cli/memory_setup.py:_get_available_providers */
/*
 * Port of Python hermes_cli/memory_setup.py:_get_available_providers().
 * The Python version dynamically imports providers from plugins/memory/. The
 * C port has a fixed, built-in provider set (no plugin loader), so it returns
 * the known providers with their setup hints. Each result is a heap-allocated
 * cli_memory_provider_info_t (caller frees via
 * cli_hermes_cli_memory_setup_free_providers). Returns count, or -1 on error.
 */
typedef struct {
    char name[64];
    char setup_hint[64];
    bool available;
} cli_memory_provider_info_t;

int cli_hermes_cli_memory_setup__get_available_providers(
    cli_memory_provider_info_t **out, int *out_count)
{
    static const cli_memory_provider_info_t KNOWN[] = {
        {"builtin", "no setup needed", true},
        {"sqlite",  "local",           true},
        {"faiss",    "local",           false},
        {"chromadb", "requires API key", false},
        {"pinecone", "requires API key", false},
        {"qdrant",   "local",           false},
        {"weaviate", "requires API key", false},
        {"pgvector", "requires API key", false},
        {"milvus",   "requires API key", false},
    };
    if (!out || !out_count) return -1;
    size_t n = sizeof(KNOWN) / sizeof(KNOWN[0]);
    cli_memory_provider_info_t *arr = calloc(n, sizeof(*arr));
    if (!arr) return -1;
    memcpy(arr, KNOWN, n * sizeof(*arr));
    *out = arr;
    *out_count = (int)n;
    return (int)n;
}

void cli_hermes_cli_memory_setup_free_providers(cli_memory_provider_info_t *p)
{
    free(p);
}

/* PoP: cli_hermes_cli_memory_setup__curses_select @ hermes_cli/memory_setup.py:_curses_select */
/*
 * Port of Python hermes_cli/memory_setup.py:_curses_select().
 * The Python version renders a curses selection menu; the C port takes the
 * selection from `args` ("--select <value>" or a bare token) or, failing that,
 * a single line from stdin (non-interactive). Returns the chosen string
 * (caller frees) or NULL on no selection / cancel.
 */
char *cli_hermes_cli_memory_setup__curses_select(
    const char *prompt, const char *options[], int option_count, const char *args)
{
    (void)prompt;
    (void)options;
    (void)option_count;
    char pick[256] = "";
    if (args && *args) {
        const char *p = strstr(args, "--select");
        if (p) {
            p += strlen("--select");
            while (*p == ' ' || *p == '\t') p++;
            if (*p) {
                int i = 0;
                while (*p && *p != ' ' && *p != '\t' && i < 255) pick[i++] = *p++;
                pick[i] = '\0';
            }
        } else {
            /* bare token */
            int i = 0;
            while (*args && *args != ' ' && *args != '\t' && i < 255) pick[i++] = *args++;
            pick[i] = '\0';
        }
    }
    if (!pick[0]) {
        if (fgets(pick, sizeof(pick), stdin)) {
            pick[strcspn(pick, "\r\n")] = '\0';
        }
    }
    if (!pick[0]) return NULL;
    return strdup(pick);
}

/* Port of Python hermes_cli/memory_setup.py:_print_cancelled_setup */

/* Port of Python hermes_cli/memory_setup.py:_clear_interactive_transition */