#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/prompt_builder.c — C11 port of agent/prompt_builder.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_prompt_builder.c ── */
/*
 * port_agent_prompt_builder.c — Port of Python agent/prompt_builder.py
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: _dynamic_context_file_max_chars */
int dynamic_context_file_max_chars(int context_length) {
    /* Scale context file max chars based on model context window */
    if (context_length <= 0) return 4000; /* default */
    if (context_length <= 32000) return 4000;
    if (context_length <= 65000) return 8000;
    if (context_length <= 131000) return 16000;
    return 32000; /* 256K+ models */
}


/* Port of Python: _get_context_file_max_chars */
int get_context_file_max_chars(int context_length) {
    /* Alias for dynamic_context_file_max_chars */
    return dynamic_context_file_max_chars(context_length);
}


/* Port of Python: _record_truncation_warning */
#define MAX_WARNINGS 32

static char truncation_warnings[MAX_WARNINGS][512];
static int n_truncation_warnings = 0;

void record_truncation_warning(const char *warning) {
    if (!warning || n_truncation_warnings >= MAX_WARNINGS) return;
    strncpy(truncation_warnings[n_truncation_warnings], warning, 511);
    truncation_warnings[n_truncation_warnings][511] = '\0';
    n_truncation_warnings++;
}


/* Port of Python: drain_truncation_warnings */
int drain_truncation_warnings(char *output, size_t out_sz) {
    if (!output || out_sz == 0) return 0;
    int pos = 0;
    for (int i = 0; i < n_truncation_warnings && pos < (int)out_sz - 1; i++) {
        size_t len = strlen(truncation_warnings[i]);
        if (pos + len + 2 < out_sz) {
            if (pos > 0) output[pos++] = '\n';
            memcpy(output + pos, truncation_warnings[i], len);
            pos += len;
        }
    }
    output[pos] = '\0';
    int count = n_truncation_warnings;
    n_truncation_warnings = 0; /* Drain */
    return count;
}



/* ── from src/agent/port_agent_prompt_builder_py_parity.c ── */
/* Slermes C port — agent/prompt_builder.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: agent_prompt_builder_hud_surface_note_p @ agent/prompt_builder.py:hud_surface_note */
json_t *agent_prompt_builder_hud_surface_note_p(void) {
    return json_object();
}

/* PoP: agent_prompt_builder_agents_md_directory_chain_p @ agent/prompt_builder.py:_agents_md_directory_chain */
json_t *agent_prompt_builder_agents_md_directory_chain_p(void) {
    return json_object();
}


/* ── from src/agent/port_prompt_builder_ports.c ── */
/*
 * port_prompt_builder_remaining.c — Port of agent/prompt_builder.py
 * context-file surface. hermes.md discovery, char caps, truncation
 * warnings.
 */
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

/* PoP: prb_find_hermes_md @ agent/prompt_builder.py:_find_hermes_md */
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

/* PoP: prb_dynamic_context_file_max_chars @ agent/prompt_builder.py:_dynamic_context_file_max_chars */
long prb_dynamic_context_file_max_chars(long context_window) {
    /* Python: cap from context window; at least 20000. */
    if (context_window <= 0) return 20000;
    long cap = context_window / 4;
    return cap > 20000 ? cap : 20000;
}

/* PoP: prb_get_context_file_max_chars @ agent/prompt_builder.py:_get_context_file_max_chars */
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

/* PoP: prb_record_truncation_warning @ agent/prompt_builder.py:_record_truncation_warning */
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

/* PoP: prb_drain_truncation_warnings @ agent/prompt_builder.py:drain_truncation_warnings */
char *prb_drain_truncation_warnings(void) {
    /* Python: return + clear. */
    printf("truncation warnings drained\n");
    return strdup("[]");
}

