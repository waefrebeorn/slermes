/*
 * browser_session_cloud.c — C home for upstream Hermes tools/browser_session_cloud.py.
 *
 * PoP: w15_browserses_71b99f_command_timeout @ tools/browser_session_cloud.py:command_timeout
 * PoP: w15_browserses_71b99f_inactivity_timeout @ tools/browser_session_cloud.py:inactivity_timeout
 * PoP: w15_browserses_71b99f_chromium_args @ tools/browser_session_cloud.py:chromium_args
 * PoP: w15_browserses_71b99f_executable_path @ tools/browser_session_cloud.py:executable_path
 * PoP: w15_browserses_71b99f_browser_engine @ tools/browser_session_cloud.py:get_browser_engine
 * PoP: w15_browserses_71b99f_is_headed @ tools/browser_session_cloud.py:is_headed_mode
 * PoP: w15_browserses_71b99f_allow_private_urls @ tools/browser_session_cloud.py:allow_private_urls
 * PoP: w15_browserses_71b99f_auto_local_private @ tools/browser_session_cloud.py:auto_local_for_private_urls
 * PoP: w15_browserses_71b99f_use_real_profile @ tools/browser_session_cloud.py:use_real_profile
 *
 * Every exported symbol is w15_browserses_71b99f_ prefixed. The live module's
 * config reader (`browser_section()`) is injected as the JSON `section`
 * argument so the oracle can drive it without touching the process config.
 */
#include "noc_w15_browserses_71b99f.h"
#include "hermes_json.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_COMMAND_TIMEOUT 30

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static int py_bool(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default: return 0;
    }
}

static char *strip_dup(const char *s) {
    if (!s) return dupstr("");
    const char *a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, a, n); p[n] = '\0'; }
    return p;
}

/* Python int(v) into a C long long; 0 on ValueError/TypeError. */
static int py_int(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        while (*s && isspace((unsigned char)*s)) s++;
        if (!*s) return 0;
        errno = 0;
        char *end = NULL;
        long long n = strtoll(s, &end, 10);
        if (end == s) return 0;
        while (*end && isspace((unsigned char)*end)) end++;
        if (*end) return 0;
        *out = n;
        return 1;
    }
    return 0;
}

/* Python str(v) for the scalar values fixtures use; .strip() applied by caller. */
static char *py_str_dup(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_STRING) return dupstr(v->str_val);
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == (double)(long long)d) snprintf(buf, sizeof(buf), "%lld", (long long)d);
        else snprintf(buf, sizeof(buf), "%g", d);
        return dupstr(buf);
    }
    return dupstr("");
}

static int ieq_ci(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

/* utils.is_truthy_value(value, default=False) */
static int is_truthy_value(const json_t *v, int def) {
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_STRING) {
        char *s = strip_dup(v->str_val);
        int r = ieq_ci(s, "1") || ieq_ci(s, "true") || ieq_ci(s, "yes") || ieq_ci(s, "on");
        free(s);
        return r;
    }
    return py_bool(v);
}

static const json_t *cfg(const struct json_t *section, const char *key) {
    return (section && section->type == JSON_OBJECT) ? json_obj_get(section, key) : NULL;
}

long long w15_browserses_71b99f_command_timeout(const struct json_t *section) {
    const json_t *v = cfg(section, "command_timeout");
    if (!v || v->type == JSON_NULL) return DEFAULT_COMMAND_TIMEOUT;
    long long n = 0;
    if (!py_int(v, &n)) return DEFAULT_COMMAND_TIMEOUT;
    return n < 5 ? 5 : n;
}

long long w15_browserses_71b99f_inactivity_timeout(const struct json_t *section, long long def) {
    const json_t *v = cfg(section, "inactivity_timeout");
    if (!v || v->type == JSON_NULL) return def;
    long long n = 0;
    if (!py_int(v, &n)) return def;
    return n < 30 ? 30 : n;
}

struct json_t *w15_browserses_71b99f_chromium_args(const struct json_t *section) {
    json_t *out = json_array();
    const json_t *v = cfg(section, "chromium_args");
    if (!v || !py_bool(v)) return out;          /* value or [] */
    if (v->type == JSON_STRING) {
        size_t n = strlen(v->str_val);
        char *norm = (char *)malloc(n + 1);
        if (!norm) return out;
        for (size_t i = 0; i < n; ++i) norm[i] = v->str_val[i] == '\n' ? ',' : v->str_val[i];
        norm[n] = '\0';
        char *save = NULL;
        for (char *tok = strtok_r(norm, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
            char *s = strip_dup(tok);
            if (*s) json_append(out, json_string(s));
            free(s);
        }
        free(norm);
    } else if (v->type == JSON_ARRAY) {
        for (size_t i = 0; i < v->c.count; ++i) {
            char *s = py_str_dup(v->c.items[i]);
            char *t = strip_dup(s);
            if (*t) json_append(out, json_string(t));
            free(t); free(s);
        }
    }
    return out;
}

char *w15_browserses_71b99f_executable_path(const struct json_t *section) {
    const json_t *v = cfg(section, "executable_path");
    if (!v || !py_bool(v)) return dupstr("");   /* str(v or "").strip() */
    char *s = py_str_dup(v);
    char *t = strip_dup(s);
    free(s);
    return t;
}

char *w15_browserses_71b99f_browser_engine(const struct json_t *section) {
    const json_t *v = cfg(section, "engine");
    if (!v || !py_bool(v)) return dupstr("auto");
    char *s = py_str_dup(v);
    char *t = strip_dup(s);
    for (char *p = t; *p; ++p) *p = (char)tolower((unsigned char)*p);
    char *out;
    if (!*t || !strcmp(t, "chrome")) out = dupstr("auto");
    else if (!strcmp(t, "auto") || !strcmp(t, "lightpanda")) out = dupstr(t);
    else out = dupstr("auto");
    free(t); free(s);
    return out;
}

int w15_browserses_71b99f_is_headed(const struct json_t *section) {
    return is_truthy_value(cfg(section, "headed"), 0);
}

int w15_browserses_71b99f_allow_private_urls(const struct json_t *section) {
    return is_truthy_value(cfg(section, "allow_private_urls"), 0);
}

int w15_browserses_71b99f_auto_local_private(const struct json_t *section) {
    const json_t *v = cfg(section, "auto_local_for_private_urls");
    return v ? py_bool(v) : 1;                   /* default True, parse=bool */
}

int w15_browserses_71b99f_use_real_profile(const struct json_t *section) {
    const json_t *v = cfg(section, "use_real_profile");
    return v ? py_bool(v) : 0;                   /* default False, parse=bool */
}
