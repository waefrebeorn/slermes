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

