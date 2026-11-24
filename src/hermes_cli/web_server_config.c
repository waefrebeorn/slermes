/*
 * web_server_config.c — C port of upstream Hermes hermes_cli/web_server_config.py.
 *
 * PoP: w16_webserver_33d5d4_infer_type @ hermes_cli/web_server_config.py:_infer_type
 * PoP: w16_webserver_33d5d4_is_command_provider_block @ hermes_cli/web_server_config.py:_is_command_provider_block
 * PoP: w16_webserver_33d5d4_normalize_config_for_web @ hermes_cli/web_server_config.py:_normalize_config_for_web
 * PoP: w16_webserver_33d5d4_endpoint_known_to_config @ hermes_cli/web_server_config.py:_endpoint_known_to_config
 */
#include "noc_w16_webserver_33d5d4.h"
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

static int json_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

static char *trim_lower_dup(const char *s) {
    if (!s) return dupstr("");
    while (*s && isspace((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - s);
    char *p = (char *)malloc(n + 1);
    if (!p) return dupstr("");
    for (size_t i = 0; i < n; i++) p[i] = (char)tolower((unsigned char)s[i]);
    p[n] = '\0';
    return p;
}

/* Python: str(value or "").strip().rstrip("/").lower() */
static char *norm_url(const json_t *v) {
    const char *s = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
    char *t = trim_lower_dup(s);
    size_t n = strlen(t);
    while (n > 0 && t[n - 1] == '/') t[--n] = '\0';  /* NUL at the new length */
    return t;
}

static const json_t *first_truthy(const json_t *obj, const char *const *keys, size_t nkeys) {
    for (size_t i = 0; i < nkeys; i++) {
        const json_t *v = json_obj_get(obj, keys[i]);
        if (json_truthy(v)) return v;
    }
    return NULL;
}

struct json_t *w16_webserver_33d5d4_infer_type(const json_t *value) {
    if (!value) return json_string("string");
    switch (value->type) {
        case JSON_BOOL:   return json_string("boolean");
        case JSON_NUMBER: return json_string("number");
        case JSON_ARRAY:  return json_string("list");
        case JSON_OBJECT: return json_string("object");
        default:          return json_string("string");
    }
}

bool w16_webserver_33d5d4_is_command_provider_block(const json_t *value) {
    if (!json_is_object(value)) return false;
    const json_t *pt = json_obj_get(value, "type");
    char *ptype = dupstr("");
    if (json_truthy(pt) && pt->type == JSON_STRING) ptype = trim_lower_dup(pt->str_val);
    int bad = ptype[0] && strcmp(ptype, "command");
    free(ptype);
    if (bad) return false;
    const json_t *cmd = json_obj_get(value, "command");
    if (!cmd || cmd->type != JSON_STRING || !cmd->str_val) return false;
    char *c = trim_lower_dup(cmd->str_val);
    int ok = c[0] != '\0';
    free(c);
    return ok ? true : false;
}

struct json_t *w16_webserver_33d5d4_normalize_config_for_web(const json_t *config) {
    struct json_t *out = json_object();
    if (!json_is_object(config)) return out;
    for (size_t i = 0; i < json_object_size(config); i++) {
        const char *k = json_object_get_key_at(config, i);
        json_t *v = json_object_get_at(config, i);
        if (k) json_set(out, k, v ? json_copy(v) : json_null());
    }
    const json_t *mv = json_obj_get(config, "model");
    if (json_is_object(mv)) {
        const json_t *ctx = json_obj_get(mv, "context_length");
        int is_int = 0;
        if (ctx && ctx->type == JSON_BOOL) is_int = 1;  /* bool is an int in Python */
        else if (ctx && ctx->type == JSON_NUMBER)
            is_int = ctx->num_val == floor(ctx->num_val) && fabs(ctx->num_val) < 9e15;
        json_set(out, "model_context_length",
                 is_int && ctx ? json_copy(ctx) : json_number(0));

        const json_t *dflt = json_obj_get(mv, "default");
        if (dflt) json_set(out, "model", json_copy(dflt));
        else {
            const json_t *nm = json_obj_get(mv, "name");
            json_set(out, "model", nm ? json_copy(nm) : json_string(""));
        }
    } else {
        json_set(out, "model_context_length", json_number(0));
    }
    return out;
}

bool w16_webserver_33d5d4_endpoint_known_to_config(const json_t *cfg, const char *base_url) {
    if (!base_url) base_url = "";
    while (*base_url && isspace((unsigned char)*base_url)) base_url++;
    const char *e = base_url + strlen(base_url);
    while (e > base_url && isspace((unsigned char)e[-1])) e--;
    size_t blen = (size_t)(e - base_url);
    char *want = (char *)malloc(blen + 1);
    if (!want) return false;
    for (size_t i = 0; i < blen; i++) want[i] = (char)tolower((unsigned char)base_url[i]);
    want[blen] = '\0';
    while (blen > 0 && want[blen - 1] == '/') want[--blen] = '\0';  /* NUL at new length */
    if (blen == 0) { free(want); return false; }

    int found = 0;
    const json_t *model_cfg = json_obj_get(cfg, "model");
    if (json_is_object(model_cfg)) {
        char *cand = norm_url(json_obj_get(model_cfg, "base_url"));
        if (!strcmp(cand, want)) found = 1;
        free(cand);
    }
    if (!found) {
        static const char *const sections[] = {"providers", "custom_providers"};
        static const char *const keys[] = {"base_url", "url", "api"};
        for (size_t s = 0; s < 2 && !found; s++) {
            const json_t *section = json_obj_get(cfg, sections[s]);
            size_t n = json_is_array(section) ? json_len(section)
                     : json_is_object(section) ? json_object_size(section) : 0;
            for (size_t i = 0; i < n; i++) {
                const json_t *entry = json_is_array(section) ? json_get(section, i)
                                      : json_object_get_at(section, i);
                if (!json_is_object(entry)) continue;
                const json_t *v = first_truthy(entry, keys, 3);
                char *cand = norm_url(v);
                if (!strcmp(cand, want)) found = 1;
                free(cand);
                if (found) break;
            }
        }
    }
    free(want);
    return found ? true : false;
}
