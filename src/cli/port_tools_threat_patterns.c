/*
 * port_tools_threat_patterns.c — C port of tools/threat_patterns.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>

/* Compiled patterns storage */
static int _patterns_compiled = 0;

/* Simplified pattern set for C implementation */
typedef struct {
    regex_t regex;
    const char *pattern;
    const char *id;
    const char *scope; /* "all", "context", "strict" */
} threat_pattern_t;

static threat_pattern_t _patterns[32];
static int _pattern_count = 0;

/* PoP: cli_tools_threat_patterns__compile @ tools/threat_patterns.py:_compile */

/* Port of Python tools/threat_patterns.py:_compile */
/* Compile pattern sets for each scope. */
void cli_tools_threat_patterns__compile(void)
{
    if (_patterns_compiled) return;

    /* Define core threat patterns (simplified from Python version) */
    static const struct {
        const char *pattern;
        const char *id;
        const char *scope;
    } raw_patterns[] = {
        /* All scopes: classic injection */
        {"ignore.*(previous|above|prior).*instructions", "prompt_injection", "all"},
        {"disregard.*(system|initial|original).*prompt", "prompt_injection", "all"},
        {"you are now.*(ignore|forget).*rules", "role_hijack", "all"},
        /* Context scope */
        {"send.*(data|info|content).*to.*http", "data_exfil", "context"},
        {"curl.*http", "data_exfil", "context"},
        {"wget.*http", "data_exfil", "context"},
        /* Strict scope */
        {"crontab.*-e", "persistence", "strict"},
        {"authorized_keys", "ssh_backdoor", "strict"},
        {"\\.ssh/", "ssh_backdoor", "strict"},
        {NULL, NULL, NULL}
    };

    _pattern_count = 0;
    for (int i = 0; raw_patterns[i].pattern && _pattern_count < 32; i++) {
        int ret = regcomp(&_patterns[_pattern_count].regex,
            raw_patterns[i].pattern,
            REG_EXTENDED | REG_ICASE);
        if (ret == 0) {
            _patterns[_pattern_count].pattern = raw_patterns[i].pattern;
            _patterns[_pattern_count].id = raw_patterns[i].id;
            _patterns[_pattern_count].scope = raw_patterns[i].scope;
            _pattern_count++;
        }
    }

    _patterns_compiled = 1;
}

/* PoP: cli_tools_threat_patterns_scan_for_threats @ tools/threat_patterns.py:scan_for_threats */

/* Port of Python tools/threat_patterns.py:scan_for_threats */
/* Return a list of matched pattern IDs in content at the given scope. */
char *cli_tools_threat_patterns_scan_for_threats(
    const char *content, const char *scope)
{
    if (!content || !*content) {
        return strdup("[]");
    }

    if (!_patterns_compiled) {
        cli_tools_threat_patterns__compile();
    }

    /* Determine which scopes to check */
    int check_all = (strcmp(scope, "all") == 0);
    int check_context = check_all || (strcmp(scope, "context") == 0);
    int check_strict = check_context || (strcmp(scope, "strict") == 0);

    size_t buf_size = 4096;
    char *result = (char *)malloc(buf_size);
    if (!result) return strdup("[]");

    int pos = 0;
    pos += snprintf(result + pos, buf_size - pos, "[");
    int found = 0;

    for (int i = 0; i < _pattern_count; i++) {
        /* Check if this pattern's scope matches the requested scope */
        int should_check = 0;
        if (check_all && strcmp(_patterns[i].scope, "all") == 0) should_check = 1;
        if (check_context && strcmp(_patterns[i].scope, "context") == 0) should_check = 1;
        if (check_strict && strcmp(_patterns[i].scope, "strict") == 0) should_check = 1;

        if (!should_check) continue;

        /* Check for match (use a copy since regexec doesn't take const) */
        char *content_copy = strdup(content);
        if (!content_copy) continue;

        regmatch_t match;
        if (regexec(&_patterns[i].regex, content_copy, 1, &match, 0) == 0) {
            pos += snprintf(result + pos, buf_size - pos,
                "%s\"%s\"", found ? "," : "", _patterns[i].id);
            found++;
        }
        free(content_copy);

        if (pos > (int)buf_size - 64) break;
    }

    /* Check for invisible unicode characters */
    if (check_context || check_strict) {
        for (const unsigned char *p = (const unsigned char *)content; *p; p++) {
            if (*p < 0x20 && *p != '\n' && *p != '\r' && *p != '\t') {
                pos += snprintf(result + pos, buf_size - pos,
                    "%sinvisible_unicode_U+%04X\"", found ? "," : "", *p);
                found++;
            } else if (*p >= 0x80 && *p < 0xA0) {
                pos += snprintf(result + pos, buf_size - pos,
                    "%sinvisible_unicode_U+%04X\"", found ? "," : "", *p);
                found++;
            }
        }
    }

    pos += snprintf(result + pos, buf_size - pos, "]");
    return result;
}

/* PoP: cli_tools_threat_patterns_first_threat_message @ tools/threat_patterns.py:first_threat_message */

/* Port of Python tools/threat_patterns.py:first_threat_message */
/* Return a human-readable error string for the first threat found, or NULL. */
char *cli_tools_threat_patterns_first_threat_message(
    const char *content, const char *scope)
{
    char *findings = cli_tools_threat_patterns_scan_for_threats(content, scope);
    if (!findings) return NULL;

    /* Check if empty */
    if (strcmp(findings, "[]") == 0) {
        free(findings);
        return NULL;
    }

    /* Extract first pattern ID from JSON array */
    const char *start = strchr(findings, '"');
    if (!start) {
        free(findings);
        return NULL;
    }
    start++; /* skip opening quote */

    const char *end = strchr(start, '"');
    if (!end) {
        free(findings);
        return NULL;
    }

    size_t pid_len = (size_t)(end - start);
    char *pid = (char *)malloc(pid_len + 1);
    if (!pid) {
        free(findings);
        return NULL;
    }
    memcpy(pid, start, pid_len);
    pid[pid_len] = '\0';
    free(findings);

    /* Build message */
    size_t msg_size = 256 + pid_len;
    char *msg = (char *)malloc(msg_size);
    if (!msg) {
        free(pid);
        return NULL;
    }

    if (strncmp(pid, "invisible_unicode_", 17) == 0) {
        snprintf(msg, msg_size,
            "Blocked: content contains invisible unicode character %s (possible injection).",
            pid + 17);
    } else {
        snprintf(msg, msg_size,
            "Blocked: content matches threat pattern '%s'. "
            "Content is injected into the system prompt and must not contain "
            "injection or exfiltration payloads.",
            pid);
    }

    free(pid);
    return msg;
}
