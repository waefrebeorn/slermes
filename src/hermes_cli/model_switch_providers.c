/*
 * model_switch_providers.c - C11 port of upstream
 * hermes_cli/model_switch_providers.py (gates w28_modelswitc_6dc907_p1/p2/p3).
 *
 * PoP: w28ms_p1_catalog_stale @ hermes_cli/model_switch_providers.py:_discovered_catalog_stale
 * PoP: w28ms_p1_any_env @ hermes_cli/model_switch_providers.py:_any_env
 * PoP: w28ms_p1_skip @ hermes_cli/model_switch_providers.py:_skip
 * PoP: w28ms_p1_azure_entra_configured @ hermes_cli/model_switch_providers.py:_azure_entra_configured
 * PoP: w28ms_p1_fast_aws_signal @ hermes_cli/model_switch_providers.py:_has_fast_aws_sdk_signal
 * PoP: w28ms_p1_aws_creds_for_listing @ hermes_cli/model_switch_providers.py:_has_aws_sdk_creds_for_listing
 * PoP: w28ms_p1_is_aws_sdk @ hermes_cli/model_switch_providers.py:_is_aws_sdk
 * PoP: w28ms_p1_first_curated @ hermes_cli/model_switch_providers.py:_first_curated
 * PoP: w28ms_p1_free_tier_nous_row @ hermes_cli/model_switch_providers.py:_free_tier_nous_row
 * PoP: w28ms_p1_cap_models @ hermes_cli/model_switch_providers.py:_cap_models
 * PoP: w28ms_p1_split_chain_entries @ hermes_cli/model_switch_providers.py:_split_chain_entries
 * PoP: w28ms_p1_extend_unique @ hermes_cli/model_switch_providers.py:_extend_unique
 * PoP: w28ms_p1_norm_url @ hermes_cli/model_switch_providers.py:_norm_url
 * PoP: w28ms_p1_entry_base_url @ hermes_cli/model_switch_providers.py:_entry_base_url
 * PoP: w28ms_p1_entry_api_mode @ hermes_cli/model_switch_providers.py:_entry_api_mode
 * PoP: w28ms_p1_entry_credentials @ hermes_cli/model_switch_providers.py:_entry_credentials
 * PoP: w28ms_p1_discover_flag @ hermes_cli/model_switch_providers.py:_discover_flag
 * PoP: w28ms_p1_display_prefix @ hermes_cli/model_switch_providers.py:_display_prefix
 * PoP: w28ms_p1_group_display_name @ hermes_cli/model_switch_providers.py:_group_display_name
 * PoP: w28ms_p2_discover_models @ hermes_cli/model_switch_providers.py:_discover_endpoint_models
 * PoP: w28ms_p2_resolved_fast_probe @ hermes_cli/model_switch_providers.py:_PickerBuild.resolved_fast_custom_probe
 * PoP: w28ms_p2_can_probe_custom @ hermes_cli/model_switch_providers.py:_PickerBuild.can_probe_custom
 * PoP: w28ms_p2_endpoint_is_current @ hermes_cli/model_switch_providers.py:_PickerBuild.endpoint_is_current
 * PoP: w28ms_p2_record_section3_pair @ hermes_cli/model_switch_providers.py:_PickerBuild.record_section3_pair
 * PoP: w28ms_p2_add_builtin_row @ hermes_cli/model_switch_providers.py:_PickerBuild.add_builtin_row
 * PoP: w28ms_p2_add_endpoint_row @ hermes_cli/model_switch_providers.py:_PickerBuild.add_endpoint_row
 * PoP: w28ms_p2_discover_endpoint @ hermes_cli/model_switch_providers.py:_PickerBuild.discover_endpoint
 * PoP: w28ms_p2_lmstudio_row @ hermes_cli/model_switch_providers.py:_lap_lmstudio_row
 * PoP: w28ms_p3_finalize_rows @ hermes_cli/model_switch_providers.py:_finalize_picker_rows
 * PoP: w28ms_p3_prepend_moa @ hermes_cli/model_switch_providers.py:_prepend_moa_picker_provider
 * PoP: w28ms_p3_picker_filter @ hermes_cli/model_switch_providers.py:list_picker_providers
 * PoP: w28ms_p3_local_runtime_rows @ hermes_cli/model_switch_providers.py:_with_local_runtime_row
 * PoP: w28ms_p3_attach_window_notes @ hermes_cli/model_switch_providers.py:_attach_window_model_notes
 *
 * Seams (kept in the caller, never started here): auth.json / credential pool
 * reads, models.dev + PROVIDER_REGISTRY lookups, anon_auth guest state,
 * cached_provider_model_ids / /v1/models fetches, config load+save, inventory
 * row injection, the openrouter catalog fetch, plan-limit pricing and account
 * discovery.  Each one enters this file only as the value it would return.
 *
 * Python value semantics are emulated: absent == null == None, True == 1,
 * 1 == 1.0, empty string/list/dict are falsy.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "hermes_json.h"
#include "noc_w28_modelswitc_6dc907_p1.h"
#include "noc_w28_modelswitc_6dc907_p2.h"
#include "noc_w28_modelswitc_6dc907_p3.h"

/* ------------------------------------------------------------------ */
/* Python value semantics                                              */
/* ------------------------------------------------------------------ */

static char *ms_dup(const char *s)
{
    size_t n;
    char *r;
    if (!s) s = "";
    n = strlen(s);
    r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n + 1);
    return r;
}

static const char *ms_s(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

static int ms_truthy(const json_t *v)
{
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static int ms_num(const json_t *v, double *out)
{
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python str() over the scalar shapes a value can hold. */
static char *ms_pystr(const json_t *v)
{
    char buf[64];
    if (!v) return ms_dup("");
    switch (v->type) {
    case JSON_NULL: return ms_dup("");
    case JSON_BOOL: return ms_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return ms_dup(buf);
    case JSON_STRING: return ms_dup(v->str_val ? v->str_val : "");
    default: {
        char *d = json_dumps(v, 0);
        char *r = d ? ms_dup(d) : ms_dup("");
        free(d);
        return r;
    }
    }
}

/* Python str(x or "") */
static char *ms_pystr_or_empty(const json_t *v)
{
    return ms_truthy(v) ? ms_pystr(v) : ms_dup("");
}

static char *ms_trim_dup(const char *s)
{
    const char *a;
    size_t n;
    char *r;
    if (!s) return ms_dup("");
    a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    n = strlen(a);
    while (n > 0 && isspace((unsigned char)a[n - 1])) n--;
    r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, a, n);
    r[n] = '\0';
    return r;
}

static char *ms_lower_dup(const char *s)
{
    char *r = ms_dup(s);
    size_t i;
    if (!r) return NULL;
    for (i = 0; r[i]; i++) r[i] = (char)tolower((unsigned char)r[i]);
    return r;
}

static char *ms_rstrip_slash_dup(const char *s)
{
    char *r = ms_dup(s);
    size_t n;
    if (!r) return NULL;
    n = strlen(r);
    while (n > 0 && r[n - 1] == '/') r[--n] = '\0';
    return r;
}

/* Python '==' over JSON values (None == None, True == 1 == 1.0). */
static int ms_equal(const json_t *a, const json_t *b)
{
    double x = 0.0, y = 0.0;
    size_t i, n;

    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;

    if (a->type == JSON_BOOL || a->type == JSON_NUMBER) {
        if (!ms_num(b, &y)) return 0;
        ms_num(a, &x);
        return x == y;
    }
    if (a->type == JSON_STRING) {
        return b->type == JSON_STRING &&
               strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    }
    if (a->type == JSON_ARRAY) {
        n = json_len(a);
        if (b->type != JSON_ARRAY || n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!ms_equal(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT) {
        n = json_object_size(a);
        if (b->type != JSON_OBJECT || n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!ms_equal(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

/* Python `item in list`. */
static int ms_contains(const json_t *arr, const json_t *item)
{
    size_t i;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (i = 0; i < json_len(arr); i++)
        if (ms_equal(json_get(arr, i), item)) return 1;
    return 0;
}

static int ms_contains_str(const json_t *arr, const char *s)
{
    json_t *probe = json_string(s ? s : "");
    int hit = ms_contains(arr, probe);
    json_free(probe);
    return hit;
}

/* list(dict) == list of strings?  1 when equal. */
static int ms_keys_equal(const json_t *obj, const json_t *ids)
{
    size_t i, n = json_object_size(obj);
    if (!ids || ids->type != JSON_ARRAY || json_len(ids) != n) return 0;
    for (i = 0; i < n; i++) {
        json_t *elem = json_get(ids, i);
        const char *k = json_object_get_key_at(obj, i);
        if (!k || !elem || elem->type != JSON_STRING) return 0;
        if (strcmp(k, elem->str_val ? elem->str_val : "") != 0) return 0;
    }
    return 1;
}

/* Iteration like Python: array items, string characters, object keys. */
static size_t ms_iter_len(const json_t *v)
{
    if (!v) return 0;
    if (v->type == JSON_ARRAY) return json_len(v);
    if (v->type == JSON_OBJECT) return json_object_size(v);
    if (v->type == JSON_STRING) return v->str_val ? strlen(v->str_val) : 0;
    return 0;
}

/* Borrowed for arrays/objects, freshly allocated string for characters. */
static json_t *ms_iter_at(const json_t *v, size_t i, json_t **owned)
{
    *owned = NULL;
    if (!v) return NULL;
    if (v->type == JSON_ARRAY) return json_get(v, i);
    if (v->type == JSON_OBJECT) return json_string(json_object_get_key_at(v, i));
    if (v->type == JSON_STRING) {
        char buf[2];
        if (!v->str_val || i >= strlen(v->str_val)) return NULL;
        buf[0] = v->str_val[i];
        buf[1] = '\0';
        *owned = json_string(buf);
        return *owned;
    }
    return NULL;
}

/* Append a value that may be C-NULL as JSON null (json_set drops NULL vals). */
static void ms_put(json_t *obj, const char *key, json_t *val)
{
    json_set(obj, key, val ? val : json_null());
}

static const json_t *ms_field(const json_t *obj, const char *key)
{
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}

/* _UNCAPPED_PICKER_PROVIDERS */
static int ms_uncapped(const char *slug)
{
    static const char *const uncapped[] = {"opencode-zen", "opencode-go", "nous", "openrouter"};
    size_t i;
    if (!slug) return 0;
    for (i = 0; i < sizeof(uncapped) / sizeof(uncapped[0]); i++)
        if (strcmp(slug, uncapped[i]) == 0) return 1;
    return 0;
}

/* _cap_models(model_ids, max_models, slug), as a fresh array. */
static json_t *ms_cap_models(const json_t *model_ids, const json_t *max_models, const char *slug)
{
    json_t *out = json_array();
    size_t len = (model_ids && model_ids->type == JSON_ARRAY) ? json_len(model_ids) : 0;
    size_t keep, i;
    double d;

    if (!max_models || max_models->type == JSON_NULL || !ms_num(max_models, &d) ||
        (ms_truthy(max_models) && slug && ms_uncapped(slug))) {
        for (i = 0; i < len; i++) json_append(out, json_copy(json_get(model_ids, i)));
        return out;
    }
    {
        long long n = (long long)d;
        if (n < 0) {
            long long k = (long long)len + n;
            keep = k > 0 ? (size_t)k : 0;
        } else {
            keep = (n < (long long)len) ? (size_t)n : len;
        }
    }
    for (i = 0; i < keep; i++) json_append(out, json_copy(json_get(model_ids, i)));
    return out;
}

/* _extend_unique(target, items), in place. */
static void ms_extend_unique(json_t *target, const json_t *items)
{
    size_t i;
    for (i = 0; i < ms_iter_len(items); i++) {
        json_t *owned = NULL;
        const json_t *it = ms_iter_at(items, i, &owned);
        if (it && ms_truthy(it) && !ms_contains(target, it))
            json_append(target, json_copy(it));
        if (owned) json_free(owned);
    }
}

/* Stable sort over copies of an array; cmp(a,b) > 0 means a sorts after b. */
typedef int (*ms_cmp_fn)(const json_t *a, const json_t *b);

static json_t *ms_sorted_copy(const json_t *arr, ms_cmp_fn cmp)
{
    size_t n = json_len(arr), i, j;
    json_t **items = n ? (json_t **)malloc(n * sizeof(*items)) : NULL;
    json_t *out = json_array();

    if (!items) {
        for (i = 0; i < n; i++) json_append(out, json_copy(json_get(arr, i)));
        return out;
    }
    for (i = 0; i < n; i++) items[i] = json_copy(json_get(arr, i));
    for (i = 1; i < n; i++) {
        json_t *key = items[i];
        j = i;
        while (j > 0 && cmp(items[j - 1], key) > 0) {
            items[j] = items[j - 1];
            j--;
        }
        items[j] = key;
    }
    for (i = 0; i < n; i++) json_append(out, items[i]);
    free(items);
    return out;
}

/* (not is_current, -total_models) */
static int ms_cmp_finalize(const json_t *a, const json_t *b)
{
    int k1a = !ms_truthy(ms_field(a, "is_current"));
    int k1b = !ms_truthy(ms_field(b, "is_current"));
    double ta = 0.0, tb = 0.0;
    ms_num(ms_field(a, "total_models"), &ta);
    ms_num(ms_field(b, "total_models"), &tb);
    if (k1a != k1b) return k1a - k1b;
    if (-ta < -tb) return -1;
    if (-ta > -tb) return 1;
    return 0;
}

/* not is_current */
static int ms_cmp_current_first(const json_t *a, const json_t *b)
{
    int k1a = !ms_truthy(ms_field(a, "is_current"));
    int k1b = !ms_truthy(ms_field(b, "is_current"));
    return k1a - k1b;
}

/* ------------------------------------------------------------------ */
/* Part 1                                                              */
/* ------------------------------------------------------------------ */

json_t *w28ms_p1_catalog_stale(const json_t *entry, const json_t *model_ids)
{
    const json_t *existing = ms_field(entry, "models");
    const json_t *flag = ms_field(entry, "models_discovered");
    int legacy = 0, entry_disc;

    if (existing && existing->type == JSON_OBJECT) {
        const json_t *sent = json_obj_get(existing, "__discovered_model_catalog__");
        legacy = (sent && sent->type == JSON_BOOL && sent->bool_val) ? 1 : 0;
    }
    entry_disc = ((flag && flag->type == JSON_BOOL && flag->bool_val) ? 1 : 0) || legacy;

    if (existing && existing->type == JSON_OBJECT)
        return json_bool(entry_disc && (legacy || !ms_keys_equal(existing, model_ids)));
    if (existing && existing->type == JSON_ARRAY) {
        int any_dict = 0;
        size_t i;
        for (i = 0; i < json_len(existing); i++) {
            const json_t *m = json_get(existing, i);
            if (m && m->type == JSON_OBJECT) { any_dict = 1; break; }
        }
        return json_bool(!any_dict && !ms_equal(existing, model_ids));
    }
    return json_bool(1);
}

json_t *w28ms_p1_any_env(const json_t *env_vars, const json_t *env)
{
    size_t i, n = ms_iter_len(env_vars);
    for (i = 0; i < n; i++) {
        json_t *owned = NULL;
        const json_t *k = ms_iter_at(env_vars, i, &owned);
        char *name = ms_pystr(k);
        int hit = ms_truthy(ms_field(env, name));
        free(name);
        if (owned) json_free(owned);
        if (hit) return json_bool(1);
    }
    return json_bool(0);
}

json_t *w28ms_p1_skip(const json_t *seen, const json_t *excluded, const json_t *keys)
{
    size_t i, n = ms_iter_len(keys);
    for (i = 0; i < n; i++) {
        json_t *owned = NULL;
        const json_t *k = ms_iter_at(keys, i, &owned);
        char *lowered = ms_lower_dup(ms_s(k));
        int hit = ms_contains_str(seen, lowered) || ms_contains_str(excluded, lowered);
        free(lowered);
        if (owned) json_free(owned);
        if (hit) return json_bool(1);
    }
    return json_bool(0);
}

json_t *w28ms_p1_azure_entra_configured(const json_t *model_cfg, const json_t *env)
{
    char *prov = ms_lower_dup(ms_trim_dup(ms_pystr_or_empty(ms_field(model_cfg, "provider"))));
    char *auth = ms_lower_dup(ms_trim_dup(ms_pystr_or_empty(ms_field(model_cfg, "auth_mode"))));
    char *base = ms_trim_dup(ms_pystr_or_empty(ms_field(model_cfg, "base_url")));
    int ok = 0;

    if (prov && auth && strcmp(prov, "azure-foundry") == 0 && strcmp(auth, "entra_id") == 0) {
        if (base && base[0]) ok = 1;
        else ok = ms_truthy(ms_field(env, "AZURE_FOUNDRY_BASE_URL"));
    }
    free(prov);
    free(auth);
    free(base);
    return json_bool(ok);
}

static int ms_env_set(const json_t *env, const char *name)
{
    const json_t *v = ms_field(env, name);
    char *s;
    int set;
    if (!v) return 0;
    s = ms_trim_dup(ms_pystr(v));
    set = s && s[0] != '\0';
    free(s);
    return set;
}

json_t *w28ms_p1_fast_aws_signal(const json_t *env)
{
    static const char *const any_of[] = {
        "AWS_PROFILE", "AWS_CONTAINER_CREDENTIALS_RELATIVE_URI",
        "AWS_CONTAINER_CREDENTIALS_FULL_URI", "AWS_WEB_IDENTITY_TOKEN_FILE",
    };
    size_t i;
    if (ms_env_set(env, "AWS_BEARER_TOKEN_BEDROCK")) return json_bool(1);
    if (ms_env_set(env, "AWS_ACCESS_KEY_ID") && ms_env_set(env, "AWS_SECRET_ACCESS_KEY"))
        return json_bool(1);
    for (i = 0; i < sizeof(any_of) / sizeof(any_of[0]); i++)
        if (ms_env_set(env, any_of[i])) return json_bool(1);
    return json_bool(0);
}

json_t *w28ms_p1_aws_creds_for_listing(const json_t *slug, const json_t *current_provider,
                                       const json_t *env, const json_t *sdk_creds)
{
    char *a, *b;
    int same;
    if (ms_truthy(w28ms_p1_fast_aws_signal(env))) return json_bool(1);
    a = ms_lower_dup(ms_trim_dup(ms_pystr_or_empty(slug)));
    b = ms_lower_dup(ms_trim_dup(ms_pystr_or_empty(current_provider)));
    same = a && b && strcmp(a, b) == 0;
    free(a);
    free(b);
    return json_bool(same && ms_truthy(sdk_creds));
}

json_t *w28ms_p1_is_aws_sdk(const json_t *pconfig)
{
    const json_t *auth;
    if (!ms_truthy(pconfig)) return json_bool(0);
    auth = ms_field(pconfig, "auth_type");
    return json_bool(auth && auth->type == JSON_STRING && auth->str_val &&
                     strcmp(auth->str_val, "aws_sdk") == 0);
}

json_t *w28ms_p1_first_curated(const json_t *curated, const json_t *keys)
{
    size_t i, n = ms_iter_len(keys);
    json_t *last = json_array();
    for (i = 0; i < n; i++) {
        json_t *owned = NULL;
        const json_t *k = ms_iter_at(keys, i, &owned);
        const json_t *v = ms_field(curated, ms_s(k));
        json_free(last);
        last = v ? json_copy(v) : json_array();
        if (owned) json_free(owned);
        if (ms_truthy(last)) break;
    }
    return last;
}

static json_t *ms_free_tier_row(const json_t *row, const json_t *flags)
{
    json_t *out, *models;
    if (!ms_truthy(ms_field(flags, "guest"))) return row ? json_copy(row) : json_null();
    if (!ms_truthy(ms_field(flags, "guest_allowed"))) return json_null();
    if (!ms_truthy(ms_field(flags, "free_inference"))) return row ? json_copy(row) : json_null();

    out = row ? json_copy(row) : json_object();
    ms_put(out, "name", json_string(ms_s(ms_field(flags, "label"))));
    models = json_array();
    json_append(models, json_string(ms_s(ms_field(flags, "guest_model"))));
    json_set(out, "models", models);
    json_set(out, "total_models", json_number(1));
    json_set(out, "free_tier_row", json_bool(1));
    return out;
}

json_t *w28ms_p1_free_tier_nous_row(const json_t *row, const json_t *flags)
{
    return ms_free_tier_row(row, flags);
}

json_t *w28ms_p1_cap_models(const json_t *model_ids, const json_t *max_models, const json_t *slug)
{
    char *s = ms_pystr_or_empty(slug);
    json_t *out = ms_cap_models(model_ids, max_models, s);
    free(s);
    return out;
}

json_t *w28ms_p1_split_chain_entries(const json_t *models)
{
    json_t *out = json_array();
    json_t *snapshot;
    size_t i, n = json_len(models);

    for (i = 0; i < n; i++) json_append(out, json_copy(json_get(models, i)));
    snapshot = json_copy(out);
    for (i = 0; i < json_len(snapshot); i++) {
        const json_t *m = json_get(snapshot, i);
        if (m && m->type == JSON_STRING && m->str_val && strchr(m->str_val, ',')) {
            json_t *parts = json_array();
            const char *p = m->str_val;
            while (1) {
                const char *comma = strchr(p, ',');
                size_t len = comma ? (size_t)(comma - p) : strlen(p);
                char *chunk = (char *)malloc(len + 1);
                char *trimmed;
                if (!chunk) break;
                memcpy(chunk, p, len);
                chunk[len] = '\0';
                trimmed = ms_trim_dup(chunk);
                json_append(parts, json_string(trimmed ? trimmed : ""));
                free(trimmed);
                free(chunk);
                if (!comma) break;
                p = comma + 1;
            }
            ms_extend_unique(out, parts);
            json_free(parts);
        }
    }
    json_free(snapshot);
    return out;
}

json_t *w28ms_p1_extend_unique(const json_t *target, const json_t *items)
{
    json_t *out = json_array();
    size_t i;
    for (i = 0; i < json_len(target); i++) json_append(out, json_copy(json_get(target, i)));
    ms_extend_unique(out, items);
    return out;
}

json_t *w28ms_p1_norm_url(const json_t *url)
{
    char *raw = ms_pystr_or_empty(url);
    char *trimmed = ms_trim_dup(raw);
    char *stripped = ms_rstrip_slash_dup(trimmed);
    char *lowered = ms_lower_dup(stripped);
    json_t *out = json_string(lowered ? lowered : "");
    free(raw);
    free(trimmed);
    free(stripped);
    free(lowered);
    return out;
}

static const char *const MS_BASE_URL_KEYS[] = {"base_url", "url", "api"};

json_t *w28ms_p1_entry_base_url(const json_t *entry, const json_t *keys)
{
    size_t i, n;
    if (keys && keys->type == JSON_ARRAY) {
        n = json_len(keys);
        for (i = 0; i < n; i++) {
            const json_t *v = ms_field(entry, ms_s(json_get(keys, i)));
            if (ms_truthy(v)) return json_copy(v);
        }
    } else {
        for (i = 0; i < 3; i++) {
            const json_t *v = ms_field(entry, MS_BASE_URL_KEYS[i]);
            if (ms_truthy(v)) return json_copy(v);
        }
    }
    return json_string("");
}

json_t *w28ms_p1_entry_api_mode(const json_t *entry)
{
    const json_t *v = ms_field(entry, "api_mode");
    char *raw, *trimmed, *lowered;
    json_t *out;

    if (!ms_truthy(v)) v = ms_field(entry, "transport");
    raw = ms_pystr_or_empty(v);
    trimmed = ms_trim_dup(raw);
    lowered = ms_lower_dup(trimmed);
    out = (lowered && lowered[0]) ? json_string(lowered) : json_null();
    free(raw);
    free(trimmed);
    free(lowered);
    return out;
}

json_t *w28ms_p1_entry_credentials(const json_t *entry, const json_t *key_env_keys,
                                   const json_t *key_cmd_identity)
{
    size_t i, n = ms_iter_len(key_env_keys);
    char *key_env = ms_dup("");
    json_t *out = json_object();

    for (i = 0; i < n; i++) {
        json_t *owned = NULL;
        const json_t *k = ms_iter_at(key_env_keys, i, &owned);
        const json_t *v = ms_field(entry, ms_s(k));
        int hit = ms_truthy(v);
        if (owned) json_free(owned);
        if (hit) {
            free(key_env);
            key_env = ms_trim_dup(ms_pystr(v));
            break;
        }
    }
    ms_put(out, "key_env", json_string(key_env ? key_env : ""));
    if (key_cmd_identity && key_cmd_identity->type != JSON_NULL) {
        ms_put(out, "identity", json_copy(key_cmd_identity));
        json_set(out, "source", json_bool(1));
    } else {
        char *inline_key = ms_trim_dup(ms_pystr_or_empty(ms_field(entry, "api_key")));
        char *identity;
        if (inline_key && inline_key[0]) {
            identity = ms_dup(inline_key);
        } else if (key_env && key_env[0]) {
            size_t need = strlen(key_env) + 5;
            identity = (char *)malloc(need);
            if (identity) snprintf(identity, need, "env:%s", key_env);
        } else {
            identity = ms_dup("");
        }
        ms_put(out, "identity", json_string(identity ? identity : ""));
        json_set(out, "source", json_bool(0));
        free(inline_key);
        free(identity);
    }
    free(key_env);
    return out;
}

json_t *w28ms_p1_discover_flag(const json_t *entry)
{
    const json_t *v = ms_field(entry, "discover_models");
    if (!v) return json_bool(1);
    if (v->type == JSON_STRING) {
        char *lowered = ms_lower_dup(v->str_val ? v->str_val : "");
        int truthy = !(lowered && (!strcmp(lowered, "false") || !strcmp(lowered, "no") ||
                                   !strcmp(lowered, "0")));
        free(lowered);
        return json_bool(truthy);
    }
    return json_copy(v);
}

json_t *w28ms_p1_display_prefix(const json_t *name)
{
    const char *s = ms_s(name);
    const char *sep;
    char *chunk, *trimmed;
    json_t *out;

    sep = strstr(s, "\xE2\x80\x94");           /* em dash "-" (U+2014) */
    if (!sep) sep = strstr(s, " - ");
    if (!sep) return json_string(s);

    chunk = (char *)malloc((size_t)(sep - s) + 1);
    if (!chunk) return json_string(s);
    memcpy(chunk, s, (size_t)(sep - s));
    chunk[sep - s] = '\0';
    trimmed = ms_trim_dup(chunk);
    out = json_string(trimmed ? trimmed : "");
    free(trimmed);
    free(chunk);
    return out;
}

json_t *w28ms_p1_group_display_name(const json_t *name)
{
    const char *raw = ms_s(name);
    json_t *prefix = w28ms_p1_display_prefix(name);
    const char *grp = ms_s(prefix);
    char *copy = ms_dup(grp);
    char *w[64];
    size_t nw = 0, i, k;
    long cut_at = -1;
    json_t *out;
    char *p = copy;

    while (p && *p && nw < 64) {
        char *start;
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        w[nw++] = start;
        if (*p) *p++ = '\0';
    }

    for (i = 0; i < nw; i++) {
        const char *t = w[i];
        size_t a = 0, b = strlen(t);
        int digit = 0;
        while (a < b && (t[a] == '.' || t[a] == ',' || t[a] == '(' || t[a] == ')')) a++;
        while (b > a && (t[b - 1] == '.' || t[b - 1] == ',' || t[b - 1] == '(' || t[b - 1] == ')'))
            b--;
        for (k = a; k < b; k++)
            if (t[k] >= '0' && t[k] <= '9') { digit = 1; break; }
        if (digit) { cut_at = (long)i; break; }
    }

    if (cut_at >= 2) {
        size_t need = 1;
        char *joined;
        for (i = 0; i < (size_t)cut_at; i++) need += strlen(w[i]) + 1;
        joined = (char *)malloc(need);
        if (joined) {
            joined[0] = '\0';
            for (i = 0; i < (size_t)cut_at; i++) {
                if (i) strcat(joined, " ");
                strcat(joined, w[i]);
            }
            out = json_string(joined[0] ? joined : raw);
            free(joined);
        } else {
            out = json_string(grp[0] ? grp : raw);
        }
    } else {
        out = json_string(grp[0] ? grp : raw);
    }
    json_free(prefix);
    free(copy);
    return out;
}

/* ------------------------------------------------------------------ */
/* Part 2                                                              */
/* ------------------------------------------------------------------ */

json_t *w28ms_p2_discover_models(const json_t *live_models, const json_t *opts)
{
    int native = ms_truthy(ms_field(opts, "native"));
    int has_explicit = ms_truthy(ms_field(opts, "has_explicit_models"));
    int probe_live = ms_truthy(ms_field(opts, "probe_live"));
    int discovery_allowed = ms_truthy(ms_field(opts, "discovery_allowed"));
    json_t *out = json_object();
    json_t *models = json_null();
    int empty = 0;
    int live_none = (!live_models || live_models->type == JSON_NULL);

    if (probe_live) {
        if (native && has_explicit) {
            ms_put(out, "models", json_null());
            json_set(out, "native_catalog_empty", json_bool(0));
            return out;
        }
        if (!live_none && (ms_truthy(live_models) || !has_explicit || native)) {
            models = json_copy(live_models);
            empty = native && !ms_truthy(live_models);
        }
    } else if (discovery_allowed) {
        if (has_explicit && native) {
            ms_put(out, "models", json_null());
            json_set(out, "native_catalog_empty", json_bool(0));
            return out;
        }
        if (!live_none && (ms_truthy(live_models) || native)) {
            models = json_copy(live_models);
            empty = native && !ms_truthy(live_models);
        }
    }
    ms_put(out, "models", models);
    json_set(out, "native_catalog_empty", json_bool(empty));
    return out;
}

json_t *w28ms_p2_resolved_fast_probe(const json_t *for_picker, const json_t *fast_custom_probe)
{
    if (!fast_custom_probe || fast_custom_probe->type == JSON_NULL)
        return for_picker ? json_copy(for_picker) : json_null();
    return json_copy(fast_custom_probe);
}

json_t *w28ms_p2_can_probe_custom(const json_t *probe_custom_providers,
                                  const json_t *probe_current_custom_provider,
                                  const json_t *row_is_current)
{
    return json_bool(ms_truthy(probe_custom_providers) ||
                     (ms_truthy(probe_current_custom_provider) && ms_truthy(row_is_current)));
}

json_t *w28ms_p2_endpoint_is_current(const json_t *slug, const json_t *aliases,
                                     const json_t *url_norm, const json_t *cfg)
{
    char *cpn = ms_lower_dup(ms_pystr_or_empty(ms_field(cfg, "current_provider")));
    char *base_raw = ms_lower_dup(ms_pystr_or_empty(ms_field(cfg, "current_base_url")));
    char *cbn = ms_rstrip_slash_dup(base_raw ? base_raw : "");
    char *slug_n = ms_lower_dup(ms_trim_dup(ms_pystr_or_empty(slug)));
    const json_t *match_ok = ms_field(cfg, "url_match_ok");
    int url_match_ok = (!match_ok || match_ok->type == JSON_NULL) ? 1 : ms_truthy(match_ok);
    int hit = 0;

    if (strcmp(slug_n ? slug_n : "", cpn ? cpn : "") == 0) hit = 1;
    else if (ms_contains_str(aliases, cpn ? cpn : "")) hit = 1;
    else if (strcmp(cpn ? cpn : "", "custom") == 0 && cbn && cbn[0] &&
             strcmp(ms_s(url_norm), cbn) == 0 && url_match_ok)
        hit = 1;

    free(cpn);
    free(base_raw);
    free(cbn);
    free(slug_n);
    return json_bool(hit);
}

json_t *w28ms_p2_record_section3_pair(const json_t *name, const json_t *url_norm)
{
    char *nm = ms_lower_dup(ms_trim_dup(ms_pystr(name)));
    json_t *out = json_object();
    int ok = nm && nm[0] && ms_truthy(url_norm);

    json_set(out, "added", json_bool(ok));
    if (ok) {
        json_t *pair = json_array();
        json_append(pair, json_string(nm));
        json_append(pair, json_copy(url_norm));
        json_set(out, "pair", pair);
    } else {
        ms_put(out, "pair", json_null());
    }
    free(nm);
    return out;
}

json_t *w28ms_p2_add_builtin_row(const json_t *req)
{
    const json_t *slug_v = ms_field(req, "slug");
    const json_t *model_ids = ms_field(req, "model_ids");
    const json_t *uncapped = ms_field(req, "uncapped_ok");
    char *slug = ms_pystr_or_empty(slug_v);
    char *slug_l = ms_lower_dup(slug);
    json_t *row, *out, *seen, *models;
    size_t len = (model_ids && model_ids->type == JSON_ARRAY) ? json_len(model_ids) : 0;

    models = ms_cap_models(model_ids, ms_field(req, "max_models"),
                           (!uncapped || uncapped->type == JSON_NULL || ms_truthy(uncapped))
                               ? slug : "");
    row = json_object();
    ms_put(row, "slug", json_copy(slug_v));
    ms_put(row, "name", ms_field(req, "name") ? json_copy(ms_field(req, "name")) : json_string(""));
    json_set(row, "is_current", json_bool(ms_truthy(ms_field(req, "is_current"))));
    json_set(row, "is_user_defined", json_bool(0));
    json_set(row, "models", models);
    json_set(row, "total_models", json_number((double)len));
    ms_put(row, "source", ms_field(req, "source") ? json_copy(ms_field(req, "source"))
                                                  : json_string(""));

    if (strcmp(slug, "nous") == 0) {
        json_t *tier = ms_free_tier_row(row, ms_field(req, "flags"));
        json_free(row);
        row = tier;
    }

    out = json_object();
    ms_put(out, "row", row); /* null when the free-tier guest is switched off */
    seen = json_array();
    json_append(seen, json_string(slug_l ? slug_l : ""));
    json_set(out, "seen", seen);
    free(slug);
    free(slug_l);
    return out;
}

json_t *w28ms_p2_add_endpoint_row(const json_t *req)
{
    const json_t *slug_v = ms_field(req, "slug");
    const json_t *models = ms_field(req, "models");
    const json_t *shown = ms_field(req, "shown");
    char *slug = ms_pystr_or_empty(slug_v);
    char *slug_l = ms_lower_dup(slug);
    json_t *row = json_object();
    json_t *out = json_object();
    json_t *seen = json_array();
    size_t len = (models && models->type == JSON_ARRAY) ? json_len(models) : 0;

    ms_put(row, "slug", json_copy(slug_v));
    ms_put(row, "name", ms_field(req, "name") ? json_copy(ms_field(req, "name")) : json_string(""));
    json_set(row, "is_current", json_bool(ms_truthy(ms_field(req, "is_current"))));
    json_set(row, "is_user_defined", json_bool(1));
    json_set(row, "models", (!shown || shown->type == JSON_NULL)
                                ? (models ? json_copy(models) : json_null())
                                : json_copy(shown));
    json_set(row, "total_models", json_number((double)len));
    ms_put(row, "source", ms_field(req, "source") ? json_copy(ms_field(req, "source"))
                                                  : json_string("user-config"));
    ms_put(row, "api_url", ms_field(req, "api_url") ? json_copy(ms_field(req, "api_url"))
                                                    : json_string(""));
    json_set(row, "native_catalog_empty",
             json_bool(ms_truthy(ms_field(req, "native_catalog_empty"))));

    json_set(out, "row", row);
    json_append(seen, json_string(slug_l ? slug_l : ""));
    json_set(out, "seen", seen);
    free(slug);
    free(slug_l);
    return out;
}

json_t *w28ms_p2_discover_endpoint(const json_t *req)
{
    int discovery_allowed = ms_truthy(ms_field(req, "discovery_allowed"));
    int has_api_key = ms_truthy(ms_field(req, "api_key"));
    int has_explicit = ms_truthy(ms_field(req, "has_explicit_models"));
    int pcp = ms_truthy(ms_field(req, "probe_custom_providers"));
    int pccp = ms_truthy(ms_field(req, "probe_current_custom_provider"));
    int is_current = ms_truthy(ms_field(req, "is_current"));
    json_t *out = json_object();

    json_set(out, "probe_live",
             json_bool(discovery_allowed && (has_api_key || !has_explicit) &&
                       (pcp || (pccp && is_current))));
    return out;
}

json_t *w28ms_p2_lmstudio_row(const json_t *req)
{
    const json_t *configured = ms_field(req, "configured");
    const json_t *declared = ms_field(req, "declared_models");
    const json_t *curated = ms_field(req, "curated");
    int configured_dict = (configured && configured->type == JSON_OBJECT);
    char *current = ms_lower_dup(ms_pystr_or_empty(ms_field(req, "current_provider")));
    int is_current = current && strcmp(current, "lmstudio") == 0;
    json_t *out = json_object();

    if (ms_contains_str(ms_field(req, "excluded"), "lmstudio") ||
        ms_contains_str(ms_field(req, "seen"), "lmstudio")) {
        ms_put(out, "row", json_null());
        free(current);
        return out;
    }
    if (configured_dict) {
        json_t *base = w28ms_p1_entry_base_url(configured, NULL);
        int has_base = ms_truthy(base);
        json_free(base);
        if (has_base) {
            ms_put(out, "row", json_null());
            free(current);
            return out;
        }
    }
    if (!(is_current || configured_dict)) {
        ms_put(out, "row", json_null());
        free(current);
        return out;
    }
    {
        json_t *merged = json_array();
        json_t *row;
        size_t i;
        for (i = 0; i < ms_iter_len(declared); i++) {
            json_t *owned = NULL;
            const json_t *it = ms_iter_at(declared, i, &owned);
            if (it && !ms_contains(merged, it)) json_append(merged, json_copy(it));
            if (owned) json_free(owned);
        }
        for (i = 0; i < ms_iter_len(curated); i++) {
            json_t *owned = NULL;
            const json_t *it = ms_iter_at(curated, i, &owned);
            if (it && !ms_contains(merged, it)) json_append(merged, json_copy(it));
            if (owned) json_free(owned);
        }
        row = json_object();
        json_set(row, "slug", json_string("lmstudio"));
        ms_put(row, "name", ms_field(req, "label") ? json_copy(ms_field(req, "label"))
                                                   : json_string("lmstudio"));
        json_set(row, "is_current", json_bool(is_current));
        json_set(row, "is_user_defined", json_bool(0));
        json_set(row, "models", ms_cap_models(merged, ms_field(req, "max_models"), "lmstudio"));
        json_set(row, "total_models", json_number((double)json_len(merged)));
        json_set(row, "source", json_string("hermes"));
        json_free(merged);
        json_set(out, "row", row);
    }
    free(current);
    return out;
}

/* ------------------------------------------------------------------ */
/* Part 3                                                              */
/* ------------------------------------------------------------------ */

/* config_providers.is_provider_enabled */
static int ms_provider_enabled(const json_t *cfg)
{
    const json_t *flag;
    if (!cfg || cfg->type != JSON_OBJECT) return 1;
    flag = json_obj_get(cfg, "enabled");
    if (!flag) return 1;
    if (flag->type == JSON_BOOL) return flag->bool_val ? 1 : 0;
    if (flag->type == JSON_STRING) {
        char *l = ms_lower_dup(ms_trim_dup(flag->str_val ? flag->str_val : ""));
        int enabled = !(l && (!strcmp(l, "false") || !strcmp(l, "0") ||
                              !strcmp(l, "no") || !strcmp(l, "off")));
        free(l);
        return enabled;
    }
    return ms_truthy(flag);
}

static int ms_is_disabled_name(const json_t *disabled, const json_t *value)
{
    char *l = ms_lower_dup(ms_trim_dup(ms_pystr_or_empty(value)));
    int hit = ms_contains_str(disabled, l ? l : "");
    free(l);
    return hit;
}

json_t *w28ms_p3_finalize_rows(const json_t *req)
{
    const json_t *results = ms_field(req, "results");
    const json_t *user_providers = ms_field(req, "user_providers");
    const json_t *requires = ms_field(req, "requires_discovery");
    char *current_model = ms_dup(ms_s(ms_field(req, "current_model")));
    json_t *disabled = json_array();
    json_t *kept = json_array();
    size_t i;
    int have_disabled = 0;

    if (user_providers && user_providers->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(user_providers); i++) {
            const char *k = json_object_get_key_at(user_providers, i);
            const json_t *cfg = json_object_get_at(user_providers, i);
            if (cfg && cfg->type == JSON_OBJECT && !ms_provider_enabled(cfg)) {
                char *name = ms_lower_dup(ms_trim_dup(k ? k : ""));
                json_append(disabled, json_string(name ? name : ""));
                free(name);
            }
        }
        have_disabled = json_len(disabled) > 0;
    }
    if (have_disabled) {
        for (i = 0; i < json_len(results); i++) {
            const json_t *r = json_get(results, i);
            if (ms_is_disabled_name(disabled, ms_field(r, "provider_id"))) continue;
            if (ms_is_disabled_name(disabled, ms_field(r, "slug"))) continue;
            json_append(kept, json_copy(r));
        }
    } else {
        for (i = 0; i < json_len(results); i++) json_append(kept, json_copy(json_get(results, i)));
    }

    if (current_model && current_model[0]) {
        for (i = 0; i < json_len(kept); i++) {
            json_t *row = json_get(kept, i);
            const json_t *models = ms_field(row, "models");
            size_t j;
            int requires_discovery = 0;

            if (!ms_truthy(ms_field(row, "is_current"))) continue;
            if (ms_truthy(ms_field(row, "native_catalog_empty"))) continue;
            if (!ms_truthy(models)) models = NULL;
            if (!ms_contains_str(models, current_model)) {
                for (j = 0; j < json_len(requires); j++) {
                    const json_t *pair = json_get(requires, j);
                    json_t *probe = json_string(current_model);
                    int hit = pair && pair->type == JSON_ARRAY && json_len(pair) >= 2 &&
                              ms_equal(json_get(pair, 0), ms_field(row, "slug")) &&
                              ms_equal(json_get(pair, 1), probe);
                    json_free(probe);
                    if (hit) { requires_discovery = 1; break; }
                }
                if (!requires_discovery) {
                    json_t *next = json_array();
                    const json_t *tm = ms_field(row, "total_models");
                    double total = 0.0;
                    json_append(next, json_string(current_model));
                    for (j = 0; j < json_len(models); j++)
                        json_append(next, json_copy(json_get(models, j)));
                    json_set(row, "models", next);
                    if (ms_num(tm, &total))
                        json_set(row, "total_models", json_number(total + 1.0));
                    else
                        json_set(row, "total_models", json_number((double)json_len(next)));
                }
            }
            break; /* only the first current row is touched */
        }
    }

    {
        json_t *sorted = ms_sorted_copy(kept, ms_cmp_finalize);
        json_free(kept);
        kept = sorted;
    }
    json_free(disabled);
    free(current_model);
    return kept;
}

json_t *w28ms_p3_prepend_moa(const json_t *req)
{
    const json_t *providers = ms_field(req, "providers");
    const json_t *moa_row = ms_field(req, "moa_row");
    json_t *out = json_array();
    size_t i;

    if (!moa_row || moa_row->type == JSON_NULL) {
        for (i = 0; i < json_len(providers); i++) json_append(out, json_copy(json_get(providers, i)));
        return out;
    }
    json_append(out, json_copy(moa_row));
    for (i = 0; i < json_len(providers); i++) {
        const json_t *p = json_get(providers, i);
        char *slug = ms_lower_dup(ms_pystr_or_empty(ms_field(p, "slug")));
        int is_moa = slug && strcmp(slug, "moa") == 0;
        free(slug);
        if (!is_moa) json_append(out, json_copy(p));
    }
    return out;
}

json_t *w28ms_p3_picker_filter(const json_t *req)
{
    const json_t *providers = ms_field(req, "providers");
    const json_t *live_ids = ms_field(req, "openrouter_live_ids");
    int openrouter_error = ms_truthy(ms_field(req, "openrouter_error"));
    const json_t *max_models = ms_field(req, "max_models");
    json_t *out = json_array();
    size_t i;

    for (i = 0; i < json_len(providers); i++) {
        const json_t *p = json_get(providers, i);
        char *slug = ms_lower_dup(ms_pystr_or_empty(ms_field(p, "slug")));
        json_t *row = json_copy(p);

        if (slug && strcmp(slug, "openrouter") == 0) {
            const json_t *source = openrouter_error ? ms_field(p, "models") : live_ids;
            size_t len = (source && source->type == JSON_ARRAY) ? json_len(source) : 0;
            json_set(row, "models", ms_cap_models(source, max_models, "openrouter"));
            json_set(row, "total_models", json_number((double)len));
        }
        free(slug);

        {
            int is_custom = ms_truthy(ms_field(row, "is_user_defined")) &&
                            ms_truthy(ms_field(row, "api_url"));
            if (ms_truthy(ms_field(row, "models")) || is_custom) json_append(out, row);
            else json_free(row);
        }
    }
    return out;
}

json_t *w28ms_p3_local_runtime_rows(const json_t *req)
{
    const json_t *rows = ms_field(req, "rows");
    const json_t *local_row = ms_field(req, "local_row");
    json_t *staged = json_array();
    json_t *out;
    size_t i, n = json_len(rows);

    for (i = 0; i < n; i++) json_append(staged, json_copy(json_get(rows, i)));
    if (local_row && local_row->type == JSON_OBJECT) {
        json_t *local = json_copy(local_row);
        char *slug = ms_pystr_or_empty(ms_field(local, "slug"));
        json_set(local, "models",
                 ms_cap_models(ms_field(local, "models"), ms_field(req, "max_models"), slug));
        free(slug);
        json_append(staged, local);
    }
    out = ms_sorted_copy(staged, ms_cmp_current_first);
    json_free(staged);
    return out;
}

json_t *w28ms_p3_attach_window_notes(const json_t *req)
{
    const json_t *providers = ms_field(req, "providers");
    const json_t *usage = ms_field(req, "usage");
    json_t *out = json_array();
    size_t i, n;
    json_t *nous = NULL;

    for (i = 0; i < json_len(providers); i++) json_append(out, json_copy(json_get(providers, i)));

    for (i = 0; i < json_len(out); i++) {
        char *slug = ms_lower_dup(ms_pystr_or_empty(ms_field(json_get(out, i), "slug")));
        int hit = slug && strcmp(slug, "nous") == 0;
        free(slug);
        if (hit) { nous = json_get(out, i); break; }
    }
    if (!nous || !usage || usage->type == JSON_NULL || usage->type != JSON_OBJECT) return out;

    n = json_len(ms_field(nous, "models"));
    if (n > 0) {
        const json_t *levels = ms_field(usage, "levels");
        const json_t *tags = ms_field(usage, "tags");
        const json_t *models = ms_field(nous, "models");
        json_t *labels = json_object();

        for (i = 0; i < n; i++) {
            char *m = ms_pystr(json_get(models, i));
            const json_t *lv = ms_field(levels, m);
            const json_t *label = ms_truthy(lv) ? ms_field(tags, ms_s(lv)) : NULL;
            if (ms_truthy(label)) json_set(labels, m, json_copy(label));
            free(m);
        }
        if (json_object_size(labels) > 0) json_set(nous, "model_labels", labels);
        else json_free(labels);
    }

    if (!ms_truthy(ms_field(usage, "fresh"))) return out;
    if (ms_truthy(ms_field(usage, "limit_line")))
        json_set(nous, "plan_line", json_copy(ms_field(usage, "limit_line")));
    return out;
}
