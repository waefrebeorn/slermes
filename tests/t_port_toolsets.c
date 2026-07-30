/* t_port_toolsets.c — oracle harness for the toolsets + platform-tools
 * stack. Reads fixture lines (see tests/oracle/fixtures/toolsets/cases.in),
 * emits one JSON line per case matching tests/sta_oracle_toolsets.py
 * (json.dumps sort_keys=True: {"case": ..., "out": ...}).
 */
#include "toolsets.h"
#include "platform_tools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void emit_str_json(const char *s) {
    putchar('"');
    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else if (*p == '\n') fputs("\\n", stdout);
        else putchar(*p);
    }
    putchar('"');
}

static void emit_list(char **v, size_t n) {
    putchar('[');
    for (size_t i = 0; i < n; i++) {
        if (i) fputs(", ", stdout);
        emit_str_json(v[i]);
    }
    putchar(']');
}

static void emit_case_head(const char *case_str) {
    fputs("{\"case\": ", stdout);
    emit_str_json(case_str);
    fputs(", \"out\": ", stdout);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 2; }

    /* Deterministic oracle contract: empty HERMES home (no auth store, no
     * .env/config credentials), mirroring the Python oracle's neutralized
     * _xai_credentials_present / plugin hooks. */
    mkdir("/tmp/oracle_toolsets_home", 0755);
    setenv("HERMES_HOME", "/tmp/oracle_toolsets_home", 1);
    unsetenv("XAI_API_KEY");
    unsetenv("HASS_TOKEN");

    FILE *fh = fopen(argv[1], "r");
    if (!fh) { perror("fixture"); return 2; }
    char line[8192];
    while (fgets(line, sizeof(line), fh)) {
        size_t L = strlen(line);
        while (L && (line[L - 1] == '\n' || line[L - 1] == '\r')) line[--L] = '\0';
        if (!L) continue;
        char *sp = strchr(line, ' ');
        if (!sp) continue;
        *sp = '\0';
        const char *kind = line;
        char *rest = sp + 1;

        if (strcmp(kind, "resolve") == 0) {
            char case_str[8300];
            snprintf(case_str, sizeof(case_str), "resolve %s", rest);
            size_t n = 0;
            char **v = toolsets_resolve(rest, false, &n);
            emit_case_head(case_str);
            emit_list(v, n);
            fputs("}\n", stdout);
            toolsets_free_list(v, n);
        } else if (strcmp(kind, "validate") == 0) {
            char case_str[8300];
            snprintf(case_str, sizeof(case_str), "validate %s", rest);
            emit_case_head(case_str);
            fputs(toolsets_validate(rest) ? "true" : "false", stdout);
            fputs("}\n", stdout);
        } else if (strcmp(kind, "bundle") == 0) {
            char case_str[8300];
            snprintf(case_str, sizeof(case_str), "bundle %s", rest);
            size_t n = 0;
            char **v = toolsets_bundle_non_core_tools(rest, &n);
            emit_case_head(case_str);
            emit_list(v, n);
            fputs("}\n", stdout);
            toolsets_free_list(v, n);
        } else if (strcmp(kind, "platform") == 0) {
            char *label = rest;
            char *sp2 = strchr(label, ' ');
            if (!sp2) continue;
            *sp2 = '\0';
            char *platform = sp2 + 1;
            char *sp3 = strchr(platform, ' ');
            if (!sp3) continue;
            *sp3 = '\0';
            char *cfg_json = sp3 + 1;
            json_t *cfg = json_parse(cfg_json, NULL);
            size_t n = 0;
            char **v = platform_tools_get(cfg, platform, true, &n);
            char case_str[256];
            snprintf(case_str, sizeof(case_str), "platform %s", label);
            emit_case_head(case_str);
            emit_list(v, n);
            fputs("}\n", stdout);
            platform_tools_free_list(v, n);
            json_free(cfg);
        }
    }
    fclose(fh);
    return 0;
}
