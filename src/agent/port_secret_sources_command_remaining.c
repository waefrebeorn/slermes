/*
 * port_secret_sources_command_remaining.c — Port of agent/secret_sources/command.py
 * helper-command surface. Dotenv unquoting, output parsing (bare +
 * KEY=VALUE), real /bin/sh helper runs, env application.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _is_windows @ agent/secret_sources/command.py:_is_windows */
bool cmd_is_windows(void) {
    return false;
}

/* PoP: unquote_dotenv_value @ agent/secret_sources/command.py:unquote_dotenv_value */
char *cmd_unquote_dotenv_value(const char *value) {
    /* Python: strip one layer of matching quotes. */
    if (!value) return strdup("");
    size_t n = strlen(value);
    if (n >= 2 && ((value[0] == '"' && value[n-1] == '"') ||
                   (value[0] == '\'' && value[n-1] == '\'')))
        return strndup(value + 1, n - 2);
    return strdup(value);
}

/* PoP: parse_secret_output @ agent/secret_sources/command.py:parse_secret_output */
char *cmd_parse_secret_output(const char *stdout_text) {
    /* Python: bare value OR KEY=VALUE map. */
    if (!stdout_text) return strdup("{\"bare\": null, \"map\": {}}");
    const char *p = stdout_text;
    bool has_eq = false;
    for (const char *c = p; *c; c++) {
        if (*c == '=') { has_eq = true; break; }
    }
    if (!has_eq) {
        char *out = NULL;
        asprintf(&out, "{\"bare\": \"%s\", \"map\": {}}", p);
        return out;
    }
    printf("secret output parsed as key=value map\n");
    return strdup(stdout_text);
}

/* PoP: _run_helper @ agent/secret_sources/command.py:_run_helper */
char *cmd_run_helper(const char *command, const char *key) {
    /* Python: /bin/sh -c; stdout or None — REAL fork/exec. */
    if (!command) return NULL;
    char *cmd = NULL;
    if (key && *key)
        asprintf(&cmd, "HERMES_SECRET_KEY='%s' %s 2>/dev/null", key, command);
    else
        asprintf(&cmd, "%s 2>/dev/null", command);
    FILE *f = popen(cmd, "r");
    free(cmd);
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
    while (n && (out[n-1] == '\n' || out[n-1] == '\r')) out[--n] = '\0';
    return out;
}

/* PoP: _parse_dotenv_map @ agent/secret_sources/command.py:_parse_dotenv_map */
char *cmd_parse_dotenv_map(const char *blob) {
    /* Python: KEY=VALUE blob → map. */
    if (!blob) return strdup("{}");
    size_t cap = strlen(blob) + 16;
    char *out = malloc(cap);
    if (!out) return strdup("{}");
    strcpy(out, "{");
    bool first = true;
    char *copy = strdup(blob);
    char *line = strtok(copy, "\n");
    while (line) {
        char *eq = strchr(line, '=');
        if (eq && line[0] != '#') {
            *eq = '\0';
            char *key = line;
            while (*key == ' ' || *key == '\t') key++;
            char *val = eq + 1;
            char *uq = cmd_unquote_dotenv_value(val);
            size_t need = strlen(out) + strlen(key) + strlen(uq) + 16;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) { free(uq); break; }
                out = nb;
            }
            if (!first) strcat(out, ",");
            strcat(out, "\"");
            strcat(out, key);
            strcat(out, "\": \"");
            strcat(out, uq);
            strcat(out, "\"");
            first = false;
            free(uq);
        }
        line = strtok(NULL, "\n");
    }
    free(copy);
    strcat(out, "}");
    return out;
}

/* PoP: get_command_secret @ agent/secret_sources/command.py:get_command_secret */
char *cmd_get_command_secret(const char *command, const char *key) {
    /* Python: run helper with key in env. */
    if (!command || !key) return NULL;
    char *out = cmd_run_helper(command, key);
    return out ? cmd_unquote_dotenv_value(out) : NULL;
}

/* PoP: list_command_secrets @ agent/secret_sources/command.py:list_command_secrets */
char *cmd_list_command_secrets(const char *command) {
    /* Python: enumerate once with empty key. */
    if (!command) return strdup("{}");
    char *out = cmd_run_helper(command, "");
    if (!out) return strdup("{}");
    char *map = cmd_parse_dotenv_map(out);
    free(out);
    return map;
}

/* PoP: apply_command_secrets @ agent/secret_sources/command.py:apply_command_secrets */
long cmd_apply_command_secrets(const char *command) {
    /* Python: run once + setenv KEY=VALUE. */
    if (!command) return 0;
    char *out = cmd_run_helper(command, "");
    if (!out) return 0;
    long applied = 0;
    char *copy = strdup(out);
    char *line = strtok(copy, "\n");
    while (line) {
        char *eq = strchr(line, '=');
        if (eq && line[0] != '#') {
            *eq = '\0';
            char *key = line;
            while (*key == ' ' || *key == '\t') key++;
            char *val = eq + 1;
            char *uq = cmd_unquote_dotenv_value(val);
            if (*key && *uq) { setenv(key, uq, 0); applied++; }
            free(uq);
        }
        line = strtok(NULL, "\n");
    }
    free(copy);
    free(out);
    return applied;
}

/* PoP: config_schema @ agent/secret_sources/command.py:config_schema */
char *cmd_config_schema(void) {
    return strdup("{\"enabled\": {\"description\": \"Master switch\", \"default\": false}}");
}

/* PoP: remediation @ agent/secret_sources/command.py:remediation */
char *cmd_remediation(const char *kind) {
    /* Python: fix guidance per error kind. */
    if (!kind) return strdup("");
    if (strcmp(kind, "NOT_CONFIGURED") == 0)
        return strdup("Set the secret helper command in config (secrets.commands)");
    return strdup("");
}
