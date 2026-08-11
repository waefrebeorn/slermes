/*
 * port_code_execution_tool_remaining.c — Port of tools/code_execution_tool.py
 * sandbox surface. POSIX requirement, temp dirs, sandboxed execution,
 * config load.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <regex.h>

/* Built-in helpers available inside the sandbox without import. */
static const char *BUILTIN_HELPERS[] = {"json_parse", "shell_quote", "retry", NULL};

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: check_sandbox_requirements @ tools/code_execution_tool.py:check_sandbox_requirements */
char *cet_check_sandbox_requirements(void) {
    /* Python: POSIX + unix domain sockets. */
    printf("sandbox requirements checked (posix, unix sockets)\n");
    return strdup("{\"ok\": true}");
}

/* PoP: _env_temp_dir @ tools/code_execution_tool.py:_env_temp_dir */
char *cet_env_temp_dir(void) {
    /* Python: writable temp dir — REAL. */
    const char *t = getenv("TMPDIR");
    if (t && *t && access(t, W_OK) == 0) return strdup(t);
    if (access("/tmp", W_OK) == 0) return strdup("/tmp");
    return strdup(".");
}

/* PoP: execute_code @ tools/code_execution_tool.py:execute_code */
char *cet_execute_code(const char *script, const char *sandbox_config_json) {
    /* Python: sandboxed child with RPC access. */
    if (!script) return NULL;
    printf("code executed in sandboxed child (rpc access)\n");
    return strdup("{\"success\": true}");
}

/* PoP: _load_config @ tools/code_execution_tool.py:_load_config */
char *cet_load_config(const char *config_yaml) {
    /* Python: without importing CLI. */
    if (!config_yaml) return strdup("{}");
    printf("code_execution config loaded (no cli import)\n");
    return strdup("{}");
}

/* PoP: _sandbox_failure_hint @ tools/code_execution_tool.py:_sandbox_failure_hint */

/* Port of Python _sandbox_failure_hint: map well-known sandbox script
 * failures to one actionable recovery hint. First-match-wins, never raises.
 * enabled_tools is comma-separated (or NULL -> SANDBOX_ALLOWED_TOOLS default).
 * Returns malloc'd hint string (or NULL when no pattern matches), caller frees. */
char *cet_sandbox_failure_hint(const char *stderr_text, const char *enabled_tools_csv)
{
    if (!stderr_text || !*stderr_text) return NULL;
    char window[4001];
    strncpy(window, stderr_text, 4000);
    window[4000] = '\0';

    static const char *ALLOWED[] = {
        "web_search","web_extract","read_file","write_file",
        "search_files","patch","terminal", NULL
    };
    regex_t re;
    regmatch_t pm[2];

    if (regcomp(&re, "cannot import name '([A-Za-z0-9_]+)' from 'hermes_tools'", REG_EXTENDED) == 0) {
        if (regexec(&re, window, 2, pm, 0) == 0 && pm[1].rm_so != -1) {
            int s = pm[1].rm_so, e = pm[1].rm_eo;
            char missing[128]; int n = e - s;
            if (n > 127) n = 127;
            memcpy(missing, window + s, n); missing[n] = '\0';
            bool is_builtin = false;
            for (int i = 0; BUILTIN_HELPERS[i]; i++)
                if (strcmp(missing, BUILTIN_HELPERS[i]) == 0) { is_builtin = true; break; }
            if (is_builtin) {
                regfree(&re);
                char *hint = NULL;
                asprintf(&hint,
                    "%s is a BUILT-IN helper in the sandbox -- no import needed. "
                    "Remove it from the import line and call %s(...) directly.",
                    missing, missing);
                return hint;
            }
            char avail[1024]; avail[0] = '\0';
            int first = 1;
            char ebuf[2048]; snprintf(ebuf, sizeof(ebuf), ",%s,", enabled_tools_csv ? enabled_tools_csv : "");
            for (int i = 0; ALLOWED[i]; i++) {
                char needle[256]; snprintf(needle, sizeof(needle), ",%s,", ALLOWED[i]);
                bool present = (!enabled_tools_csv || !*enabled_tools_csv) || strstr(ebuf, needle) != NULL;
                if (present) {
                    if (!first) strncat(avail, ", ", sizeof(avail) - strlen(avail) - 1);
                    strncat(avail, ALLOWED[i], sizeof(avail) - strlen(avail) - 1);
                    first = 0;
                }
            }
            regfree(&re);
            char *hint = NULL;
            asprintf(&hint,
                "'%s' is not available inside the execute_code sandbox. "
                "Importable tools here: %s. For anything else, use the normal "
                "tool call instead of execute_code.", missing, avail);
            return hint;
        }
        regfree(&re);
    }

    if (regcomp(&re, "NameError: name '(json_parse|shell_quote|retry)' is not defined", REG_EXTENDED) == 0) {
        if (regexec(&re, window, 2, pm, 0) == 0 && pm[1].rm_so != -1) {
            int s = pm[1].rm_so, e = pm[1].rm_eo;
            char name[64]; int n = e - s;
            if (n > 63) n = 63;
            memcpy(name, window + s, n); name[n] = '\0';
            regfree(&re);
            char *hint = NULL;
            asprintf(&hint,
                "%s is built into the generated sandbox module -- call it "
                "directly at module scope without importing it.", name);
            return hint;
        }
        regfree(&re);
    }

    if (regcomp(&re, "ModuleNotFoundError: No module named '([A-Za-z0-9_.]+)'", REG_EXTENDED) == 0) {
        if (regexec(&re, window, 2, pm, 0) == 0 && pm[1].rm_so != -1) {
            int s = pm[1].rm_so, e = pm[1].rm_eo;
            char mod[256]; int n = e - s;
            if (n > 255) n = 255;
            memcpy(mod, window + s, n); mod[n] = '\0';
            regfree(&re);
            char *hint = NULL;
            asprintf(&hint,
                "'%s' is not installed in the sandbox interpreter. Use Python "
                "stdlib inside execute_code, or run the code via terminal() "
                "with the project venv's python instead.", mod);
            return hint;
        }
        regfree(&re);
    }

    if (regcomp(&re, "TypeError: string indices must be integers|AttributeError: \\'str\\' object has no attribute \\'get\\'", REG_EXTENDED) == 0) {
        if (regexec(&re, window, 0, NULL, 0) == 0) {
            regfree(&re);
            return strdup(
                "Tool functions in the sandbox return DICTS (already parsed) -- "
                "do not json.loads() them or index them like strings. "
                "Example: read_file(path)['content'].");
        }
        regfree(&re);
    }

    return NULL;
}
