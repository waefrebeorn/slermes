/*
 * main_provider_setup.c — C home for upstream Hermes hermes_cli/main_provider_setup.py.
 *
 * PoP: mainprovid_short_url @ hermes_cli/main_provider_setup.py:_short_url
 * PoP: mainprovid_aux_effort_word @ hermes_cli/main_provider_setup.py:_aux_effort_word
 * PoP: mainprovid_format_aux_current @ hermes_cli/main_provider_setup.py:_format_aux_current
 * PoP: mainprovid_delegation_cfg_as_task @ hermes_cli/main_provider_setup.py:_delegation_cfg_as_task
 * PoP: mainprovid_auto_provider_name @ hermes_cli/main_provider_setup.py:_auto_provider_name
 * PoP: mainprovid_api_key_config_value @ hermes_cli/main_provider_setup.py:_custom_provider_api_key_config_value
 * PoP: mainprovid_base_url_config_value @ hermes_cli/main_provider_setup.py:_custom_provider_base_url_config_value
 * PoP: mainprovid_infer_stepfun_region @ hermes_cli/main_provider_setup.py:_infer_stepfun_region
 */
#include "noc_w18_mainprovid_56bced.h"

#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (out) { if (n) memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

typedef struct { char *p; size_t n, cap; } sb_t;
static void sb_add(sb_t *b, const char *s, size_t len) {
    if (b->n + len + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 128;
        while (nc < b->n + len + 1) nc *= 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->n, s, len); b->n += len; b->p[b->n] = '\0';
}
static void sb_puts(sb_t *b, const char *s) { sb_add(b, s, strlen(s)); }
static char *sb_take(sb_t *b) {
    char *p = b->p;
    b->p = NULL; b->n = b->cap = 0;
    return p ? p : dupstr("");
}

static char *py_strip(const char *s) {
    static const char *WS = " \t\n\r\v\f";
    if (!s) return dupstr("");
    const char *a = s;
    while (*a && strchr(WS, *a)) a++;
    const char *b = a + strlen(a);
    while (b > a && strchr(WS, b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (out) { if (n) memcpy(out, a, n); out[n] = '\0'; }
    return out;
}
static char *dup_lower(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (out) { for (size_t i = 0; i < n; ++i) out[i] = (char)tolower((unsigned char)s[i]); out[n] = '\0'; }
    return out;
}
static char *py_strip_lower(const char *s) {
    char *t = py_strip(s);
    char *out = dup_lower(t);
    free(t);
    return out;
}
static int py_falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_BOOL) return !v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val == 0.0;
    if (v->type == JSON_STRING) return !v->str_val || !*v->str_val;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count == 0;
    return 0;
}
/* Python str(x) for the JSON scalar types the fixtures use. */
static char *py_str_json(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return dupstr(buf);
    }
    return dupstr("");
}
/* Python str(x or default) */
static char *py_str_or(const json_t *v, const char *def) {
    return py_falsy(v) ? dupstr(def) : py_str_json(v);
}
static char *field_strip(const json_t *obj, const char *key, const char *def) {
    char *s = py_str_or(obj ? json_obj_get(obj, key) : NULL, def);
    char *r = py_strip(s);
    free(s);
    return r;
}
static char *replace_all(const char *s, const char *needle, const char *repl) {
    sb_t b = {0};
    size_t nl = strlen(needle);
    const char *p = s ? s : "";
    while (*p) {
        const char *q = strstr(p, needle);
        if (!q) { sb_puts(&b, p); break; }
        sb_add(&b, p, (size_t)(q - p));
        sb_puts(&b, repl);
        p = q + nl;
    }
    return sb_take(&b);
}

char *mainprovid_short_url(const char *url) {
    char *a = replace_all(url, "https://", "");
    char *b = replace_all(a, "http://", "");
    free(a);
    size_t n = strlen(b);
    while (n && b[n - 1] == '/') b[--n] = '\0';   /* rstrip("/") */
    return b;
}

char *mainprovid_aux_effort_word(const struct json_t *cfg) {
    if (!cfg || cfg->type != JSON_OBJECT) return dupstr("");
    json_t *raw = json_obj_get(cfg, "reasoning_effort");
    if (raw && raw->type == JSON_BOOL && !raw->bool_val) return dupstr("none");
    char *s = py_str_or(raw, "");
    char *out = py_strip_lower(s);
    free(s);
    return out;
}

char *mainprovid_format_aux_current(const struct json_t *cfg) {
    if (!cfg || cfg->type != JSON_OBJECT) return dupstr("auto");
    char *base = field_strip(cfg, "base_url", "");
    char *prov = field_strip(cfg, "provider", "auto");
    if (!*prov) { free(prov); prov = dupstr("auto"); }
    char *model = field_strip(cfg, "model", "");
    char *effort = mainprovid_aux_effort_word(cfg);
    sb_t b = {0};
    if (*base) {
        char *su = mainprovid_short_url(base);
        sb_puts(&b, "custom ("); sb_puts(&b, su); sb_puts(&b, ")");
        free(su);
    } else {
        sb_puts(&b, prov);
    }
    if (*model) { sb_puts(&b, " · "); sb_puts(&b, model); }
    if (*effort) { sb_puts(&b, " · "); sb_puts(&b, effort); }
    free(base); free(prov); free(model); free(effort);
    return sb_take(&b);
}

struct json_t *mainprovid_delegation_cfg_as_task(const struct json_t *cfg) {
    json_t *out = json_object();
    const json_t *d = cfg ? json_obj_get(cfg, "delegation") : NULL;
    if (!d || d->type != JSON_OBJECT) d = NULL;
    static const char *const KEYS[] = {"provider", "model", "base_url", "api_key"};
    for (size_t i = 0; i < 4; ++i) {
        char *s = field_strip(d, KEYS[i], "");
        json_set(out, KEYS[i], json_string(s));
        free(s);
    }
    json_t *re = d ? json_obj_get(d, "reasoning_effort") : NULL;
    json_set(out, "reasoning_effort", re ? json_copy(re) : json_string(""));
    return out;
}

char *mainprovid_auto_provider_name(const char *base_url) {
    char *su = mainprovid_short_url(base_url);
    size_t n = strlen(su);
    if (n >= 4 && !strcmp(su + n - 4, "/v1/")) su[n - 4] = '\0';
    else if (n >= 3 && !strcmp(su + n - 3, "/v1")) su[n - 3] = '\0';
    char *slash = strchr(su, '/');
    if (slash) *slash = '\0';
    sb_t b = {0};
    if (strstr(su, "localhost") || strstr(su, "127.0.0.1")) {
        sb_puts(&b, "Local ("); sb_puts(&b, su); sb_puts(&b, ")");
    } else {
        char *lo = dup_lower(su);
        if (strstr(lo, "runpod")) {
            sb_puts(&b, "RunPod ("); sb_puts(&b, su); sb_puts(&b, ")");
        } else {
            if (*su) {
                char first[2] = {(char)toupper((unsigned char)su[0]), 0};
                sb_puts(&b, first);
                if (su[1]) { char *rest = dup_lower(su + 1); sb_puts(&b, rest); free(rest); }
            }
        }
        free(lo);
    }
    free(su);
    return sb_take(&b);
}

char *mainprovid_api_key_config_value(const struct json_t *info, const char *resolved_api_key) {
    char *ref = field_strip(info, "api_key_ref", "");
    if (*ref) return ref;
    char *key_env = field_strip(info, "key_env", "");
    char *api_key = field_strip(info, "api_key", "");
    if (*key_env && !*api_key) {
        sb_t b = {0};
        sb_puts(&b, "${"); sb_puts(&b, key_env); sb_puts(&b, "}");
        free(ref); free(key_env); free(api_key);
        return sb_take(&b);
    }
    free(ref); free(key_env); free(api_key);
    char *r = dupstr(resolved_api_key ? resolved_api_key : "");
    char *out = py_strip(r);
    free(r);
    return out;
}

char *mainprovid_base_url_config_value(const struct json_t *info, const char *resolved_base_url) {
    char *ref = field_strip(info, "base_url_ref", "");
    if (*ref) return ref;
    free(ref);
    char *r = dupstr(resolved_base_url ? resolved_base_url : "");
    char *out = py_strip(r);
    free(r);
    return out;
}

char *mainprovid_infer_stepfun_region(const char *base_url) {
    char *v = py_strip_lower(base_url);
    int china = v && strstr(v, "api.stepfun.com");
    free(v);
    return dupstr(china ? "china" : "international");
}
