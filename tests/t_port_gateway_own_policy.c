/* Oracle harness: gateway/run.py:_own_policy_open_startup_violation
 * vs live Python. Reads a config.yaml fixture from argv[1], writes it to
 * $SLERMES_HOME/config.yaml, loads the global gateway config, then evaluates
 * gw_own_policy_open_startup_violation(). Emits a single JSON line:
 *   {"out":"<reason>"}   or   {"out":null}
 * Must match the Python oracle exactly (byte-for-byte on the JSON string). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_gateway_config.h"
#include "port_gateway_run_deps.h"

static const char *js(const char *s) {
    static char buf[1024];
    char *q = buf; *q++ = '"';
    for (const char *p = s; p && *p && q - buf < 1000; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else *q++ = c;
    }
    *q++ = '"'; *q = '\0';
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) { printf("{\"out\":null}\n"); return 0; }
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = ".";

    /* Optional env file: env.<casename> with KEY=VALUE lines, exported before
     * evaluating (mirrors Python's os.getenv folding). */
    const char *fixture = argv[1];
    char envarg[4096];
    snprintf(envarg, sizeof(envarg), "%s", fixture);
    char *dot = strrchr(envarg, '.');
    if (dot) { *dot = '\0'; }
    strcat(envarg, ".env");
    FILE *fenv = fopen(envarg, "r");
    if (fenv) {
        char line[512];
        while (fgets(line, sizeof(line), fenv)) {
            size_t L = strlen(line);
            while (L && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
            if (!*line || *line == '#') continue;
            char *eq = strchr(line, '=');
            if (eq) { *eq = '\0'; setenv(line, eq + 1, 1); }
        }
        fclose(fenv);
    }

    char path[2048];
    snprintf(path, sizeof(path), "%s/config.yaml", home);

    /* Read fixture content. */
    FILE *fin = fopen(argv[1], "rb");
    if (!fin) { printf("{\"out\":null}\n"); return 0; }
    fseek(fin, 0, SEEK_END);
    long sz = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    char *content = (char *)malloc(sz + 1);
    if (!content) { fclose(fin); printf("{\"out\":null}\n"); return 0; }
    fread(content, 1, sz, fin);
    content[sz] = '\0';
    fclose(fin);

    FILE *fout = fopen(path, "wb");
    if (fout) { fwrite(content, 1, sz, fout); fclose(fout); }
    free(content);

    gateway_config_load_global();
    const gateway_config_t *cfg = gateway_config_get_global();
    char *r = cfg ? gw_own_policy_open_startup_violation(cfg) : NULL;
    if (r) {
        /* Python oracle uses json.dumps default separators: '{"out": "..."}' */
        printf("{\"out\": %s}\n", js(r));
        free(r);
    } else {
        printf("{\"out\": null}\n");
    }
    return 0;
}
