/*
 * runtime_readiness.c — real C11 port of selected helpers in upstream Hermes
 * hermes_cli/runtime_readiness.py.
 *
 * PoP: w16_runtimerea_133542_missing_suffix @ hermes_cli/runtime_readiness.py:missing_credentials_suffix
 * PoP: w16_runtimerea_133542_has_credentials @ hermes_cli/runtime_readiness.py:runtime_has_credentials
 * PoP: w16_runtimerea_133542_readiness_failure @ hermes_cli/runtime_readiness.py:readiness_failure
 */
#include "noc_w16_runtimerea_133542.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int is_space_c(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python str.strip() then .lower() over the ASCII fixtures this gate uses. */
static char *strip_lower(const char *s) {
    const char *start, *end;
    char *out;
    size_t n, i;
    if (!s) s = "";
    start = s;
    while (*start && is_space_c((unsigned char)*start)) start++;
    end = start + strlen(start);
    while (end > start && is_space_c((unsigned char)end[-1])) end--;
    n = (size_t)(end - start);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)start[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : (char)c;
    }
    out[n] = '\0';
    return out;
}

static char *strip_only(const char *s) {
    const char *start, *end;
    char *out;
    size_t n;
    if (!s) s = "";
    start = s;
    while (*start && is_space_c((unsigned char)*start)) start++;
    end = start + strlen(start);
    while (end > start && is_space_c((unsigned char)end[-1])) end--;
    n = (size_t)(end - start);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, start, n);
    out[n] = '\0';
    return out;
}

char *w16_runtimerea_133542_missing_suffix(const char *provider, int profile_present,
                                           const char *hint, int hint_kind) {
    char *name = strip_lower(provider);
    char *stripped;
    char *out;
    size_t n;
    if (!name) return dupstr("");
    if (!name[0] || !profile_present) { free(name); return dupstr(""); }
    free(name);
    if (hint_kind == 1 || hint_kind == 2) return dupstr(""); /* hook raised / non-string */
    stripped = strip_only(hint);
    if (!stripped) return dupstr("");
    n = strlen(stripped);
    if (n == 0) { free(stripped); return dupstr(""); }
    out = (char *)malloc(n + 2);
    if (!out) { free(stripped); return dupstr(""); }
    out[0] = ' ';
    memcpy(out + 1, stripped, n);
    out[n + 1] = '\0';
    free(stripped);
    return out;
}

int w16_runtimerea_133542_has_credentials(const char *api_key, int api_key_callable,
                                          const char *command, json_t *usable_secrets) {
    char *text;
    int found = 0;
    size_t i, count;
    if (api_key_callable || (command && command[0])) return 1;
    text = strip_only(api_key);
    if (!text) return 0;
    if (!strcmp(text, "aws-sdk") || !strcmp(text, "no-key-required")) {
        free(text);
        return 1;
    }
    if (usable_secrets && usable_secrets->type == JSON_ARRAY) {
        count = usable_secrets->c.count;
        for (i = 0; i < count; i++) {
            json_t *s = usable_secrets->c.items[i];
            if (s && s->type == JSON_STRING && s->str_val && !strcmp(s->str_val, text)) {
                found = 1;
                break;
            }
        }
    }
    free(text);
    return found ? 1 : 0;
}

json_t *w16_runtimerea_133542_readiness_failure(const char *error_text,
                                                int has_error_code, const char *error_code) {
    json_t *obj = json_object();
    json_set(obj, "ok", json_bool(0));
    json_set(obj, "error", json_string(error_text ? error_text : ""));
    if (has_error_code)
        json_set(obj, "error_code", json_string(error_code ? error_code : ""));
    return obj;
}
