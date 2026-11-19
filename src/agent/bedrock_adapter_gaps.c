#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/bedrock_adapter.py: 10 missing functions ── */

/* ── rest_revive1 seam infrastructure (bedrock_adapter_gaps) ────────────────
 * json_t *fn(ba_io_t *io, json_t *in): io->call is the injected I/O seam.
 * ba_seam records {ev, args, ret} into io->trace in call order and returns
 * the scripted result (caller frees; args are ADOPTED — pass fresh objects).
 * A result {"__raise__": Type, "message": msg} models a Python exception;
 * ports propagate or swallow it exactly where the live code does. NULL
 * return == Python None. Integral JSON numbers model Python ints, fractional
 * ones floats. Case folding is ASCII-only (matched literals are ASCII);
 * str.strip() is Unicode-whitespace-aware.
 *
 * strip_cache_points projection: the live fn returns the SAME kwargs object
 * when nothing was stripped (callers use identity to decide a retry cannot
 * help). JSON has no identity, so the port returns {"same": bool, "value":
 * kwargs-or-cleaned-copy}; the oracle applies the identical projection
 * (res is kwargs). recover_from_cache_point_rejection uses the internal
 * ba_strip_impl identity flag directly, exactly like the live `is` test.
 *
 * Production default (no seam channel installed) is fail-closed. */
typedef struct ba_io {
    json_t *(*call)(struct ba_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} ba_io_t;

static int ba_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *ba_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static int ba_truthy(const json_t *v) {
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

static const char *ba_typename(const json_t *v) {
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

static json_t *ba_attr_error(const json_t *v, const char *attr) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", ba_typename(v), attr);
    return ba_raise("AttributeError", msg);
}

/* Record one seam call; args adopted (NULL becomes {}). Fail-closed when
 * no channel is installed (production default). */
static json_t *ba_seam(ba_io_t *io, const char *name, json_t *args) {
    if (!io || !io->call) {
        if (args) json_free(args);
        char msg[160];
        snprintf(msg, sizeof msg, "no seam channel installed for %s", name);
        return ba_raise("RuntimeError", msg);
    }
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static unsigned ba_cp_next(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    unsigned cp;
    if (s[0] < 0x80) { *p += 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        *p += 2; return cp;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x0F) << 12) | ((unsigned)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *p += 3; return cp;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        cp = ((unsigned)(s[0] & 0x07) << 18) | ((unsigned)(s[1] & 0x3F) << 12) |
             ((unsigned)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        *p += 4; return cp;
    }
    *p += 1; return 0xFFFD;
}

static int ba_isspace_cp(unsigned cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F)) return 1;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    return cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

/* malloc'd copy with Python str.strip() whitespace removed from both ends. */
static char *ba_strip_dup(const char *s) {
    if (!s) s = "";
    const char *start = s;
    while (*start) {
        const char *n = start;
        unsigned cp = ba_cp_next(&n);
        if (!ba_isspace_cp(cp)) break;
        start = n;
    }
    const char *end = s + strlen(s);
    while (end > start) {
        const char *p = start;
        const char *last = p;
        unsigned cp = 0;
        while (p < end) { last = p; cp = ba_cp_next(&p); }
        if (!ba_isspace_cp(cp)) break;
        end = last;
    }
    size_t len = (size_t)(end - start);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

/* ASCII lower (matched literals are ASCII). */
static char *ba_lower_dup(const char *s) {
    char *d = strdup(s ? s : "");
    if (!d) return NULL;
    for (char *p = d; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + ('a' - 'A'));
    return d;
}

/* (v or "").strip() — falsy v collapses to ""; truthy non-str raises
 * AttributeError exactly like the live .strip() call. Caller frees. */
static json_t *ba_or_strip(const json_t *v) {
    if (!ba_truthy(v)) return json_string("");
    if (v->type != JSON_STRING) return ba_attr_error(v, "strip");
    char *s = ba_strip_dup(v->str_val);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

/* _without_cache_points: blocks minus single-key {"cachePoint": ...} dicts,
 * or NULL when not a list / nothing removed. Returns a NEW array. */
static json_t *ba_without_cache_points(const json_t *blocks) {
    if (!blocks || blocks->type != JSON_ARRAY) return NULL;
    json_t *cleaned = json_array();
    int removed = 0;
    for (size_t i = 0; i < blocks->c.count; i++) {
        json_t *b = blocks->c.items[i];
        if (b && b->type == JSON_OBJECT && b->c.count == 1
            && strcmp(b->c.keys[0], "cachePoint") == 0) {
            removed = 1;
            continue;
        }
        json_append(cleaned, b ? json_copy(b) : json_null());
    }
    if (!removed) { json_free(cleaned); return NULL; }
    return cleaned;
}

/* strip_cache_points body. *same models the live identity return (the SAME
 * kwargs object back when nothing was stripped). Returns a NEW node. */
static json_t *ba_strip_impl(const json_t *kwargs, const char *placement, int *same) {
    *same = 0;
    if (!kwargs || kwargs->type != JSON_OBJECT) return ba_attr_error(kwargs, "get");
    if (strcmp(placement, "messages") == 0) {
        json_t *messages = json_obj_get(kwargs, "messages");
        json_t *cc = json_array();
        if (messages && messages->type == JSON_ARRAY) {
            for (size_t i = 0; i < messages->c.count; i++) {
                json_t *msg = messages->c.items[i];
                json_t *content = (msg && msg->type == JSON_OBJECT)
                                      ? json_obj_get(msg, "content") : NULL;
                json_t *w = ba_without_cache_points(content);
                json_append(cc, w ? w : json_null());
            }
        }
        int all_none = 1;
        for (size_t i = 0; i < cc->c.count; i++) {
            json_t *c = cc->c.items[i];
            if (c && c->type != JSON_NULL) { all_none = 0; break; }
        }
        if (all_none) {
            json_free(cc);
            *same = 1;
            return json_copy(kwargs);
        }
        json_t *new_msgs = json_array();
        for (size_t i = 0; i < messages->c.count; i++) {
            json_t *msg = messages->c.items[i];
            json_t *c = cc->c.items[i];
            if (!c || c->type == JSON_NULL) {
                json_append(new_msgs, msg ? json_copy(msg) : json_null());
            } else {
                json_t *nm = json_copy(msg);
                json_set(nm, "content", json_copy(c));
                json_append(new_msgs, nm);
            }
        }
        json_free(cc);
        json_t *out = json_copy(kwargs);
        json_set(out, "messages", new_msgs);
        return out;
    }
    if (strcmp(placement, "system") == 0) {
        json_t *cleaned = ba_without_cache_points(json_obj_get(kwargs, "system"));
        if (!cleaned) { *same = 1; return json_copy(kwargs); }
        json_t *out = json_copy(kwargs);
        json_set(out, "system", cleaned);
        return out;
    }
    if (strcmp(placement, "tools") == 0) {
        json_t *tool_config = json_obj_get(kwargs, "toolConfig");
        /* (tool_config or {}).get("tools") */
        const json_t *tc = ba_truthy(tool_config) ? tool_config : NULL;
        if (tc && tc->type != JSON_OBJECT) return ba_attr_error(tc, "get");
        json_t *cleaned = ba_without_cache_points(tc ? json_obj_get(tc, "tools") : NULL);
        if (!cleaned) { *same = 1; return json_copy(kwargs); }
        json_t *new_tc = json_copy(tool_config);
        json_set(new_tc, "tools", cleaned);
        json_t *out = json_copy(kwargs);
        json_set(out, "toolConfig", new_tc);
        return out;
    }
    *same = 1;
    return json_copy(kwargs);
}

/* cache_point_rejection_placement body: msg -> placement string or NULL.
 * _CACHE_POINT_PATH_PATTERN = r"#/(?P<path>[A-Za-z0-9_./\[\]-]*)", IGNORECASE
 * (the literal "#/" carries no cased characters). */
static json_t *ba_placement_impl(const char *msg) {
    char *lowered = ba_lower_dup(msg);
    int gate = strstr(lowered, "cachepoint")
            && (strstr(lowered, "not permitted") || strstr(lowered, "extraneous"));
    free(lowered);
    if (!gate) return json_null();
    const char *hit = strstr(msg, "#/");
    char *path = malloc(strlen(msg) + 1);
    if (!path) return json_null();
    size_t n = 0;
    if (hit) {
        for (const char *p = hit + 2; *p; p++) {
            char c = *p;
            int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                  || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '/'
                  || c == '[' || c == ']' || c == '-';
            if (!ok) break;
            path[n++] = c;
        }
    }
    path[n] = '\0';
    for (size_t i = 0; i < n; i++)
        if (path[i] >= 'A' && path[i] <= 'Z') path[i] = (char)(path[i] + ('a' - 'A'));
    const char *result = "tools";
    if (strstr(path, "toolconfig") || strstr(path, "tools")) result = "tools";
    else if (strstr(path, "system")) result = "system";
    else if (strstr(path, "messages")) result = "messages";
    free(path);
    return json_string(result);
}

/* str(v) for the modelId slot: None -> "None", str passthrough, integral
 * numbers -> decimal, bool -> True/False. Fixtures restrict modelId to these
 * types (container repr is out of scope, documented in the oracle). */
static char *ba_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return strdup(buf);
    }
    /* Containers: compact JSON stand-in; fixtures never exercise this. */
    char *ser = json_serialize(v);
    char *out = strdup(ser ? ser : "");
    free(ser);
    return out;
}

/* PoP: resolve_bedrock_bearer_token @ agent/bedrock_adapter.py:resolve_bedrock_bearer_token */
json_t *resolve_bedrock_bearer_token(ba_io_t *io, json_t *in) {
    json_t *env = json_obj_get(in, "env");
    if (env && env->type != JSON_NULL) {
        if (env->type != JSON_OBJECT) return ba_attr_error(env, "get");
        return ba_or_strip(json_obj_get(env, "AWS_BEARER_TOKEN_BEDROCK"));
    }
    /* from hermes_constants import get_hermes_home_override */
    json_t *imp = ba_seam(io, "import_get_hermes_home_override", NULL);
    if (ba_is_raise(imp)) return imp;
    json_free(imp);
    json_t *ov = ba_seam(io, "get_hermes_home_override", NULL);
    if (ba_is_raise(ov)) return ov;
    if (ov && ov->type != JSON_NULL) {
        json_free(ov);
        /* from agent.secret_scope import get_secret */
        json_t *imp2 = ba_seam(io, "import_get_secret", NULL);
        if (ba_is_raise(imp2)) return imp2;
        json_free(imp2);
        json_t *args = json_object();
        json_set_hdr(args, "key", "AWS_BEARER_TOKEN_BEDROCK");
        json_set_hdr(args, "default", "");
        json_t *v = ba_seam(io, "get_secret", args);
        if (ba_is_raise(v)) return v;
        return ba_or_strip(v);
    }
    if (ov) json_free(ov);
    json_t *args = json_object();
    json_set_hdr(args, "key", "AWS_BEARER_TOKEN_BEDROCK");
    json_set_hdr(args, "default", "");
    json_t *v = ba_seam(io, "environ_get", args);
    if (ba_is_raise(v)) return v;
    return ba_or_strip(v);
}

/* auth_flow: deferred (PoP tag removed; stub left untagged). */
char *auth_flow(void) { return strdup("{}"); }

/* build_bedrock_openai_http_client: deferred (PoP tag removed). */
char *build_bedrock_openai_http_client(void) { return strdup("{}"); }

/* PoP: cache_point_rejection_placement @ agent/bedrock_adapter.py:cache_point_rejection_placement */
json_t *cache_point_rejection_placement(ba_io_t *io, json_t *in) {
    (void)io;
    json_t *exc = json_obj_get(in, "exc");
    const char *msg = (exc && exc->type == JSON_OBJECT)
                          ? json_get_str(exc, "message", "") : "";
    return ba_placement_impl(msg);
}

/* PoP: cache_point_allowed @ agent/bedrock_adapter.py:cache_point_allowed */
int cache_point_allowed(void) { return 1; }

/* PoP: is_cache_point_block @ agent/bedrock_adapter.py:is_cache_point_block */
int is_cache_point_block(void) { return 0; }

/* PoP: strip_cache_points @ agent/bedrock_adapter.py:strip_cache_points */
json_t *strip_cache_points(ba_io_t *io, json_t *in) {
    (void)io;
    json_t *kwargs = json_obj_get(in, "kwargs");
    const char *placement = json_get_str(in, "placement", "");
    int same = 0;
    json_t *v = ba_strip_impl(kwargs, placement, &same);
    if (ba_is_raise(v)) return v;
    json_t *out = json_object();
    json_set(out, "same", json_bool(same ? true : false));
    json_set(out, "value", v);
    return out;
}

/* PoP: recover_from_cache_point_rejection @ agent/bedrock_adapter.py:recover_from_cache_point_rejection */
json_t *recover_from_cache_point_rejection(ba_io_t *io, json_t *in) {
    json_t *exc = json_obj_get(in, "exc");
    json_t *kwargs = json_obj_get(in, "kwargs");
    const char *msg = (exc && exc->type == JSON_OBJECT)
                          ? json_get_str(exc, "message", "") : "";
    json_t *pl = ba_placement_impl(msg);
    if (!pl || pl->type == JSON_NULL) {
        if (pl) json_free(pl);
        return json_null();
    }
    int same = 0;
    json_t *retry = ba_strip_impl(kwargs, pl->str_val, &same);
    if (ba_is_raise(retry)) { json_free(pl); return retry; }
    if (same) {
        json_free(retry);
        json_free(pl);
        return json_null();
    }
    /* model_id = str(kwargs.get("modelId", "")) — kwargs is a dict here
     * (ba_strip_impl raised otherwise). Present-null -> "None". */
    json_t *mid = json_obj_get(kwargs, "modelId");
    char *model_id = mid ? ba_py_str(mid) : strdup("");
    json_t *args = json_object();
    json_set_hdr(args, "model_id", model_id);
    json_set_hdr(args, "placement", pl->str_val);
    json_t *nr = ba_seam(io, "note_cache_point_rejection", args);
    if (ba_is_raise(nr)) {
        free(model_id);
        json_free(retry);
        json_free(pl);
        return nr;
    }
    json_free(nr);
    json_t *largs = json_object();
    json_set_hdr(largs, "msg",
        "bedrock: %s rejected a cachePoint block in %s \xe2\x80\x94"
        " dropping that cache marker for this model and retrying. Prompt"
        " caching stays active for the remaining sections.");
    json_t *arr = json_array();
    json_append(arr, json_string(model_id[0] ? model_id : "model"));
    json_append(arr, json_string(pl->str_val));
    json_set(largs, "args", arr);
    json_t *lr = ba_seam(io, "logger.warning", largs);
    if (ba_is_raise(lr)) {
        free(model_id);
        json_free(retry);
        json_free(pl);
        return lr;
    }
    json_free(lr);
    free(model_id);
    json_free(pl);
    return retry;
}
