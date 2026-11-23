/*
 * synthetic_turn.c — C port of upstream Hermes tui_gateway/synthetic_turn.py.
 *
 * PoP: w14_synthetict_f47965_synth_turn_armed @ tui_gateway/synthetic_turn.py:synth_turn_armed
 * PoP: w14_synthetict_f47965_parse_spec @ tui_gateway/synthetic_turn.py:SyntheticHeavyAgent._parse_spec
 * PoP: w14_synthetict_f47965_maybe_build_agent @ tui_gateway/synthetic_turn.py:maybe_build_synthetic_agent
 *
 * Env reads and the arm flag are explicit fixture seams (no process env mutation).
 */
#include "noc_w14_synthetict_f47965.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupn_local(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static char *strip_ws(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b-1])) b--;
    return dupn_local(s + a, b - a);
}
static int truthy(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}
/* Python float(v): number/bool/numeric string; 0 on failure. */
static int py_float(const struct json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_STRING && v->str_val) {
        const char *s = v->str_val;
        while (*s && isspace((unsigned char)*s)) s++;
        if (!*s) return 0;
        char *end = NULL; double d = strtod(s, &end);
        while (*end && isspace((unsigned char)*end)) end++;
        if (end && *end == '\0') { *out = d; return 1; }
    }
    return 0;
}
/* Python int(v): number/bool/base-10 integer string; 0 on failure. */
static int py_int(const struct json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_STRING && v->str_val) {
        const char *s = v->str_val;
        while (*s && isspace((unsigned char)*s)) s++;
        if (!*s) return 0;
        char *end = NULL; long long x = strtoll(s, &end, 10);
        while (*end && isspace((unsigned char)*end)) end++;
        if (end && *end == '\0') { *out = x; return 1; }
    }
    return 0;
}
/* str(v or "") for the model-override seam. */
static char *str_or_empty(const struct json_t *v) {
    char buf[64];
    if (!truthy(v)) return dupn_local("", 0);
    if (v->type == JSON_STRING) return dupn_local(v->str_val ? v->str_val : "", strlen(v->str_val ? v->str_val : ""));
    if (v->type == JSON_BOOL) return dupn_local(v->bool_val ? "True" : "False", v->bool_val ? 4 : 5);
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%g", v->num_val);
        return dupn_local(buf, strlen(buf));
    }
    return dupn_local("", 0);
}

struct json_t *w14_synthetict_f47965_synth_turn_armed(const struct json_t *env_value) {
    return json_bool(env_value && env_value->type == JSON_STRING && env_value->str_val
                     && !strcmp(env_value->str_val, "1"));
}

struct json_t *w14_synthetict_f47965_parse_spec(const struct json_t *message, const struct json_t *defaults) {
    struct json_t *spec = NULL;
    if (message && message->type == JSON_STRING && message->str_val) {
        char *t = strip_ws(message->str_val);
        if (t[0] == '{') {
            char *err = NULL;
            spec = json_parse(t, &err);
            free(err);
        }
        free(t);
    }
    const struct json_t *d = (defaults && defaults->type == JSON_OBJECT) ? defaults : NULL;
    const struct json_t *src = (spec && spec->type == JSON_OBJECT) ? spec : NULL;

    double duration = 8.0, delta = 0.05, sleep_s = 0.0;
    long long chunk = 20000, tpd = 512;
    if (d) {
        double x; long long i;
        if (py_float(json_obj_get(d, "HERMES_ISO_CERTIFY_DURATION_S"), &x)) duration = x;
        if (py_float(json_obj_get(d, "HERMES_ISO_CERTIFY_DELTA_S"), &x)) delta = x;
        if (py_int(json_obj_get(d, "HERMES_ISO_CERTIFY_CHUNK"), &i)) chunk = i;
        if (py_int(json_obj_get(d, "HERMES_ISO_CERTIFY_TPD"), &i)) tpd = i;
    }
    /* spec.get(key, default()) then the field's caster */
    double vd; long long vi;
    const struct json_t *f;
    if (src && (f = json_obj_get(src, "duration_s")) && py_float(f, &vd)) duration = vd;
    if (src && (f = json_obj_get(src, "delta_interval_s")) && py_float(f, &vd)) delta = vd;
    if (src && (f = json_obj_get(src, "sleep_s")) && py_float(f, &vd)) sleep_s = vd;
    if (src && (f = json_obj_get(src, "chunk")) && py_int(f, &vi)) chunk = vi;
    if (src && (f = json_obj_get(src, "tokens_per_delta")) && py_int(f, &vi)) tpd = vi;

    struct json_t *out = json_object();
    json_set(out, "duration_s", json_number(duration));
    json_set(out, "chunk", json_number((double)chunk));
    json_set(out, "delta_interval_s", json_number(delta));
    json_set(out, "tokens_per_delta", json_number((double)tpd));
    json_set(out, "sleep_s", json_number(sleep_s));
    if (spec) json_free(spec);
    return out;
}

struct json_t *w14_synthetict_f47965_maybe_build_agent(const struct json_t *session_id,
                                     const struct json_t *model_override,
                                     const struct json_t *armed) {
    (void)session_id;
    if (!armed || armed->type != JSON_BOOL || !armed->bool_val) return json_null();
    const struct json_t *mo = model_override;
    char *label = NULL;
    if (mo && mo->type == JSON_OBJECT) {
        const struct json_t *mv = json_obj_get(mo, "model");
        label = truthy(mv) ? str_or_empty(mv) : dupn_local("", 0);
    } else if (mo && mo->type == JSON_STRING) {
        label = str_or_empty(mo);
    } else {
        label = dupn_local("", 0);
    }
    const char *model = label[0] ? label : "synthetic-heavy";
    struct json_t *out = json_string(model);
    free(label);
    return out;
}
