/*
 * run_config_loaders.c — C port of upstream Hermes gateway/run_config_loaders.py.
 *
 * PoP: w13_runconfig_65d8f6_busy_modes_from_config @ gateway/run_config_loaders.py:GatewayConfigLoadersMixin._busy_modes_from_config
 * PoP: w13_runconfig_65d8f6_busy_text_timing_from_config @ gateway/run_config_loaders.py:GatewayConfigLoadersMixin._busy_text_timing_from_config
 * PoP: w13_runconfig_65d8f6_human_delay_from_config @ gateway/run_config_loaders.py:GatewayConfigLoadersMixin._human_delay_from_config
 *
 * Pure config loaders. Process env and file I/O are replaced by the `config`
 * JSON seam; cfg_get's nested-dict lookup is mirrored locally.
 */
#include "noc_w13_runconfig_65d8f6.h"
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEF_DEBOUNCE 0.35
#define DEF_HARD_CAP 1.0
#define DEF_LO_MS 800
#define DEF_HI_MS 2500

/* cfg_get(cfg, section, key, default): NULL means "absent" (use default);
 * an explicit JSON_NULL is returned as-is (Python `dict.get` semantics). */
static const json_t *cfg_val(const json_t *cfg, const char *sec, const char *key) {
    if (!json_is_object(cfg)) return NULL;
    const json_t *s = json_obj_get(cfg, sec);
    if (!json_is_object(s)) return NULL;
    return json_obj_get(s, key);
}

/* str(value or "").strip().lower() for the scalar types the loaders accept. */
static void lower_strip(const json_t *v, char *out, size_t cap) {
    out[0] = '\0';
    const char *s = NULL;
    char num[64];
    if (v && v->type == JSON_STRING) s = v->str_val;
    else if (v && v->type == JSON_NUMBER) { snprintf(num, sizeof num, "%.17g", v->num_val); s = num; }
    else if (v && v->type == JSON_BOOL) s = v->bool_val ? "true" : "false";
    if (!s) return;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    size_t k = 0;
    for (size_t i = a; i < b && k + 1 < cap; i++) out[k++] = (char)tolower((unsigned char)s[i]);
    out[k] = '\0';
}

static int is_busy_input(const char *s) {
    return !strcmp(s, "interrupt") || !strcmp(s, "queue") || !strcmp(s, "steer");
}

json_t *w13_runconfig_65d8f6_busy_modes_from_config(const struct json_t *config,
                                                     const struct json_t *fallback_input,
                                                     const struct json_t *fallback_text) {
    char raw_in[128], raw_tx[128], fb_in[128], fb_tx[128];
    lower_strip(cfg_val(config, "display", "busy_input_mode"), raw_in, sizeof raw_in);
    lower_strip(cfg_val(config, "display", "busy_text_mode"), raw_tx, sizeof raw_tx);
    lower_strip(fallback_input, fb_in, sizeof fb_in);
    lower_strip(fallback_text, fb_tx, sizeof fb_tx);
    int valid_in = is_busy_input(raw_in);
    const char *input_mode = valid_in ? raw_in : fb_in;
    const char *text_mode;
    if (!strcmp(raw_tx, "interrupt") || !strcmp(raw_tx, "queue")) {
        text_mode = raw_tx;
    } else if (valid_in) {
        text_mode = !strcmp(input_mode, "queue") ? "queue" : "interrupt";
    } else {
        text_mode = fb_tx;
    }
    json_t *out = json_array();
    json_append(out, json_string(input_mode));
    json_append(out, json_string(text_mode));
    return out;
}

/* float(raw) with Python's "reject negative / NaN / unparseable" behaviour. */
static double parse_timeout(const json_t *v, double def) {
    if (!v || v->type == JSON_NULL) return def;
    double x;
    if (v->type == JSON_NUMBER) {
        x = v->num_val;
    } else if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        if (!*s) return def;
        char *end = NULL;
        x = strtod(s, &end);
        if (!end || *end != '\0') return def;
    } else {
        return def;
    }
    if (x < 0 || x != x) return def;
    return x;
}

json_t *w13_runconfig_65d8f6_busy_text_timing_from_config(const struct json_t *config) {
    double debounce = parse_timeout(cfg_val(config, "display", "busy_text_debounce_seconds"), DEF_DEBOUNCE);
    double cap = parse_timeout(cfg_val(config, "display", "busy_text_hard_cap_seconds"), DEF_HARD_CAP);
    json_t *out = json_array();
    json_append(out, json_number(debounce));
    json_append(out, json_number(cap));
    return out;
}

static long long parse_int_ms(const json_t *v, long long def) {
    if (!v || v->type == JSON_NULL) return def;
    long long x;
    if (v->type == JSON_NUMBER) {
        x = (long long)v->num_val; /* Python int(float) truncates toward zero */
    } else if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        if (!*s) return def;
        char *end = NULL;
        x = strtoll(s, &end, 10);
        if (!end || *end != '\0') return def; /* int("5.7") raises */
    } else {
        return def;
    }
    if (x < 0) return def;
    return x;
}

json_t *w13_runconfig_65d8f6_human_delay_from_config(const struct json_t *config) {
    char mode[128];
    lower_strip(cfg_val(config, "human_delay", "mode"), mode, sizeof mode);
    if (!mode[0] || !strcmp(mode, "off")) return json_null();
    if (strcmp(mode, "custom") != 0) {
        /* natural and any unknown mode both use the fixed range */
        json_t *out = json_array();
        json_append(out, json_number(DEF_LO_MS));
        json_append(out, json_number(DEF_HI_MS));
        return out;
    }
    long long lo = parse_int_ms(cfg_val(config, "human_delay", "min_ms"), DEF_LO_MS);
    long long hi = parse_int_ms(cfg_val(config, "human_delay", "max_ms"), DEF_HI_MS);
    if (lo > hi) { lo = DEF_LO_MS; hi = DEF_HI_MS; }
    json_t *out = json_array();
    json_append(out, json_number((double)lo));
    json_append(out, json_number((double)hi));
    return out;
}

