/*
 * port_nous_subscription.c
 *
 * Pure helpers ported from hermes_cli/nous_subscription.py - the feature gates
 * that decide which tools a Nous subscription unlocks.
 *
 * The module's account lookup, managed-tool-gateway calls and config writes
 * that need a live login stay in Python. What is portable is the section
 * access and the provider/feature decisions:
 *
 *   sub_norm              <- nous_subscription.py:_norm
 *   sub_section           <- nous_subscription.py:_section
 *   sub_ensure_section    <- nous_subscription.py:_ensure_section
 *   sub_provider_is_nous  <- nous_subscription.py:_provider_is_nous
 *   sub_audio_provider    <- nous_subscription.py:_audio_provider
 *   sub_uses_gateway      <- nous_subscription.py:_uses_gateway
 */

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* ===========================================================================
 *  _norm  (nous_subscription.py:_norm)
 *
 *      str(value or default).strip().lower()
 *
 *  `value or default` is Python TRUTHINESS, not a NULL check: 0, 0.0, "" and
 *  false all fall through to the default. Returns a NEW string, or NULL when
 *  value is NULL and no default was given (the caller treats that as "").
 * =========================================================================== */

/* Core string normaliser: strip().lower() of a C string. Returns a NEW string
 * (possibly ""), or NULL on OOM. */
static char *sub_norm_str(const char *value, const char *fallback)
{
    const char *raw = (value && value[0]) ? value : (fallback ? fallback : "");
    const unsigned char *s = (const unsigned char *)raw;
    size_t len = strlen(raw), start = 0, end = len;
    while (start < end && isspace(s[start])) start++;
    while (end > start && isspace(s[end - 1])) end--;

    char *out = (char *)malloc((end - start) + 1);
    if (!out) return NULL;
    size_t n = 0;
    for (size_t i = start; i < end; i++) out[n++] = (char)tolower(s[i]);
    out[n] = '\0';
    return out;
}

/* String-only variant used when the value has already been read out of a JSON
 * object. NULL/"" are the falsy spellings and yield NULL (no default). */
static char *sub_norm_json(const char *value)
{
    if (!value || !value[0]) return NULL;
    return sub_norm_str(value, "");
}

/* PoP: _norm @ hermes_cli/nous_subscription.py:_norm */
char *sub_norm(const json_t *value, const char *fallback)
{
    const char *raw = NULL;
    char numbuf[64];

    if (value) {
        switch (value->type) {
        case JSON_STRING: if (value->str_val && value->str_val[0]) raw = value->str_val; break;
        case JSON_NUMBER:
            /* 0 and 0.0 are falsy in Python */
            if (value->num_val != 0.0) { snprintf(numbuf, sizeof numbuf, "%.17g", value->num_val); raw = numbuf; }
            break;
        case JSON_BOOL:   if (value->bool_val) raw = "true"; break;
        default: break;   /* null / object / array -> falsy */
        }
    }
    return sub_norm_str(raw, fallback);
}

/* ===========================================================================
 *  _section  (nous_subscription.py:_section)
 *
 *      value = config.get(key)
 *      return value if isinstance(value, dict) else {}
 *
 *  Read-only view: a NEW empty object when the key is missing or not an object,
 *  otherwise the SAME object (borrowed, not copied). Caller frees only when
 *  created - use sub_section_owned() to know.
 * =========================================================================== */

/* PoP: _section @ hermes_cli/nous_subscription.py:_section */
const json_t *sub_section(const json_t *config, const char *key, json_t **created)
{
    if (created) *created = NULL;
    if (config && config->type == JSON_OBJECT) {
        json_t *v = json_obj_get(config, key);
        if (v && v->type == JSON_OBJECT) return v;
    }
    json_t *empty = json_object();
    if (created) *created = empty;
    return empty;
}

/* ===========================================================================
 *  _ensure_section  (nous_subscription.py:_ensure_section)
 *
 *      value = config.get(key)
 *      if not isinstance(value, dict):
 *          value = config[key] = {}
 *      return value
 *
 *  Creates (or REPLACES) the section in place. Returns the section, which is
 *  owned by config - caller frees nothing.
 * =========================================================================== */

/* PoP: _ensure_section @ hermes_cli/nous_subscription.py:_ensure_section */
json_t *sub_ensure_section(json_t *config, const char *key)
{
    if (!config || config->type != JSON_OBJECT) return NULL;
    json_t *v = json_obj_get(config, key);
    if (!v || v->type != JSON_OBJECT) {
        v = json_object();
        json_set(config, key, v);   /* json_set takes ownership */
    }
    return v;
}

/* ===========================================================================
 *  _provider_is_nous  (nous_subscription.py:_provider_is_nous)
 *
 *      _norm(_section(config, "model").get("provider")) == "nous"
 * =========================================================================== */

/* PoP: _provider_is_nous @ hermes_cli/nous_subscription.py:_provider_is_nous */
int sub_provider_is_nous(const json_t *config)
{
    json_t *created = NULL;
    const json_t *model = sub_section(config, "model", &created);
    const char *provider = NULL;
    if (model && model->type == JSON_OBJECT) {
        json_t *p = json_obj_get(model, "provider");
        if (p && p->type == JSON_STRING) provider = p->str_val;
    }
    char *norm = sub_norm_json(provider);
    int is_nous = norm && strcmp(norm, "nous") == 0;
    free(norm);
    if (created) json_free(created);
    return is_nous;
}

/* ===========================================================================
 *  _audio_provider  (nous_subscription.py:_audio_provider)
 *
 *      provider = _norm(cfg.get("provider"), default)
 *      return "openai" if (provider == "nous" or gw) else (provider or default)
 *
 *  The managedNous selection is served by OpenAI's API, and a gateway account
 *  likewise. Returns a NEW string the caller frees.
 * =========================================================================== */

/* PoP: _audio_provider @ hermes_cli/nous_subscription.py:_audio_provider */
char *sub_audio_provider(const json_t *cfg, const char *fallback, int gw)
{
    const char *provider = NULL;
    if (cfg && cfg->type == JSON_OBJECT) {
        json_t *p = json_obj_get(cfg, "provider");
        if (p && p->type == JSON_STRING) provider = p->str_val;
    }
    char *norm = sub_norm_json(provider);
    const char *use = (norm && norm[0]) ? norm : (fallback ? fallback : "");

    char *out;
    if (gw || (use[0] && strcmp(use, "nous") == 0)) {
        out = (char *)malloc(6);
        if (out) strcpy(out, "openai");
    } else {
        out = (char *)malloc(strlen(use) + 1);
        if (out) strcpy(out, use);
    }
    free(norm);
    return out;
}

/* ===========================================================================
 *  _uses_gateway  (nous_subscription.py:_uses_gateway)
 *
 *      isinstance(section, dict) and is_truthy_value(section.get("use_gateway"),
 *                                                   default=False)
 *
 *  is_truthy_value lives in utils and treats the usual YAML scalars as truthy
 *  (true/yes/on/1). `section` being a non-object is simply False.
 * =========================================================================== */

/* PoP: _uses_gateway @ hermes_cli/nous_subscription.py:_uses_gateway */
int sub_uses_gateway(const json_t *section)
{
    if (!section || section->type != JSON_OBJECT) return 0;
    json_t *v = json_obj_get(section, "use_gateway");
    if (!v) return 0;
    switch (v->type) {
    case JSON_BOOL:   return v->bool_val;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: {
        const char *s = v->str_val;
        if (!s) return 0;
        char buf[8];
        size_t n = 0;
        while (s[n] && n < sizeof buf - 1) { buf[n] = (char)tolower((unsigned char)s[n]); n++; }
        buf[n] = '\0';
        return strcmp(buf, "true") == 0 || strcmp(buf, "yes") == 0 ||
               strcmp(buf, "on") == 0   || strcmp(buf, "1") == 0;
    }
    default: return 0;
    }
}
