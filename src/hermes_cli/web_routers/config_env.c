/*
 * config_env.c — C11 home for upstream hermes_cli/web_routers/config_env.py.
 *
 * Two differential gates share this one file:
 *
 *   w28_configenv_e9e22f_p1 — the env-var / Keys-tab surface: the redaction
 *     sentinel and the predicate that refuses it as a write, the GET /api/env
 *     row build, the error->status mapping, and the pure value helpers the
 *     custom-endpoint routes share (id slug, model list, key display,
 *     transport pin, provider-name match).
 *   w28_configenv_e9e22f_p2 — the config-payload surface: GET /api/config's
 *     internal-key strip, _custom_endpoint_response(), the legacy-provider
 *     pop / main-slot detach / stale-address edits, entry resolution, the
 *     create-or-update validation preamble and the empty-/models probe text.
 *
 * SEAMS (I/O, not logic — never modelled here): .env load/save plus the
 * profile scope lock, config.yaml read/write and CONFIG_MUTATION_LOCK, the
 * credential store / credential_pool, read_raw_config(), the provider catalog
 * descriptors, the httpx probes, and the legacy ``custom_providers:``
 * normalization done by config_providers.get_compatible_custom_providers.
 * The oracles patch those in (env map, profile scope, catalog metadata) so no
 * host state is ever read.
 *
 * PoP: w28_configenv_e9e22f_p1_env_preview          @ hermes_cli/web_routers/config_env.py:_get_env_vars_sync._row (via _common.redacted_credential_preview)
 * PoP: w28_configenv_e9e22f_p1_is_redacted_preview  @ hermes_cli/web_routers/config_env.py:set_env_var (via _common.is_redacted_credential_preview)
 * PoP: w28_configenv_e9e22f_p1_env_vars             @ hermes_cli/web_routers/config_env.py:_get_env_vars_sync
 * PoP: w28_configenv_e9e22f_p1_env_error            @ hermes_cli/web_routers/config_env.py:_env_write_errors
 * PoP: w28_configenv_e9e22f_p1_custom_endpoint_id   @ hermes_cli/web_routers/config_env.py:_custom_endpoint_id
 * PoP: w28_configenv_e9e22f_p1_models_from_entry    @ hermes_cli/web_routers/config_env.py:_models_from_custom_endpoint_entry
 * PoP: w28_configenv_e9e22f_p1_api_key_display      @ hermes_cli/web_routers/config_env.py:_api_key_display
 * PoP: w28_configenv_e9e22f_p1_endpoint_api_mode    @ hermes_cli/web_routers/config_env.py:_endpoint_api_mode
 * PoP: w28_configenv_e9e22f_p1_model_names_provider @ hermes_cli/web_routers/config_env.py:_model_names_provider
 * PoP: w28_configenv_e9e22f_p2_strip_internal       @ hermes_cli/web_routers/config_env.py:get_config
 * PoP: w28_configenv_e9e22f_p2_endpoint_row         @ hermes_cli/web_routers/config_env.py:_endpoint_row
 * PoP: w28_configenv_e9e22f_p2_custom_endpoint_response @ hermes_cli/web_routers/config_env.py:_custom_endpoint_response
 * PoP: w28_configenv_e9e22f_p2_pop_legacy_provider  @ hermes_cli/web_routers/config_env.py:_pop_legacy_custom_provider
 * PoP: w28_configenv_e9e22f_p2_detach_main_model    @ hermes_cli/web_routers/config_env.py:_detach_main_model_from_provider
 * PoP: w28_configenv_e9e22f_p2_drop_stale_addresses @ hermes_cli/web_routers/config_env.py:_drop_stale_addresses
 * PoP: w28_configenv_e9e22f_p2_resolve_entry        @ hermes_cli/web_routers/config_env.py:_resolve_custom_endpoint_entry
 * PoP: w28_configenv_e9e22f_p2_validate_endpoint    @ hermes_cli/web_routers/config_env.py:_write_custom_endpoint
 * PoP: w28_configenv_e9e22f_p2_no_models_message    @ hermes_cli/web_routers/config_env.py:_no_models_probe_message
 */
#include "hermes_json.h"
#include "noc_w28_configenv_e9e22f_p1.h"
#include "noc_w28_configenv_e9e22f_p2.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── shared value helpers ───────────────────────────────────────────────── */

static char *ce_dup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *out = (char *)malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

/* Python truthiness over the JSON subset. */
static bool ce_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
    case JSON_NULL:   return false;
    case JSON_BOOL:   return v->bool_val;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return false;
}

static const json_t *ce_jget(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT || !key) return NULL;
    return json_obj_get(obj, key);
}

static bool ce_has(const json_t *obj, const char *key) {
    return ce_jget(obj, key) != NULL;
}

/* Python str() over the JSON subset the router actually stringifies. */
static char *ce_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ce_dup("None");
    switch (v->type) {
    case JSON_BOOL:   return ce_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return ce_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER: {
        char tmp[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(tmp, sizeof(tmp), "%lld", (long long)v->num_val);
        else
            snprintf(tmp, sizeof(tmp), "%.17g", v->num_val);
        return ce_dup(tmp);
    }
    case JSON_ARRAY:
    case JSON_OBJECT: {
        /* approximation of str(); fixtures keep str() inputs scalar */
        char *ser = json_dumps(v, 0);
        char *out = ce_dup(ser ? ser : "");
        free(ser);
        return out;
    }
    default: return ce_dup("");
    }
}

/* str(v or dflt) — the JSON-visible leg of `v or "..."`. */
static char *ce_str_of(const json_t *v, const char *dflt) {
    if (ce_truthy(v)) return ce_str(v);
    return ce_dup(dflt ? dflt : "");
}

static bool ce_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip(); mutates and returns the first kept byte. */
static char *ce_strip_inplace(char *s) {
    if (!s) return s;
    size_t n = strlen(s), i = 0;
    while (i < n && ce_is_space(s[i])) i++;
    size_t j = n;
    while (j > i && ce_is_space(s[j - 1])) j--;
    s[j] = '\0';
    return s + i;
}

/* Python str.strip(chars); mutates and returns the first kept byte. */
static char *ce_strip_chars(char *s, const char *chars) {
    if (!s) return s;
    size_t n = strlen(s), i = 0;
    while (i < n && s[i] && strchr(chars, s[i])) i++;
    size_t j = n;
    while (j > i && s[j - 1] && strchr(chars, s[j - 1])) j--;
    s[j] = '\0';
    return s + i;
}

static void ce_lower_inplace(char *s) {
    if (!s) return;
    for (size_t i = 0; s[i]; i++)
        if (s[i] >= 'A' && s[i] <= 'Z') s[i] = (char)(s[i] - 'A' + 'a');
}

/* str(v or dflt).strip() as a fresh allocation. */
static char *ce_strip_of(const json_t *v, const char *dflt) {
    char *raw = ce_str_of(v, dflt);
    char *base = ce_strip_inplace(raw);
    char *out = ce_dup(base);
    free(raw);
    return out;
}

/* coerce_provider_id(): None -> "", anything else str().strip(). */
static char *ce_coerce_id(const json_t *v) {
    /* coerce_provider_id(): None -> "", ANYTHING else str().strip(). Truthiness
     * must not enter here: str(0) is "0", so a numeric endpoint id slugs to "0"
     * rather than falling through to the default. */
    char *raw, *base, *out;
    if (!v || v->type == JSON_NULL) return ce_dup("");
    raw = ce_str(v);
    base = ce_strip_inplace(raw);
    out = ce_dup(base);
    free(raw);
    return out;
}

static json_t *ce_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

static void ce_set(json_t *obj, const char *key, json_t *val) {
    json_set(obj, key, val ? val : json_null());
}

/* `obj.get(key, dflt)` — takes ownership of dflt. */
static json_t *ce_getd(const json_t *obj, const char *key, json_t *dflt) {
    const json_t *v = ce_jget(obj, key);
    if (v) return json_copy(v);
    return dflt ? dflt : json_null();
}

/* `a or b` over two borrowed JSON nodes. */
static const json_t *ce_or_node(const json_t *a, const json_t *b) {
    return ce_truthy(a) ? a : b;
}

/* `value in channel_keys` (a list of strings). */
static bool ce_in_strlist(const json_t *list, const char *key) {
    if (!list || list->type != JSON_ARRAY || !key) return false;
    for (size_t i = 0; i < json_len(list); i++) {
        const json_t *v = json_get(list, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, key)) return true;
    }
    return false;
}

/* ── mask_secret / redaction sentinel (agent.redact + web_routers/_common) ─ */

static bool ce_cp_is_display_control(unsigned cp) {
    if (cp <= 0x1F) return true;
    if (cp == 0x7F) return true;
    if (cp >= 0x80 && cp <= 0x9F) return true;      /* C1 controls */
    if (cp >= 0x200B && cp <= 0x200F) return true;  /* ZWSP .. RLM */
    if (cp >= 0x202A && cp <= 0x202E) return true;  /* bidi embeddings */
    if (cp >= 0x2060 && cp <= 0x2064) return true;  /* word joiner .. invisible plus */
    return false;
}

/* Decode one UTF-8 code point at s[i]; *width gets the byte width. */
static unsigned ce_utf8_next(const char *s, size_t n, size_t i, size_t *width) {
    const unsigned char *b = (const unsigned char *)s;
    unsigned char c = b[i];
    if (c < 0x80) { *width = 1; return c; }
    if ((c & 0xE0) == 0xC0 && i + 1 < n) {
        *width = 2;
        return ((unsigned)(c & 0x1F) << 6) | (b[i + 1] & 0x3Fu);
    }
    if ((c & 0xF0) == 0xE0 && i + 2 < n) {
        *width = 3;
        return ((unsigned)(c & 0x0F) << 12) | ((unsigned)(b[i + 1] & 0x3Fu) << 6) |
               (b[i + 2] & 0x3Fu);
    }
    if ((c & 0xF8) == 0xF0 && i + 3 < n) {
        *width = 4;
        return ((unsigned)(c & 0x07) << 18) | ((unsigned)(b[i + 1] & 0x3Fu) << 12) |
               ((unsigned)(b[i + 2] & 0x3Fu) << 6) | (b[i + 3] & 0x3Fu);
    }
    *width = 1;
    return c;
}

/* agent.redact.mask_secret(value, head=4, tail=4, floor=12), non-empty input. */
static char *ce_mask_secret(const char *value) {
    size_t n = value ? strlen(value) : 0;
    char *filt = (char *)malloc(n + 1);
    size_t *starts = (size_t *)malloc((n + 1) * sizeof(size_t));
    if (!filt || !starts) { free(filt); free(starts); return ce_dup(""); }
    size_t fl = 0, cc = 0, i = 0;
    while (i < n) {
        size_t width = 1;
        unsigned cp = ce_utf8_next(value, n, i, &width);
        if (!ce_cp_is_display_control(cp)) {
            starts[cc++] = fl;
            memcpy(filt + fl, value + i, width);
            fl += width;
        }
        i += width;
    }
    filt[fl] = '\0';

    char *out;
    if (cc == 0) {
        out = ce_dup("");
    } else if (cc < 12) {
        out = ce_dup("***");
    } else {
        size_t head_end = starts[4];          /* first 4 code points */
        size_t tail_start = starts[cc - 4];   /* last 4 code points */
        size_t tail_len = fl - tail_start;
        out = (char *)malloc(head_end + 3 + tail_len + 1);
        if (out) {
            memcpy(out, filt, head_end);
            memcpy(out + head_end, "...", 3);
            memcpy(out + head_end + 3, filt + tail_start, tail_len);
            out[head_end + 3 + tail_len] = '\0';
        }
    }
    free(filt);
    free(starts);
    return out;
}

json_t *w28_configenv_e9e22f_p1_env_preview(const json_t *value) {
    if (!ce_truthy(value)) return json_null();
    char *raw = ce_str(value);
    char *masked = ce_mask_secret(raw);
    size_t need = strlen(masked) + sizeof("«redacted:»") + 2;
    char *out = (char *)malloc(need);
    json_t *node = NULL;
    if (out) {
        snprintf(out, need, "«redacted:%s»", masked);
        node = json_string(out);
    }
    free(raw);
    free(masked);
    free(out);
    return node ? node : json_null();
}

json_t *w28_configenv_e9e22f_p1_is_redacted_preview(const json_t *submitted) {
    char *value = ce_truthy(submitted) ? ce_str(submitted) : ce_dup("");
    bool hit = false;
    if (value) {
        if (!strncmp(value, "\xC2\xABredacted", 10)) {   /* starts with "«redacted" */
            hit = true;
        } else if (!strcmp(value, "***")) {
            hit = true;
        } else if (!strchr(value, '\n')) {
            /* _LEGACY_MASK_RE.fullmatch(r".{4}\.\.\..{4}") — 11 code points with
             * "..." right after the first four. */
            size_t n = strlen(value), cc = 0, i = 0, dots_at = (size_t)-1;
            while (i < n) {
                size_t width = 1;
                (void)ce_utf8_next(value, n, i, &width);
                if (cc == 4) dots_at = i;
                cc++;
                i += width;
            }
            if (cc == 11 && dots_at != (size_t)-1 && value[dots_at] == '.' &&
                value[dots_at + 1] == '.' && value[dots_at + 2] == '.')
                hit = true;
        }
    }
    free(value);
    return json_bool(hit);
}

/* ── GET /api/env row build ─────────────────────────────────────────────── */

static json_t *ce_env_row(const char *var_name, const json_t *info, const json_t *value,
                          const json_t *cat_meta, const json_t *channel_keys, bool custom) {
    /* cat_meta = catalog_meta.get(var_name) or {} */
    const json_t *cat = (cat_meta && cat_meta->type == JSON_OBJECT && json_object_size(cat_meta))
                            ? cat_meta : NULL;
    json_t *row = json_object();
    if (!row) return NULL;

    ce_set(row, "is_set", json_bool(ce_truthy(value)));
    ce_set(row, "redacted_value", w28_configenv_e9e22f_p1_env_preview(value));

    /* description: hand prose wins, catalog fills the gap (default ""). */
    const json_t *hand_desc = ce_jget(info, "description");
    ce_set(row, "description", ce_truthy(hand_desc)
                                   ? json_copy(hand_desc)
                                   : ce_getd(cat, "description", json_string("")));

    /* url: the hand value unless None, else the catalog's (absent -> null). */
    const json_t *hand_url = ce_jget(info, "url");
    if (hand_url && hand_url->type != JSON_NULL)
        ce_set(row, "url", json_copy(hand_url));
    else
        ce_set(row, "url", ce_copy_or_null(ce_jget(cat, "url")));

    ce_set(row, "category", ce_truthy(ce_jget(info, "category"))
                                ? json_copy(ce_jget(info, "category"))
                                : ce_getd(cat, "category", json_string("")));

    ce_set(row, "is_password", ce_has(info, "password")
                                   ? json_copy(ce_jget(info, "password"))
                                   : ce_getd(cat, "is_password", json_bool(false)));

    ce_set(row, "tools", ce_has(info, "tools") ? json_copy(ce_jget(info, "tools"))
                                                : json_array());

    ce_set(row, "advanced", ce_has(info, "advanced")
                                ? json_copy(ce_jget(info, "advanced"))
                                : ce_getd(cat, "advanced", json_bool(false)));

    ce_set(row, "channel_managed", json_bool(ce_in_strlist(channel_keys, var_name)));

    ce_set(row, "provider", ce_getd(cat, "provider", json_string("")));
    ce_set(row, "provider_label", ce_getd(cat, "provider_label", json_string("")));
    ce_set(row, "provider_profiles", ce_getd(cat, "provider_profiles", json_array()));
    ce_set(row, "provider_primary", json_bool(ce_truthy(ce_jget(cat, "provider_primary"))));
    ce_set(row, "custom", json_bool(custom));

    if (custom) {
        /* A key in no catalogue: secret by default, category "custom". */
        ce_set(row, "category", json_string("custom"));
        ce_set(row, "is_password", json_bool(true));
    }
    return row;
}

json_t *w28_configenv_e9e22f_p1_env_vars(const json_t *optional_env_vars,
                                         const json_t *catalog_meta,
                                         const json_t *env_on_disk,
                                         const json_t *channel_keys) {
    json_t *result = json_object();
    if (!result) return NULL;
    const json_t *env = (env_on_disk && env_on_disk->type == JSON_OBJECT) ? env_on_disk : NULL;
    json_t *empty = json_object();

    /* 1. Hand entries from OPTIONAL_ENV_VARS, in declaration order. */
    if (optional_env_vars && optional_env_vars->type == JSON_OBJECT) {
        size_t n = json_object_size(optional_env_vars);
        for (size_t i = 0; i < n; i++) {
            const char *var = json_object_get_key_at(optional_env_vars, i);
            if (!var) continue;
            const json_t *info = json_object_get_at(optional_env_vars, i);
            ce_set(result, var, ce_env_row(var, info, ce_jget(env, var),
                                           ce_jget(catalog_meta, var), channel_keys, false));
        }
    }
    /* 2. Catalog provider env vars with no hand entry. */
    if (catalog_meta && catalog_meta->type == JSON_OBJECT) {
        size_t n = json_object_size(catalog_meta);
        for (size_t i = 0; i < n; i++) {
            const char *var = json_object_get_key_at(catalog_meta, i);
            if (!var || ce_has(result, var)) continue;
            ce_set(result, var, ce_env_row(var, empty, ce_jget(env, var),
                                           json_object_get_at(catalog_meta, i), channel_keys, false));
        }
    }
    /* 3. Custom keys already in .env (never the channel-managed ones). */
    if (env) {
        size_t n = json_object_size(env);
        for (size_t i = 0; i < n; i++) {
            const char *var = json_object_get_key_at(env, i);
            if (!var || ce_has(result, var) || ce_in_strlist(channel_keys, var)) continue;
            ce_set(result, var, ce_env_row(var, empty, json_object_get_at(env, i), NULL,
                                           channel_keys, true));
        }
    }
    json_free(empty);
    return result;
}

json_t *w28_configenv_e9e22f_p1_env_error(const json_t *kind, const json_t *message,
                                          const json_t *status, const json_t *detail) {
    const char *k = (kind && kind->type == JSON_STRING && kind->str_val) ? kind->str_val : "";
    json_t *out = json_object();
    if (!out) return NULL;
    json_set(out, "ok", json_bool(false));
    if (!strcmp(k, "value_error")) {
        json_set(out, "status", json_number(400));
        ce_set(out, "detail", ce_copy_or_null(message));
    } else if (!strcmp(k, "http")) {
        json_set(out, "status", ce_copy_or_null(status));
        ce_set(out, "detail", ce_copy_or_null(detail));
    } else {
        json_set(out, "status", json_number(500));
        json_set(out, "detail", json_string("Internal server error"));
    }
    return out;
}

/* ── custom-endpoint value helpers ──────────────────────────────────────── */

static bool ce_is_slug_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '-';
}

json_t *w28_configenv_e9e22f_p1_custom_endpoint_id(const json_t *raw, const char *fallback) {
    char *id = ce_coerce_id(raw);
    char *slug = id ? (char *)malloc(strlen(id) + 2) : NULL;
    if (!slug) { free(id); return json_string(fallback ? fallback : "custom"); }
    size_t o = 0;
    bool in_run = false;
    for (size_t i = 0; id[i]; i++) {
        if (ce_is_slug_char(id[i])) { slug[o++] = id[i]; in_run = false; }
        else if (!in_run) { slug[o++] = '-'; in_run = true; }
    }
    slug[o] = '\0';
    char *trimmed = ce_strip_chars(slug, "-_");
    ce_lower_inplace(trimmed);
    json_t *out = json_string(trimmed[0] ? trimmed : (fallback ? fallback : "custom"));
    free(slug);
    free(id);
    return out;
}

json_t *w28_configenv_e9e22f_p1_models_from_entry(const json_t *entry) {
    json_t *out = json_array();
    if (!out) return NULL;
    const json_t *raw = ce_jget(entry, "models");
    const json_t *pick = ce_or_node(ce_jget(entry, "model"), ce_jget(entry, "default_model"));
    char *head = ce_strip_of(pick, "");

    if (raw && (raw->type == JSON_ARRAY || raw->type == JSON_OBJECT)) {
        size_t n = (raw->type == JSON_ARRAY) ? json_len(raw) : json_object_size(raw);
        for (size_t i = 0; i < n; i++) {
            char *s;
            if (raw->type == JSON_OBJECT) {
                const char *key = json_object_get_key_at(raw, i);
                s = ce_dup(key ? key : "");
            } else {
                s = ce_str(json_get(raw, i));
            }
            char *stripped = ce_strip_inplace(s);
            if (*stripped) {
                bool dup = false;
                for (size_t k = 0; k < json_len(out); k++) {
                    const json_t *have = json_get(out, k);
                    if (have && have->str_val && !strcmp(have->str_val, stripped)) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) json_append(out, json_string(stripped));
            }
            free(s);
        }
    }

    if (head && *head) {
        bool dup = false;
        for (size_t k = 0; k < json_len(out); k++) {
            const json_t *have = json_get(out, k);
            if (have && have->str_val && !strcmp(have->str_val, head)) { dup = true; break; }
        }
        if (!dup) {
            /* models.insert(0, default_model) */
            json_t *grown = json_array();
            json_append(grown, json_string(head));
            for (size_t k = 0; k < json_len(out); k++)
                json_append(grown, json_copy(json_get(out, k)));
            json_free(out);
            out = grown;
        }
    }
    free(head);
    return out;
}

json_t *w28_configenv_e9e22f_p1_api_key_display(const json_t *entry) {
    json_t *out = json_array();
    if (!out) return NULL;
    char *plain = ce_strip_of(ce_jget(entry, "api_key"), "");
    if (plain && *plain) {
        json_t *key = json_string(plain);
        json_append(out, json_bool(true));
        json_append(out, w28_configenv_e9e22f_p1_env_preview(key));
        json_free(key);
        free(plain);
        return out;
    }
    free(plain);

    char *key_env = ce_strip_of(ce_jget(entry, "key_env"), "");
    if (key_env && *key_env) {
        size_t need = strlen(key_env) + 5;
        char *shown = (char *)malloc(need);
        if (shown) {
            snprintf(shown, need, "${%s}", key_env);
            json_append(out, json_bool(true));
            json_append(out, json_string(shown));
            free(shown);
        }
    } else {
        json_append(out, json_bool(false));
        json_append(out, json_null());
    }
    free(key_env);
    return out;
}

/* _canonical_api_mode(): alias-table lookup, unknowns pass through. */
static const char *ce_api_mode_alias(const char *key) {
    static const struct { const char *from; const char *to; } table[] = {
        {"openai", "chat_completions"},
        {"openai_chat", "chat_completions"},
        {"openai-chat", "chat_completions"},
        {"chat-completions", "chat_completions"},
        {"chatcompletions", "chat_completions"},
        {"responses", "codex_responses"},
        {"openai_responses", "codex_responses"},
        {"openai-responses", "codex_responses"},
        {"anthropic", "anthropic_messages"},
        {"anthropic-messages", "anthropic_messages"},
        {"messages", "anthropic_messages"},
        {"bedrock", "bedrock_converse"},
        {"bedrock-converse", "bedrock_converse"},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (!strcmp(table[i].from, key)) return table[i].to;
    return NULL;
}

static bool ce_is_desktop_mode(const char *mode) {
    return !strcmp(mode, "chat_completions") || !strcmp(mode, "codex_responses") ||
           !strcmp(mode, "anthropic_messages");
}

json_t *w28_configenv_e9e22f_p1_endpoint_api_mode(const json_t *entry) {
    const json_t *raw_node = ce_or_node(ce_jget(entry, "api_mode"), ce_jget(entry, "transport"));
    char *raw = ce_str_of(raw_node, "");
    char *cleaned = ce_strip_inplace(raw);
    ce_lower_inplace(cleaned);
    const char *alias = ce_api_mode_alias(cleaned);
    char *mode = ce_dup(alias ? alias : cleaned);
    ce_lower_inplace(mode);
    json_t *out = json_string(ce_is_desktop_mode(mode) ? mode : "");
    free(mode);
    free(raw);
    return out;
}

json_t *w28_configenv_e9e22f_p1_model_names_provider(const json_t *model_cfg,
                                                     const json_t *provider_key,
                                                     const json_t *entry) {
    char *key = ce_coerce_id(provider_key);
    ce_lower_inplace(key);
    char *name = NULL;
    if (entry && entry->type == JSON_OBJECT) {
        char *raw_name = ce_coerce_id(ce_jget(entry, "name"));
        if (raw_name && *raw_name) {
            ce_lower_inplace(raw_name);
            name = raw_name;
        } else {
            free(raw_name);
        }
    }
    char *current = ce_strip_of(ce_jget(model_cfg, "provider"), "");
    ce_lower_inplace(current);
    const char *bare = current;
    if (!strncmp(current, "custom:", 7)) bare = current + 7;

    bool hit = (key && !strcmp(bare, key)) || (name && !strcmp(bare, name));
    free(key);
    free(name);
    free(current);
    return json_bool(hit);
}

/* ── GET /api/config: internal keys never reach the SPA ─────────────────── */

json_t *w28_configenv_e9e22f_p2_strip_internal(const json_t *config) {
    json_t *out = json_object();
    if (!out) return NULL;
    if (!config || config->type != JSON_OBJECT) return out;
    size_t n = json_object_size(config);
    for (size_t i = 0; i < n; i++) {
        const char *key = json_object_get_key_at(config, i);
        if (!key || key[0] == '_') continue;
        json_set(out, key, json_copy(json_object_get_at(config, i)));
    }
    return out;
}

/* ── _endpoint_row ──────────────────────────────────────────────────────── */

json_t *w28_configenv_e9e22f_p2_endpoint_row(const json_t *args) {
    const json_t *key_entry = ce_jget(args, "key_entry");
    json_t *display = w28_configenv_e9e22f_p1_api_key_display(key_entry);

    json_t *row = json_object();
    if (!row) { json_free(display); return NULL; }
    ce_set(row, "id", ce_copy_or_null(ce_jget(args, "id")));
    ce_set(row, "name", ce_copy_or_null(ce_jget(args, "name")));
    ce_set(row, "base_url", ce_copy_or_null(ce_jget(args, "base_url")));
    ce_set(row, "model", ce_copy_or_null(ce_jget(args, "model")));
    ce_set(row, "models", ce_copy_or_null(ce_jget(args, "models")));
    ce_set(row, "api_mode", w28_configenv_e9e22f_p1_endpoint_api_mode(key_entry));
    ce_set(row, "context_length", ce_copy_or_null(ce_jget(args, "context_length")));
    ce_set(row, "discover_models", json_bool(ce_truthy(ce_jget(args, "discover_models"))));
    ce_set(row, "has_api_key", ce_copy_or_null(display ? json_get(display, 0) : NULL));
    ce_set(row, "api_key_preview", ce_copy_or_null(display ? json_get(display, 1) : NULL));
    ce_set(row, "is_current", json_bool(ce_truthy(ce_jget(args, "is_current"))));
    ce_set(row, "source", ce_copy_or_null(ce_jget(args, "source")));
    json_free(display);
    return row;
}

/* ── _custom_endpoint_response ──────────────────────────────────────────── */

/* str(entry.model or entry.default_model or (models[0] if models else "")) */
static json_t *ce_entry_model(const json_t *entry, const json_t *models) {
    const json_t *pick = ce_or_node(ce_jget(entry, "model"), ce_jget(entry, "default_model"));
    char *model;
    if (ce_truthy(pick))
        model = ce_str(pick);
    else if (models && json_len(models) > 0)
        model = ce_str(json_get(models, 0));
    else
        model = ce_dup("");
    json_t *out = json_string(model);
    free(model);
    return out;
}

static json_t *ce_row_from_parts(const char *id, const json_t *name, const char *base_url,
                                 const json_t *model, const json_t *models,
                                 const json_t *context_length, bool discover_models,
                                 const json_t *key_entry, bool is_current, const char *source) {
    json_t *args = json_object();
    if (!args) return NULL;
    json_set(args, "id", json_string(id ? id : ""));
    ce_set(args, "name", ce_copy_or_null(name));
    json_set(args, "base_url", json_string(base_url ? base_url : ""));
    ce_set(args, "model", ce_copy_or_null(model));
    ce_set(args, "models", ce_copy_or_null(models));
    ce_set(args, "context_length", ce_copy_or_null(context_length));
    json_set(args, "discover_models", json_bool(discover_models));
    ce_set(args, "key_entry", ce_copy_or_null(key_entry));
    json_set(args, "is_current", json_bool(is_current));
    json_set(args, "source", json_string(source ? source : ""));
    json_t *row = w28_configenv_e9e22f_p2_endpoint_row(args);
    json_free(args);
    return row;
}

json_t *w28_configenv_e9e22f_p2_custom_endpoint_response(const json_t *cfg) {
    const json_t *model_cfg = ce_jget(cfg, "model");
    if (!model_cfg || model_cfg->type != JSON_OBJECT) model_cfg = NULL;

    char *current_provider = ce_str_of(ce_jget(model_cfg, "provider"), "");
    const json_t *default_node = ce_has(model_cfg, "default") ? ce_jget(model_cfg, "default")
                                                              : ce_jget(model_cfg, "name");
    char *current_model = ce_str_of(default_node, "");
    char *current_base_url = ce_str_of(ce_jget(model_cfg, "base_url"), "");

    json_t *endpoints = json_array();
    const json_t *providers = ce_jget(cfg, "providers");
    if (providers && providers->type == JSON_OBJECT) {
        size_t n = json_object_size(providers);
        for (size_t i = 0; i < n; i++) {
            const char *provider_id = json_object_get_key_at(providers, i);
            const json_t *raw_entry = json_object_get_at(providers, i);
            if (!provider_id || !raw_entry || raw_entry->type != JSON_OBJECT) continue;
            const json_t *url_node = ce_or_node(ce_jget(raw_entry, "base_url"),
                                   ce_or_node(ce_jget(raw_entry, "url"), ce_jget(raw_entry, "api")));
            char *base_url = ce_strip_of(url_node, "");
            if (!base_url[0]) { free(base_url); continue; }

            json_t *models = w28_configenv_e9e22f_p1_models_from_entry(raw_entry);
            json_t *model = ce_entry_model(raw_entry, models);
            char *name = ce_str_of(ce_jget(raw_entry, "name"), provider_id);
            json_t *id_node = json_string(provider_id);
            json_t *is_current = w28_configenv_e9e22f_p1_model_names_provider(
                model_cfg, id_node, raw_entry);
            bool discover = ce_has(raw_entry, "discover_models")
                                ? ce_truthy(ce_jget(raw_entry, "discover_models")) : true;
            json_append(endpoints, ce_row_from_parts(
                provider_id, json_string(name), base_url, model, models,
                ce_jget(raw_entry, "context_length"), discover, raw_entry,
                ce_truthy(is_current), "providers"));
            json_free(id_node);
            json_free(is_current);
            json_free(model);
            json_free(models);
            free(name);
            free(base_url);
        }
    }

    /* Legacy ``custom_providers:`` rows are a seam: config_providers
     * .get_compatible_custom_providers normalizes them upstream. */

    char *provider_lower = ce_dup(current_provider);
    ce_lower_inplace(provider_lower);
    bool is_bare_custom = !strcmp(provider_lower, "custom") && current_base_url[0] != '\0';
    free(provider_lower);

    bool covered = false;
    for (size_t i = 0; i < json_len(endpoints); i++) {
        const json_t *row = json_get(endpoints, i);
        const char *id = row ? json_get_str(row, "id", "") : "";
        if (!strcmp(id, "custom") || ce_truthy(ce_jget(row, "is_current"))) {
            covered = true;
            break;
        }
    }
    if (is_bare_custom && !covered) {
        json_t *models = json_array();
        if (current_model[0]) json_append(models, json_string(current_model));
        json_t *row = ce_row_from_parts(
            "custom", json_string("Custom"), current_base_url, json_string(current_model),
            models, ce_jget(model_cfg, "context_length"), true, model_cfg, true, "direct-config");
        json_t *grown = json_array();
        json_append(grown, row);
        for (size_t i = 0; i < json_len(endpoints); i++)
            json_append(grown, json_copy(json_get(endpoints, i)));
        json_free(endpoints);
        endpoints = grown;
        json_free(models);
    }

    json_t *out = json_object();
    json_set(out, "endpoints", endpoints);
    json_t *current = json_object();
    json_set(current, "provider", json_string(current_provider));
    json_set(current, "model", json_string(current_model));
    json_set(current, "base_url", json_string(current_base_url));
    json_set(out, "current", current);
    free(current_provider);
    free(current_model);
    free(current_base_url);
    return out;
}

/* ── legacy-provider pop / main-slot detach / stale addresses ───────────── */

json_t *w28_configenv_e9e22f_p2_pop_legacy_provider(const json_t *cfg,
                                                    const json_t *provider_key) {
    json_t *after = cfg ? json_copy(cfg) : json_object();
    json_t *popped = json_null();
    const json_t *legacy = ce_jget(after, "custom_providers");
    if (legacy && legacy->type == JSON_ARRAY) {
        size_t n = json_len(legacy);
        const char *key = (provider_key && provider_key->type == JSON_STRING &&
                           provider_key->str_val) ? provider_key->str_val : NULL;
        for (size_t i = 0; i < n; i++) {
            const json_t *entry = json_get(legacy, i);
            if (!entry || entry->type != JSON_OBJECT) continue;
            json_t *slug = w28_configenv_e9e22f_p1_custom_endpoint_id(
                ce_jget(entry, "name"), "custom");
            bool match = key && slug && slug->str_val && !strcmp(slug->str_val, key);
            json_free(slug);
            if (!match) continue;
            /* legacy.pop(index) */
            json_t *trimmed = json_array();
            for (size_t k = 0; k < n; k++) {
                if (k == i) { popped = json_copy(entry); continue; }
                json_append(trimmed, json_copy(json_get(legacy, k)));
            }
            json_set(after, "custom_providers", trimmed);
            break;
        }
    }
    json_t *out = json_object();
    json_set(out, "cfg", after);
    json_set(out, "entry", popped);
    return out;
}

json_t *w28_configenv_e9e22f_p2_detach_main_model(const json_t *cfg,
                                                  const json_t *provider_key,
                                                  const json_t *entry) {
    json_t *after = cfg ? json_copy(cfg) : json_object();
    json_t *model_cfg = (json_t *)ce_jget(after, "model");
    if (!model_cfg || model_cfg->type != JSON_OBJECT) return after;
    json_t *named = w28_configenv_e9e22f_p1_model_names_provider(model_cfg, provider_key, entry);
    bool match = ce_truthy(named);
    json_free(named);
    if (!match) return after;
    static const char *fields[] = {"provider", "base_url", "api_key", "key_env"};
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++)
        json_obj_del(model_cfg, fields[i]);
    return after;
}

json_t *w28_configenv_e9e22f_p2_drop_stale_addresses(const json_t *entry) {
    json_t *out = entry ? json_copy(entry) : json_object();
    if (!out || out->type != JSON_OBJECT) return out;
    /* ENDPOINT_ADDRESS_FIELDS minus base_url */
    static const char *stale[] = {"url", "api", "baseUrl"};
    for (size_t i = 0; i < sizeof(stale) / sizeof(stale[0]); i++)
        json_obj_del(out, stale[i]);
    return out;
}

/* ── _resolve_custom_endpoint_entry ─────────────────────────────────────── */

/* find_provider_entry(): exact hit, then a string-identity scan. */
static void ce_find_provider(const json_t *providers, const char *want,
                             const char **stored_out, const json_t **entry_out) {
    *stored_out = NULL;
    *entry_out = NULL;
    if (!providers || providers->type != JSON_OBJECT || !want || !want[0]) return;
    const json_t *exact = json_obj_get(providers, want);
    if (exact && exact->type == JSON_OBJECT) {
        *stored_out = want;
        *entry_out = exact;
        return;
    }
    size_t n = json_object_size(providers);
    for (size_t i = 0; i < n; i++) {
        const char *stored = json_object_get_key_at(providers, i);
        const json_t *entry = json_object_get_at(providers, i);
        if (!stored || !entry || entry->type != JSON_OBJECT) continue;
        char *coerced = ce_dup(stored);
        char *trimmed = ce_strip_inplace(coerced);
        bool same = !strcmp(trimmed, want);
        free(coerced);
        if (same) {
            *stored_out = stored;
            *entry_out = entry;
            return;
        }
    }
}

json_t *w28_configenv_e9e22f_p2_resolve_entry(const json_t *providers,
                                              const json_t *endpoint_id) {
    json_t *out = json_array();
    if (!out) return NULL;
    char *want = ce_coerce_id(endpoint_id);
    const char *stored = NULL;
    const json_t *entry = NULL;
    ce_find_provider(providers, want, &stored, &entry);
    if (entry) {
        json_append(out, json_string(stored));
        json_append(out, json_copy(entry));
        free(want);
        return out;
    }

    json_t *slug = w28_configenv_e9e22f_p1_custom_endpoint_id(endpoint_id, "custom");
    const char *slugged = (slug && slug->str_val) ? slug->str_val : "";
    bool same = !strcmp(slugged, want);
    free(want);
    if (!same) {
        const char *stored2 = NULL;
        const json_t *entry2 = NULL;
        ce_find_provider(providers, slugged, &stored2, &entry2);
        if (entry2) {
            json_append(out, json_string(stored2));
            json_append(out, json_copy(entry2));
            json_free(slug);
            return out;
        }
    }
    json_append(out, json_null());
    json_append(out, json_null());
    json_free(slug);
    return out;
}

/* ── _write_custom_endpoint validation preamble ─────────────────────────── */

/* The urllib.parse.urlparse() surface the router relies on: scheme + netloc,
 * plus the Invalid IPv6 URL ValueError on unbalanced netloc brackets. */
static void ce_urlparse(const char *url, bool *has_scheme, bool *has_netloc, bool *bad_ipv6) {
    *has_scheme = false;
    *has_netloc = false;
    *bad_ipv6 = false;
    if (!url) return;
    size_t n = strlen(url);

    size_t colon = (size_t)-1;
    for (size_t i = 0; i < n; i++) {
        if (url[i] == ':') { colon = i; break; }
        if (url[i] == '/' || url[i] == '?' || url[i] == '#') break;
    }
    size_t rest = 0;
    if (colon != (size_t)-1 && colon > 0) {
        bool ok = (url[0] >= 'A' && url[0] <= 'Z') || (url[0] >= 'a' && url[0] <= 'z');
        for (size_t i = 0; ok && i < colon; i++) {
            char c = url[i];
            bool scheme_char = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                               (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
            if (!scheme_char) ok = false;
        }
        if (ok) { *has_scheme = true; rest = colon + 1; }
    }
    if (rest + 1 < n && url[rest] == '/' && url[rest + 1] == '/') {
        size_t start = rest + 2, end = start;
        while (end < n && url[end] != '/' && url[end] != '?' && url[end] != '#') end++;
        if (end > start) *has_netloc = true;
        bool open = false, close = false;
        for (size_t i = start; i < end; i++) {
            if (url[i] == '[') open = true;
            else if (url[i] == ']') close = true;
        }
        if (open != close) *bad_ipv6 = true;
    }
}

static json_t *ce_fail(const char *detail) {
    json_t *out = json_object();
    json_set(out, "ok", json_bool(false));
    json_set(out, "status", json_number(400));
    json_set(out, "detail", json_string(detail));
    return out;
}

json_t *w28_configenv_e9e22f_p2_validate_endpoint(const json_t *name,
                                                  const json_t *base_url,
                                                  const json_t *model) {
    char *nm = ce_strip_of(name, "");
    char *bu = ce_strip_of(base_url, "");
    char *md = ce_strip_of(model, "");

    /* base_url.rstrip("/") */
    size_t len = strlen(bu);
    while (len > 0 && bu[len - 1] == '/') len--;
    bu[len] = '\0';

    if (!*nm) { free(nm); free(bu); free(md); return ce_fail("name required"); }
    if (!*bu) { free(nm); free(bu); free(md); return ce_fail("base_url required"); }

    bool has_scheme = false, has_netloc = false, bad_ipv6 = false;
    ce_urlparse(bu, &has_scheme, &has_netloc, &bad_ipv6);
    if (bad_ipv6 || !has_scheme || !has_netloc) {
        free(nm); free(bu); free(md);
        return ce_fail("base_url must include scheme and host");
    }
    if (!*md) { free(nm); free(bu); free(md); return ce_fail("model required"); }

    json_t *value = json_object();
    json_set(value, "name", json_string(nm));
    json_set(value, "base_url", json_string(bu));
    json_set(value, "model", json_string(md));
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    json_set(out, "value", value);
    free(nm);
    free(bu);
    free(md);
    return out;
}

/* ── _no_models_probe_message ───────────────────────────────────────────── */

json_t *w28_configenv_e9e22f_p2_no_models_message(const json_t *req) {
    const char *base_url = json_get_str(req, "base_url", "");
    const char *content_type_raw = json_get_str(req, "content_type", "");
    bool payload_ok = json_get_bool(req, "payload_ok", true);
    const json_t *payload = ce_jget(req, "payload");

    /* (resp.headers.get("content-type") or "").split(";")[0].strip() */
    char *ct_copy = ce_dup(content_type_raw ? content_type_raw : "");
    char *semi = ct_copy ? strchr(ct_copy, ';') : NULL;
    if (semi) *semi = '\0';
    char *ct = ct_copy ? ce_strip_inplace(ct_copy) : NULL;

    size_t need = strlen(base_url ? base_url : "") + 32;
    char *url = (char *)malloc(need);
    if (!url) { free(ct_copy); return json_string(""); }
    snprintf(url, need, "%s/models", base_url ? base_url : "");

    bool is_json = payload_ok && payload &&
                   (payload->type == JSON_OBJECT || payload->type == JSON_ARRAY);
    size_t n2 = strlen(url) + 256;
    char *text = NULL;
    if (!is_json) {
        const char *kind = (ct && *ct) ? ct : "a non-JSON body";
        n2 += strlen(kind);
        text = (char *)malloc(n2);
        if (text)
            snprintf(text, n2,
                     "Connected to %s, but it returned %s instead of a JSON model list — "
                     "the URL may be an SPA catch-all or a wrong base URL, not an "
                     "OpenAI-compatible API.", url, kind);
    } else {
        const json_t *data = (payload->type == JSON_OBJECT) ? ce_jget(payload, "data") : payload;
        text = (char *)malloc(n2);
        if (text) {
            if (!data || data->type != JSON_ARRAY)
                snprintf(text, n2,
                         "Connected to %s, but its response is not an OpenAI-compatible "
                         "model list.", url);
            else
                snprintf(text, n2,
                         "Connected to %s, but it advertised no models. "
                         "Start a model on that endpoint and try again.", url);
        }
    }
    json_t *node = json_string(text ? text : "");
    free(text);
    free(url);
    free(ct_copy);
    return node;
}
