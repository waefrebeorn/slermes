/* port_models_catalog_flow.c — hermes_cli/models.py catalog/selection FLOW.
 *
 * Decision-sequence port: model listing/filtering ladder, provider/model
 * resolution, alias canonicalization, cache-serve tiers. All file/network/
 * config I/O rides the injected universal seam (mcat_call); the harness
 * scripts results and records the ordered trace the oracle diffs against
 * LIVE models.py driven under the same scripts.
 *
 * Representation contract: see include/port_models_catalog_flow.h.
 * libjson-only (default oracle deps suffice).
 */
#include "port_models_catalog_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── string helpers (ASCII fixture domain; Python str semantics) ─────────── */

static char *mcat_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static bool mcat_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* str.strip() — both ends, ASCII whitespace fixture domain. */
static char *mcat_strip(const char *s) {
    while (*s && mcat_is_ws(*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && mcat_is_ws(s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

/* str.lower() — ASCII fixture domain. */
static char *mcat_lower(const char *s) {
    char *out = mcat_strdup(s);
    if (out) for (char *p = out; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return out;
}

static char *mcat_strip_lower(const char *s) {
    char *t = mcat_strip(s);
    char *out = mcat_lower(t ? t : "");
    free(t);
    return out;
}

/* str.rstrip("/") */
static char *mcat_rstrip_slash(const char *s) {
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == '/') n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

static bool mcat_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

/* ── JSON value helpers ──────────────────────────────────────────────────── */

static bool mcat_node_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: {
            /* {"$curated": [...]} tag: truthiness follows the items. */
            json_t *items = json_obj_get(v, "$curated");
            if (items && items->type == JSON_ARRAY) return items->c.count > 0;
            return v->c.count > 0;
        }
        default: return false;
    }
}

static bool mcat_is_curated(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "$curated") != NULL;
}

/* Borrowed items array of a list value (plain array or $curated tag). */
static json_t *mcat_items(const json_t *v) {
    if (!v) return NULL;
    if (v->type == JSON_ARRAY) return (json_t *)v;
    if (v->type == JSON_OBJECT) return json_obj_get(v, "$curated");
    return NULL;
}

static json_t *mcat_list_copy(const json_t *v) {
    json_t *items = mcat_items(v);
    json_t *out = json_array();
    if (items) for (size_t i = 0; i < items->c.count; i++) json_append(out, json_copy(items->c.items[i]));
    return out;
}

static json_t *mcat_curated(const json_t *items_src) {
    json_t *out = json_object();
    json_set(out, "$curated", mcat_list_copy(items_src));
    return out;
}

/* str(x or "") for the fixture domain: string verbatim, falsy -> "".
 * Truthy non-strings (container repr) are excluded from fixtures. */
static char *mcat_str_or_empty(const json_t *v) {
    if (mcat_node_truthy(v) && v->type == JSON_STRING) return mcat_strdup(v->str_val);
    return mcat_strdup("");
}

static bool mcat_arr_has_str(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *it = arr->c.items[i];
        if (it && it->type == JSON_STRING && mcat_str_eq(it->str_val, s)) return true;
    }
    return false;
}

static bool mcat_obj_has_key(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}

static bool mcat_arr_eq(const json_t *a, const json_t *b) {
    if (!a || !b || a->type != JSON_ARRAY || b->type != JSON_ARRAY) return false;
    if (a->c.count != b->c.count) return false;
    for (size_t i = 0; i < a->c.count; i++) {
        json_t *x = a->c.items[i];
        json_t *y = b->c.items[i];
        if (!x || !y || x->type != y->type) return false;
        if (x->type == JSON_STRING) { if (!mcat_str_eq(x->str_val, y->str_val)) return false; }
        else if (x->type == JSON_NUMBER) { if (x->num_val != y->num_val) return false; }
        else if (x->type == JSON_BOOL) { if (x->bool_val != y->bool_val) return false; }
        else if (x->type == JSON_NULL) { /* equal */ }
        else return false;
    }
    return true;
}

static int mcat_str_cmp(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

/* Sorted unique string array (Python set[str] canonical form). */
static json_t *mcat_sorted_set(char **vals, size_t n) {
    qsort(vals, n, sizeof(char *), mcat_str_cmp);
    json_t *out = json_array();
    for (size_t i = 0; i < n; i++) {
        if (i == 0 || strcmp(vals[i], vals[i - 1]) != 0) json_append(out, json_string(vals[i]));
    }
    return out;
}

/* ── exception plumbing ──────────────────────────────────────────────────── */

bool mcat_exc_pending(const mcat_ctx *ctx) { return ctx && ctx->exc != NULL; }

void mcat_exc_clear(mcat_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}

void mcat_raise(mcat_ctx *ctx, const char *cls) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
}

/* ── universal seam ──────────────────────────────────────────────────────── */

static json_t *mcat_script_result(mcat_ctx *ctx, const json_t *script) {
    /* Returns a NEW value (NULL == None); sets ctx->exc for a scripted raise. */
    if (!script || script->type == JSON_NULL) return NULL;
    if (script->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(script, "__raise__");
        if (raise && raise->type == JSON_STRING) {
            if (ctx->exc) json_free(ctx->exc);
            ctx->exc = json_copy(script);
            return NULL;
        }
    }
    return json_copy(script);
}

json_t *mcat_call(mcat_ctx *ctx, const char *name, json_t *args) {
    /* args ownership is transferred (NULL -> empty object). Result is NEW. */
    if (!args) args = json_object();
    json_t *ret = NULL;
    json_t *script = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *chosen = script;
    if (script && script->type == JSON_OBJECT) {
        json_t *seq = json_obj_get(script, "$seq");
        if (seq && seq->type == JSON_ARRAY) {
            double cur = json_get_num(ctx->counters, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= seq->c.count) {
                if (ctx->exc) json_free(ctx->exc);
                ctx->exc = json_object();
                json_set(ctx->exc, "__raise__", json_string("RuntimeError"));
                char msg[256];
                snprintf(msg, sizeof(msg), "script exhausted: %s", name);
                json_set(ctx->exc, "msg", json_string(msg));
                chosen = ctx->exc;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        }
    }
    if (chosen != ctx->exc) ret = mcat_script_result(ctx, chosen);
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args);
    if (ctx->exc && chosen == ctx->exc) json_set(ev, "ret", json_copy(ctx->exc));
    else if (ctx->exc && chosen && chosen->type == JSON_OBJECT && json_obj_get(chosen, "__raise__"))
        json_set(ev, "ret", json_copy(ctx->exc));
    else json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(ctx->trace, ev);
    return ret;
}

/* ── env table accessors ─────────────────────────────────────────────────── */

static json_t *mcat_env_obj(const mcat_ctx *ctx, const char *key) {
    json_t *v = ctx->env ? json_obj_get(ctx->env, key) : NULL;
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

static json_t *mcat_env_arr(const mcat_ctx *ctx, const char *key) {
    json_t *v = ctx->env ? json_obj_get(ctx->env, key) : NULL;
    return (v && v->type == JSON_ARRAY) ? v : NULL;
}

static double mcat_env_num(const mcat_ctx *ctx, const char *key, double def) {
    return ctx->env ? json_get_num(ctx->env, key, def) : def;
}

/* ── internal pure helpers (claimed siblings; NO PoP claim here) ─────────── */

/* normalize_provider: (provider or "openrouter").strip().lower() -> alias map. */
static char *mcat_normalize_provider(const mcat_ctx *ctx, const char *provider) {
    char *base;
    if (!provider || !provider[0]) base = mcat_strdup("openrouter");
    else base = mcat_strip_lower(provider);
    json_t *aliases = mcat_env_obj(ctx, "aliases");
    const char *mapped = aliases ? json_get_str(aliases, base, NULL) : NULL;
    char *out = mcat_strdup(mapped ? mapped : base);
    free(base);
    return out;
}

/* _provider_keys: {k for k in (raw_key, normalized) if k}, sorted. */
static json_t *mcat_provider_keys(const mcat_ctx *ctx, const char *provider) {
    char *key = mcat_strip_lower(provider ? provider : "");
    char *norm = mcat_normalize_provider(ctx, provider);
    char *vals[2];
    size_t n = 0;
    if (key[0]) vals[n++] = key;
    if (norm[0] && strcmp(norm, key) != 0) vals[n++] = norm;
    json_t *out = mcat_sorted_set(vals, n);
    free(key);
    free(norm);
    return out;
}

/* _cache_entry_valid: well-formed row for fp; numeric non-bool `at`. */
static bool mcat_cache_entry_valid(const json_t *entry, const char *fp, bool allow_empty) {
    if (!entry || entry->type != JSON_OBJECT) return false;
    const char *efp = json_get_str(entry, "fp", NULL);
    if (!efp || !mcat_str_eq(efp, fp)) return false;
    json_t *models = json_obj_get(entry, "models");
    if (!models || models->type != JSON_ARRAY) return false;
    if (!allow_empty && models->c.count == 0) return false;
    json_t *at = json_obj_get(entry, "at");
    if (!at || at->type != JSON_NUMBER) return false; /* bool is JSON_BOOL: rejected */
    return true;
}

/* _model_dedup_key: strip().lower() then model_alias_canonical seam; the
 * seam raising (or the import failing) falls back to the folded key. */
static char *mcat_model_dedup_key(mcat_ctx *ctx, const char *model_id) {
    char *key = mcat_strip_lower(model_id ? model_id : "");
    json_t *args = json_object();
    json_set(args, "key", json_string(key));
    json_t *r = mcat_call(ctx, "model_alias_canonical", args);
    if (mcat_exc_pending(ctx)) {
        mcat_exc_clear(ctx);
        json_free(r);
        return key;
    }
    if (r && r->type == JSON_STRING) {
        char *out = mcat_strdup(r->str_val);
        json_free(r);
        free(key);
        return out;
    }
    json_free(r);
    return key;
}

/* Attribute access on a profile/object double. mcat_attr models `obj.name`
 * (AttributeError when absent — propagates unless the flow catches it);
 * mcat_attr_def models getattr(obj, name, default) (never raises). */
static json_t *mcat_attr(mcat_ctx *ctx, json_t *obj, const char *name) {
    if (!obj || obj->type != JSON_OBJECT || !json_obj_get(obj, name)) {
        mcat_raise(ctx, "AttributeError");
        return NULL;
    }
    return json_obj_get(obj, name);
}

static const char *mcat_attr_def_str(json_t *obj, const char *name, const char *def) {
    if (!obj || obj->type != JSON_OBJECT) return def;
    json_t *v = json_obj_get(obj, name);
    if (!v || v->type != JSON_STRING) return def;
    return v->str_val;
}
/* ── ported flows/leaves ─────────────────────────────────────────────────── */

/* PoP: mcat_merge_unique @ hermes_cli/models.py:_merge_unique */
json_t *mcat_merge_unique(mcat_ctx *ctx, const json_t *primary, const json_t *secondary, bool dedup_key) {
    json_t *merged = json_array();
    char **seen = NULL;
    size_t nseen = 0, cap = 0;
    const json_t *p = mcat_items(primary);
    const json_t *s = mcat_items(secondary);
    /* seen = {key(m) for m in primary} — every primary key computed first. */
    for (size_t i = 0; p && i < p->c.count; i++) {
        json_t *m = p->c.items[i];
        const char *ms = (m && m->type == JSON_STRING) ? m->str_val : "";
        char *k = dedup_key ? mcat_model_dedup_key(ctx, ms) : mcat_lower(ms);
        if (nseen == cap) { cap = cap ? cap * 2 : 8; seen = (char **)realloc(seen, cap * sizeof(char *)); }
        seen[nseen++] = k;
        json_append(merged, json_copy(m));
    }
    for (size_t i = 0; s && i < s->c.count; i++) {
        json_t *m = s->c.items[i];
        const char *ms = (m && m->type == JSON_STRING) ? m->str_val : "";
        char *k = dedup_key ? mcat_model_dedup_key(ctx, ms) : mcat_lower(ms);
        bool found = false;
        for (size_t j = 0; j < nseen; j++) if (strcmp(seen[j], k) == 0) { found = true; break; }
        if (!found) {
            if (nseen == cap) { cap = cap ? cap * 2 : 8; seen = (char **)realloc(seen, cap * sizeof(char *)); }
            seen[nseen++] = k;
            json_append(merged, json_copy(m));
        } else {
            free(k);
        }
    }
    for (size_t j = 0; j < nseen; j++) free(seen[j]);
    free(seen);
    return merged;
}

/* PoP: mcat_configured_provider_ids @ hermes_cli/models.py:_configured_provider_ids */
json_t *mcat_configured_provider_ids(mcat_ctx *ctx) {
    json_t *cfg = mcat_call(ctx, "load_config", json_object());
    if (mcat_exc_pending(ctx)) {
        mcat_exc_clear(ctx);
        json_free(cfg);
        return json_array();
    }
    json_t *providers = NULL;
    if (mcat_node_truthy(cfg)) {
        if (cfg->type != JSON_OBJECT) { /* (x or {}).get -> AttributeError -> except */
            json_free(cfg);
            return json_array();
        }
        providers = json_obj_get(cfg, "providers");
    }
    if (!providers || providers->type != JSON_OBJECT) {
        json_free(cfg);
        return json_array();
    }
    char **vals = NULL;
    size_t n = 0;
    for (size_t i = 0; i < providers->c.count; i++) {
        char *k = mcat_strip_lower(providers->c.keys[i]);
        if (k[0]) {
            vals = (char **)realloc(vals, (n + 1) * sizeof(char *));
            vals[n++] = k;
        } else {
            free(k);
        }
    }
    json_t *out = mcat_sorted_set(vals, n);
    for (size_t i = 0; i < n; i++) free(vals[i]);
    free(vals);
    json_free(cfg);
    return out;
}

/* PoP: mcat_resolve_provider_prefix @ hermes_cli/models.py:_resolve_provider_prefix */
json_t *mcat_resolve_provider_prefix(mcat_ctx *ctx, const char *model_name) {
    const char *name = model_name ? model_name : "";
    const char *slash = strchr(name, '/');
    if (!slash) return NULL;
    size_t vlen = (size_t)(slash - name);
    char *vraw = (char *)malloc(vlen + 1);
    memcpy(vraw, name, vlen);
    vraw[vlen] = '\0';
    char *vendor = mcat_strip_lower(vraw);
    char *model = mcat_strip(slash + 1);
    free(vraw);
    if (!vendor[0] || !model[0]) {
        free(vendor);
        free(model);
        return NULL;
    }
    json_t *configured = mcat_configured_provider_ids(ctx);
    json_t *aliases = mcat_env_obj(ctx, "aliases");
    const char *mapped = aliases ? json_get_str(aliases, vendor, NULL) : NULL;
    const char *candidates[2] = { vendor, mapped ? mapped : vendor };
    json_t *out = NULL;
    for (int i = 0; i < 2; i++) {
        if (mcat_arr_has_str(configured, candidates[i])) {
            out = json_array();
            json_append(out, json_string(candidates[i]));
            json_append(out, json_string(model));
            break;
        }
    }
    json_free(configured);
    free(vendor);
    free(model);
    return out;
}

/* PoP: mcat_static_catalog @ hermes_cli/models.py:static_catalog */
json_t *mcat_static_catalog(mcat_ctx *ctx, const char *provider) {
    const char *pid = provider ? provider : "";
    json_t *pm = mcat_env_obj(ctx, "provider_models");
    if (mcat_obj_has_key(pm, pid)) return json_copy(json_obj_get(pm, pid));
    json_t *profile = NULL;
    if (pid[0]) {
        json_t *args = json_object();
        json_set(args, "provider", json_string(pid));
        profile = mcat_call(ctx, "get_provider_profile", args);
        if (mcat_exc_pending(ctx)) {
            json_free(profile);
            return NULL;
        }
    }
    if (!profile) return json_array(); /* profile is None */
    json_t *sm = mcat_attr(ctx, profile, "static_models");
    if (mcat_exc_pending(ctx)) {
        json_free(profile);
        return NULL;
    }
    if (!sm || sm->type != JSON_ARRAY) { /* list(None) / list(non-iterable) */
        mcat_raise(ctx, "TypeError");
        json_free(profile);
        return NULL;
    }
    json_t *out = mcat_list_copy(sm);
    json_free(profile);
    return out;
}

/* PoP: mcat_static_catalog_providers @ hermes_cli/models.py:static_catalog_providers */
json_t *mcat_static_catalog_providers(mcat_ctx *ctx) {
    json_t *rows = mcat_call(ctx, "list_providers", json_object());
    if (mcat_exc_pending(ctx)) {
        json_free(rows);
        return NULL;
    }
    if (!rows || rows->type != JSON_ARRAY) { /* iterating None */
        mcat_raise(ctx, "TypeError");
        json_free(rows);
        return NULL;
    }
    json_t *pm = mcat_env_obj(ctx, "provider_models");
    char **plugin = NULL;
    size_t np = 0;
    for (size_t i = 0; i < rows->c.count; i++) {
        json_t *p = rows->c.items[i];
        /* condition order: p.static_models first, then p.name */
        json_t *sm = mcat_attr(ctx, p, "static_models");
        if (mcat_exc_pending(ctx)) goto fail;
        if (!mcat_node_truthy(sm)) continue;
        json_t *nm = mcat_attr(ctx, p, "name");
        if (mcat_exc_pending(ctx)) goto fail;
        if (!nm || nm->type != JSON_STRING) {
            mcat_raise(ctx, "TypeError");
            goto fail;
        }
        if (!mcat_obj_has_key(pm, nm->str_val)) {
            plugin = (char **)realloc(plugin, (np + 1) * sizeof(char *));
            plugin[np++] = mcat_strdup(nm->str_val);
        }
    }
    json_t *out = json_array();
    if (pm) for (size_t i = 0; i < pm->c.count; i++) json_append(out, json_string(pm->c.keys[i]));
    for (size_t i = 0; i < np; i++) { /* dict.fromkeys: first occurrence wins */
        bool dup = false;
        for (size_t j = 0; j < i; j++) if (strcmp(plugin[j], plugin[i]) == 0) { dup = true; break; }
        if (!dup) json_append(out, json_string(plugin[i]));
    }
    for (size_t i = 0; i < np; i++) free(plugin[i]);
    free(plugin);
    json_free(rows);
    return out;
fail:
    for (size_t i = 0; i < np; i++) free(plugin[i]);
    free(plugin);
    json_free(rows);
    return NULL;
}

/* PoP: mcat_static_catalog_matches @ hermes_cli/models.py:_static_catalog_matches */
json_t *mcat_static_catalog_matches(mcat_ctx *ctx, const char *name, const char *current_provider) {
    const char *cp = current_provider ? current_provider : "";
    char *name_lower = mcat_lower(name ? name : "");
    json_t *current_keys = mcat_provider_keys(ctx, cp);
    json_t *aggs = mcat_env_arr(ctx, "aggregators");
    json_t *borrowed = mcat_env_arr(ctx, "borrowed");
    json_t *out = json_array();
    /* Step 1: direct static-catalog match (never TO aggregators/borrowed,
     * never away from custom / custom:*). */
    if (!mcat_str_eq(cp, "custom") && strncmp(cp, "custom:", 7) != 0) {
        json_t *providers = mcat_static_catalog_providers(ctx);
        if (mcat_exc_pending(ctx)) {
            json_free(providers);
            json_free(out);
            json_free(current_keys);
            free(name_lower);
            return NULL;
        }
        for (size_t i = 0; providers && i < providers->c.count; i++) {
            json_t *pidn = providers->c.items[i];
            if (!pidn || pidn->type != JSON_STRING) continue;
            const char *pid = pidn->str_val;
            if (mcat_arr_has_str(current_keys, pid) || mcat_arr_has_str(aggs, pid) ||
                mcat_arr_has_str(borrowed, pid)) continue;
            json_t *args = json_object();
            json_set(args, "name_lower", json_string(name_lower));
            json_t *one = json_array();
            json_append(one, json_string(pid));
            json_set(args, "providers", one);
            json_t *hit = mcat_call(ctx, "model_in_provider_catalog", args);
            if (mcat_exc_pending(ctx)) {
                json_free(hit);
                json_free(providers);
                json_free(out);
                json_free(current_keys);
                free(name_lower);
                return NULL;
            }
            if (mcat_node_truthy(hit)) {
                json_t *pair = json_array();
                json_append(pair, json_string(pid));
                json_append(pair, json_string(name ? name : ""));
                json_append(out, pair);
            }
            json_free(hit);
        }
        json_free(providers);
    }
    /* Step 2: borrow-list providers, only when not the current provider. */
    for (size_t i = 0; borrowed && i < borrowed->c.count; i++) {
        json_t *pidn = borrowed->c.items[i];
        if (!pidn || pidn->type != JSON_STRING) continue;
        const char *pid = pidn->str_val;
        if (mcat_arr_has_str(current_keys, pid)) continue; /* short-circuit: no seam */
        json_t *args = json_object();
        json_set(args, "name_lower", json_string(name_lower));
        json_t *one = json_array();
        json_append(one, json_string(pid));
        json_set(args, "providers", one);
        json_t *hit = mcat_call(ctx, "model_in_provider_catalog", args);
        if (mcat_exc_pending(ctx)) {
            json_free(hit);
            json_free(out);
            json_free(current_keys);
            free(name_lower);
            return NULL;
        }
        if (mcat_node_truthy(hit)) {
            json_t *pair = json_array();
            json_append(pair, json_string(pid));
            json_append(pair, json_string(name ? name : ""));
            json_append(out, pair);
        }
        json_free(hit);
    }
    json_free(current_keys);
    free(name_lower);
    return out;
}

/* PoP: mcat_detection_candidates @ hermes_cli/models.py:_detection_candidates */
json_t *mcat_detection_candidates(mcat_ctx *ctx, const char *name, const char *current_provider) {
    const char *cp = current_provider ? current_provider : "";
    const char *nm = name ? name : "";
    json_t *out = json_array();
    json_t *args = json_object();
    json_set(args, "model_name", json_string(nm));
    json_set(args, "current_provider", json_string(cp));
    json_t *static_match = mcat_call(ctx, "detect_static_provider_for_model", args);
    if (mcat_exc_pending(ctx)) {
        json_free(static_match);
        json_free(out);
        return NULL;
    }
    if (mcat_node_truthy(static_match)) {
        json_append(out, json_copy(static_match));
        json_t *sibs = mcat_static_catalog_matches(ctx, nm, cp);
        if (mcat_exc_pending(ctx)) {
            json_free(sibs);
            json_free(static_match);
            json_free(out);
            return NULL;
        }
        for (size_t i = 0; sibs && i < sibs->c.count; i++) {
            if (!mcat_arr_eq(sibs->c.items[i], static_match)) json_append(out, json_copy(sibs->c.items[i]));
        }
        json_free(sibs);
    }
    json_free(static_match);
    char *name_lower = mcat_lower(nm);
    json_t *current_keys = mcat_provider_keys(ctx, cp);
    args = json_object();
    json_set(args, "name_lower", json_string(name_lower));
    json_set(args, "providers", current_keys); /* ownership transferred; sorted */
    json_t *hit = mcat_call(ctx, "model_in_provider_catalog", args);
    if (mcat_exc_pending(ctx)) {
        json_free(hit);
        json_free(out);
        free(name_lower);
        return NULL;
    }
    if (mcat_node_truthy(hit)) {
        json_free(hit);
        json_append(out, json_null()); /* yield None: current catalog owns it */
        free(name_lower);
        return out;
    }
    json_free(hit);
    args = json_object();
    json_set(args, "model_name", json_string(nm));
    json_t *slug = mcat_call(ctx, "find_openrouter_slug", args);
    if (mcat_exc_pending(ctx)) {
        json_free(slug);
        json_free(out);
        free(name_lower);
        return NULL;
    }
    if (mcat_node_truthy(slug)) {
        if (mcat_str_eq(cp, "openrouter") && slug->type == JSON_STRING && mcat_str_eq(slug->str_val, nm)) {
            json_free(slug);
            json_append(out, json_null());
            free(name_lower);
            return out;
        }
        json_t *pair = json_array();
        json_append(pair, json_string("openrouter"));
        json_append(pair, json_copy(slug));
        json_append(out, pair);
    }
    json_free(slug);
    json_t *prefixed = mcat_resolve_provider_prefix(ctx, nm);
    if (mcat_exc_pending(ctx)) {
        json_free(prefixed);
        json_free(out);
        free(name_lower);
        return NULL;
    }
    if (mcat_node_truthy(prefixed)) json_append(out, prefixed);
    else json_free(prefixed);
    free(name_lower);
    return out;
}

/* PoP: mcat_catalog_fetcher @ hermes_cli/models.py:_catalog_fetcher */
json_t *mcat_catalog_fetcher(mcat_ctx *ctx, const char *normalized) {
    const char *pid = normalized ? normalized : "";
    json_t *fetchers = mcat_env_arr(ctx, "fetchers");
    if (mcat_arr_has_str(fetchers, pid)) return json_string(pid);
    json_t *args = json_object();
    json_set(args, "provider", json_string(pid));
    json_t *profile = mcat_call(ctx, "get_provider_profile", args);
    if (mcat_exc_pending(ctx)) {
        json_free(profile);
        return NULL;
    }
    if (!profile) return NULL;
    bool is_pp = profile->type == JSON_OBJECT && json_get_bool(profile, "is_provider_profile", true);
    if (!is_pp) {
        json_free(profile);
        return NULL;
    }
    json_t *hook = mcat_attr(ctx, profile, "picker_catalog");
    if (mcat_exc_pending(ctx)) {
        json_free(profile);
        return NULL;
    }
    json_t *out = NULL;
    if (hook && hook->type != JSON_NULL) out = json_string("profile-hook"); /* hook is not None */
    json_free(profile);
    return out;
}

/* PoP: mcat_api_key_credentials @ hermes_cli/models.py:_api_key_credentials */
json_t *mcat_api_key_credentials(mcat_ctx *ctx, const char *normalized) {
    json_t *args = json_object();
    json_set(args, "provider", json_string(normalized ? normalized : ""));
    json_t *creds = mcat_call(ctx, "resolve_api_key_provider_credentials", args);
    if (mcat_exc_pending(ctx)) {
        mcat_exc_clear(ctx);
        json_free(creds);
        creds = NULL;
    }
    char *key = mcat_strdup("");
    char *base = mcat_strdup("");
    if (creds && creds->type == JSON_OBJECT) {
        char *k = mcat_str_or_empty(json_obj_get(creds, "api_key"));
        char *b = mcat_str_or_empty(json_obj_get(creds, "base_url"));
        free(key);
        free(base);
        key = mcat_strip(k);
        base = mcat_strip(b);
        free(k);
        free(b);
    }
    json_free(creds);
    json_t *out = json_array();
    json_append(out, json_string(key));
    json_append(out, json_string(base));
    free(key);
    free(base);
    return out;
}
/* PoP: mcat_api_key_provider_live @ hermes_cli/models.py:_api_key_provider_live */
json_t *mcat_api_key_provider_live(mcat_ctx *ctx, const char *normalized, bool force_refresh) {
    (void)force_refresh; /* live signature carries it; the body never reads it */
    json_t *creds = mcat_api_key_credentials(ctx, normalized);
    const char *key = creds->c.items[0]->str_val;
    const char *base = creds->c.items[1]->str_val;
    if (!(key[0] && base[0])) {
        json_free(creds);
        return NULL;
    }
    json_t *args = json_object();
    json_set(args, "api_key", json_string(key));
    json_set(args, "base_url", json_string(base));
    json_t *live = mcat_call(ctx, "fetch_api_models", args);
    json_free(creds);
    if (mcat_exc_pending(ctx)) {
        mcat_exc_clear(ctx);
        json_free(live);
        return NULL;
    }
    if (!mcat_node_truthy(live)) { /* fetch_api_models(...) or None */
        json_free(live);
        return NULL;
    }
    return live;
}

/* PoP: mcat_stepfun_catalog @ hermes_cli/models.py:_stepfun_catalog */
json_t *mcat_stepfun_catalog(mcat_ctx *ctx, const char *normalized, bool force_refresh) {
    json_t *live = mcat_api_key_provider_live(ctx, normalized, force_refresh);
    if (!mcat_node_truthy(live)) return live; /* None OR [] returned verbatim */
    json_t *curated = mcat_static_catalog(ctx, normalized);
    if (mcat_exc_pending(ctx)) {
        json_free(live);
        json_free(curated);
        return NULL;
    }
    if (!mcat_node_truthy(curated)) {
        json_free(curated);
        return live;
    }
    json_t *merged = mcat_merge_unique(ctx, live, curated, true);
    json_free(live);
    json_free(curated);
    return merged;
}

/* PoP: mcat_profile_live_catalog @ hermes_cli/models.py:_profile_live_catalog */
json_t *mcat_profile_live_catalog(mcat_ctx *ctx, const char *normalized) {
    json_t *args = json_object();
    json_set(args, "provider", json_string(normalized ? normalized : ""));
    json_t *profile = mcat_call(ctx, "get_provider_profile", args);
    if (mcat_exc_pending(ctx)) {
        json_free(profile);
        return NULL;
    }
    if (!mcat_node_truthy(profile)) {
        json_free(profile);
        return NULL;
    }
    json_t *at = mcat_attr(ctx, profile, "auth_type");
    if (mcat_exc_pending(ctx)) {
        json_free(profile);
        return NULL;
    }
    const char *ats = (at && at->type == JSON_STRING) ? at->str_val : "";
    if (mcat_str_eq(ats, "external_process")) {
        json_t *live = mcat_call(ctx, "fetch_models", json_object());
        if (mcat_exc_pending(ctx)) { /* logger.debug only; degrade to curated */
            mcat_exc_clear(ctx);
            json_free(live);
            live = NULL;
        }
        json_t *arg = mcat_node_truthy(live) ? mcat_list_copy(live) : NULL;
        json_free(live);
        json_t *out = mcat_merge_profile_catalog(ctx, normalized, profile, arg);
        json_free(arg);
        json_free(profile);
        return out;
    }
    bool api_path = false;
    if (mcat_str_eq(ats, "api_key")) {
        json_t *bu = mcat_attr(ctx, profile, "base_url"); /* short-circuit: only for api_key */
        if (mcat_exc_pending(ctx)) {
            json_free(profile);
            return NULL;
        }
        api_path = mcat_node_truthy(bu);
    }
    if (!api_path) {
        json_t *fb = mcat_attr(ctx, profile, "fallback_models");
        if (mcat_exc_pending(ctx)) {
            json_free(profile);
            return NULL;
        }
        json_t *out = mcat_node_truthy(fb) ? mcat_list_copy(fb) : NULL; /* list(...) or None */
        json_free(profile);
        return out;
    }
    json_t *creds = mcat_api_key_credentials(ctx, normalized);
    const char *key = creds->c.items[0]->str_val;
    const char *cbase = creds->c.items[1]->str_val;
    json_t *bu = json_obj_get(profile, "base_url");
    const char *eff = NULL;
    if (cbase[0]) eff = cbase;
    else if (bu && bu->type == JSON_STRING && bu->str_val[0]) eff = bu->str_val;
    json_t *out = mcat_probe_profile_catalog(ctx, normalized, profile, key, eff);
    json_free(creds);
    json_free(profile);
    return out;
}

/* PoP: mcat_probe_profile_catalog @ hermes_cli/models.py:probe_profile_catalog */
json_t *mcat_probe_profile_catalog(mcat_ctx *ctx, const char *normalized, const json_t *profile,
                                   const char *api_key, const char *base_url) {
    json_t *live = NULL;
    if (api_key && api_key[0]) { /* no key -> no doomed probe */
        json_t *args = json_object();
        json_set(args, "api_key", json_string(api_key));
        if (base_url) json_set(args, "base_url", json_string(base_url));
        live = mcat_call(ctx, "fetch_models", args);
        if (mcat_exc_pending(ctx)) {
            mcat_exc_clear(ctx);
            json_free(live);
            live = NULL;
        }
    }
    json_t *out = mcat_merge_profile_catalog(ctx, normalized, profile, live);
    json_free(live);
    return out;
}

/* PoP: mcat_merge_profile_catalog @ hermes_cli/models.py:merge_profile_catalog */
json_t *mcat_merge_profile_catalog(mcat_ctx *ctx, const char *normalized, const json_t *profile,
                                   const json_t *live) {
    json_t *rows = NULL;
    if (!mcat_node_truthy(live)) {
        json_t *fb = mcat_attr(ctx, (json_t *)profile, "fallback_models");
        if (mcat_exc_pending(ctx)) return NULL;
        rows = mcat_node_truthy(fb) ? mcat_curated(fb) : NULL;
    } else {
        json_t *sc = mcat_static_catalog(ctx, normalized);
        if (mcat_exc_pending(ctx)) {
            json_free(sc);
            return NULL;
        }
        json_t *curated;
        if (mcat_node_truthy(sc)) {
            curated = sc;
        } else {
            json_free(sc);
            json_t *fb = mcat_attr(ctx, (json_t *)profile, "fallback_models");
            if (mcat_exc_pending(ctx)) return NULL;
            curated = mcat_node_truthy(fb) ? mcat_list_copy(fb) : json_array();
        }
        if (!mcat_node_truthy(curated)) {
            rows = mcat_is_curated(live) ? mcat_curated(live) : mcat_list_copy(live); /* rows = live keeps type */
            json_free(curated);
        } else {
            bool live_first = mcat_arr_has_str(mcat_env_arr(ctx, "live_first"), normalized);
            const json_t *primary = live_first ? live : curated;
            const json_t *secondary = live_first ? curated : live;
            rows = mcat_merge_unique(ctx, primary, secondary, true);
            json_free(curated);
        }
    }
    return mcat_drop_delisted_opencode_models(ctx, normalized, rows);
}

/* PoP: mcat_drop_delisted_opencode_models @ hermes_cli/models.py:_drop_delisted_opencode_models */
json_t *mcat_drop_delisted_opencode_models(mcat_ctx *ctx, const char *normalized, json_t *rows) {
    if (mcat_node_truthy(rows) && mcat_arr_has_str(mcat_env_arr(ctx, "live_first"), normalized)) {
        json_t *items = mcat_items(rows);
        json_t *excluded = mcat_env_arr(ctx, "opencode_excluded");
        json_t *filtered = json_array();
        for (size_t i = 0; items && i < items->c.count; i++) {
            json_t *m = items->c.items[i];
            const char *ms = (m && m->type == JSON_STRING) ? m->str_val : "";
            char *low = mcat_lower(ms);
            if (!mcat_arr_has_str(excluded, low)) json_append(filtered, json_copy(m));
            free(low);
        }
        json_t *out;
        if (mcat_is_curated(rows)) { /* type(rows)(...) keeps the list subclass */
            out = json_object();
            json_set(out, "$curated", filtered);
        } else {
            out = filtered;
        }
        json_free(rows);
        return out;
    }
    return rows;
}

/* PoP: mcat_configured_relay_base_url @ hermes_cli/models.py:_configured_relay_base_url */
json_t *mcat_configured_relay_base_url(mcat_ctx *ctx, const char *provider) {
    const char *prov = provider ? provider : "";
    json_t *model_cfg = mcat_call(ctx, "get_model_config_dict", json_object());
    if (mcat_exc_pending(ctx)) {
        mcat_exc_clear(ctx);
        json_free(model_cfg);
        return json_string("");
    }
    if (!model_cfg || model_cfg->type != JSON_OBJECT) {
        /* model_cfg.get sits OUTSIDE the try: AttributeError propagates */
        mcat_raise(ctx, "AttributeError");
        json_free(model_cfg);
        return NULL;
    }
    char *rawp = mcat_str_or_empty(json_obj_get(model_cfg, "provider"));
    char *cfg_provider = mcat_strip_lower(rawp);
    free(rawp);
    if (!cfg_provider[0] || !prov[0]) {
        free(cfg_provider);
        json_free(model_cfg);
        return json_string("");
    }
    char *normalized = mcat_normalize_provider(ctx, prov);
    char *norm_cfg = mcat_normalize_provider(ctx, cfg_provider);
    free(cfg_provider);
    if (!mcat_str_eq(normalized, norm_cfg)) {
        free(normalized);
        free(norm_cfg);
        json_free(model_cfg);
        return json_string("");
    }
    free(norm_cfg);
    /* one config snapshot serves both fields (live reads model_cfg once) */
    char *braw = mcat_str_or_empty(json_obj_get(model_cfg, "base_url"));
    char *bstrip = mcat_strip(braw);
    free(braw);
    char *base_url = mcat_rstrip_slash(bstrip);
    free(bstrip);
    json_free(model_cfg);
    if (!base_url[0]) {
        free(normalized);
        free(base_url);
        return json_string("");
    }
    char *result = NULL;
    char *canon[2] = { NULL, NULL };
    char **canonical = NULL;
    size_t nc = 0;
    json_t *args = json_object();
    json_set(args, "provider", json_string(normalized));
    json_t *profile = mcat_call(ctx, "get_provider_profile", args);
    if (mcat_exc_pending(ctx)) {
        mcat_exc_clear(ctx);
        json_free(profile);
        result = base_url; /* lookup failed: stay a relay */
        goto done;
    }
    args = json_object();
    json_set(args, "provider", json_string(normalized));
    json_t *registry = mcat_call(ctx, "registry_entry", args);
    if (mcat_exc_pending(ctx)) {
        mcat_exc_clear(ctx);
        json_free(registry);
        json_free(profile);
        result = base_url;
        goto done;
    }
    canon[0] = mcat_strdup(mcat_attr_def_str(profile, "base_url", ""));
    canon[1] = mcat_strdup(mcat_attr_def_str(registry, "inference_base_url", ""));
    json_free(profile);
    json_free(registry);
    for (int i = 0; i < 2; i++) { /* set comprehension order: profile, registry */
        if (!canon[i][0]) continue;
        args = json_object();
        json_set(args, "base_url", json_string(canon[i]));
        json_t *n = mcat_call(ctx, "normalize_route_base_url", args);
        if (mcat_exc_pending(ctx)) {
            mcat_exc_clear(ctx);
            json_free(n);
            result = base_url;
            goto done;
        }
        if (n && n->type == JSON_STRING) {
            canonical = (char **)realloc(canonical, (nc + 1) * sizeof(char *));
            canonical[nc++] = mcat_strdup(n->str_val);
        }
        json_free(n);
    }
    args = json_object();
    json_set(args, "base_url", json_string(base_url));
    json_t *nbase = mcat_call(ctx, "normalize_route_base_url", args);
    if (mcat_exc_pending(ctx)) {
        mcat_exc_clear(ctx);
        json_free(nbase);
        result = base_url;
        goto done;
    }
    bool in_canonical = false;
    if (nbase && nbase->type == JSON_STRING) {
        for (size_t i = 0; i < nc; i++) if (strcmp(canonical[i], nbase->str_val) == 0) { in_canonical = true; break; }
    }
    json_free(nbase);
    result = in_canonical ? mcat_strdup("") : mcat_strdup(base_url);
done:
    free(canon[0]);
    free(canon[1]);
    for (size_t i = 0; i < nc; i++) free(canonical[i]);
    free(canonical);
    free(normalized);
    if (result != base_url) free(base_url);
    json_t *out = json_string(result ? result : "");
    free(result);
    return out;
}

/* PoP: mcat_relay_model_catalog @ hermes_cli/models.py:_relay_model_catalog */
json_t *mcat_relay_model_catalog(mcat_ctx *ctx, const char *normalized, const char *relay) {
    const char *rl = relay ? relay : "";
    json_t *args = json_object();
    json_set(args, "provider", json_string(normalized ? normalized : ""));
    json_t *profile = mcat_call(ctx, "get_provider_profile", args);
    if (mcat_exc_pending(ctx)) {
        mcat_exc_clear(ctx);
        json_free(profile);
        return NULL;
    }
    if (!profile) return NULL;
    const char *at = mcat_attr_def_str(profile, "auth_type", "");
    if (!mcat_str_eq(at, "api_key")) {
        json_free(profile);
        return NULL;
    }
    json_t *creds = mcat_api_key_credentials(ctx, normalized);
    const char *key = creds->c.items[0]->str_val;
    const char *cbase = creds->c.items[1]->str_val;
    args = json_object();
    json_set(args, "provider", json_string(normalized ? normalized : ""));
    json_set(args, "relay", json_string(rl));
    json_set(args, "api_key", json_string(key));
    json_set(args, "base_url", json_string(cbase));
    json_t *ak = mcat_call(ctx, "relay_probe_api_key", args);
    json_free(creds);
    if (mcat_exc_pending(ctx)) {
        mcat_exc_clear(ctx);
        json_free(ak);
        json_free(profile);
        return NULL;
    }
    args = json_object();
    if (ak && ak->type == JSON_STRING) json_set(args, "api_key", json_copy(ak));
    json_set(args, "base_url", json_string(rl));
    json_free(ak);
    json_t *live = mcat_call(ctx, "fetch_models", args);
    json_free(profile);
    if (mcat_exc_pending(ctx)) {
        mcat_exc_clear(ctx);
        json_free(live);
        return NULL;
    }
    json_t *out = json_array();
    if (live && live->type == JSON_ARRAY) {
        for (size_t i = 0; i < live->c.count; i++) {
            json_t *m = live->c.items[i];
            if (mcat_node_truthy(m)) json_append(out, json_copy(m)); /* str(m) for m if m */
        }
    }
    json_free(live);
    if (out->c.count == 0) { /* [...] or None */
        json_free(out);
        return NULL;
    }
    return out;
}

/* PoP: mcat_disk_serve_tier @ hermes_cli/models.py:_disk_serve_tier */
json_t *mcat_disk_serve_tier(mcat_ctx *ctx, const json_t *entry, const char *fp, double now,
                             bool is_ollama, double ttl_seconds) {
    double ollama_ttl = mcat_env_num(ctx, "ollama_ttl", 300);
    double stale_max = mcat_env_num(ctx, "stale_max", 604800);
    double fallback_ttl = mcat_env_num(ctx, "fallback_ttl", 60);
    if (is_ollama && ollama_ttl < ttl_seconds) ttl_seconds = ollama_ttl;
    if (!mcat_cache_entry_valid(entry, fp ? fp : "", is_ollama)) return NULL;
    double age = now - json_get_num(entry, "at", 0);
    bool fallback = mcat_node_truthy(json_obj_get(entry, "fallback"));
    double limit = fallback ? fallback_ttl : ttl_seconds;
    if (age < limit) return json_string("fresh");
    json_t *models = json_obj_get(entry, "models");
    if (models && models->c.count > 0 && !fallback && age < stale_max) return json_string("stale");
    return NULL;
}
