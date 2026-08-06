/*
 * Oracle test harness for agent/backend_identity.py and agent/redact.py pure helpers.
 * Reads \t-delimited commands from stdin, prints results to stdout.
 * Protocol: CMD\targ1\targ2...
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "port_backend_identity.h"

/* redact helpers */
bool agent_redact_is_word_start(const char *s, size_t i);
bool agent_redact_is_word_end(const char *s, size_t j, bool allow_plural);
bool agent_redact_key_has_secret_keyword(const char *key);

int main(int argc, char **argv) {
    FILE *in = stdin;
    if (argc > 1) {
        in = fopen(argv[1], "r");
        if (!in) { fprintf(stderr, "cannot open fixture %s\n", argv[1]); return 1; }
    }
    char *line = NULL;
    size_t line_cap = 0;
    ssize_t n;
    while ((n = getline(&line, &line_cap, in)) != -1) {
        /* strip trailing newline */
        if (n > 0 && line[n-1] == '\n') line[--n] = '\0';
        if (n == 0) continue;

        /* split on tab */
        char *parts[16];
        int np = 0;
        char *p = line;
        while (p && np < 16) {
            char *tab = strchr(p, '\t');
            if (tab) {
                *tab = '\0';
                parts[np++] = p;
                p = tab + 1;
                while (*p == ' ') p++; /* skip spaces after tab */
            } else {
                parts[np++] = p;
                break;
            }
        }
        if (np == 0) continue;

        char *cmd = parts[0];

        if (strcmp(cmd, "norm_provider") == 0) {
            char *r = agent_backend_identity_norm_provider(parts[1] ? parts[1] : "");
            printf("%s\n", r);
            free(r);
        } else if (strcmp(cmd, "norm_model") == 0) {
            char *r = agent_backend_identity_norm_model(parts[1] ? parts[1] : "");
            printf("%s\n", r);
            free(r);
        } else if (strcmp(cmd, "norm_base_url") == 0) {
            char *r = agent_backend_identity_norm_base_url(parts[1] ? parts[1] : "");
            printf("%s\n", r);
            free(r);
        } else if (strcmp(cmd, "classify_failure_scope") == 0) {
            int scope = agent_backend_identity_classify_failure_scope(parts[1] ? parts[1] : "");
            const char *name = "MODEL";
            if (scope == 1) name = "CREDENTIAL";
            else if (scope == 2) name = "ENDPOINT";
            printf("%s\n", name);
        } else if (strcmp(cmd, "is_word_start") == 0) {
            const char *s = parts[1] ? parts[1] : "";
            size_t i = np > 2 && parts[2] ? (size_t)atoi(parts[2]) : 0;
            bool r = agent_redact_is_word_start(s, i);
            printf("%s\n", r ? "true" : "false");
        } else if (strcmp(cmd, "is_word_end") == 0) {
            const char *s = parts[1] ? parts[1] : "";
            size_t j = np > 2 && parts[2] ? (size_t)atoi(parts[2]) : 0;
            bool allow_plural = (np > 3 && parts[3] && strcmp(parts[3], "true") == 0);
            bool r = agent_redact_is_word_end(s, j, allow_plural);
            printf("%s\n", r ? "true" : "false");
        } else if (strcmp(cmd, "key_has_secret_keyword") == 0) {
            const char *key = parts[1] ? parts[1] : "";
            bool r = agent_redact_key_has_secret_keyword(key);
            printf("%s\n", r ? "true" : "false");
        }
    }
    free(line);
    return 0;
}
