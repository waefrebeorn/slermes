/*
 * verification_stop.c - C home for upstream Hermes agent/verification_stop.py.
 *
 * PoP: w9_verification_1c81c6_is_non_code_path @ agent/verification_stop.py:_is_non_code_path
 * PoP: w9_verification_1c81c6_format_changed_paths @ agent/verification_stop.py:_format_changed_paths
 * PoP: w9_verification_1c81c6_verify_on_stop_enabled_ex @ agent/verification_stop.py:verify_on_stop_enabled
 *
 * The env lookup is an explicit seam: Windows CRT setenv(name,"") UNSETS the
 * variable while Python os.environ[name]="" keeps it, so the caller injects the
 * resolved HERMES_VERIFY_ON_STOP value instead of mutating the process env.
 */
#include "noc_w9_verification_1c81c6.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const VS_EXTENSIONS[] = {
    ".md", ".markdown", ".mdx", ".rst", ".txt", ".text", ".adoc", ".asciidoc", ".org", ".log", ".csv", ".tsv"
};
static const char *const VS_FILENAMES[] = {
    "license", "licence", "notice", "authors", "contributors", "changelog", "codeowners"
};
static const int VS_MAX_PATHS = 8;

static char *vs_lower_dup(const char *s) {
    char *out = _strdup(s ? s : "");
    for (char *p = out; *p; ++p) *p = (char)tolower((unsigned char)*p);
    return out;
}

static char *vs_strip_lower(const char *s) {
    while (s && isspace((unsigned char)*s)) s++;
    size_t n = s ? strlen(s) : 0;
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    for (size_t i = 0; i < n; ++i) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

static int vs_in(const char *needle, const char *const *table, size_t n) {
    for (size_t i = 0; i < n; ++i) if (!strcmp(needle, table[i])) return 1;
    return 0;
}

int w9_verification_1c81c6_is_non_code_path(const char *raw) {
    if (!raw) return 0;
    const char *base = raw;
    for (const char *p = raw; *p; ++p) if (*p == '/' || *p == '\\') base = p + 1;
    const char *dot = NULL;
    for (const char *p = base; *p; ++p) if (*p == '.') dot = p;
    char *low = vs_lower_dup(base);
    int answer;
    if (dot && dot > base) {
        char *suffix = vs_lower_dup(dot);
        answer = vs_in(suffix, VS_EXTENSIONS, sizeof(VS_EXTENSIONS) / sizeof(VS_EXTENSIONS[0]));
        free(suffix);
    } else {
        answer = vs_in(low, VS_FILENAMES, sizeof(VS_FILENAMES) / sizeof(VS_FILENAMES[0]));
    }
    free(low);
    return answer;
}

json_t *w9_verification_1c81c6_format_changed_paths(json_t *paths) {
    json_t *out = json_array();
    size_t n = json_is_array(paths) ? json_len(paths) : 0;
    size_t cap = n < (size_t)VS_MAX_PATHS ? n : (size_t)VS_MAX_PATHS;
    for (size_t i = 0; i < cap; ++i) {
        json_t *p = json_get(paths, i);
        const char *s = (p && p->type == JSON_STRING) ? p->str_val : "";
        char buf[8192];
        snprintf(buf, sizeof(buf), "- `%s`", s ? s : "");
        json_append(out, json_string(buf));
    }
    if (n > (size_t)VS_MAX_PATHS) {
        char buf[128];
        snprintf(buf, sizeof(buf), "- ... and %zu more", n - VS_MAX_PATHS);
        json_append(out, json_string(buf));
    }
    /* Join with a single newline. */
    size_t total = 1;
    for (size_t i = 0; i < json_len(out); ++i) {
        total += strlen(json_string_value(json_get(out, i)));
        if (i) total++;
    }
    char *joined = (char *)calloc(total + 1, 1);
    for (size_t i = 0; i < json_len(out); ++i) {
        if (i) strcat(joined, "\n");
        strcat(joined, json_string_value(json_get(out, i)));
    }
    json_t *result = json_string(joined);
    free(joined);
    json_free(out);
    return result;
}

int w9_verification_1c81c6_verify_on_stop_enabled_ex(const char *env_value,
                                                      json_t *config, int messaging_surface) {
    static const char *const FALSY[] = {"0", "false", "no", "off"};
    static const char *const TRUTHY[] = {"1", "true", "yes", "on"};
    if (env_value) {
        char *tok = vs_strip_lower(env_value);
        int answer = !vs_in(tok, FALSY, 4);
        free(tok);
        return answer;
    }
    json_t *agent_cfg = json_is_object(config) ? json_obj_get(config, "agent") : NULL;
    json_t *val = json_is_object(agent_cfg) ? json_obj_get(agent_cfg, "verify_on_stop") : NULL;
    if (val && val->type == JSON_BOOL) return val->bool_val ? 1 : 0;
    char *tok = (val && val->type == JSON_STRING) ? vs_strip_lower(val->str_val) : _strdup("");
    int answer;
    if (!strcmp(tok, "auto")) {
        answer = messaging_surface ? 0 : 1;
    } else {
        answer = vs_in(tok, TRUTHY, 4);
    }
    free(tok);
    return answer;
}
