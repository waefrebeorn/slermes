/*
 * setup_migration.c — faithful C11 port of upstream Hermes
 * hermes_cli/setup_migration.py (wave14 gate w14_setupmigra_bd703d).
 *
 * PoP: w14_setupmigra_bd703d_model_summary @ hermes_cli/setup_migration.py:_model_summary
 * PoP: w14_setupmigra_bd703d_cfg_summary @ hermes_cli/setup_migration.py:_cfg_summary
 * PoP: w14_setupmigra_bd703d_get_section_summary @ hermes_cli/setup_migration.py:_get_section_config_summary
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n; char *p;
    if (!s) return NULL;
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

/* Python str.strip() of ASCII whitespace (fixtures are ASCII). */
static char *py_strip(const char *s) {
    size_t a = 0, b;
    char *r;
    if (!s) return dupstr("");
    b = strlen(s);
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    r = (char *)malloc(b - a + 1);
    if (r) { memcpy(r, s + a, b - a); r[b - a] = '\0'; }
    return r;
}

/* Python truthiness of a JSON value. */
static int truthy(const struct json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: return json_len(v) > 0;
        case JSON_OBJECT: return json_object_size(v) > 0;
        default: return 0;
    }
}

/* Python str() coercion for the scalar values we exercise. */
static void py_str(const struct json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) {
        snprintf(buf, n, "None");
    } else if (v->type == JSON_STRING) {
        snprintf(buf, n, "%s", v->str_val ? v->str_val : "");
    } else if (v->type == JSON_BOOL) {
        snprintf(buf, n, "%s", v->bool_val ? "True" : "False");
    } else if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, n, "%lld", (long long)v->num_val);
        else
            snprintf(buf, n, "%g", v->num_val);
    } else {
        buf[0] = '\0';
    }
}

static struct json_t *model_summary_core(const struct json_t *config, int has_creds) {
    const struct json_t *model;
    char val[4096];
    if (!has_creds) return json_null();
    model = config ? json_obj_get(config, "model") : NULL;
    if (model && model->type == JSON_STRING) {
        char *t = py_strip(model->str_val ? model->str_val : "");
        if (t && t[0]) { struct json_t *out = json_string(t); free(t); return out; }
        free(t);
    }
    if (model && model->type == JSON_OBJECT) {
        const struct json_t *v = json_obj_get(model, "default");
        if (!truthy(v)) v = json_obj_get(model, "model");
        if (!truthy(v)) return json_string("configured");
        py_str(v, val, sizeof val);
        return json_string(val);
    }
    return json_string("configured");
}

struct json_t *w14_setupmigra_bd703d_model_summary(const struct json_t *in) {
    const struct json_t *config = json_obj_get(in, "config");
    int has = json_get_bool(in, "has_creds", false) ? 1 : 0;
    return model_summary_core(config, has);
}

/* hermes_cli.config.cfg_get(cfg, section, key, default=default). */
static const struct json_t *cfg_get3(const struct json_t *config, const char *section,
                                     const char *key, const struct json_t *dflt) {
    const struct json_t *node, *v;
    if (!config || config->type != JSON_OBJECT) return dflt;
    node = json_obj_get(config, section);
    if (!node || node->type != JSON_OBJECT) return dflt;
    v = json_obj_get(node, key);
    return v ? v : dflt;
}

static struct json_t *cfg_core(const struct json_t *config, const char *section,
                               const char *key, const struct json_t *dflt,
                               const char *prefix) {
    char val[4096], out[8192];
    py_str(cfg_get3(config, section, key, dflt), val, sizeof val);
    snprintf(out, sizeof out, "%s%s", prefix ? prefix : "", val);
    return json_string(out);
}

struct json_t *w14_setupmigra_bd703d_cfg_summary(const struct json_t *in) {
    const struct json_t *config = json_obj_get(in, "config");
    const char *section = json_get_str(in, "section", "");
    const char *key = json_get_str(in, "key", "");
    const char *prefix = json_get_str(in, "prefix", "");
    return cfg_core(config, section, key, json_obj_get(in, "default"), prefix);
}

struct json_t *w14_setupmigra_bd703d_get_section_summary(const struct json_t *in) {
    const char *sk = json_get_str(in, "section_key", "");
    const struct json_t *config = json_obj_get(in, "config");
    int has = json_get_bool(in, "has_creds", false) ? 1 : 0;
    struct json_t *d, *out;

    if (!strcmp(sk, "model")) return model_summary_core(config, has);
    if (!strcmp(sk, "terminal")) {
        d = json_string("local");
        out = cfg_core(config, "terminal", "backend", d, "backend: ");
        json_free(d);
        return out;
    }
    if (!strcmp(sk, "agent")) {
        d = json_number(90);
        out = cfg_core(config, "agent", "max_turns", d, "max turns: ");
        json_free(d);
        return out;
    }
    return json_null();
}
