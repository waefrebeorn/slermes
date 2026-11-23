/*
 * _launchers.c -- faithful C11 port of the pure helpers in
 *                 hermes_cli/_launchers.py.
 *
 * PoP: w8_launchers_7f2d24_pin_default_home_flag   @ hermes_cli/_launchers.py:PIN_DEFAULT_HOME_FLAG
 * PoP: w8_launchers_7f2d24_windows_bin_launchers   @ hermes_cli/_launchers.py:WINDOWS_BIN_LAUNCHERS
 * PoP: w8_launchers_7f2d24_entry_points            @ hermes_cli/_launchers.py:ENTRY_POINTS
 * PoP: w8_launchers_7f2d24_inline_string_literal   @ hermes_cli/_launchers.py:_inline_string_literal
 * PoP: w8_launchers_7f2d24_first_token             @ hermes_cli/_launchers.py:_first_token
 * PoP: w8_launchers_7f2d24_merge_user_path         @ hermes_cli/_launchers.py:_merge_user_path
 */
#include "noc_w8_launchers_7f2d24.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int w8l_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *w8l_strip(const char *s) {
    size_t a = 0, b = strlen(s);
    while (a < b && w8l_space((unsigned char)s[a])) a++;
    while (b > a && w8l_space((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *w8l_rstrip_seps(const char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\\' || s[n - 1] == '/')) n--;
    char *out = (char *)malloc(n + 1);
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

const char *w8_launchers_7f2d24_pin_default_home_flag(void) {
    return "_hermes_pin_default_home";
}

struct json_t *w8_launchers_7f2d24_windows_bin_launchers(void) {
    json_t *out = json_array();
    json_append(out, json_string("hermes"));
    json_append(out, json_string("hermes-acp"));
    return out;
}

struct json_t *w8_launchers_7f2d24_entry_points(void) {
    json_t *out = json_object();
    json_t *a = json_array();
    json_append(a, json_string("hermes_cli.main"));
    json_append(a, json_string("main"));
    json_set(out, "hermes", a);
    json_t *b = json_array();
    json_append(b, json_string("acp_adapter.entry"));
    json_append(b, json_string("main"));
    json_set(out, "hermes-acp", b);
    return out;
}

/* os.name == 'nt' on this host: value.encode('unicode_escape').decode('ascii'),
 * then ' -> \x27 and " -> \x22, wrapped in single quotes. */
struct json_t *w8_launchers_7f2d24_inline_string_literal(const char *value) {
    if (!value) value = "";
    size_t n = strlen(value);
    char *buf = (char *)malloc(4 * n + 4);
    size_t k = 0;
    buf[k++] = '\'';
    static const char *HEX = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)value[i];
        if (c == '\'') { buf[k++] = '\\'; buf[k++] = 'x'; buf[k++] = '2'; buf[k++] = '7'; }
        else if (c == '"') { buf[k++] = '\\'; buf[k++] = 'x'; buf[k++] = '2'; buf[k++] = '2'; }
        else if (c == '\\') { buf[k++] = '\\'; buf[k++] = '\\'; }
        else if (c == '\n') { buf[k++] = '\\'; buf[k++] = 'n'; }
        else if (c == '\r') { buf[k++] = '\\'; buf[k++] = 'r'; }
        else if (c == '\t') { buf[k++] = '\\'; buf[k++] = 't'; }
        else if (c >= 0x20 && c <= 0x7e) buf[k++] = (char)c;
        else {
            buf[k++] = '\\'; buf[k++] = 'x';
            buf[k++] = HEX[c >> 4]; buf[k++] = HEX[c & 15];
        }
    }
    buf[k++] = '\'';
    buf[k] = '\0';
    json_t *out = json_string(buf);
    free(buf);
    return out;
}

/* str(Path(token)) on Windows: '/' separators become '\'; Path('') -> '.'. */
struct json_t *w8_launchers_7f2d24_first_token(const char *line) {
    char *text = w8l_strip(line ? line : "");
    if (!text[0]) { free(text); return json_null(); }
    char *token = NULL;
    if (text[0] == '"' || text[0] == '\'') {
        char *end = strchr(text + 1, text[0]);
        if (!end) { free(text); return json_null(); }
        size_t len = (size_t)(end - (text + 1));
        token = (char *)malloc(len + 1);
        memcpy(token, text + 1, len);
        token[len] = '\0';
    } else {
        size_t len = 0;
        while (text[len] && !w8l_space((unsigned char)text[len])) len++;
        token = (char *)malloc(len + 1);
        memcpy(token, text, len);
        token[len] = '\0';
    }
    free(text);
    if (!token[0]) { free(token); return json_string("."); }
    for (char *p = token; *p; p++) if (*p == '/') *p = '\\';
    json_t *out = json_string(token);
    free(token);
    return out;
}

static void w8l_lower(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

struct json_t *w8_launchers_7f2d24_merge_user_path(const char *existing, const char *entry) {
    if (!existing) existing = "";
    if (!entry) entry = "";
    char *wanted = w8l_rstrip_seps(entry);
    char *wl = (char *)malloc(strlen(wanted) + 1);
    strcpy(wl, wanted); w8l_lower(wl);

    /* parts = [part for part in (existing or "").split(";") if part] */
    size_t cap = 8, nparts = 0;
    char **parts = (char **)malloc(cap * sizeof(char *));
    const char *p = existing;
    while (1) {
        const char *semi = strchr(p, ';');
        size_t len = semi ? (size_t)(semi - p) : strlen(p);
        if (len) {
            char *part = (char *)malloc(len + 1);
            memcpy(part, p, len); part[len] = '\0';
            if (nparts == cap) { cap *= 2; parts = (char **)realloc(parts, cap * sizeof(char *)); }
            parts[nparts++] = part;
        }
        if (!semi) break;
        p = semi + 1;
    }

    for (size_t i = 0; i < nparts; i++) {
        char *stripped = w8l_rstrip_seps(parts[i]);
        w8l_lower(stripped);
        if (!strcmp(stripped, wl)) {
            free(stripped); free(wl); free(wanted);
            for (size_t j = 0; j < nparts; j++) free(parts[j]);
            free(parts);
            return json_null();
        }
        free(stripped);
    }
    free(wl); free(wanted);

    size_t total = strlen(entry) + 1;
    for (size_t i = 0; i < nparts; i++) total += 1 + strlen(parts[i]);
    char *out = (char *)malloc(total + 1);
    strcpy(out, entry);
    for (size_t i = 0; i < nparts; i++) {
        strcat(out, ";");
        strcat(out, parts[i]);
        free(parts[i]);
    }
    free(parts);
    json_t *res = json_string(out);
    free(out);
    return res;
}
