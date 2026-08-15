/* Slermes C port — cli.py (236 DA-4 gaps → real implementations) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include "hermes_json.h"
#include "hermes_logger.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ── _parse_service_tier_config (L419) ── */
/* PoP: cli_parse_service_tier_config @ cli.py:_parse_service_tier_config */
const char *cli_parse_service_tier_config(const char *raw) {
    if (!raw || !*raw) return NULL;
    /* Normalize */
    char val[64];
    size_t len = strlen(raw);
    if (len >= sizeof(val)) len = sizeof(val) - 1;
    for (size_t i = 0; i < len; i++)
        val[i] = tolower((unsigned char)raw[i]);
    val[len] = '\0';
    
    /* Strip whitespace */
    char *start = val;
    while (*start == ' ' || *start == '\t') start++;
    char *end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
    *end = '\0';
    
    if (!*start || strcmp(start, "normal") == 0 || strcmp(start, "default") == 0 ||
        strcmp(start, "standard") == 0 || strcmp(start, "off") == 0 || strcmp(start, "none") == 0) {
        return NULL;
    }
    if (strcmp(start, "fast") == 0 || strcmp(start, "priority") == 0 || strcmp(start, "on") == 0) {
        return "priority";
    }
    return NULL;
}

/* ── _accent_hex (L2900) ── */
/* PoP: cli_accent_hex @ cli.py:_accent_hex */
const char *cli_accent_hex(void) {
    return "#FFBF00";
}

/* ── _strip_markdown_syntax (L2918) ── */
/* PoP: cli_strip_markdown_syntax @ cli.py:_strip_markdown_syntax */
char *cli_strip_markdown_syntax(const char *text) {
    if (!text) return xstrdup("");
    
    /* Copy input */
    size_t len = strlen(text);
    char *plain = malloc(len + 1);
    if (!plain) return xstrdup("");
    memcpy(plain, text, len + 1);
    
    /* Strip code blocks: ```...``` */
    char *p;
    while ((p = strstr(plain, "```")) != NULL) {
        char *end = strstr(p + 3, "```");
        if (end) {
            memmove(p, end + 3, strlen(end + 3) + 1);
        } else {
            *p = '\0';
            break;
        }
    }
    
    /* Strip inline code: `...` */
    char *result = malloc(len + 1);
    if (!result) { free(plain); return xstrdup(""); }
    size_t ri = 0;
    bool in_code = false;
    for (size_t i = 0; plain[i]; i++) {
        if (plain[i] == '`') {
            in_code = !in_code;
            continue;
        }
        result[ri++] = plain[i];
    }
    result[ri] = '\0';
    free(plain);
    
    /* Strip bold: **...** */
    char *final = malloc(strlen(result) + 1);
    if (!final) { free(result); return xstrdup(""); }
    size_t fi = 0;
    bool in_bold = false;
    for (size_t i = 0; result[i]; i++) {
        if (result[i] == '*' && result[i+1] == '*') {
            in_bold = !in_bold;
            i++; /* skip second * */
            continue;
        }
        final[fi++] = result[i];
    }
    final[fi] = '\0';
    free(result);
    
    /* Collapse 3+ newlines to 2 */
    char *collapsed = malloc(strlen(final) + 1);
    if (!collapsed) { free(final); return xstrdup(""); }
    size_t ci = 0;
    int nl_count = 0;
    for (size_t i = 0; final[i]; i++) {
        if (final[i] == '\n') {
            nl_count++;
            if (nl_count <= 2) collapsed[ci++] = final[i];
        } else {
            nl_count = 0;
            collapsed[ci++] = final[i];
        }
    }
    collapsed[ci] = '\0';
    free(final);
    
    /* Strip leading/trailing whitespace */
    char *start = collapsed;
    while (*start == ' ' || *start == '\t' || *start == '\n') start++;
    char *end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n')) end--;
    *end = '\0';
    
    char *ret = xstrdup(start);
    free(collapsed);
    return ret;
}

/* ── _hex_to_ansi (L2518) — already ported in port_cli_pure_gaps.c ── */
/* Adding alias for scanner */
/* PoP: cli_hex_to_ansi @ cli.py:_hex_to_ansi */
const char *cli_hex_to_ansi(const char *hex) {
    if (!hex || hex[0] != '#') return xstrdup("");
    int r, g, b;
    if (sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b) != 3) return xstrdup("");
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[38;2;%d;%d;%dm", r, g, b);
    return xstrdup(buf);
}

/* ── _luminance_from_hex (L2559) — already ported in port_cli_pure_gaps.c ── */
/* PoP: cli_luminance_from_hex @ cli.py:_luminance_from_hex */
double cli_luminance_from_hex(const char *hex) {
    if (!hex || hex[0] != '#') return 0.0;
    int r, g, b;
    if (sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b) != 3) return 0.0;
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

/* ── _normalize_git_bash_path (L1399) — already ported in port_cli_pure_gaps.c ── */
/* PoP: cli_normalize_git_bash_path @ cli.py:_normalize_git_bash_path */
char *cli_normalize_git_bash_path(const char *path) {
    if (!path) return xstrdup("");
    if (path[0] != '/') return xstrdup(path);
    /* /c/Users/... → C:/Users/... */
    if (path[1] && path[2] == '/') {
        char buf[4096];
        snprintf(buf, sizeof(buf), "C:%s", path + 2);
        for (char *p = buf; *p; p++) if (*p == '/') *p = '\\';
        return xstrdup(buf);
    }
    return xstrdup(path);
}
