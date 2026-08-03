/*
 * port_display_config.c — faithful C11 port of gateway/display_config.py.
 *
 * Ports (single-concern, oracle-verified against LIVE Python):
 *   - display_config__normalise(setting, value)         <- _normalise
 *   - display_config__resolve(user_config, platform_key, setting, fallback)
 *                                                 <- resolve_display_setting
 *
 * Both are pure transforms over a parsed config dict; no external I/O, no
 * globals mutated per-call (the default tables are immutable build-time data).
 *
 * PoP: display_config__normalise @ gateway/display_config.py:_normalise
 * PoP: display_config__resolve @ gateway/display_config.py:resolve_display_setting
 */

#include "hermes_json.h"
#include "hermes_gateway_display_config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>

/* ------------------------------------------------------------------ */
/* Canonical JSON scalar serialiser (matches json.dumps(ensure_ascii=False)) */
/* ------------------------------------------------------------------ */

static char *dc_quote_escape(const char *s)
{
    if (!s) s = "";
    size_t need = 3; /* quotes + nul */
    for (const char *p = s; *p; ++p) {
        if (*p == '"' || *p == '\\' || *p < 0x20) need += 2;
        else need += 1;
    }
    char *out = malloc(need);
    if (!out) return NULL;
    char *o = out;
    *o++ = '"';
    for (const char *p = s; *p; ++p) {
        if (*p == '"' || *p == '\\') { *o++ = '\\'; *o++ = *p; }
        else if (*p < 0x20) { /* emit \u00XX for control chars */
            static const char *hex = "0123456789abcdef";
            *o++ = '\\'; *o++ = 'u'; *o++ = '0'; *o++ = '0';
            *o++ = hex[((unsigned char)*p) >> 4];
            *o++ = hex[((unsigned char)*p) & 0xf];
        } else *o++ = *p;
    }
    *o++ = '"';
    *o = '\0';
    return out;
}

/* Return a freshly malloc'd canonical JSON string for a scalar/leaf node. */
static char *dc_canonical(const json_t *v)
{
    if (!v) return strdup("null");
    switch (v->type) {
    case JSON_NULL:
        return strdup("null");
    case JSON_BOOL:
        return strdup(v->bool_val ? "true" : "false");
    case JSON_STRING:
        return dc_quote_escape(v->str_val);
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == (long)d) {
            char buf[32];
            snprintf(buf, sizeof buf, "%ld", (long)d);
            return strdup(buf);
        }
        char buf[64];
        snprintf(buf, sizeof buf, "%.15g", d);
        return strdup(buf);
    }
    default:
        /* arrays / objects: libjson serialises compact, matching Python. */
        return json_serialize(v);
    }
}

/* Python str(value) for non-string scalars (number/bool/null/container). */
static char *dc_py_str(const json_t *v)
{
    if (!v) return strdup("None");
    switch (v->type) {
    case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
    case JSON_NULL:   return strdup("None");
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == (long)d) {
            char buf[32]; snprintf(buf, sizeof buf, "%ld", (long)d);
            return strdup(buf);
        }
        char buf[64]; snprintf(buf, sizeof buf, "%.15g", d);
        return strdup(buf);
    }
    default: return json_serialize(v);
    }
}

/* truthy() ~ Python bool(x) for the non-string branch of the bool settings. */
static bool dc_truthy(const json_t *v)
{
    if (!v) return false;
    switch (v->type) {
    case JSON_BOOL:   return v->bool_val;
    case JSON_NULL:   return false;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    default:          return false;
    }
}

/* ------------------------------------------------------------------ */
/* _normalise (pure)                                                   */
/* ------------------------------------------------------------------ */

static bool dc_in(const char *s, ...)
{
    if (!s) return false;
    bool found = false;
    va_list ap;
    va_start(ap, s);
    const char *c;
    while ((c = va_arg(ap, const char *)) != NULL) {
        if (strcmp(s, c) == 0) { found = true; break; }
    }
    va_end(ap);
    return found;
}

/* Returns an OWNED json_t node holding the normalised value. */
json_t *display_config__normalise_node(const char *setting, const json_t *value)
{
    if (setting && strcmp(setting, "tool_progress") == 0) {
        if (value && value->type == JSON_BOOL)
            return json_string(value->bool_val ? "all" : "off");
        char *raw = dc_py_str(value);
        for (char *p = raw; *p; ++p) *p = (char)tolower((unsigned char)*p);
        /* Match Python _normalise: recognised values pass through;
         * unrecognised values (including "separate") fall back to "all". */
        if (strcmp(raw, "off") != 0 && strcmp(raw, "new") != 0 &&
            strcmp(raw, "all") != 0 && strcmp(raw, "verbose") != 0 &&
            strcmp(raw, "log") != 0)
            free(raw), raw = strdup("all");
        json_t *r = json_string(raw);
        free(raw);
        return r;
    }

    if (setting && dc_in(setting,
            "show_reasoning", "streaming", "interim_assistant_messages",
            "long_running_notifications", "busy_ack_detail", "cleanup_progress",
            NULL)) {
        if (value && value->type == JSON_STRING) {
            char *low = strdup(value->str_val ? value->str_val : "");
            for (char *p = low; *p; ++p) *p = (char)tolower((unsigned char)*p);
            bool t = dc_in(low, "true", "1", "yes", "on", NULL);
            free(low);
            return json_bool(t);
        }
        return json_bool(dc_truthy(value));
    }

    if (setting && strcmp(setting, "tool_progress_grouping") == 0) {
        char *raw = dc_py_str(value);
        for (char *p = raw; *p; ++p) *p = (char)tolower((unsigned char)*p);
        bool ok = dc_in(raw, "accumulate", "separate", NULL);
        json_t *r = json_string(ok ? raw : "accumulate");
        free(raw);
        return r;
    }

    if (setting && strcmp(setting, "reasoning_style") == 0) {
        char *raw = dc_py_str(value);
        for (char *p = raw; *p; ++p) *p = (char)tolower((unsigned char)*p);
        bool ok = dc_in(raw, "code", "blockquote", "subtext", NULL);
        json_t *r = json_string(ok ? raw : "code");
        free(raw);
        return r;
    }

    if (setting && strcmp(setting, "tool_preview_length") == 0) {
        long n = 0;
        if (value) {
            if (value->type == JSON_NUMBER) n = (long)value->num_val;
            else if (value->type == JSON_STRING && value->str_val) {
                char *end = NULL;
                long parsed = strtol(value->str_val, &end, 10);
                if (end && *end == '\0') n = parsed;
            }
        }
        return json_number((double)n);
    }

    /* Unknown setting: return value unchanged (normalise falls through). */
    return value ? json_copy(value) : json_null();
}

/* ------------------------------------------------------------------ */
/* Built-in default tables (immutable; built once)                     */
/* ------------------------------------------------------------------ */

static json_t *g_global_defaults = NULL;
static json_t *g_platform_defaults = NULL;

static json_t *dc_build_global_defaults(void)
{
    json_t *g = json_object();
    json_set(g, "tool_progress", json_string("all"));
    json_set(g, "tool_progress_grouping", json_string("accumulate"));
    json_set(g, "show_reasoning", json_bool(false));
    json_set(g, "reasoning_style", json_string("code"));
    json_set(g, "tool_preview_length", json_number(0.0));
    json_set(g, "streaming", json_null());
    json_set(g, "interim_assistant_messages", json_bool(true));
    json_set(g, "long_running_notifications", json_bool(true));
    json_set(g, "busy_ack_detail", json_bool(true));
    json_set(g, "cleanup_progress", json_bool(false));
    return g;
}

static json_t *dc_tier(const char *tool_progress, long preview, bool streaming,
                       bool interim, bool lrn, bool busy)
{
    json_t *t = json_object();
    json_set(t, "tool_progress", json_string(tool_progress));
    json_set(t, "show_reasoning", json_bool(false));
    json_set(t, "tool_preview_length", json_number((double)preview));
    json_set(t, "streaming", json_null());
    json_set(t, "interim_assistant_messages", json_bool(interim));
    json_set(t, "long_running_notifications", json_bool(lrn));
    json_set(t, "busy_ack_detail", json_bool(busy));
    return t;
}

static json_t *dc_build_platform_defaults(void)
{
    json_t *p = json_object();

    json_t *high = dc_tier("all", 40, false, true, true, true);
    json_t *tg = json_copy(high);
    json_set(tg, "tool_progress", json_string("off"));
    json_set(tg, "busy_ack_detail", json_bool(false));
    json_set(p, "telegram", tg);

    json_t *td = json_copy(high);
    json_set(td, "reasoning_style", json_string("subtext"));
    json_set(p, "discord", td);

    json_t *med = dc_tier("new", 40, false, true, true, true);
    json_t *ts = json_copy(med);
    json_set(ts, "tool_progress", json_string("off"));
    json_set(p, "slack", ts);
    json_set(p, "mattermost", json_copy(med));
    json_set(p, "matrix", json_copy(med));
    json_set(p, "feishu", json_copy(med));

    json_t *low = dc_tier("off", 40, false, false, false, false);
    json_set(p, "signal", json_copy(low));
    json_t *wa = json_copy(med); /* Baileys bridge supports /edit */
    json_set(p, "whatsapp", wa);
    json_set(p, "whatsapp_cloud", json_copy(low));
    json_set(p, "bluebubbles", json_copy(low));
    json_set(p, "weixin", json_copy(low));
    json_set(p, "wecom", json_copy(low));
    json_set(p, "wecom_callback", json_copy(low));
    json_set(p, "dingtalk", json_copy(low));

    json_t *min = dc_tier("off", 0, false, false, false, false);
    json_set(p, "email", json_copy(min));
    json_set(p, "sms", json_copy(min));
    json_set(p, "webhook", json_copy(min));
    json_set(p, "homeassistant", json_copy(min));

    json_t *api = json_copy(high);
    json_set(api, "tool_preview_length", json_number(0.0));
    json_set(p, "api_server", api);

    json_free(high); json_free(med); json_free(low); json_free(min);
    return p;
}

static void dc_ensure_defaults(void)
{
    if (!g_global_defaults)  g_global_defaults = dc_build_global_defaults();
    if (!g_platform_defaults) g_platform_defaults = dc_build_platform_defaults();
}

/* Safe obj_get: returns NULL unless it's a present, non-null member. */
static const json_t *dc_present(const json_t *obj, const char *key)
{
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    const json_t *v = json_obj_get(obj, key);
    if (!v || v->type == JSON_NULL) return NULL;
    return v;
}

/* ------------------------------------------------------------------ */
/* resolve_display_setting (pure)                                      */
/* ------------------------------------------------------------------ */

/* Returns an OWNED json_t node holding the resolved value. */
json_t *display_config__resolve_node(const json_t *user_config,
                                     const char *platform_key,
                                     const char *setting,
                                     const json_t *fallback)
{
    dc_ensure_defaults();

    const json_t *display_cfg = dc_present(user_config, "display");
    const json_t *platforms = dc_present(display_cfg, "platforms");
    const json_t *plat_overrides = dc_present(platforms, platform_key);

    /* 1. Explicit per-platform override */
    if (plat_overrides) {
        const json_t *val = dc_present(plat_overrides, setting);
        if (val) return display_config__normalise_node(setting, val);
    }

    /* 1b. Backward compat: display.tool_progress_overrides.<platform> */
    if (setting && strcmp(setting, "tool_progress") == 0) {
        const json_t *legacy = dc_present(display_cfg, "tool_progress_overrides");
        const json_t *val = dc_present(legacy, platform_key);
        if (val) return display_config__normalise_node(setting, val);
    }

    /* 2. Global user setting (skip for streaming — CLI-only) */
    if (!(setting && strcmp(setting, "streaming") == 0)) {
        const json_t *val = dc_present(display_cfg, setting);
        if (val) return display_config__normalise_node(setting, val);
    }

    /* 3. Built-in platform default (NOT normalised) */
    {
        const json_t *pdef = json_obj_get(g_platform_defaults, platform_key);
        if (pdef && pdef->type == JSON_OBJECT) {
            const json_t *val = dc_present(pdef, setting);
            if (val) return json_copy(val);
        }
    }

    /* 4. Built-in global default */
    const json_t *gdef = dc_present(g_global_defaults, setting);
    if (gdef) return json_copy(gdef);

    /* 5. Fallback */
    return fallback ? json_copy(fallback) : json_null();
}

/* Convenience: serialize normalise result to a freshly malloc'd canonical str. */
char *display_config__normalise(const char *setting, const json_t *value)
{
    json_t *n = display_config__normalise_node(setting, value);
    char *s = dc_canonical(n);
    json_free(n);
    return s;
}

/* Convenience: serialize resolve result to a freshly malloc'd canonical str. */
char *display_config__resolve(const json_t *user_config, const char *platform_key,
                              const char *setting, const json_t *fallback)
{
    json_t *r = display_config__resolve_node(user_config, platform_key, setting, fallback);
    char *s = dc_canonical(r);
    json_free(r);
    return s;
}
