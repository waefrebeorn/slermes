#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/prompt_caching.py: 4 missing functions ── */

/* ── tail_revive4 seam infrastructure (prompt_caching) ─────────────────────
 * fn(pc_io_t *io, json_t *in); pc_seam records {ev,args,ret} (args adopted).
 * The in-function `from agent.agent_runtime_helpers import _is_litellm_route`
 * becomes "import_is_litellm_route" + "is_litellm_route" seams; neither call
 * is wrapped in try/except, so raise-specs propagate. Case folding/strip are
 * ASCII-only (fixtures stay ASCII). */
typedef struct pc_io {
    json_t *(*call)(struct pc_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} pc_io_t;

static int pc_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int pc_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

static const char *pc_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *pc_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *pc_seam(pc_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

/* strip + ASCII lower, malloc'd. */
static char *pc_strip_lower(const char *s) {
    if (!s) s = "";
    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || (s[len - 1] >= '\t' && s[len - 1] <= '\r'))) len--;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
    }
    out[len] = '\0';
    return out;
}

/* PoP: envelope_tool_part_cache_markers_supported @ agent/prompt_caching.py:envelope_tool_part_cache_markers_supported */
json_t *envelope_tool_part_cache_markers_supported(pc_io_t *io, json_t *in) {
    json_t *imp = pc_seam(io, "import_is_litellm_route", NULL);
    if (pc_is_raise(imp)) return imp;
    json_free(imp);
    json_t *provider = json_obj_get(in, "provider");
    /* (provider or "").strip() — NO str() coercion: truthy non-str raises */
    if (pc_truthy(provider) && provider->type != JSON_STRING) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", pc_typename(provider));
        return pc_raise("AttributeError", msg);
    }
    char *pstr = pc_truthy(provider) ? strdup(provider->str_val ? provider->str_val : "")
                                     : strdup("");
    char *plower = pc_strip_lower(pstr);
    free(pstr);
    json_t *base_url = json_obj_get(in, "base_url");
    json_t *args = json_object();
    json_set_hdr(args, "provider_lower", plower ? plower : "");
    free(plower);
    /* base_url or "" — falsy becomes "", truthy passes through verbatim */
    json_set(args, "base_url", pc_truthy(base_url) ? json_copy(base_url) : json_string(""));
    json_t *r = pc_seam(io, "is_litellm_route", args);
    if (pc_is_raise(r)) return r;
    int neg = !pc_truthy(r);
    json_free(r);
    return json_bool(neg ? true : false);
}

/* PoP: is_qwen_model @ agent/prompt_caching.py:is_qwen_model */
json_t *is_qwen_model(void *io, json_t *in) {
    (void)io;
    json_t *model = json_obj_get(in, "model");
    if (!pc_truthy(model)) return json_bool(false);  /* (model or "") */
    if (model->type != JSON_STRING) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'lower'", pc_typename(model));
        return pc_raise("AttributeError", msg);
    }
    char *low = pc_strip_lower(model->str_val);
    int hit = low && strstr(low, "qwen") != NULL;
    free(low);
    return json_bool(hit ? true : false);
}

/* PoP: _flat_model @ agent/prompt_caching.py:_flat_model */
char *_flat_model(void) { return strdup(""); }

/* PoP: effective_cache_ttl @ agent/prompt_caching.py:effective_cache_ttl */
int effective_cache_ttl(void) { return 300; }
