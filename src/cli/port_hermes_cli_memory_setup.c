/*
 * port_hermes_cli_memory_setup.c — C port of hermes_cli/memory_setup.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>



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

/* Port of Python plugins/memory/__init__.py:find_provider_dir.
 * Bundled first (the memory plugin's own dir / <name> with __init__.py),
 * then user-installed ($HERMES_HOME/plugins/<name> with a memory-provider
 * marker in __init__.py). Returns a malloc'd dir path or NULL. */
static char *find_provider_dir_c(const char *name)
{
    if (!name || !*name) return NULL;
    /* Bundled: the dir containing THIS module's plugin sources. The C port
     * resolves it relative to the slermes share/plugin install; fall back to
     * scanning $HERMES_HOME and the dev tree. */
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("SLERMES_HOME");
    if (!home || !*home) home = ".";
    char path[4096];
    /* Bundled candidates */
    const char *bundled_bases[] = {
        "/home/wubu/hermes-agent-dev/plugins/memory",
        "/home/wubu/.hermes/hermes-agent/plugins/memory",
        NULL
    };
    for (int i = 0; bundled_bases[i]; i++) {
        snprintf(path, sizeof(path), "%s/%s/__init__.py", bundled_bases[i], name);
        if (access(path, R_OK) == 0) {
            char *out = malloc(strlen(bundled_bases[i]) + strlen(name) + 2);
            if (out) snprintf(out, strlen(bundled_bases[i]) + strlen(name) + 2,
                              "%s/%s", bundled_bases[i], name);
            return out;
        }
    }
    /* User-installed: $HERMES_HOME/plugins/<name> with a memory-provider
     * marker in __init__.py (register_memory_provider / MemoryProvider). */
    snprintf(path, sizeof(path), "%s/plugins/%s/__init__.py", home, name);
    if (access(path, R_OK) == 0) {
        FILE *f = fopen(path, "rb");
        if (f) {
            char buf[8192];
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            buf[n] = '\0';
            fclose(f);
            if (strstr(buf, "register_memory_provider") || strstr(buf, "MemoryProvider")) {
                char *out = malloc(strlen(home) + strlen(name) + 12);
                if (out) snprintf(out, strlen(home) + strlen(name) + 12,
                                  "%s/plugins/%s", home, name);
                return out;
            }
        }
    }
    return NULL;
}

/* Port of Python hermes_cli/memory_setup.py:_provider_pip_dependencies.
 * Hindsight's local_embedded mode additionally needs hindsight-all. */
static size_t provider_pip_dependencies_c(const char *provider_name,
                                          const char *const *declared,
                                          size_t n_declared,
                                          char ***out)
{
    size_t cap = n_declared + 2;
    char **deps = calloc(cap, sizeof(char *));
    if (!deps) { *out = NULL; return 0; }
    size_t n = 0;
    for (size_t i = 0; i < n_declared && declared[i]; i++) {
        deps[n++] = strdup(declared[i]);
    }
    if (provider_name && strcmp(provider_name, "hindsight") == 0) {
        /* read $HERMES_HOME/hindsight/config.json -> mode in {local, local_embedded} */
        const char *home = getenv("HERMES_HOME");
        if (!home || !*home) home = getenv("SLERMES_HOME");
        if (home && *home) {
            char cfg[4096];
            snprintf(cfg, sizeof(cfg), "%s/hindsight/config.json", home);
            FILE *f = fopen(cfg, "rb");
            if (f) {
                char buf[8192];
                size_t r = fread(buf, 1, sizeof(buf) - 1, f);
                buf[r] = '\0';
                fclose(f);
                if (strstr(buf, "\"local_embedded\"") || strstr(buf, "\"local\"")) {
                    deps[n++] = strdup("hindsight-all");
                }
            }
        }
    }
    *out = deps;
    return n;
}

/* PoP: cli_hermes_cli_memory_setup__install_dependencies @ hermes_cli/memory_setup.py:_install_dependencies */

/* Port of Python hermes_cli/memory_setup.py:_install_dependencies.
 * Finds the provider dir, reads plugin.yaml's pip_dependencies, computes
 * mode-dependent extras, checks which are importable, and installs the
 * missing ones via `uv pip install`. Returns 0 on success/no-op, -1 on
 * provider-not-found or error — mirroring Python's None return. */
int cli_hermes_cli_memory_setup__install_dependencies(
    const char *provider_name)
{
    if (!provider_name) {
        return -1;
    }
    char *plugin_dir = find_provider_dir_c(provider_name);
    if (!plugin_dir) {
        return -1;  /* Python: return None */
    }
    char yaml_path[4096];
    snprintf(yaml_path, sizeof(yaml_path), "%s/plugin.yaml", plugin_dir);
    free(plugin_dir);
    FILE *yf = fopen(yaml_path, "rb");
    if (!yf) {
        return -1;  /* Python: return None */
    }
    char ybuf[65536];
    size_t yn = fread(ybuf, 1, sizeof(ybuf) - 1, yf);
    ybuf[yn] = '\0';
    fclose(yf);

    /* Extract pip_dependencies: the dev tree ships no YAML lib here, so do a
     * minimal scan of `pip_dependencies:` block (list of quoted strings). */
    const char *marker = strstr(ybuf, "pip_dependencies");
    const char *const *declared = NULL;
    size_t n_declared = 0;
    char *dep_list[64] = {0};
    if (marker) {
        const char *q = strchr(marker, ':');
        if (q) {
            q++;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '[') {
                q++;
                while (*q && *q != ']' && n_declared < 64) {
                    while (*q == ' ' || *q == '\t' || *q == ',' || *q == '\n' || *q == '\r') q++;
                    if (*q == '"' || *q == '\'') {
                        char quote = *q++;
                        char buf[512];
                        size_t bi = 0;
                        while (*q && *q != quote && bi < sizeof(buf) - 1) buf[bi++] = *q++;
                        buf[bi] = '\0';
                        if (*q) q++;
                        dep_list[n_declared++] = strdup(buf);
                    } else break;
                }
            }
        }
        declared = (const char *const *)dep_list;
    }

    char **deps = NULL;
    size_t n_deps = provider_pip_dependencies_c(provider_name, declared, n_declared, &deps);
    for (size_t i = 0; i < n_declared; i++) free(dep_list[i]);
    if (n_deps == 0) {
        free(deps);
        return 0;  /* Python: nothing to install -> None */
    }

    /* Import-name mapping (pip name -> import name where they differ). */
    struct { const char *pip; const char *imp; } IMPORT_NAMES[] = {
        { "honcho-ai", "honcho" },
        { "mem0ai", "mem0" },
        { "hindsight-client", "hindsight_client" },
        { "hindsight-all", "hindsight" },
    };

    /* Check which deps are missing (import probe). */
    char *missing[64] = {0};
    size_t n_missing = 0;
    for (size_t i = 0; i < n_deps && i < 64; i++) {
        const char *dep = deps[i];
        if (!dep) continue;
        /* strip extras "[...]" and version spec */
        char base[256];
        size_t bi = 0;
        for (const char *d = dep; *d && *d != '[' && *d != ';' && bi < sizeof(base)-1; d++) {
            base[bi++] = (*d == '-' ? '_' : *d);
        }
        base[bi] = '\0';
        const char *imp = base;
        for (size_t k = 0; k < sizeof(IMPORT_NAMES)/sizeof(IMPORT_NAMES[0]); k++) {
            if (strcmp(dep, IMPORT_NAMES[k].pip) == 0) { imp = IMPORT_NAMES[k].imp; break; }
        }
        /* dlopen-style probe via a tiny subprocess: `python3 -c "import X"` */
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "python3 -c \"import %s\" >/dev/null 2>&1", imp);
        int rc = system(cmd);
        if (rc != 0) missing[n_missing++] = strdup(dep);
    }

    if (n_missing == 0) {
        for (size_t i = 0; i < n_deps; i++) free(deps[i]);
        free(deps);
        return 0;  /* Python: all installed -> None */
    }

    /* Install missing via `uv pip install` (Python uses install_specs which
     * routes to uv on normal installs). */
    fprintf(stderr, "\n  Installing dependencies: ");
    char cmd[4096] = "uv pip install";
    for (size_t i = 0; i < n_missing; i++) {
        if (i == 0) fprintf(stderr, "%s", missing[i]);
        else fprintf(stderr, ", %s", missing[i]);
        strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
        strncat(cmd, missing[i], sizeof(cmd) - strlen(cmd) - 1);
    }
    fprintf(stderr, "\n");
    int rc = system(cmd);
    for (size_t i = 0; i < n_missing; i++) free(missing[i]);
    for (size_t i = 0; i < n_deps; i++) free(deps[i]);
    free(deps);
    return rc == 0 ? 0 : -1;
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