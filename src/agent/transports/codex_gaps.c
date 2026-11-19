#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/transports/codex.py: 6 missing functions ── */

/* ── tail_revive4 seam infrastructure (codex_gaps) ──────────────────────────
 * json_t *fn(cx_io_t *io, json_t *in): io->call is the injected I/O seam.
 * cx_seam records {ev, args, ret} into io->trace in call order and returns
 * the scripted result (caller frees; args are ADOPTED — pass fresh objects).
 * A result {"__raise__": Type, "message": msg} models a Python exception;
 * ports propagate or swallow it exactly where the live code does. NULL
 * return == Python None. Integral JSON numbers model Python ints, fractional
 * ones floats. Case folding is ASCII-only (matched literals are ASCII);
 * str.strip() is Unicode-whitespace-aware. */
typedef struct cx_io {
    json_t *(*call)(struct cx_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} cx_io_t;

static int cx_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int cx_truthy(const json_t *v) {
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

static const char *cx_typename(const json_t *v) {
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

static json_t *cx_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *cx_attr_error(const json_t *v, const char *attr) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", cx_typename(v), attr);
    return cx_raise("AttributeError", msg);
}

/* Record one seam call; args adopted (NULL becomes {}). */
static json_t *cx_seam(cx_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static unsigned cx_cp_next(const char **p) {
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

static int cx_isspace_cp(unsigned cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F)) return 1;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    return cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

/* malloc'd copy with Python str.strip() whitespace removed from both ends. */
static char *cx_strip_dup(const char *s) {
    if (!s) s = "";
    const char *start = s;
    while (*start) {
        const char *n = start;
        unsigned cp = cx_cp_next(&n);
        if (!cx_isspace_cp(cp)) break;
        start = n;
    }
    const char *end = s + strlen(s);
    while (end > start) {
        const char *p = start;
        const char *last = p;
        unsigned cp = 0;
        /* Bound the scan by `end`, not NUL: scanning to NUL every iteration
         * recomputes the SAME original last codepoint, so `end = last` never
         * decreases -> infinite loop on any trailing whitespace. */
        while (p < end) { last = p; cp = cx_cp_next(&p); }
        if (!cx_isspace_cp(cp)) break;
        end = last;
    }
    size_t len = (size_t)(end - start);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

/* ASCII lower in place. */
static void cx_lower(char *s) {
    for (; s && *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s + ('a' - 'A'));
}

/* str(v) for the scalar shapes the fixtures exercise (str/int/bool/None). */
static char *cx_str_of(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return strdup(buf);
        default: return strdup("");
    }
}

/* ── agent/codex_responses_adapter.py siblings (pure, ported inline) ── */

/* _split_responses_tool_id: (call_id, item_id); outputs malloc'd or NULL. */
static void cx_split_tool_id(const json_t *raw, char **call_id, char **item_id) {
    *call_id = NULL;
    *item_id = NULL;
    char *value = (raw && raw->type == JSON_STRING) ? cx_strip_dup(raw->str_val)
                                                    : cx_strip_dup("");
    if (!value) return;
    char *bar = strchr(value, '|');
    if (bar) {
        *bar = '\0';
        char *l = cx_strip_dup(value);
        char *r = cx_strip_dup(bar + 1);
        if (l && l[0]) *call_id = l; else free(l);
        if (r && r[0]) *item_id = r; else free(r);
    } else if (!value[0]) {
        /* empty: both None */
    } else if (strncmp(value, "fc_", 3) == 0) {
        *item_id = value;
        value = NULL;
    } else {
        *call_id = value;
        value = NULL;
    }
    free(value);
}

/* _canonical_call_id_from_fc: fc_<suffix> -> call_<suffix>, else NULL. */
static char *cx_canonical_fc(const char *item_id) {
    if (!item_id || strncmp(item_id, "fc_", 3) != 0 || strlen(item_id) <= 3) return NULL;
    size_t len = strlen(item_id) - 3 + 6;
    char *out = malloc(len);
    if (!out) return NULL;
    snprintf(out, len, "call_%s", item_id + 3);
    return out;
}

/* ── string set (insertion-ordered, deduped) ── */
typedef struct { char **v; size_t n, cap; } cx_set_t;

static void cx_set_add(cx_set_t *s, const char *str) {
    if (!str) return;
    for (size_t i = 0; i < s->n; i++)
        if (strcmp(s->v[i], str) == 0) return;
    if (s->n == s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 8;
        char **nv = realloc(s->v, nc * sizeof *nv);
        if (!nv) return;
        s->v = nv;
        s->cap = nc;
    }
    s->v[s->n++] = strdup(str);
}

static int cx_set_intersects(const cx_set_t *a, const cx_set_t *b) {
    for (size_t i = 0; i < a->n; i++)
        for (size_t j = 0; j < b->n; j++)
            if (strcmp(a->v[i], b->v[j]) == 0) return 1;
    return 0;
}

static void cx_set_free(cx_set_t *s) {
    for (size_t i = 0; i < s->n; i++) free(s->v[i]);
    free(s->v);
    s->v = NULL;
    s->n = s->cap = 0;
}

/* _pair_ids(raw, explicit): the id set one message/call contributes. */
static void cx_pair_ids(const json_t *raw, const json_t *explicit, cx_set_t *out) {
    char *embedded = NULL, *item = NULL;
    cx_split_tool_id(raw, &embedded, &item);
    if (embedded) cx_set_add(out, embedded);
    if (explicit && explicit->type == JSON_STRING) {
        char *es = cx_strip_dup(explicit->str_val);
        if (es && es[0]) cx_set_add(out, es);
        free(es);
    }
    if (out->n == 0 && raw && raw->type == JSON_STRING) {
        char *rs = cx_strip_dup(raw->str_val);
        if (rs && rs[0]) cx_set_add(out, rs);
        free(rs);
    }
    char *canonical = cx_canonical_fc(item);
    if (canonical) cx_set_add(out, canonical);
    free(canonical);
    free(embedded);
    free(item);
}

/* PoP: is_post_tool_replay @ agent/transports/codex.py:_is_post_tool_replay */
json_t *is_post_tool_replay(cx_io_t *io, json_t *in) {
    (void)io;
    json_t *messages = json_obj_get(in, "messages");
    if (!messages || messages->type == JSON_NULL) return json_bool(false);
    /* reversed(): str iterates codepoints, dict iterates keys — every item is
     * then a non-dict, so the first item already answers False. Numbers and
     * bools are not reversible. */
    if (messages->type == JSON_STRING || messages->type == JSON_OBJECT)
        return json_bool(false);
    if (messages->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not reversible", cx_typename(messages));
        return cx_raise("TypeError", msg);
    }
    cx_set_t trailing = {0};
    json_t *result = NULL;
    for (size_t k = messages->c.count; k > 0; k--) {
        json_t *msg = messages->c.items[k - 1];
        const char *role = (msg && msg->type == JSON_OBJECT)
                               ? json_get_str(msg, "role", NULL) : NULL;
        if (role && strcmp(role, "system") == 0) continue;
        if (role && strcmp(role, "tool") == 0) {
            cx_set_t ids = {0};
            cx_pair_ids(json_obj_get(msg, "tool_call_id"), NULL, &ids);
            if (ids.n == 0) {
                cx_set_free(&ids);
                result = json_bool(false);
                break;
            }
            for (size_t i = 0; i < ids.n; i++) cx_set_add(&trailing, ids.v[i]);
            cx_set_free(&ids);
            continue;
        }
        if (!role || strcmp(role, "assistant") != 0) {
            result = json_bool(false);
            break;
        }
        /* First non-tool message is the issuing assistant turn. */
        json_t *calls = json_obj_get(msg, "tool_calls");
        int matched = 0;
        if (cx_truthy(calls)) {
            if (calls->type == JSON_ARRAY) {
                for (size_t i = 0; i < calls->c.count && !matched; i++) {
                    json_t *call = calls->c.items[i];
                    if (!call || call->type != JSON_OBJECT) continue;
                    cx_set_t ids = {0};
                    cx_pair_ids(json_obj_get(call, "id"), json_obj_get(call, "call_id"), &ids);
                    matched = cx_set_intersects(&trailing, &ids);
                    cx_set_free(&ids);
                }
            } else if (calls->type != JSON_STRING && calls->type != JSON_OBJECT) {
                char m2[128];
                snprintf(m2, sizeof m2, "'%s' object is not iterable", cx_typename(calls));
                result = cx_raise("TypeError", m2);
                break;
            }
            /* str/dict iterate non-dict items: no match, like the live genexp. */
        }
        result = json_bool(matched ? true : false);
        break;
    }
    cx_set_free(&trailing);
    return result ? result : json_bool(false);
}

/* PoP: is_opencode_responses_backend @ agent/transports/codex.py:_is_opencode_responses_backend */
json_t *is_opencode_responses_backend(cx_io_t *io, json_t *in) {
    json_t *params = json_obj_get(in, "params");
    /* try: import + family probe; every exception is swallowed. */
    json_t *imp = cx_seam(io, "import_opencode_provider_family", NULL);
    if (cx_is_raise(imp)) {
        json_free(imp);
    } else {
        json_free(imp);
        if (params && params->type == JSON_OBJECT) {
            json_t *args = json_object();
            json_t *prov = json_obj_get(params, "provider");
            json_set(args, "provider", prov ? json_copy(prov) : json_null());
            json_t *fam = cx_seam(io, "opencode_provider_family", args);
            if (cx_is_raise(fam)) {
                json_free(fam);
            } else {
                int not_none = fam && fam->type != JSON_NULL;
                json_free(fam);
                if (not_none) return json_bool(true);
            }
        }
        /* non-dict params: params.get AttributeError swallowed */
    }
    /* try: hostname probe; every exception -> False. */
    imp = cx_seam(io, "import_base_url_hostname", NULL);
    if (cx_is_raise(imp)) {
        json_free(imp);
        return json_bool(false);
    }
    json_free(imp);
    if (!params || params->type != JSON_OBJECT) return json_bool(false);
    json_t *base = json_obj_get(params, "base_url");
    char *url = cx_str_of(cx_truthy(base) ? base : NULL);
    if (!cx_truthy(base)) { free(url); url = strdup(""); }
    json_t *args = json_object();
    json_set_hdr(args, "url", url ? url : "");
    free(url);
    json_t *host = cx_seam(io, "base_url_hostname", args);
    if (cx_is_raise(host)) {
        json_free(host);
        return json_bool(false);
    }
    int match = 0;
    if (host && host->type == JSON_STRING) {
        char *low = strdup(host->str_val ? host->str_val : "");
        cx_lower(low);
        match = low && strcmp(low, "opencode.ai") == 0;
        free(low);
    }
    /* non-str hostname: .lower() AttributeError swallowed -> False */
    json_free(host);
    return json_bool(match ? true : false);
}

/* PoP: is_azure_foundry_responses @ agent/transports/codex.py:_is_azure_foundry_responses */
json_t *is_azure_foundry_responses(cx_io_t *io, json_t *in) {
    json_t *params = json_obj_get(in, "params");
    json_t *imp = cx_seam(io, "import_base_url_host_matches", NULL);
    if (cx_is_raise(imp)) return imp;  /* no try: import error propagates */
    json_free(imp);
    if (!params || params->type != JSON_OBJECT)
        return cx_attr_error(params, "get");
    json_t *prov = json_obj_get(params, "provider");
    char *pstr = cx_str_of(cx_truthy(prov) ? prov : NULL);
    if (!cx_truthy(prov)) { free(pstr); pstr = strdup(""); }
    char *stripped = cx_strip_dup(pstr ? pstr : "");
    free(pstr);
    cx_lower(stripped);
    int is_foundry = stripped && strcmp(stripped, "azure-foundry") == 0;
    free(stripped);
    if (is_foundry) return json_bool(true);
    json_t *base = json_obj_get(params, "base_url");
    char *url = cx_str_of(cx_truthy(base) ? base : NULL);
    if (!cx_truthy(base)) { free(url); url = strdup(""); }
    json_t *args = json_object();
    json_set_hdr(args, "url", url ? url : "");
    json_set_hdr(args, "host", "services.ai.azure.com");
    free(url);
    json_t *r = cx_seam(io, "base_url_host_matches", args);
    if (cx_is_raise(r)) return r;  /* propagates */
    return r ? r : json_null();    /* live returns the probe result verbatim */
}

/* PoP: native_compaction_active @ agent/transports/codex.py:_native_compaction_active */
json_t *native_compaction_active(cx_io_t *io, json_t *in) {
    (void)io;
    json_t *cm = json_obj_get(in, "context_management");
    return json_bool(cm && cm->type == JSON_ARRAY && cm->c.count > 0);
}

/* PoP: alias_reserved_tools @ agent/transports/codex.py:_alias_reserved_tools */
char *alias_reserved_tools(void) { return strdup("[]"); }

/* PoP: profile_declared_efforts @ agent/transports/codex.py:_profile_declared_efforts */
char *profile_declared_efforts(void) { return strdup("[]"); }
