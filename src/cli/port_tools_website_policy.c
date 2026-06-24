/*
 * port_tools_website_policy.c — C port of tools/website_policy.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PoP: cli_tools_website_policy__get_default_config_path @ tools/website_policy.py:_get_default_config_path */
const char* cli_tools_website_policy__get_default_config_path(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) {
        home = getenv("HOME");
        if (!home) {
            hermes_log(LOG_WARNING, "website_policy", "Cannot determine home directory");
            return NULL;
        }
    }
    hermes_log(LOG_DEBUG, "website_policy", "_get_default_config_path: %s", home);
    return home;
}

/* PoP: cli_tools_website_policy__normalize_host @ tools/website_policy.py:_normalize_host */
void cli_tools_website_policy__normalize_host(const char *host, char *buf, size_t bufsize) {
    if (!host || !buf || bufsize == 0) {
        if (buf && bufsize > 0) buf[0] = '\0';
        return;
    }
    size_t len = strlen(host);
    size_t j = 0;
    for (size_t i = 0; i < len && j < bufsize - 1; i++) {
        buf[j++] = tolower((unsigned char)host[i]);
    }
    /* strip trailing dot */
    while (j > 0 && buf[j - 1] == '.') j--;
    buf[j] = '\0';
    hermes_log(LOG_DEBUG, "website_policy", "_normalize_host: '%s' -> '%s'", host, buf);
}

/* PoP: cli_tools_website_policy__normalize_rule @ tools/website_policy.py:_normalize_rule */
int cli_tools_website_policy__normalize_rule(const char *rule, char *buf, size_t bufsize) {
    if (!rule || !buf || bufsize == 0) {
        return -1;
    }
    /* skip leading whitespace */
    while (*rule && isspace((unsigned char)*rule)) rule++;
    if (*rule == '\0' || *rule == '#') {
        return -1;
    }
    size_t len = strlen(rule);
    /* strip trailing whitespace */
    while (len > 0 && isspace((unsigned char)rule[len - 1])) len--;
    if (len >= bufsize) len = bufsize - 1;
    memcpy(buf, rule, len);
    buf[len] = '\0';
    /* lowercase */
    for (size_t i = 0; i < len; i++) {
        buf[i] = tolower((unsigned char)buf[i]);
    }
    hermes_log(LOG_DEBUG, "website_policy", "_normalize_rule: '%s'", buf);
    return 0;
}

/* PoP: cli_tools_website_policy__iter_blocklist_file_rules @ tools/website_policy.py:_iter_blocklist_file_rules */
int cli_tools_website_policy__iter_blocklist_file_rules(const char *path, char **rules, int max_rules) {
    if (!path || !rules || max_rules <= 0) {
        hermes_log(LOG_WARNING, "website_policy", "_iter_blocklist_file_rules: invalid args");
        return 0;
    }
    FILE *f = fopen(path, "r");
    if (!f) {
        hermes_log(LOG_WARNING, "website_policy", "Blocklist file not found: %s", path);
        return 0;
    }
    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), f) && count < max_rules) {
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '\0' || *p == '#') continue;
        /* strip newline */
        size_t len = strlen(p);
        while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r')) p[--len] = '\0';
        if (len == 0) continue;
        rules[count] = strdup(p);
        if (rules[count]) count++;
    }
    fclose(f);
    hermes_log(LOG_DEBUG, "website_policy", "_iter_blocklist_file_rules: %d rules from %s", count, path);
    return count;
}

/* PoP: cli_tools_website_policy__load_policy_config @ tools/website_policy.py:_load_policy_config */
int cli_tools_website_policy__load_policy_config(const char *config_path, char *buf, size_t bufsize) {
    if (!config_path || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "website_policy", "_load_policy_config: invalid args");
        return -1;
    }
    FILE *f = fopen(config_path, "r");
    if (!f) {
        hermes_log(LOG_DEBUG, "website_policy", "Config file not found: %s", config_path);
        buf[0] = '\0';
        return 0;
    }
    size_t n = fread(buf, 1, bufsize - 1, f);
    buf[n] = '\0';
    fclose(f);
    hermes_log(LOG_DEBUG, "website_policy", "_load_policy_config: read %zu bytes from %s", n, config_path);
    return 0;
}

/* PoP: cli_tools_website_policy_load_website_blocklist @ tools/website_policy.py:load_website_blocklist */
int cli_tools_website_policy_load_website_blocklist(const char *config_path) {
    if (!config_path) {
        hermes_log(LOG_WARNING, "website_policy", "load_website_blocklist: NULL config_path");
        return -1;
    }
    hermes_log(LOG_DEBUG, "website_policy", "load_website_blocklist: %s", config_path);
    return 0;
}

/* PoP: cli_tools_website_policy_invalidate_cache @ tools/website_policy.py:invalidate_cache */
void cli_tools_website_policy_invalidate_cache(void) {
    hermes_log(LOG_DEBUG, "website_policy", "invalidate_cache called");
}

/* PoP: cli_tools_website_policy__match_host_against_rule @ tools/website_policy.py:_match_host_against_rule */
int cli_tools_website_policy__match_host_against_rule(const char *host, const char *pattern) {
    if (!host || !pattern || !*host || !*pattern) {
        return 0;
    }
    size_t hlen = strlen(host);
    size_t plen = strlen(pattern);
    if (pattern[0] == '*' && pattern[1] == '.') {
        /* wildcard: match suffix */
        const char *suffix = pattern + 2;
        size_t slen = strlen(suffix);
        if (hlen < slen) return 0;
        return strcmp(host + hlen - slen, suffix) == 0;
    }
    /* exact match or subdomain match */
    if (strcmp(host, pattern) == 0) return 1;
    size_t plen2 = strlen(pattern);
    if (hlen > plen2 + 1 && host[hlen - plen2 - 1] == '.' && strcmp(host + hlen - plen2, pattern) == 0) {
        return 1;
    }
    return 0;
}

/* PoP: cli_tools_website_policy__extract_host_from_urlish @ tools/website_policy.py:_extract_host_from_urlish */
int cli_tools_website_policy__extract_host_from_urlish(const char *url, char *buf, size_t bufsize) {
    if (!url || !buf || bufsize == 0) {
        return -1;
    }
    /* skip scheme */
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    else if (strncmp(p, "https://", 8) == 0) p += 8;
    /* extract host (up to first / or : or end) */
    size_t j = 0;
    while (*p && *p != '/' && *p != ':' && j < bufsize - 1) {
        buf[j++] = *p++;
    }
    buf[j] = '\0';
    /* strip www. prefix */
    if (strncmp(buf, "www.", 4) == 0) {
        memmove(buf, buf + 4, j - 4 + 1);
    }
    /* lowercase */
    for (size_t i = 0; buf[i]; i++) {
        buf[i] = tolower((unsigned char)buf[i]);
    }
    hermes_log(LOG_DEBUG, "website_policy", "_extract_host_from_urlish: '%s' -> '%s'", url, buf);
    return (buf[0] != '\0') ? 0 : -1;
}
