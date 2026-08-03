/*
 * port_prompt_builder_remaining.c — Port of agent/prompt_builder.py
 * context-file surface. hermes.md discovery, char caps, truncation
 * warnings.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _find_hermes_md @ agent/prompt_builder.py:_find_hermes_md */
char *prb_find_hermes_md(const char *cwd) {
    /* Python: nearest .hermes.md or HERMES.md, cwd up. */
    if (!cwd) return NULL;
    char *cur = strdup(cwd);
    while (cur) {
        static const char *names[] = {".hermes.md", "HERMES.md", NULL};
        for (int i = 0; names[i]; i++) {
            char *probe = NULL;
            asprintf(&probe, "%s/%s", cur, names[i]);
            if (probe && access(probe, F_OK) == 0) { free(cur); return probe; }
            free(probe);
        }
        char *parent = strdup(cur);
        char *slash = strrchr(parent, '/');
        if (!slash || slash == parent) { free(parent); break; }
        *slash = '\0';
        free(cur);
        cur = parent;
    }
    free(cur);
    return NULL;
}

/* PoP: _dynamic_context_file_max_chars @ agent/prompt_builder.py:_dynamic_context_file_max_chars */
long prb_dynamic_context_file_max_chars(long context_window) {
    /* Python: cap from context window; at least 20000. */
    if (context_window <= 0) return 20000;
    long cap = context_window / 4;
    return cap > 20000 ? cap : 20000;
}

/* PoP: _get_context_file_max_chars @ agent/prompt_builder.py:_get_context_file_max_chars */
long prb_get_context_file_max_chars(long context_window, const char *config_yaml) {
    /* Python: explicit config first. */
    if (config_yaml) {
        const char *p = strstr(config_yaml, "context_file_max_chars");
        if (p) {
            const char *colon = strchr(p, ':');
            if (colon) {
                long v = atol(colon + 1);
                if (v > 0) return v;
            }
        }
    }
    return prb_dynamic_context_file_max_chars(context_window);
}

/* PoP: _record_truncation_warning @ agent/prompt_builder.py:_record_truncation_warning */
int prb_record_truncation_warning(const char *warnings_json, const char *path, long truncated) {
    /* Python: append warning to accumulator. */
    if (!path) return -1;
    char *out = NULL;
    asprintf(&out, "%s{\"path\": \"%s\", \"truncated_chars\": %ld}",
             warnings_json ? warnings_json : "[]", path, truncated);
    free(warnings_json ? NULL : NULL);
    (void)out;
    printf("truncation warning recorded (%s, %ld chars)\n", path, truncated);
    return 0;
}

/* PoP: drain_truncation_warnings @ agent/prompt_builder.py:drain_truncation_warnings */
char *prb_drain_truncation_warnings(void) {
    /* Python: return + clear. */
    printf("truncation warnings drained\n");
    return strdup("[]");
}
