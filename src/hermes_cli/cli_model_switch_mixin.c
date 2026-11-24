/*
 * cli_model_switch_mixin.c - C11 port of upstream
 * hermes_cli/cli_model_switch_mixin.py (gates w28_climodelsw_0eb324_p1,
 * w28_climodelsw_0eb324_p2).
 *
 * PoP: w28_climodelsw_0eb324_p1_heal_provider @ hermes_cli/cli_model_switch_mixin.py:_heal_bare_custom_provider
 * PoP: w28_climodelsw_0eb324_p1_runtime_fields @ hermes_cli/cli_model_switch_mixin.py:_runtime_fields
 * PoP: w28_climodelsw_0eb324_p1_snapshot_runtime @ hermes_cli/cli_model_switch_mixin.py:CLIModelSwitchMixin._snapshot_model_runtime
 * PoP: w28_climodelsw_0eb324_p1_configured_route @ hermes_cli/cli_model_switch_mixin.py:_configured_default_route
 * PoP: w28_climodelsw_0eb324_p1_stored_session_route @ hermes_cli/cli_model_switch_mixin.py:stored_session_route
 * PoP: w28_climodelsw_0eb324_p1_usage_rows @ hermes_cli/cli_model_switch_mixin.py:_MODEL_USAGE_ROWS
 * PoP: w28_climodelsw_0eb324_p2_reasoning_rows @ hermes_cli/cli_model_switch_mixin.py:_picker_reasoning_rows
 * PoP: w28_climodelsw_0eb324_p2_offers_reasoning @ hermes_cli/cli_model_switch_mixin.py:_picker_offers_reasoning
 * PoP: w28_climodelsw_0eb324_p2_filter_entries @ hermes_cli/cli_model_switch_mixin.py:CLIModelSwitchMixin._filter_model_picker_entries
 * PoP: w28_climodelsw_0eb324_p2_viewport @ hermes_cli/cli_model_switch_mixin.py:CLIModelSwitchMixin._compute_model_picker_viewport
 * PoP: w28_climodelsw_0eb324_p2_initial_state @ hermes_cli/cli_model_switch_mixin.py:CLIModelSwitchMixin._open_model_picker<state>
 * PoP: w28_climodelsw_0eb324_p2_selection_step @ hermes_cli/cli_model_switch_mixin.py:CLIModelSwitchMixin._handle_model_picker_selection
 *
 * What is ported is the decision core of the /model switch mixin: which route
 * a stored session row resolves to, what a bare ``custom`` provider heals
 * into, the runtime snapshot a one-turn override restores from, and the
 * three-stage picker state machine.  Every store stays a seam and its answer
 * arrives in the request object (config.yaml model section, custom-provider
 * identity lookup, gateway runtime row reader, provider-profile wire
 * derivation, i18n catalog, provider inventory, the switch result).  No UI or
 * DB side effect is executed here: invalidate/close/commit are reported back
 * as the action the caller must take.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_climodelsw_0eb324_p1.h"
#include "noc_w28_climodelsw_0eb324_p2.h"

/* ---------------------------------------------------------------- helpers */

/* _RUNTIME_FIELDS, in declaration order. */
static const char *const NC_RUNTIME_FIELDS[W28_CLIMODELSW_0EB324_P1_RUNTIME_FIELDS] = {
    "model", "provider", "requested_provider", "_explicit_api_key",
    "_explicit_base_url", "api_key", "base_url", "api_mode", "reasoning_config",
};

/* _MODEL_USAGE_ROWS and the description keys they render. */
static const char *const NC_USAGE_FORMS[8] = {
    "/model <name>", "/model <name> --global", "/model <name> --once",
    "/model <name> --session", "/model <name> --provider <slug>",
    "/model <name> --reasoning <level>", "/model --provider <slug>",
    "/model --refresh",
};
static const char *const NC_USAGE_KEYS[8] = {
    "cli.model.usage_switch", "cli.model.usage_switch_global",
    "cli.model.usage_switch_once", "cli.model.usage_switch_session",
    "cli.model.usage_switch_provider_model", "cli.model.usage_switch_reasoning",
    "cli.model.usage_switch_provider", "cli.model.usage_refresh",
};

/* Python truthiness of a JSON value (a missing key is None). */
static int nc_truthy(const json_t *v) {
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

/* Python '==' over the shapes a provider/model/base_url can hold. */
static int nc_equal(const json_t *a, const json_t *b) {
    size_t i, n;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if ((a->type == JSON_NUMBER || a->type == JSON_BOOL) &&
        (b->type == JSON_NUMBER || b->type == JSON_BOOL)) {
        double x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type != b->type) return 0;
    if (a->type == JSON_ARRAY) {
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!nc_equal(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT) {
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !nc_equal(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

static int nc_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int nc_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }

/* Copy of a value, or an owned None for a missing key. */
static json_t *nc_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

/* A borrowed object member, or NULL when the container is not an object. */
static const json_t *nc_member(const json_t *obj, const char *key) {
    return nc_is_obj(obj) ? json_obj_get(obj, key) : NULL;
}

/* Python indexing into an array (negative counts from the end). */
static const json_t *nc_index(const json_t *arr, long idx) {
    size_t n;
    if (!nc_is_arr(arr)) return NULL;
    n = json_len(arr);
    if (idx < 0) idx += (long)n;
    if (idx < 0 || (size_t)idx >= n) return NULL;
    return json_get(arr, (size_t)idx);
}

/* ---- Python str()/repr() of a JSON value (the picker filters on it) ----- */

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} nc_sb;

static void nc_sb_init(nc_sb *b) {
    b->cap = 64;
    b->len = 0;
    b->buf = (char *)malloc(b->cap);
    if (b->buf) b->buf[0] = '\0';
}

static void nc_sb_add(nc_sb *b, const char *s) {
    size_t add;
    if (!b->buf || !s) return;
    add = strlen(s);
    if (b->len + add + 1 > b->cap) {
        char *nb;
        while (b->len + add + 1 > b->cap) b->cap *= 2;
        nb = (char *)realloc(b->buf, b->cap);
        if (!nb) return;
        b->buf = nb;
    }
    memcpy(b->buf + b->len, s, add + 1);
    b->len += add;
}

static void nc_sb_close(nc_sb *b) {
    if (!b->buf) return;
    if (b->len + 2 > b->cap) {
        char *nb = (char *)realloc(b->buf, b->len + 2);
        if (!nb) return;
        b->buf = nb;
        b->cap = b->len + 2;
    }
    b->buf[b->len] = '\0';
}

static void nc_repr(const json_t *v, nc_sb *b) {
    size_t i, n;
    char tmp[64];
    if (!v || v->type == JSON_NULL) { nc_sb_add(b, "None"); return; }
    switch (v->type) {
    case JSON_NULL:
        break;              /* handled above; keeps -Wswitch quiet */
    case JSON_BOOL:
        nc_sb_add(b, v->bool_val ? "True" : "False");
        return;
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(tmp, sizeof(tmp), "%.0f", v->num_val);
        else
            snprintf(tmp, sizeof(tmp), "%.15g", v->num_val);
        nc_sb_add(b, tmp);
        return;
    case JSON_STRING: {
        const char *s = v->str_val ? v->str_val : "";
        nc_sb_add(b, "'");
        for (; *s; s++) {
            if (*s == '\'' || *s == '\\') {
                char esc[3] = { '\\', *s, '\0' };
                nc_sb_add(b, esc);
            } else if (*s == '\n') {
                nc_sb_add(b, "\\n");
            } else {
                char one[2] = { *s, '\0' };
                nc_sb_add(b, one);
            }
        }
        nc_sb_add(b, "'");
        return;
    }
    case JSON_ARRAY:
        nc_sb_add(b, "[");
        n = json_len(v);
        for (i = 0; i < n; i++) {
            if (i) nc_sb_add(b, ", ");
            nc_repr(json_get(v, i), b);
        }
        nc_sb_add(b, "]");
        return;
    case JSON_OBJECT:
        nc_sb_add(b, "{");
        n = json_object_size(v);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(v, i);
            json_t *key = json_string(k ? k : "");
            if (i) nc_sb_add(b, ", ");
            nc_repr(key, b);
            nc_sb_add(b, ": ");
            nc_repr(json_object_get_at(v, i), b);
            json_free(key);
        }
        nc_sb_add(b, "}");
        return;
    }
}

/* str(x or ""): None/""/0/False/[]/{} all become "". */
static char *nc_str_or_empty(const json_t *v) {
    nc_sb b;
    if (!nc_truthy(v)) {
        char *s = (char *)malloc(1);
        if (s) s[0] = '\0';
        return s;
    }
    if (v->type == JSON_STRING) {
        size_t n = strlen(v->str_val ? v->str_val : "");
        char *s = (char *)malloc(n + 1);
        if (s) memcpy(s, v->str_val ? v->str_val : "", n + 1);
        return s;
    }
    nc_sb_init(&b);
    nc_repr(v, &b);
    nc_sb_close(&b);
    return b.buf;
}

static int nc_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str(x).strip().lower() */
static char *nc_strip_lower(const char *s) {
    size_t i, n;
    char *out;
    if (!s) s = "";
    while (*s && nc_space((unsigned char)*s)) s++;
    n = strlen(s);
    while (n > 0 && nc_space((unsigned char)s[n - 1])) n--;
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i] = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
    }
    out[n] = '\0';
    return out;
}

/* str(x).strip() (case preserved) */
static char *nc_strip(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    while (*s && nc_space((unsigned char)*s)) s++;
    n = strlen(s);
    while (n > 0 && nc_space((unsigned char)s[n - 1])) n--;
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* _heal_bare_custom_provider's decision core.  ``main_serves``,
 * ``canonical`` and ``empty`` are the config/identity-lookup seam answers. */
static json_t *nc_heal_provider(const json_t *provider, const json_t *base_url,
                                const json_t *model, int main_serves,
                                const json_t *canonical, const json_t *empty) {
    char *raw = nc_str_or_empty(provider);
    char *norm = nc_strip_lower(raw);
    (void)base_url;
    (void)model;
    if (!norm || !raw) { free(raw); free(norm); return json_null(); }
    if (strcmp(norm, "") != 0 && strcmp(norm, "custom") != 0) {
        free(raw);
        free(norm);
        return nc_copy_or_null(provider);   /* already a routable identity */
    }
    if (norm[0] == '\0') {                  /* an empty provider names none */
        free(raw);
        free(norm);
        if (main_serves) return json_null();
        return nc_copy_or_null(empty);
    }
    free(raw);                              /* bare "custom": a billing class */
    free(norm);
    if (!nc_truthy(canonical)) return json_null();
    return json_copy(canonical);
}

/* _picker_offers_reasoning over an inventory provider dict. */
static int nc_offers_reasoning(const json_t *provider_data, const json_t *model) {
    const json_t *caps = nc_member(provider_data, "capabilities");
    const json_t *entry = NULL;
    if (nc_is_obj(caps) && model && model->type == JSON_STRING)
        entry = json_obj_get(caps, model->str_val ? model->str_val : "");
    if (nc_is_obj(entry)) {
        const json_t *r = json_obj_get(entry, "reasoning");
        if (r && r->type == JSON_BOOL && !r->bool_val) return 0;  /* reasoning is False */
    }
    return 1;
}

/* ===================================================================== p1 */

json_t *w28_climodelsw_0eb324_p1_heal_provider(const json_t *req) {
    return nc_heal_provider(nc_member(req, "provider"), nc_member(req, "base_url"),
                            nc_member(req, "model"),
                            nc_truthy(nc_member(req, "main_slot_serves")),
                            nc_member(req, "canonical"), nc_member(req, "empty_identity"));
}

json_t *w28_climodelsw_0eb324_p1_runtime_fields(const json_t *req) {
    const json_t *state = nc_member(req, "state");
    json_t *out = json_object();
    size_t i;
    if (!out) return json_null();
    for (i = 0; i < W28_CLIMODELSW_0EB324_P1_RUNTIME_FIELDS; i++)
        json_set(out, NC_RUNTIME_FIELDS[i], nc_copy_or_null(nc_member(state, NC_RUNTIME_FIELDS[i])));
    return out;
}

json_t *w28_climodelsw_0eb324_p1_snapshot_runtime(const json_t *req) {
    const json_t *state = nc_member(req, "state");
    const json_t *agent = nc_member(req, "agent");
    json_t *out = w28_climodelsw_0eb324_p1_runtime_fields(req);
    if (!nc_is_obj(out)) return out;
    /* reasoning_config is deep-copied so a later in-place edit cannot alias it. */
    json_set(out, "reasoning_config", nc_copy_or_null(nc_member(state, "reasoning_config")));
    if (agent && agent->type != JSON_NULL)
        json_set(out, "agent_primary_runtime", nc_copy_or_null(nc_member(agent, "_primary_runtime")));
    else
        json_set(out, "agent_primary_runtime", json_null());
    return out;
}

json_t *w28_climodelsw_0eb324_p1_configured_route(const json_t *req) {
    const json_t *cfg = nc_member(req, "model_cfg");
    json_t *out = json_array();
    char *model = NULL, *provider = NULL;
    if (!out) return json_null();
    if (!nc_is_obj(cfg)) {
        /* a bare string names the model; anything else names nothing */
        model = nc_strip(cfg && cfg->type == JSON_STRING && cfg->str_val ? cfg->str_val : "");
    } else {
        const json_t *pv = nc_member(cfg, "provider");
        const json_t *mv = nc_member(cfg, "default");
        char *raw_provider = nc_str_or_empty(pv);
        char *raw_model;
        if (!nc_truthy(mv)) mv = nc_member(cfg, "model");
        if (!nc_truthy(mv)) mv = NULL;
        raw_model = nc_str_or_empty(mv);
        model = nc_strip(raw_model);
        provider = nc_strip(raw_provider);
        free(raw_model);
        free(raw_provider);
        if (provider) {
            char *low = nc_strip_lower(provider);
            if (low && strcmp(low, "auto") == 0) { free(provider); provider = NULL; }
            free(low);
        }
    }
    /* the (model, provider) pair _configured_default_route returns */
    json_append(out, json_string(model ? model : ""));
    json_append(out, json_string(provider ? provider : ""));
    free(model);
    free(provider);
    return out;
}

json_t *w28_climodelsw_0eb324_p1_stored_session_route(const json_t *req) {
    const json_t *resume = nc_member(req, "resume");
    const json_t *stored_model = nc_member(resume, "model");
    const json_t *cur_model = nc_member(req, "current_model");
    const json_t *cur_provider = nc_member(req, "current_provider");
    const json_t *derived = nc_member(req, "derived");
    json_t *base_url = NULL, *api_mode = NULL, *stored_provider = NULL, *provider = NULL;
    json_t *out;
    int provider_changed;

    if (!nc_truthy(stored_model)) return json_null();   /* nothing recorded */
    base_url = nc_truthy(nc_member(resume, "base_url")) ? json_copy(nc_member(resume, "base_url")) : NULL;
    api_mode = nc_truthy(nc_member(resume, "api_mode")) ? json_copy(nc_member(resume, "api_mode")) : NULL;
    stored_provider = nc_truthy(nc_member(resume, "provider"))
        ? json_copy(nc_member(resume, "provider")) : NULL;

    provider = nc_heal_provider(stored_provider, base_url, stored_model,
                                nc_truthy(nc_member(req, "main_slot_serves")),
                                nc_member(req, "canonical"), nc_member(req, "empty_identity"));
    if (!nc_equal(provider, stored_provider)) {
        /* A healed entry resolves its own current endpoint; a route that did
         * not heal is gone with its endpoint and wire. */
        json_free(base_url);
        base_url = NULL;
        if (!nc_truthy(provider) || !nc_truthy(stored_provider)) {
            json_free(api_mode);
            api_mode = NULL;   /* an empty provider's saved wire belonged elsewhere */
        }
    }
    provider_changed = nc_truthy(provider) && !nc_equal(provider, cur_provider);
    if (nc_equal(stored_model, cur_model) && !provider_changed) {
        json_free(provider); json_free(base_url); json_free(api_mode); json_free(stored_provider);
        return json_null();     /* already the current route */
    }
    if (nc_truthy(nc_member(req, "foreign"))) {
        json_free(base_url); base_url = NULL;
        json_free(api_mode); api_mode = NULL;
    }
    if (derived && derived->type != JSON_NULL) {
        /* the wire is re-derived from the FINAL model, not the row */
        json_free(api_mode);
        api_mode = json_copy(derived);
        json_free(base_url);
        base_url = nc_truthy(nc_member(req, "normalized_base_url"))
            ? json_copy(nc_member(req, "normalized_base_url")) : NULL;
    }
    out = json_array();
    json_append(out, nc_copy_or_null(stored_model));
    json_append(out, provider ? provider : json_null());
    json_append(out, base_url ? base_url : json_null());
    json_append(out, api_mode ? api_mode : json_null());
    json_append(out, json_bool(provider_changed ? true : false));
    /* json_append took ownership of provider/base_url/api_mode */
    json_free(stored_provider);
    return out;
}

json_t *w28_climodelsw_0eb324_p1_usage_rows(const json_t *req) {
    const json_t *descs = nc_member(req, "descriptions");
    json_t *out = json_array();
    size_t i;
    if (!out) return json_null();
    for (i = 0; i < sizeof(NC_USAGE_FORMS) / sizeof(NC_USAGE_FORMS[0]); i++) {
        const char *form = NC_USAGE_FORMS[i];
        const json_t *d = nc_member(descs, NC_USAGE_KEYS[i]);
        const char *desc = (d && d->type == JSON_STRING && d->str_val) ? d->str_val : NC_USAGE_KEYS[i];
        size_t fl = strlen(form);
        size_t pad = fl < W28_CLIMODELSW_0EB324_P1_USAGE_COL ? W28_CLIMODELSW_0EB324_P1_USAGE_COL - fl : 0;
        char *line = (char *)malloc(2 + fl + pad + 2 + strlen(desc));
        json_t *row = json_object();
        if (!line || !row) { free(line); json_free(row); break; }
        memcpy(line, "  ", 2);
        memcpy(line + 2, form, fl);
        memset(line + 2 + fl, ' ', pad);
        line[2 + fl + pad] = ' ';
        strcpy(line + 2 + fl + pad + 1, desc);
        json_set(row, "form", json_string(form));
        json_set(row, "desc", json_string(desc));
        json_set(row, "line", json_string(line));
        json_append(out, row);
        free(line);
    }
    return out;
}

/* ===================================================================== p2 */

json_t *w28_climodelsw_0eb324_p2_reasoning_rows(const json_t *req) {
    const json_t *efforts = nc_member(req, "efforts");
    const json_t *ln = nc_member(req, "label_none");
    const json_t *lk = nc_member(req, "label_keep");
    json_t *out = json_array();
    size_t i, n = nc_is_arr(efforts) ? json_len(efforts) : 0;
    json_t *none_row, *keep_row;
    if (!out) return json_null();
    for (i = 0; i < n; i++) {
        json_t *row = json_array();
        json_append(row, nc_copy_or_null(json_get(efforts, i)));
        json_append(row, nc_copy_or_null(json_get(efforts, i)));
        json_append(out, row);
    }
    none_row = json_array();
    json_append(none_row, json_string("none"));
    json_append(none_row, ln && ln->type == JSON_STRING ? json_copy(ln) : json_null());
    json_append(out, none_row);
    keep_row = json_array();
    json_append(keep_row, json_string(""));
    json_append(keep_row, lk && lk->type == JSON_STRING ? json_copy(lk) : json_null());
    json_append(out, keep_row);
    return out;
}

json_t *w28_climodelsw_0eb324_p2_offers_reasoning(const json_t *req) {
    return json_bool(nc_offers_reasoning(nc_member(req, "provider_data"),
                                         nc_member(req, "model")) ? true : false);
}

static int nc_subseq(const char *needle, const char *hay) {
    for (; *needle; needle++) {
        const char *p = strchr(hay, *needle);
        if (!p) return 0;
        hay = p + 1;
    }
    return 1;
}

json_t *w28_climodelsw_0eb324_p2_filter_entries(const json_t *req) {
    const json_t *entries = nc_member(req, "entries");
    const json_t *query = nc_member(req, "query");
    json_t *out = json_array();
    size_t i, n = nc_is_arr(entries) ? json_len(entries) : 0;
    char *q = NULL;
    if (!out) return json_null();
    if (query && query->type == JSON_STRING) q = nc_strip_lower(query->str_val);
    if (!q || q[0] == '\0') {   /* an empty query matches every row */
        for (i = 0; i < n; i++) {
            json_t *pair = json_array();
            json_append(pair, json_number((double)i));
            json_append(pair, nc_copy_or_null(json_get(entries, i)));
            json_append(out, pair);
        }
        free(q);
        return out;
    }
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(entries, i);
        char *raw = nc_str_or_empty(e);
        char *low = nc_strip_lower(raw);
        int hit = low && nc_subseq(q, low);
        free(raw);
        free(low);
        if (!hit) continue;
        {
            json_t *pair = json_array();
            json_append(pair, json_number((double)i));
            json_append(pair, nc_copy_or_null(e));
            json_append(out, pair);
        }
    }
    free(q);
    return out;
}

json_t *w28_climodelsw_0eb324_p2_viewport(const json_t *req) {
    double sel = json_get_num(req, "selected", 0.0);
    double off = json_get_num(req, "scroll_offset", 0.0);
    double n = json_get_num(req, "n", 0.0);
    double term = json_get_num(req, "term_rows", 0.0);
    double reserved = json_get_num(req, "reserved_below", W28_CLIMODELSW_0EB324_P2_VIEWPORT_RESERVED);
    double chrome = json_get_num(req, "panel_chrome", W28_CLIMODELSW_0EB324_P2_VIEWPORT_CHROME);
    double minvis = json_get_num(req, "min_visible", W28_CLIMODELSW_0EB324_P2_VIEWPORT_MIN_VISIBLE);
    double max_visible = term - reserved - chrome;
    json_t *out = json_array();
    if (max_visible < minvis) max_visible = minvis;
    if (!out) return json_null();
    if (n <= max_visible) {
        json_append(out, json_number(0.0));
        json_append(out, json_number(n));
        return out;
    }
    if (sel < off) off = sel;
    else if (sel >= off + max_visible) off = sel - max_visible + 1;
    if (off > n - max_visible) off = n - max_visible;
    if (off < 0) off = 0;
    json_append(out, json_number(off));
    json_append(out, json_number(max_visible));
    return out;
}

json_t *w28_climodelsw_0eb324_p2_initial_state(const json_t *req) {
    const json_t *providers = nc_member(req, "providers");
    json_t *out = json_object();
    double selected = 0.0;
    size_t i, n = nc_is_arr(providers) ? json_len(providers) : 0;
    if (!out) return json_null();
    for (i = 0; i < n; i++) {
        const json_t *p = json_get(providers, i);
        if (nc_truthy(nc_member(p, "is_current"))) { selected = (double)i; break; }
    }
    json_set(out, "stage", json_string("provider"));
    json_set(out, "providers", nc_is_arr(providers) ? json_copy(providers) : json_array());
    json_set(out, "selected", json_number(selected));
    json_set(out, "current_model", nc_copy_or_null(nc_member(req, "current_model")));
    json_set(out, "current_provider", nc_copy_or_null(nc_member(req, "current_provider")));
    json_set(out, "user_provs", nc_copy_or_null(nc_member(req, "user_provs")));
    json_set(out, "custom_provs", nc_copy_or_null(nc_member(req, "custom_provs")));
    json_set(out, "filter", json_string(""));
    return out;
}

/* The visible labels of the model stage: the fuzzy filter's pairs, else every
 * row of the list (pairs carry the ORIGINAL index). */
static json_t *nc_visible_labels(const json_t *state, const json_t *model_list) {
    const json_t *pairs = nc_member(state, "_filtered_pairs");
    json_t *out = json_array();
    size_t i, n;
    if (!out) return NULL;
    if (nc_is_arr(pairs)) {
        n = json_len(pairs);
        for (i = 0; i < n; i++) {
            const json_t *pair = json_get(pairs, i);
            if (nc_is_arr(pair) && json_len(pair) >= 2)
                json_append(out, nc_copy_or_null(json_get(pair, 1)));
        }
        return out;
    }
    n = nc_is_arr(model_list) ? json_len(model_list) : 0;
    for (i = 0; i < n; i++) json_append(out, nc_copy_or_null(json_get(model_list, i)));
    return out;
}

static json_t *nc_step_reply(const char *action, const json_t *state, json_t *commit) {
    json_t *reply = json_object();
    if (!reply) return json_null();
    json_set(reply, "action", json_string(action));
    json_set(reply, "state", state ? json_copy(state) : json_null());
    json_set(reply, "commit", commit ? commit : json_null());
    return reply;
}

static json_t *nc_step_commit(const json_t *state, const json_t *result,
                              int persist_global, const json_t *effort) {
    json_t *commit = json_object();
    if (!commit) return NULL;
    json_set(commit, "result", nc_copy_or_null(result));
    json_set(commit, "persist_global", json_bool(persist_global ? true : false));
    /* the picker-path commit leaves the effort at its "" default */
    json_set(commit, "reasoning_effort", effort ? json_copy(effort) : json_string(""));
    return nc_step_reply("commit", state, commit);
}

json_t *w28_climodelsw_0eb324_p2_selection_step(const json_t *req) {
    const json_t *state = nc_member(req, "state");
    const char *stage;
    long sel;
    int persist_global = nc_truthy(nc_member(req, "persist_global"));
    if (!nc_is_obj(state)) return nc_step_reply("none", nc_copy_or_null(state), NULL);
    stage = json_get_str(state, "stage", "");
    sel = (long)json_get_num(state, "selected", 0.0);

    if (!strcmp(stage, "provider")) {
        const json_t *providers = nc_member(state, "providers");
        const json_t *provider_data, *models, *offered;
        json_t *next;
        size_t np = nc_is_arr(providers) ? json_len(providers) : 0;
        if (sel >= 0 && (size_t)sel >= np) return nc_step_reply("close", NULL, NULL);
        provider_data = nc_index(providers, sel);
        models = nc_member(provider_data, "models");
        if (!nc_is_arr(models)) models = NULL;
        if (!models || json_len(models) == 0) {
            const json_t *cached = nc_member(req, "cached");
            if (nc_truthy(cached)) models = cached;
        }
        offered = nc_member(req, "offered");
        if (offered && offered->type != JSON_NULL) models = offered;
        next = json_copy(state);
        json_set(next, "stage", json_string("model"));
        json_set(next, "provider_data", nc_copy_or_null(provider_data));
        json_set(next, "model_list", nc_is_arr(models) ? json_copy(models) : json_array());
        json_set(next, "selected", json_number(0.0));
        json_set(next, "filter", json_string(""));
        json_set(next, "_filtered_pairs", json_null());
        {
            json_t *reply = nc_step_reply("stage_model", next, NULL);
            json_free(next);
            return reply;
        }
    }

    if (!strcmp(stage, "model")) {
        const json_t *provider_data = nc_member(state, "provider_data");
        const json_t *model_list = nc_member(state, "model_list");
        json_t *visible = nc_visible_labels(state, model_list);
        size_t back_idx = visible ? json_len(visible) : 0;
        if (!nc_is_obj(provider_data)) provider_data = NULL;
        if (sel == (long)back_idx) {
            /* <- Back to the provider list, on the provider this stage came from */
            const json_t *providers = nc_member(state, "providers");
            const json_t *slug = nc_member(provider_data, "slug");
            double idx = 0.0;
            size_t i, np = nc_is_arr(providers) ? json_len(providers) : 0;
            json_t *next;
            for (i = 0; i < np; i++) {
                if (nc_equal(nc_member(json_get(providers, i), "slug"), slug)) { idx = (double)i; break; }
            }
            next = json_copy(state);
            json_set(next, "stage", json_string("provider"));
            json_set(next, "filter", json_string(""));
            json_set(next, "_filtered_pairs", json_null());
            json_set(next, "selected", json_number(idx));
            json_free(visible);
            {
                json_t *reply = nc_step_reply("back_to_provider", next, NULL);
                json_free(next);
                return reply;
            }
        }
        if (sel > (long)back_idx) { json_free(visible); return nc_step_reply("close", NULL, NULL); }
        if (sel >= 0 && (size_t)sel < back_idx) {
            const json_t *result = nc_member(req, "switch");
            const json_t *new_model = nc_member(result, "new_model");
            json_free(visible);
            if (nc_truthy(nc_member(result, "success")) && nc_offers_reasoning(provider_data, new_model)) {
                json_t *next = json_copy(state);
                json_t *reply;
                json_set(next, "stage", json_string("reasoning"));
                json_set(next, "switch_result", nc_copy_or_null(result));
                json_set(next, "selected", json_number(0.0));
                json_set(next, "_scroll_offset", json_number(0.0));
                reply = nc_step_reply("stage_reasoning", next, NULL);
                json_free(next);
                return reply;
            }
            return nc_step_commit(state, result, persist_global, NULL);
        }
        json_free(visible);
        return nc_step_reply("close", NULL, NULL);
    }

    if (!strcmp(stage, "reasoning")) {
        const json_t *rows = nc_member(req, "rows");
        const json_t *result = nc_member(state, "switch_result");
        size_t nr = nc_is_arr(rows) ? json_len(rows) : 0;
        json_t *next, *reply;
        const json_t *row, *effort;
        if (sel == (long)nr) {   /* <- Back to the model list */
            next = json_copy(state);
            json_set(next, "stage", json_string("model"));
            json_set(next, "selected", json_number(0.0));
            json_set(next, "_scroll_offset", json_number(0.0));
            json_set(next, "switch_result", json_null());
            reply = nc_step_reply("back_to_model", next, NULL);
            json_free(next);
            return reply;
        }
        if (sel > (long)nr || !result || result->type == JSON_NULL)
            return nc_step_reply("close", NULL, NULL);
        row = nc_index(rows, sel);
        effort = (nc_is_arr(row) && json_len(row) > 0) ? json_get(row, 0) : NULL;
        return nc_step_commit(state, result, persist_global, effort);
    }

    return nc_step_reply("none", state, NULL);
}
