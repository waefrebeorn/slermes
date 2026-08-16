/*
 * port_moa_config.c — Faithful C11 port of hermes_cli/moa_config.py
 *
 * Pure config normalization + base64 turn-encoding for Mixture-of-Agents.
 * No IO. Events/configs modeled as libjson objects.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "json.h"
#include "moa_config.h"

const char *MOA_MARKER_PREFIX = "__HERMES_MOA_TURN_V1__";
const char *MOA_DEFAULT_PRESET_NAME = "default";

/* Default reference models + aggregator (mirrors Python module constants).
 * Live-tested 2026-07-24 on integrate.api.nvidia.com — ALL FREE on NVIDIA NIM.
 * NVIDIA is a cloud provider hosting non-NVIDIA models (GLM, DeepSeek, MiniMax, etc.)
 */
/* PoP: moa_default_reference_models @ hermes_cli/moa_config.py:_default_reference_models */
static json_t *moa_default_reference_models(void) {
    json_t *a = json_array();
    /* TIER S: Verified-callable agentic leaders */
    json_t *s1 = json_object();
    json_set(s1, "provider", json_string("nvidia_nim"));
    json_set(s1, "model", json_string("z-ai/glm-5.2"));
    json_append(a, s1);
    json_t *s2 = json_object();
    json_set(s2, "provider", json_string("nvidia_nim"));
    json_set(s2, "model", json_string("nvidia/nemotron-3-ultra-550b-a55b"));
    json_append(a, s2);
    json_t *s3 = json_object();
    json_set(s3, "provider", json_string("nvidia_nim"));
    json_set(s3, "model", json_string("nvidia/nemotron-3-super-120b-a12b"));
    json_append(a, s3);
    json_t *s4 = json_object();
    json_set(s4, "provider", json_string("nvidia_nim"));
    json_set(s4, "model", json_string("minimaxai/minimax-m3"));
    json_append(a, s4);
    json_t *s5 = json_object();
    json_set(s5, "provider", json_string("nvidia_nim"));
    json_set(s5, "model", json_string("deepseek-ai/deepseek-v4-flash"));
    json_append(a, s5);
    json_t *s6 = json_object();
    json_set(s6, "provider", json_string("nvidia_nim"));
    json_set(s6, "model", json_string("mistralai/mistral-small-4-119b-2603"));
    json_append(a, s6);
    /* TIER B: Fast/creative/specialist */
    json_t *s7 = json_object();
    json_set(s7, "provider", json_string("nvidia_nim"));
    json_set(s7, "model", json_string("mistralai/mistral-nemotron"));
    json_append(a, s7);
    json_t *s8 = json_object();
    json_set(s8, "provider", json_string("nvidia_nim"));
    json_set(s8, "model", json_string("nvidia/nemotron-3-nano-omni-30b-a3b-reasoning"));
    json_append(a, s8);
    /* OpenRouter free tier */
    json_t *s9 = json_object();
    json_set(s9, "provider", json_string("openrouter"));
    json_set(s9, "model", json_string("nvidia/nemotron-3-ultra-550b-a55b:free"));
    json_append(a, s9);
    return a;
}

static json_t *moa_default_aggregator(void) {
    json_t *a = json_object();
    json_set(a, "provider", json_string("nvidia_nim"));
    json_set(a, "model", json_string("z-ai/glm-5.2"));
    return a;
}

/* PoP: moa_coerce_float @ hermes_cli/moa_config.py:_coerce_float */
double moa_coerce_float(const json_t *value, double default_val) {
    if (!value) return default_val;
    if (value->type == JSON_NULL) return default_val;
    if (value->type == JSON_NUMBER) return value->num_val;
    if (value->type == JSON_STRING) {
        if (value->str_val && value->str_val[0] == '\0') return default_val;
        char *end = NULL;
        double d = strtod(value->str_val ? value->str_val : "", &end);
        if (end == (value->str_val ? value->str_val : "")) return default_val;
        return d;
    }
    return default_val;
}

/* PoP: moa_coerce_int @ hermes_cli/moa_config.py:_coerce_int */
long moa_coerce_int(const json_t *value, long default_val) {
    if (!value) return default_val;
    if (value->type == JSON_NULL) return default_val;
    if (value->type == JSON_NUMBER) return (long)value->num_val;
    if (value->type == JSON_STRING) {
        if (value->str_val && value->str_val[0] == '\0') return default_val;
        char *end = NULL;
        long l = strtol(value->str_val ? value->str_val : "", &end, 10);
        if (end != (value->str_val ? value->str_val : "")) return l;
        /* try float fallback */
        double d = strtod(value->str_val ? value->str_val : "", &end);
        if (end != (value->str_val ? value->str_val : "")) return (long)d;
        return default_val;
    }
    return default_val;
}

/* PoP: moa_clean_slot @ hermes_cli/moa_config.py:_clean_slot */
json_t *moa_clean_slot(const json_t *slot) {
    if (!slot || slot->type != JSON_OBJECT) return NULL;
    const json_t *p = json_obj_get(slot, "provider");
    const json_t *m = json_obj_get(slot, "model");
    char provider[256] = "";
    char model[256] = "";
    if (p && p->type == JSON_STRING) {
        strncpy(provider, p->str_val ? p->str_val : "", sizeof(provider)-1);
    }
    if (m && m->type == JSON_STRING) {
        strncpy(model, m->str_val ? m->str_val : "", sizeof(model)-1);
    }
    /* strip */
    char *pe = provider + strlen(provider);
    while (pe > provider && (*pe==' '||*pe=='\t'||*pe=='\r'||*pe=='\n')) *--pe='\0';
    char *ps = provider;
    while (*ps==' '||*ps=='\t'||*ps=='\r'||*ps=='\n') ps++;
    if (ps != provider) memmove(provider, ps, strlen(ps)+1);
    pe = model + strlen(model);
    while (pe > model && (*pe==' '||*pe=='\t'||*pe=='\r'||*pe=='\n')) *--pe='\0';
    char *ms = model;
    while (*ms==' '||*ms=='\t'||*ms=='\r'||*ms=='\n') ms++;
    if (ms != model) memmove(model, ms, strlen(ms)+1);

    if (provider[0]=='\0' || model[0]=='\0') return NULL;
    /* reject moa provider (recursive) */
    char pl[256];
    snprintf(pl, sizeof(pl), "%s", provider);
    for (char *c = pl; *c; c++) *c = (char)tolower((unsigned char)*c);
    if (strcmp(pl, "moa") == 0) return NULL;

    json_t *out = json_object();
    json_set(out, "provider", json_string(provider));
    json_set(out, "model", json_string(model));
    return out;
}

/* PoP: moa_default_preset @ hermes_cli/moa_config.py:_default_preset */
json_t *moa_default_preset(void) {
    json_t *p = json_object();
    json_set(p, "reference_models", moa_default_reference_models());
    json_set(p, "aggregator", moa_default_aggregator());
    json_set(p, "reference_temperature", json_number(0.6));
    json_set(p, "aggregator_temperature", json_number(0.4));
    json_set(p, "max_tokens", json_number(4096));
    json_set(p, "enabled", json_bool(1));
    return p;
}

/* PoP: moa_normalize_preset @ hermes_cli/moa_config.py:_normalize_preset */
json_t *moa_normalize_preset(const json_t *raw) {
    if (!raw || raw->type != JSON_OBJECT) raw = json_object();

    /* reference_models */
    const json_t *raw_refs = json_obj_get(raw, "reference_models");
    json_t *refs = json_array();
    if (raw_refs && raw_refs->type == JSON_ARRAY) {
        size_t n = json_len(raw_refs);
        for (size_t i = 0; i < n; i++) {
            json_t *clean = moa_clean_slot(json_get(raw_refs, i));
            if (clean) json_append(refs, clean);
        }
    } else if (raw_refs && raw_refs->type == JSON_OBJECT) {
        json_t *clean = moa_clean_slot(raw_refs);
        if (clean) json_append(refs, clean);
    }
    if (json_len(refs) == 0) {
        json_free(refs);
        refs = moa_default_reference_models();
    }

    const json_t *agg_raw = json_obj_get(raw, "aggregator");
    json_t *agg = moa_clean_slot(agg_raw);
    if (!agg) agg = moa_default_aggregator();

    json_t *p = json_object();
    json_set(p, "enabled", json_bool(json_get_bool(raw, "enabled", 1)));
    json_set(p, "reference_models", refs);
    json_set(p, "aggregator", agg);
    json_set(p, "reference_temperature", json_number(moa_coerce_float(json_obj_get(raw, "reference_temperature"), 0.6)));
    json_set(p, "aggregator_temperature", json_number(moa_coerce_float(json_obj_get(raw, "aggregator_temperature"), 0.4)));
    json_set(p, "max_tokens", json_number(moa_coerce_int(json_obj_get(raw, "max_tokens"), 4096)));
    return p;
}

/* PoP: moa_normalize_config @ hermes_cli/moa_config.py:normalize_moa_config */
json_t *moa_normalize_config(const json_t *raw) {
    if (!raw || raw->type != JSON_OBJECT) raw = json_object();

    json_t *presets = json_object();
    const json_t *presets_raw = json_obj_get(raw, "presets");
    if (presets_raw && presets_raw->type == JSON_OBJECT) {
        /* iterate keys */
        for (size_t i = 0; i < presets_raw->c.count; i++) {
            const char *name = presets_raw->c.keys[i];
            char clean_name[256] = "";
            if (name) { strncpy(clean_name, name, sizeof(clean_name)-1); }
            /* strip */
            char *pe = clean_name + strlen(clean_name);
            while (pe > clean_name && (*pe==' '||*pe=='\t'||*pe=='\r'||*pe=='\n')) *--pe='\0';
            char *ps = clean_name;
            while (*ps==' '||*ps=='\t'||*ps=='\r'||*ps=='\n') ps++;
            if (ps != clean_name) memmove(clean_name, ps, strlen(ps)+1);
            if (clean_name[0] != '\0') {
                json_t *norm = moa_normalize_preset(presets_raw->c.items[i]);
                json_set(presets, clean_name, norm);
            }
        }
    }
    if (presets->c.count == 0) {
        json_t *def = moa_normalize_preset(raw);
        json_set(presets, MOA_DEFAULT_PRESET_NAME, def);
    }

    const char *default_name_raw = NULL;
    const json_t *dn = json_obj_get(raw, "default_preset");
    if (dn && dn->type == JSON_STRING) default_name_raw = dn->str_val;
    char default_name[256] = "";
    if (default_name_raw) { strncpy(default_name, default_name_raw, sizeof(default_name)-1); }
    char *pe = default_name + strlen(default_name);
    while (pe > default_name && (*pe==' '||*pe=='\t'||*pe=='\r'||*pe=='\n')) *--pe='\0';
    char *ps = default_name;
    while (*ps==' '||*ps=='\t'||*ps=='\r'||*ps=='\n') ps++;
    if (ps != default_name) memmove(default_name, ps, strlen(ps)+1);
    if (default_name[0]=='\0' || !json_obj_get(presets, default_name)) {
        /* next(iter(presets)) */
        if (presets->c.count > 0) {
            strncpy(default_name, presets->c.keys[0], sizeof(default_name)-1);
        } else {
            strncpy(default_name, MOA_DEFAULT_PRESET_NAME, sizeof(default_name)-1);
        }
    }
    if (!json_obj_get(presets, default_name)) {
        json_set(presets, default_name, moa_default_preset());
    }

    const char *active_raw = NULL;
    const json_t *an = json_obj_get(raw, "active_preset");
    if (an && an->type == JSON_STRING) active_raw = an->str_val;
    char active_name[256] = "";
    if (active_raw) { strncpy(active_name, active_raw, sizeof(active_name)-1); }
    pe = active_name + strlen(active_name);
    while (pe > active_name && (*pe==' '||*pe=='\t'||*pe=='\r'||*pe=='\n')) *--pe='\0';
    if (active_name[0]=='\0' || !json_obj_get(presets, active_name)) {
        active_name[0] = '\0';
    }

    const json_t *active = json_obj_get(presets, default_name);

    json_t *out = json_object();
    json_set(out, "default_preset", json_string(default_name));
    json_set(out, "active_preset", json_string(active_name));
    json_set(out, "presets", json_copy(presets));
    if (active) {
        const json_t *rm = json_obj_get(active, "reference_models");
        json_set(out, "reference_models", rm ? json_copy(rm) : json_array());
        const json_t *ag = json_obj_get(active, "aggregator");
        json_set(out, "aggregator", ag ? json_copy(ag) : json_object());
        json_set(out, "reference_temperature", json_number(json_obj_get(active, "reference_temperature") ? json_obj_get(active, "reference_temperature")->num_val : 0.6));
        json_set(out, "aggregator_temperature", json_number(json_obj_get(active, "aggregator_temperature") ? json_obj_get(active, "aggregator_temperature")->num_val : 0.4));
        json_set(out, "max_tokens", json_number(json_obj_get(active, "max_tokens") ? json_obj_get(active, "max_tokens")->num_val : 4096));
        json_set(out, "enabled", json_bool(json_get_bool(active, "enabled", 1)));
    }
    json_free(presets);
    return out;
}

/* PoP: moa_list_presets @ hermes_cli/moa_config.py:list_moa_presets */
json_t *moa_list_presets(const json_t *config) {
    json_t *cfg = moa_normalize_config(config);
    json_t *presets = json_obj_get(cfg, "presets");
    json_t *out = json_array();
    if (presets && presets->type == JSON_OBJECT) {
        for (size_t i = 0; i < presets->c.count; i++) {
            json_append(out, json_string(presets->c.keys[i]));
        }
    }
    json_free(cfg);
    return out;
}

/* PoP: moa_resolve_preset @ hermes_cli/moa_config.py:resolve_moa_preset */
json_t *moa_resolve_preset(const json_t *config, const char *name) {
    json_t *cfg = moa_normalize_config(config);
    char preset_name[256] = "";
    const char *dn = NULL;
    const json_t *dp = json_obj_get(cfg, "default_preset");
    if (dp && dp->type == JSON_STRING) dn = dp->str_val;
    if (name && name[0] != '\0') {
        strncpy(preset_name, name, sizeof(preset_name)-1);
    } else if (dn) {
        strncpy(preset_name, dn, sizeof(preset_name)-1);
    } else {
        strncpy(preset_name, MOA_DEFAULT_PRESET_NAME, sizeof(preset_name)-1);
    }
    /* strip */
    char *pe = preset_name + strlen(preset_name);
    while (pe > preset_name && (*pe==' '||*pe=='\t'||*pe=='\r'||*pe=='\n')) *--pe='\0';
    char *ps = preset_name;
    while (*ps==' '||*ps=='\t'||*ps=='\r'||*ps=='\n') ps++;
    if (ps != preset_name) memmove(preset_name, ps, strlen(ps)+1);

    json_t *presets = json_obj_get(cfg, "presets");
    json_t *preset = presets ? json_obj_get(presets, preset_name) : NULL;
    json_t *result = preset ? json_copy(preset) : NULL;
    json_free(cfg);
    return result;  /* NULL if not found (caller raises KeyError equivalent) */
}

/* PoP: moa_exact_preset_name @ hermes_cli/moa_config.py:exact_moa_preset_name */
const char *moa_exact_preset_name(const json_t *config, const char *text, char *out, size_t out_cap) {
    char wanted[256] = "";
    if (text) { strncpy(wanted, text, sizeof(wanted)-1); }
    char *pe = wanted + strlen(wanted);
    while (pe > wanted && (*pe==' '||*pe=='\t'||*pe=='\r'||*pe=='\n')) *--pe='\0';
    char *ps = wanted;
    while (*ps==' '||*ps=='\t'||*ps=='\r'||*ps=='\n') ps++;
    if (ps != wanted) memmove(wanted, ps, strlen(ps)+1);
    if (wanted[0] == '\0') return NULL;

    json_t *cfg = moa_normalize_config(config);
    json_t *presets = json_obj_get(cfg, "presets");
    json_t *preset = presets ? json_obj_get(presets, wanted) : NULL;
    const char *result = NULL;
    if (preset && json_get_bool(preset, "enabled", 1)) {
        snprintf(out, out_cap, "%s", wanted);
        result = out;
    }
    json_free(cfg);
    return result;
}

/* PoP: moa_set_active_preset @ hermes_cli/moa_config.py:set_active_moa_preset */
json_t *moa_set_active_preset(const json_t *config, const char *name) {
    json_t *cfg = moa_normalize_config(config);
    char clean[256] = "";
    if (name) { strncpy(clean, name, sizeof(clean)-1); }
    char *pe = clean + strlen(clean);
    while (pe > clean && (*pe==' '||*pe=='\t'||*pe=='\r'||*pe=='\n')) *--pe='\0';
    char *ps = clean;
    while (*ps==' '||*ps=='\t'||*ps=='\r'||*ps=='\n') ps++;
    if (ps != clean) memmove(clean, ps, strlen(ps)+1);

    if (clean[0] != '\0') {
        json_t *presets = json_obj_get(cfg, "presets");
        if (!json_obj_get(presets, clean)) {
            json_free(cfg);
            return NULL;  /* KeyError equivalent */
        }
    }
    json_set(cfg, "active_preset", json_string(clean));
    return cfg;
}

/* ---- base64 urlsafe encode/decode for moa turn encoding ---- */
static const char *B64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void b64_encode(const unsigned char *data, size_t len, char *out) {
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        unsigned int v = data[i] << 16;
        int rem = 1;
        if (i + 1 < len) { v |= data[i+1] << 8; rem = 2; }
        if (i + 2 < len) { v |= data[i+2]; rem = 3; }
        out[o++] = B64_CHARS[(v >> 18) & 0x3F];
        out[o++] = B64_CHARS[(v >> 12) & 0x3F];
        out[o++] = (rem >= 2) ? B64_CHARS[(v >> 6) & 0x3F] : '=';
        out[o++] = (rem >= 3) ? B64_CHARS[v & 0x3F] : '=';
    }
    out[o] = '\0';
}

static int b64_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static size_t b64_decode(const char *in, unsigned char *out) {
    size_t o = 0;
    int v[4] = {0,0,0,0};
    int n = 0;
    for (const char *p = in; *p; p++) {
        if (*p == '=') {
            v[n++] = 0;
            if (n == 4) {
                out[o++] = (unsigned char)((v[0]<<2)|(v[1]>>4));
                if (v[2] != 0) out[o++] = (unsigned char)((v[1]<<4)|(v[2]>>2));
                n = 0;
            }
        } else {
            int d = b64_val(*p);
            if (d < 0) continue;
            v[n++] = d;
            if (n == 4) {
                out[o++] = (unsigned char)((v[0]<<2)|(v[1]>>4));
                out[o++] = (unsigned char)((v[1]<<4)|(v[2]>>2));
                out[o++] = (unsigned char)((v[2]<<6)|v[3]);
                n = 0;
            }
        }
    }
    /* handle remaining chars without padding */
    if (n == 2) {
        out[o++] = (unsigned char)((v[0]<<2)|(v[1]>>4));
    } else if (n == 3) {
        out[o++] = (unsigned char)((v[0]<<2)|(v[1]>>4));
        out[o++] = (unsigned char)((v[1]<<4)|(v[2]>>2));
    }
    return o;
}

/* urlsafe: replace + with - and / with _ */
static void b64_to_urlsafe(char *s) {
    for (; *s; s++) {
        if (*s == '+') *s = '-';
        else if (*s == '/') *s = '_';
    }
}

static void urlsafe_to_b64(char *s) {
    for (; *s; s++) {
        if (*s == '-') *s = '+';
        else if (*s == '_') *s = '/';
    }
}

/* PoP: moa_encode_turn @ hermes_cli/moa_config.py:encode_moa_turn */
char *moa_encode_turn(const char *prompt, const json_t *config, const char *preset) {
    json_t *resolved = moa_resolve_preset(config, preset);
    if (!resolved) resolved = moa_default_preset();

    json_t *payload = json_object();
    json_set(payload, "prompt", json_string(prompt ? prompt : ""));
    json_set(payload, "config", resolved);

    char *ser = json_serialize(payload);
    size_t slen = ser ? strlen(ser) : 0;
    size_t enc_cap = ((slen + 2) / 3) * 4 + 1;
    char *encoded = malloc(enc_cap + 64);  /* +prefix */
    if (ser) {
        b64_encode((const unsigned char *)ser, slen, encoded);
        b64_to_urlsafe(encoded);
        free(ser);
    } else {
        encoded[0] = '\0';
    }
    json_free(payload);

    size_t plen = strlen(MOA_MARKER_PREFIX);
    size_t elen = strlen(encoded);
    char *result = malloc(plen + elen + 1);
    memcpy(result, MOA_MARKER_PREFIX, plen);
    memcpy(result + plen, encoded, elen);
    result[plen + elen] = '\0';
    free(encoded);
    return result;
}

/* PoP: moa_decode_turn @ hermes_cli/moa_config.py:decode_moa_turn */
void moa_decode_turn(const char *message, char *prompt_out, size_t prompt_cap,
                      json_t **config_out) {
    *config_out = NULL;
    prompt_out[0] = '\0';
    if (!message || strncmp(message, MOA_MARKER_PREFIX, strlen(MOA_MARKER_PREFIX)) != 0) {
        if (message) { strncpy(prompt_out, message, prompt_cap-1); prompt_out[prompt_cap-1]='\0'; }
        return;
    }
    const char *encoded = message + strlen(MOA_MARKER_PREFIX);
    /* strip */
    while (*encoded == ' ' || *encoded == '\t' || *encoded == '\r' || *encoded == '\n') encoded++;
    char *b64 = malloc(strlen(encoded) + 1);
    strcpy(b64, encoded);
    urlsafe_to_b64(b64);

    size_t dec_cap = strlen(b64) * 3 / 4 + 4;
    unsigned char *dec = malloc(dec_cap);
    size_t dec_len = b64_decode(b64, dec);
    free(b64);

    char *json_str = malloc(dec_len + 1);
    memcpy(json_str, dec, dec_len);
    json_str[dec_len] = '\0';
    free(dec);

    json_t *payload = json_parse(json_str, NULL);
    free(json_str);
    if (!payload || payload->type != JSON_OBJECT) {
        if (payload) json_free(payload);
        /* return original message */
        strncpy(prompt_out, message, prompt_cap-1);
        prompt_out[prompt_cap-1] = '\0';
        return;
    }
    const json_t *p = json_obj_get(payload, "prompt");
    if (p && p->type == JSON_STRING) {
        strncpy(prompt_out, p->str_val ? p->str_val : "", prompt_cap-1);
        prompt_out[prompt_cap-1] = '\0';
    }
    const json_t *c = json_obj_get(payload, "config");
    *config_out = c ? moa_normalize_preset(c) : NULL;
    json_free(payload);
}

/* PoP: moa_build_turn_prompt @ hermes_cli/moa_config.py:build_moa_turn_prompt */
char *moa_build_turn_prompt(const char *user_prompt, const json_t *config, const char *preset) {
    return moa_encode_turn(user_prompt, config, preset);
}
