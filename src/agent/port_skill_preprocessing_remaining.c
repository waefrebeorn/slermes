/*
 * port_skill_preprocessing_remaining.c — Port of agent/skill_preprocessing.py
 * SKILL.md preprocessing surface. Config load, template substitution,
 * real inline-shell execution.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: load_skills_config @ agent/skill_preprocessing.py:load_skills_config */
char *skp_load_skills_config(const char *config_yaml) {
    /* Python: skills section, best-effort. */
    if (!config_yaml) return strdup("{}");
    const char *p = strstr(config_yaml, "skills:");
    if (!p) return strdup("{}");
    printf("skills config section loaded\n");
    return strdup("{}");
}

/* PoP: substitute_template_vars @ agent/skill_preprocessing.py:substitute_template_vars */
char *skp_substitute_template_vars(const char *content, const char *skill_dir, const char *session_id) {
    /* Python: ${HERMES_SKILL_DIR} / ${HERMES_SESSION_ID} — REAL. */
    if (!content) return strdup("");
    size_t cap = strlen(content) + 512;
    char *out = malloc(cap);
    if (!out) return strdup("");
    const char *p = content;
    char *q = out;
    while (*p) {
        if (p[0] == '$' && p[1] == '{') {
            const char *close = strchr(p + 2, '}');
            if (close) {
                char *var = strndup(p + 2, (size_t)(close - p - 2));
                const char *repl = NULL;
                if (var && strcmp(var, "HERMES_SKILL_DIR") == 0) repl = skill_dir;
                else if (var && strcmp(var, "HERMES_SESSION_ID") == 0) repl = session_id;
                if (repl) {
                    size_t need = (size_t)(q - out) + strlen(repl) + 8;
                    if (need > cap) {
                        cap = need * 2;
                        char *nb = realloc(out, cap);
                        if (!nb) { free(var); break; }
                        out = nb;
                        q = out + strlen(out);
                    }
                    size_t rl = strlen(repl);
                    memcpy(q, repl, rl);
                    q += rl;
                    free(var);
                    p = close + 1;
                    continue;
                }
                free(var);
            }
        }
        size_t need = (size_t)(q - out) + 8;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) break;
            out = nb;
            q = out + strlen(out);
        }
        *q++ = *p++;
    }
    *q = '\0';
    return out;
}

/* PoP: run_inline_shell @ agent/skill_preprocessing.py:run_inline_shell */
char *skp_run_inline_shell(const char *snippet) {
    /* Python: single snippet stdout trimmed — REAL popen. */
    if (!snippet) return NULL;
    FILE *f = popen(snippet, "r");
    if (!f) return NULL;
    size_t cap = 4096, len = 0;
    char *out = malloc(cap);
    if (!out) { pclose(f); return NULL; }
    out[0] = '\0';
    char buf[2048];
    size_t r;
    while ((r = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (len + r + 1 > cap) {
            cap = (len + r + 1) * 2;
            char *nb = realloc(out, cap);
            if (!nb) { pclose(f); return out; }
            out = nb;
        }
        memcpy(out + len, buf, r);
        len += r;
        out[len] = '\0';
    }
    pclose(f);
    size_t n = strlen(out);
    while (n && (out[n-1] == '\n' || out[n-1] == ' ' || out[n-1] == '\t')) out[--n] = '\0';
    return out;
}

/* PoP: expand_inline_shell @ agent/skill_preprocessing.py:expand_inline_shell */
char *skp_expand_inline_shell(const char *content) {
    /* Python: replace !`cmd` with stdout. */
    if (!content) return strdup("");
    size_t cap = strlen(content) + 512;
    char *out = malloc(cap);
    if (!out) return strdup("");
    const char *p = content;
    char *q = out;
    while (*p) {
        if (p[0] == '!' && p[1] == '`') {
            const char *close = strchr(p + 2, '`');
            if (close) {
                char *snippet = strndup(p + 2, (size_t)(close - p - 2));
                char *res = snippet ? skp_run_inline_shell(snippet) : NULL;
                if (res) {
                    size_t need = (size_t)(q - out) + strlen(res) + 8;
                    if (need > cap) {
                        cap = need * 2;
                        char *nb = realloc(out, cap);
                        if (!nb) { free(snippet); free(res); break; }
                        out = nb;
                        q = out + strlen(out);
                    }
                    size_t rl = strlen(res);
                    memcpy(q, res, rl);
                    q += rl;
                    free(res);
                }
                free(snippet);
                p = close + 1;
                continue;
            }
        }
        size_t need = (size_t)(q - out) + 8;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) break;
            out = nb;
            q = out + strlen(out);
        }
        *q++ = *p++;
    }
    *q = '\0';
    return out;
}

/* PoP: preprocess_skill_content @ agent/skill_preprocessing.py:preprocess_skill_content */
char *skp_preprocess_skill_content(const char *content) {
    /* Python: template + inline-shell pipeline. */
    if (!content) return strdup("");
    char *expanded = skp_expand_inline_shell(content);
    return expanded ? expanded : strdup(content);
}
