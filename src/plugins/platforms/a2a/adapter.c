/*
 * adapter.c - C11 port of upstream plugins/platforms/a2a/adapter.py
 * (gates w29_adapter_0e0bda_p1, w29_adapter_0e0bda_p2, w29_adapter_0e0bda_p3).
 *
 * PoP: w29_adapter_0e0bda_constants         @ plugins/platforms/a2a/adapter.py:<module constants>
 * PoP: w29_adapter_0e0bda_reply_timeout     @ plugins/platforms/a2a/adapter.py:_reply_timeout
 * PoP: w29_adapter_0e0bda_orphan_timeout    @ plugins/platforms/a2a/adapter.py:_orphan_timeout
 * PoP: w29_adapter_0e0bda_clean_slug        @ plugins/platforms/a2a/adapter.py:_clean_slug
 * PoP: w29_adapter_0e0bda_join_url          @ plugins/platforms/a2a/adapter.py:_join_url
 * PoP: w29_adapter_0e0bda_safe_context_slug @ plugins/platforms/a2a/adapter.py:_safe_context_slug
 * PoP: w29_adapter_0e0bda_server_class      @ plugins/platforms/a2a/adapter.py:_http_server_class
 * PoP: w29_adapter_0e0bda_client_ip         @ plugins/platforms/a2a/adapter.py:A2ARequestHandler._client_ip
 * PoP: w29_adapter_0e0bda_base_url          @ plugins/platforms/a2a/adapter.py:A2AAdapter._base_url
 * PoP: w29_adapter_0e0bda_route_for_path    @ plugins/platforms/a2a/adapter.py:A2AAdapter._route_for_path
 * PoP: w29_adapter_0e0bda_route_for_request @ plugins/platforms/a2a/adapter.py:A2AAdapter._route_for_request
 * PoP: w29_adapter_0e0bda_served_agent_summary @ plugins/platforms/a2a/adapter.py:A2AAdapter._served_agent_summary
 * PoP: w29_adapter_0e0bda_scope_for_agent   @ plugins/platforms/a2a/adapter.py:A2AAdapter._scope_for_agent
 * PoP: w29_adapter_0e0bda_build_card        @ plugins/platforms/a2a/adapter.py:A2AAdapter._build_card
 * PoP: w29_adapter_0e0bda_method_lookup     @ plugins/platforms/a2a/adapter.py:_METHODS
 * PoP: w29_adapter_0e0bda_find_task_error   @ plugins/platforms/a2a/adapter.py:A2AAdapter._find_task
 * PoP: w29_adapter_0e0bda_push_op_error     @ plugins/platforms/a2a/adapter.py:A2AAdapter._push_config_op
 * PoP: w29_adapter_0e0bda_push_create_error @ plugins/platforms/a2a/adapter.py:A2AAdapter._rpc_push_config_create
 * PoP: w29_adapter_0e0bda_inline_push_url   @ plugins/platforms/a2a/adapter.py:A2AAdapter._register_inline_push
 * PoP: w29_adapter_0e0bda_note_forwarded    @ plugins/platforms/a2a/adapter.py:A2AAdapter._note_forwarded_input
 * PoP: w29_adapter_0e0bda_forget_forwarded  @ plugins/platforms/a2a/adapter.py:A2AAdapter._forget_forwarded_input
 * PoP: w29_adapter_0e0bda_chat_info         @ plugins/platforms/a2a/adapter.py:A2AAdapter.get_chat_info
 *
 * Seams (deliberately not ported; a fixed fixture stands in for each):
 *   - the ThreadingHTTPServer listener, its daemon threads and asyncio futures;
 *   - TaskStore / TurnTracker / RateLimiter (every store lookup here is the
 *     miss branch, which is what the fixtures encode);
 *   - the live tool registry (the Agent Card's skills take the static
 *     skills_from_toolsets fallback, as they do with no registered toolsets);
 *   - profile/env capture: A2A_REPLY_TIMEOUT and A2A_PROVIDER_* arrive as
 *     arguments -- the values the adapter captured at construction time -- so
 *     no live profile context leaks into these decisions.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w29_adapter_0e0bda_p1.h"
#include "noc_w29_adapter_0e0bda_p2.h"
#include "noc_w29_adapter_0e0bda_p3.h"

/* --- module constants (adapter.py:35-41, 518) --------------------------- */
#define A2A_DEFAULT_PORT          9900
#define A2A_MIN_ORPHAN_TIMEOUT    300.0
#define A2A_MAX_ORPHAN_TIMEOUT    86400.0
#define A2A_WATCHDOG_INTERVAL     60
#define A2A_MAX_BODY              1048576
#define A2A_SSE_KEEPALIVE         5
#define A2A_MAX_FORWARDED_INPUTS  1000
#define A2A_DEFAULT_DESCRIPTION   "Hermes Agent \xe2\x80\x94 a general-purpose agent reachable over A2A."

/* --- small utilities ---------------------------------------------------- */

static char *a2a_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* Python truthiness of a JSON value. */
static int a2a_truthy(const json_t *v) {
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

/* Python str() over the scalar shapes these decisions see.  libjson has no
 * int/float tag, so an integral number renders as an int ("5"), which is how
 * json.loads() hands it back.  Arrays/objects fall back to their JSON text
 * (documented limitation: Python would render a repr there). */
static char *a2a_py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return a2a_dup("None");
    switch (v->type) {
    case JSON_BOOL: return a2a_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return a2a_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val && fabs(v->num_val) < 1e16)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return a2a_dup(buf);
    default: {
        char *s = json_dumps(v, 0);
        char *out = a2a_dup(s ? s : "");
        free(s);
        return out;
    }
    }
}

/* str(value or "") */
static char *a2a_or_empty(const json_t *v) {
    if (!a2a_truthy(v)) return a2a_dup("");
    return a2a_py_str(v);
}

/* str(obj.get(key) or "") -- safe on non-objects. */
static char *a2a_key_or_empty(const json_t *obj, const char *key) {
    return a2a_or_empty(obj && obj->type == JSON_OBJECT ? json_obj_get(obj, key) : NULL);
}

/* Python repr() of a str: 'text'. */
static char *a2a_py_repr(const char *s) {
    size_t i, n = 0, cap;
    char *out;
    if (!s) s = "";
    cap = strlen(s) * 2 + 3;
    out = (char *)malloc(cap);
    if (!out) return NULL;
    out[n++] = '\'';
    for (i = 0; s[i]; i++) {
        char c = s[i];
        if (c == '\'' || c == '\\' || c == '\n' || c == '\r' || c == '\t') {
            if (n + 3 > cap) {
                char *t = (char *)realloc(out, cap * 2);
                if (!t) { free(out); return NULL; }
                out = t;
                cap *= 2;
            }
            out[n++] = '\\';
            out[n++] = c == '\n' ? 'n' : c == '\r' ? 'r' : c == '\t' ? 't' : c;
        } else {
            out[n++] = c;
        }
    }
    out[n++] = '\'';
    out[n] = '\0';
    return out;
}

/* --- growable string buffer --------------------------------------------- */

typedef struct { char *p; size_t len, cap; } a2a_sbuf;

static void sb_init(a2a_sbuf *b) { b->p = NULL; b->len = b->cap = 0; }

static void sb_addn(a2a_sbuf *b, const char *s, size_t n) {
    size_t need;
    if (!s || n == 0) return;
    need = b->len + n + 1;
    if (need > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *p;
        while (cap < need) cap *= 2;
        p = (char *)realloc(b->p, cap);
        if (!p) return;
        b->p = p;
        b->cap = cap;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static void sb_add(a2a_sbuf *b, const char *s) { if (s) sb_addn(b, s, strlen(s)); }

static void sb_addc(a2a_sbuf *b, char c) { sb_addn(b, &c, 1); }

static char *sb_take(a2a_sbuf *b) {
    char *out;
    if (!b->p) return a2a_dup("");
    out = b->p;
    b->p = NULL;
    b->len = b->cap = 0;
    return out;
}

static void sb_drop(a2a_sbuf *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }

/* json_string() of a taken buffer, freeing it. */
static json_t *js_take(a2a_sbuf *b) {
    char *s = sb_take(b);
    json_t *out = json_string(s ? s : "");
    free(s);
    sb_drop(b);
    return out;
}

/* --- Python-ish trimming ------------------------------------------------ */

static int a2a_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str.strip() then str.strip(chars) at both ends. */
static char *a2a_trim(const char *s, const char *chars) {
    size_t start = 0, end, n;
    char *out;
    if (!s) return a2a_dup("");
    end = strlen(s);
    while (start < end && a2a_is_space(s[start])) start++;
    while (end > start && a2a_is_space(s[end - 1])) end--;
    if (chars) {
        while (start < end && strchr(chars, s[start])) start++;
        while (end > start && strchr(chars, s[end - 1])) end--;
    }
    n = end - start;
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s + start, n);
    out[n] = '\0';
    return out;
}

/* trim a heap string, dropping the input. */
static char *a2a_trim_owned(char *s, const char *chars) {
    char *t = a2a_trim(s, chars);
    free(s);
    return t;
}

/* RFC 3986 5.2.4 remove_dot_segments(). */
static void a2a_drop_last_segment(char *out, size_t *olen) {
    while (*olen > 0 && out[*olen - 1] == '/') (*olen)--;
    while (*olen > 0 && out[*olen - 1] != '/') (*olen)--;
    if (*olen > 0 && out[*olen - 1] == '/') (*olen)--;   /* its preceding "/" too */
    out[*olen] = '\0';
}

static char *a2a_remove_dot_segments(const char *path) {
    size_t n, i = 0, olen = 0;
    char *in, *out;

    if (!path) return a2a_dup("");
    n = strlen(path);
    in = (char *)malloc(n + 2);
    out = (char *)malloc(n + 2);
    if (!in || !out) { free(in); free(out); return a2a_dup(""); }
    memcpy(in, path, n + 1);
    out[0] = '\0';

    while (i < n) {
        const char *p = in + i;
        size_t rem = n - i;
        if (rem >= 3 && !strncmp(p, "../", 3)) {
            i += 3;
        } else if (rem >= 2 && !strncmp(p, "./", 2)) {
            i += 2;
        } else if (rem >= 3 && !strncmp(p, "/./", 3)) {
            i += 2;
        } else if (rem == 2 && !strncmp(p, "/.", 2)) {
            in[i + 1] = '/';
            n = i + 2;
            in[n] = '\0';
        } else if (rem >= 4 && !strncmp(p, "/../", 4)) {
            a2a_drop_last_segment(out, &olen);
            i += 3;
        } else if (rem == 3 && !strncmp(p, "/..", 3)) {
            a2a_drop_last_segment(out, &olen);
            in[i + 1] = '/';
            n = i + 2;
            in[n] = '\0';
        } else if ((rem == 1 && p[0] == '.') || (rem == 2 && !strncmp(p, "..", 2))) {
            i = n;
        } else {
            size_t start = i;
            if (in[i] == '/') i++;
            while (i < n && in[i] != '/') i++;
            memcpy(out + olen, in + start, i - start);
            olen += i - start;
            out[olen] = '\0';
        }
    }
    free(in);
    return out;
}

/* urllib.parse.urljoin(base, ref) for the references _join_url builds: the
 * base always ends with "/", the reference is a relative path. */
static char *a2a_urljoin(const char *base, const char *ref) {
    const char *sep = strstr(base, "://");
    size_t origin_len = 0;
    const char *path;
    size_t path_len;
    char *merged, *clean, *out;
    a2a_sbuf b;

    if (sep) {
        const char *auth = sep + 3;
        const char *slash = strchr(auth, '/');
        origin_len = slash ? (size_t)(slash - base) : strlen(base);
    }
    path = base + origin_len;
    path_len = strcspn(path, "?#");   /* urljoin keeps only the base's path */
    /* merge against the base path's directory, not the path itself */
    while (path_len > 0 && path[path_len - 1] != '/') path_len--;

    sb_init(&b);
    sb_addn(&b, path, path_len);
    if (path_len == 0 && origin_len > 0) sb_addc(&b, '/');
    sb_add(&b, ref);
    merged = sb_take(&b);
    sb_drop(&b);

    clean = a2a_remove_dot_segments(merged);
    free(merged);

    sb_init(&b);
    sb_addn(&b, base, origin_len);
    sb_add(&b, clean ? clean : "");
    free(clean);
    out = sb_take(&b);
    sb_drop(&b);
    return out;
}

/* _join_url: base gets a trailing "/", prefix loses its "/" edges, then urljoin. */
static json_t *a2a_join_url_json(const json_t *base, const json_t *prefix) {
    char *b = a2a_trim_owned(a2a_or_empty(base), NULL);
    char *pfx = a2a_trim_owned(a2a_or_empty(prefix), "/");
    char *ref, *joined;
    a2a_sbuf sb;
    json_t *out;

    if (!b || !*b) { free(b); b = a2a_dup("/"); }
    if (b[strlen(b) - 1] != '/') {
        size_t n = strlen(b);
        char *t = (char *)realloc(b, n + 2);
        if (t) { b = t; b[n] = '/'; b[n + 1] = '\0'; }
    }
    if (!pfx || !*pfx) {
        out = json_string(b ? b : "/");
        free(pfx);
        free(b);
        return out;
    }
    sb_init(&sb);
    sb_add(&sb, pfx);
    sb_addc(&sb, '/');
    ref = sb_take(&sb);
    sb_drop(&sb);

    joined = a2a_urljoin(b ? b : "/", ref ? ref : "");
    out = json_string(joined ? joined : "");
    free(joined);
    free(ref);
    free(pfx);
    free(b);
    return out;
}

/* --- JSON-RPC framing --------------------------------------------------- */

static json_t *a2a_err(const json_t *req_id, int code, const char *message) {
    json_t *env = json_object();
    json_t *err = json_object();
    json_set(env, "jsonrpc", json_string("2.0"));
    json_set(env, "id", req_id ? json_copy(req_id) : json_null());
    json_set(err, "code", json_number((double)code));
    json_set(err, "message", json_string(message ? message : ""));
    json_set(env, "error", err);
    return env;
}

/* --- served-agent fixture view ------------------------------------------ */

static size_t a2a_agents_len(const json_t *agents) {
    return agents && agents->type == JSON_ARRAY ? json_len(agents) : 0;
}

static const json_t *a2a_agent_at(const json_t *agents, size_t i) {
    const json_t *a = json_get(agents, i);
    return a && a->type == JSON_OBJECT ? a : NULL;
}

/* self._agents[""] */
static const json_t *a2a_root_agent(const json_t *agents) {
    size_t n = a2a_agents_len(agents), i;
    for (i = 0; i < n; i++) {
        const json_t *a = a2a_agent_at(agents, i);
        if (a && !a2a_truthy(json_obj_get(a, "slug"))) return a;
    }
    return a2a_agent_at(agents, 0);
}

static const json_t *a2a_agent_by_slug(const json_t *agents, const json_t *slug) {
    char *want = a2a_or_empty(slug);
    size_t n = a2a_agents_len(agents), i;
    const json_t *found = NULL;
    if (!want) return a2a_root_agent(agents);
    for (i = 0; i < n && !found; i++) {
        const json_t *a = a2a_agent_at(agents, i);
        char *s;
        if (!a) continue;
        s = a2a_key_or_empty(a, "slug");
        if (s && !strcmp(s, want)) found = a;
        free(s);
    }
    free(want);
    return found ? found : a2a_root_agent(agents);
}

/* urlsplit(raw_path).path or "/" */
static char *a2a_urlsplit_path(const char *raw) {
    size_t cut, end, start = 0;
    char *out;

    if (!raw || !*raw) return a2a_dup("/");
    cut = strcspn(raw, "#");          /* urlsplit drops the fragment first */
    end = strcspn(raw, "?");          /* then the query */
    if (end > cut) end = cut;
    if (end >= 2 && raw[0] == '/' && raw[1] == '/') {
        /* network-path reference: everything up to the next "/" is the netloc */
        start = 2;
        while (start < end && raw[start] != '/') start++;
    }
    if (start >= end) return a2a_dup("/");
    out = (char *)malloc(end - start + 1);
    if (!out) return a2a_dup("/");
    memcpy(out, raw + start, end - start);
    out[end - start] = '\0';
    return out;
}

/* A2AAdapter._route_for_path: longest path prefix wins, root is the fallback.
 * Returns the routed agent (borrowed) and a malloc'd subpath. */
static const json_t *a2a_route_path(const json_t *agents, const char *path, char **subpath_out) {
    size_t n = a2a_agents_len(agents), i, j;
    size_t *idx;
    const json_t *hit = NULL;
    size_t hit_len = 0;

    *subpath_out = NULL;
    idx = n ? (size_t *)calloc(n, sizeof(size_t)) : NULL;
    if (!idx) {
        *subpath_out = a2a_dup(*path ? path : "/");
        return a2a_root_agent(agents);
    }
    for (i = 0; i < n; i++) idx[i] = i;
    /* sorted(key=len(path), reverse=True) is stable -> insertion sort by length */
    for (i = 1; i < n; i++) {
        size_t cur = idx[i];
        char *ap = a2a_key_or_empty(a2a_agent_at(agents, cur), "path");
        size_t alen = ap ? strlen(ap) : 0;
        free(ap);
        j = i;
        while (j > 0) {
            char *bp = a2a_key_or_empty(a2a_agent_at(agents, idx[j - 1]), "path");
            size_t blen = bp ? strlen(bp) : 0;
            free(bp);
            if (blen >= alen) break;
            idx[j] = idx[j - 1];
            j--;
        }
        idx[j] = cur;
    }
    for (i = 0; i < n && !hit; i++) {
        const json_t *a = a2a_agent_at(agents, idx[i]);
        char *prefix;
        size_t plen;
        if (!a) continue;
        prefix = a2a_key_or_empty(a, "path");
        if (!prefix) continue;
        plen = strlen(prefix);
        if (plen == 0) { free(prefix); continue; }
        if (!strcmp(path, prefix) || (!strncmp(path, prefix, plen) && path[plen] == '/')) {
            hit = a;
            hit_len = plen;
        }
        free(prefix);
    }
    free(idx);

    if (!hit) {
        *subpath_out = a2a_dup(*path ? path : "/");
        return a2a_root_agent(agents);
    }
    *subpath_out = a2a_dup(path[hit_len] ? path + hit_len : "/");
    return hit;
}

/* protocol.skills_from_toolsets static fallback. */
static json_t *a2a_skills_from_toolsets(const json_t *toolsets) {
    char **names = NULL;
    size_t n = 0, i, j;
    json_t *out = json_array();

    if (toolsets && toolsets->type == JSON_ARRAY && json_len(toolsets) > 0) {
        size_t cap = json_len(toolsets);
        names = (char **)calloc(cap ? cap : 1, sizeof(char *));
        if (!names) return out;
        for (i = 0; i < cap; i++) {
            const json_t *v = json_get(toolsets, i);
            char *s;
            int dup = 0;
            if (!a2a_truthy(v)) continue;
            s = a2a_py_str(v);
            if (!s) continue;
            for (j = 0; j < n; j++) if (!strcmp(names[j], s)) { dup = 1; break; }
            if (dup) { free(s); continue; }
            names[n++] = s;
        }
        for (i = 1; i < n; i++) {          /* insertion sort == Python sorted() */
            char *cur = names[i];
            j = i;
            while (j > 0 && strcmp(names[j - 1], cur) > 0) { names[j] = names[j - 1]; j--; }
            names[j] = cur;
        }
    }
    for (i = 0; i < n; i++) {
        json_t *sk = json_object();
        json_t *tags = json_array();
        a2a_sbuf b;
        sb_init(&b);
        sb_add(&b, "toolset.");
        sb_add(&b, names[i]);
        json_set(sk, "id", js_take(&b));
        json_set(sk, "name", json_string(names[i]));
        sb_init(&b);
        sb_add(&b, "Hermes '");
        sb_add(&b, names[i]);
        sb_add(&b, "' capabilities");
        json_set(sk, "description", js_take(&b));
        json_append(tags, json_string(names[i]));
        json_set(sk, "tags", tags);
        json_append(out, sk);
        free(names[i]);
    }
    free(names);
    if (json_len(out) == 0) {
        json_t *sk = json_object();
        json_t *tags = json_array();
        json_set(sk, "id", json_string("general"));
        json_set(sk, "name", json_string("general"));
        json_set(sk, "description", json_string("General-purpose conversational agent"));
        json_append(tags, json_string("general"));
        json_set(sk, "tags", tags);
        json_append(out, sk);
    }
    return out;
}

/* ======================= gate p1 ======================================== */

json_t *w29_adapter_0e0bda_constants(void) {
    json_t *o = json_object();
    json_set(o, "default_port", json_number(A2A_DEFAULT_PORT));
    json_set(o, "min_orphan_timeout", json_number(A2A_MIN_ORPHAN_TIMEOUT));
    json_set(o, "max_orphan_timeout", json_number(A2A_MAX_ORPHAN_TIMEOUT));
    json_set(o, "watchdog_interval", json_number(A2A_WATCHDOG_INTERVAL));
    json_set(o, "max_body", json_number(A2A_MAX_BODY));
    json_set(o, "sse_keepalive", json_number(A2A_SSE_KEEPALIVE));
    json_set(o, "max_forwarded_inputs", json_number(A2A_MAX_FORWARDED_INPUTS));
    json_set(o, "default_description", json_string(A2A_DEFAULT_DESCRIPTION));
    return o;
}

/* float(os.getenv("A2A_REPLY_TIMEOUT", "300")) on a captured env value. */
static double a2a_reply_timeout_value(const json_t *env_value) {
    const char *raw;
    char *end = NULL;
    double parsed;

    if (!env_value || env_value->type == JSON_NULL) return 300.0;
    if (env_value->type == JSON_NUMBER) {
        parsed = env_value->num_val;
        if (parsed != parsed) return 300.0;
        return parsed > 1.0 ? parsed : 1.0;
    }
    if (env_value->type != JSON_STRING) return 300.0;
    raw = env_value->str_val ? env_value->str_val : "";
    while (*raw == ' ' || *raw == '\t' || *raw == '\n' || *raw == '\r') raw++;
    if (!*raw) return 300.0;
    parsed = strtod(raw, &end);
    if (!end) return 300.0;
    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
    if (*end != '\0') return 300.0;
    if (parsed != parsed || parsed == HUGE_VAL || parsed == -HUGE_VAL) return 300.0;
    return parsed > 1.0 ? parsed : 1.0;
}

json_t *w29_adapter_0e0bda_reply_timeout(const json_t *env_value) {
    return json_number(a2a_reply_timeout_value(env_value));
}

json_t *w29_adapter_0e0bda_orphan_timeout(const json_t *env_value) {
    double t = a2a_reply_timeout_value(env_value);
    if (t < A2A_MIN_ORPHAN_TIMEOUT) t = A2A_MIN_ORPHAN_TIMEOUT;
    if (t > A2A_MAX_ORPHAN_TIMEOUT) t = A2A_MAX_ORPHAN_TIMEOUT;
    return json_number(t);
}

json_t *w29_adapter_0e0bda_clean_slug(const json_t *value) {
    char *raw = a2a_or_empty(value);
    char *trimmed = a2a_trim(raw, "/");
    json_t *out;

    if (!raw || !trimmed) { free(raw); free(trimmed); return json_string(""); }
    if (!*trimmed || !strcmp(trimmed, "default") || !strcmp(trimmed, "root")) {
        out = json_string("");
    } else {
        char *slash = strchr(trimmed, '/');
        if (slash) *slash = '\0';
        out = json_string(trimmed);
    }
    free(raw);
    free(trimmed);
    return out;
}

json_t *w29_adapter_0e0bda_join_url(const json_t *base, const json_t *prefix) {
    return a2a_join_url_json(base, prefix);
}

json_t *w29_adapter_0e0bda_safe_context_slug(const json_t *value, const json_t *max_len) {
    char *raw = a2a_or_empty(value);
    a2a_sbuf b;
    size_t i, start, end, limit = 96;
    json_t *out;

    if (!raw) return json_string("ctx");
    if (max_len && max_len->type == JSON_NUMBER && max_len->num_val >= 0)
        limit = (size_t)max_len->num_val;

    sb_init(&b);
    for (i = 0; raw[i]; i++) {
        char c = raw[i];
        int safe = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
        if (safe) sb_addc(&b, c);
        else if (!b.p || b.p[b.len - 1] != '-') sb_addc(&b, '-');
    }
    free(raw);

    start = 0;
    end = b.len;
    while (start < end && (b.p[start] == '-' || b.p[start] == '.' || b.p[start] == '_')) start++;
    while (end > start && (b.p[end - 1] == '-' || b.p[end - 1] == '.' || b.p[end - 1] == '_')) end--;

    if (end == start) {
        char *ctx = a2a_dup("ctx");
        if (ctx && strlen(ctx) > limit) ctx[limit] = '\0';
        out = json_string(ctx ? ctx : "");
        free(ctx);
    } else {
        size_t n;
        char *slug;
        if (end - start > limit) end = start + limit;
        n = end - start;
        slug = (char *)malloc(n + 1);
        if (slug) { memcpy(slug, b.p + start, n); slug[n] = '\0'; }
        out = json_string(slug ? slug : "");
        free(slug);
    }
    sb_drop(&b);
    return out;
}

json_t *w29_adapter_0e0bda_server_class(const json_t *host) {
    char *h = a2a_or_empty(host);
    json_t *out = json_string(h && strchr(h, ':') ? "v6" : "v4");
    free(h);
    return out;
}

json_t *w29_adapter_0e0bda_client_ip(const json_t *addr) {
    char *ip = a2a_or_empty(addr);
    json_t *out;
    if (!ip) return json_string("");
    out = (!strncmp(ip, "::ffff:", 7) && strchr(ip, '.')) ? json_string(ip + 7) : json_string(ip);
    free(ip);
    return out;
}

/* ======================= gate p2 ======================================== */

json_t *w29_adapter_0e0bda_base_url(const json_t *host, const json_t *port, const json_t *public_url) {
    char *pub = a2a_trim_owned(a2a_or_empty(public_url), NULL);
    char *h;
    a2a_sbuf b;
    json_t *out;

    if (pub && *pub) {
        out = json_string(pub);
        free(pub);
        return out;
    }
    free(pub);

    h = a2a_or_empty(host);
    sb_init(&b);
    sb_add(&b, "http://");
    if (h && strchr(h, ':')) {
        sb_addc(&b, '[');
        sb_add(&b, h);
        sb_addc(&b, ']');
    } else {
        sb_add(&b, h ? h : "");
    }
    sb_addc(&b, ':');
    if (port && port->type == JSON_NUMBER) {
        char num[64];
        if (port->num_val == (double)(long long)port->num_val)
            snprintf(num, sizeof num, "%lld", (long long)port->num_val);
        else
            snprintf(num, sizeof num, "%.17g", port->num_val);
        sb_add(&b, num);
    } else {
        char *p = a2a_or_empty(port);
        sb_add(&b, p ? p : "");
        free(p);
    }
    sb_add(&b, "/");
    out = js_take(&b);
    free(h);
    return out;
}

json_t *w29_adapter_0e0bda_route_for_path(const json_t *agents, const json_t *raw_path) {
    char *raw = a2a_or_empty(raw_path);
    char *path = a2a_urlsplit_path(raw);
    char *sub = NULL;
    const json_t *agent = a2a_route_path(agents, path ? path : "/", &sub);
    char *slug = a2a_key_or_empty(agent, "slug");
    json_t *out = json_object();

    json_set(out, "slug", json_string(slug ? slug : ""));
    json_set(out, "subpath", json_string(sub ? sub : "/"));
    free(slug);
    free(sub);
    free(path);
    free(raw);
    return out;
}

json_t *w29_adapter_0e0bda_route_for_request(const json_t *agents, const json_t *raw_path, const json_t *params) {
    char *raw = a2a_or_empty(raw_path);
    char *path = a2a_urlsplit_path(raw);
    char *sub = NULL;
    const json_t *agent = a2a_route_path(agents, path ? path : "/", &sub);
    char *tenant = a2a_key_or_empty(params, "tenant");
    json_t *out = NULL;

    if (tenant && *tenant) {
        char *slug = a2a_key_or_empty(agent, "slug");
        if (slug && !*slug) {
            size_t n = a2a_agents_len(agents), i;
            for (i = 0; i < n; i++) {
                const json_t *a = a2a_agent_at(agents, i);
                char *t;
                if (!a) continue;
                t = a2a_key_or_empty(a, "tenant");
                if (t && !strcmp(t, tenant)) { agent = a; free(t); break; }
                free(t);
            }
        }
        free(slug);
    }

    {
        char *expected = a2a_key_or_empty(agent, "tenant");
        if (tenant && *tenant && expected && *expected && strcmp(tenant, expected)) {
            char *slug = a2a_key_or_empty(agent, "slug");
            char *trepr = a2a_py_repr(tenant);
            a2a_sbuf b;
            sb_init(&b);
            sb_add(&b, "tenant ");
            sb_add(&b, trepr ? trepr : "''");
            sb_add(&b, " does not match routed agent ");
            sb_add(&b, slug && *slug ? slug : "default");
            out = json_object();
            json_set(out, "error", js_take(&b));
            free(trepr);
            free(slug);
        }
        free(expected);
    }

    if (!out) {
        char *slug = a2a_key_or_empty(agent, "slug");
        out = json_object();
        json_set(out, "slug", json_string(slug ? slug : ""));
        json_set(out, "subpath", json_string(sub ? sub : "/"));
        free(slug);
    }
    free(tenant);
    free(sub);
    free(path);
    free(raw);
    return out;
}

json_t *w29_adapter_0e0bda_served_agent_summary(const json_t *agents, const json_t *host,
                                                const json_t *port, const json_t *public_url) {
    json_t *base = w29_adapter_0e0bda_base_url(host, port, public_url);
    char *base_s = a2a_py_str(base);
    json_t *out = json_array();
    size_t n = a2a_agents_len(agents), i;

    for (i = 0; i < n; i++) {
        const json_t *a = a2a_agent_at(agents, i);
        json_t *row, *ju;
        char *slug, *path, *url;
        const json_t *name, *tenant, *profile, *local;

        if (!a) continue;
        slug = a2a_key_or_empty(a, "slug");
        path = a2a_key_or_empty(a, "path");
        ju = a2a_join_url_json(json_string(base_s ? base_s : ""), json_string(path ? path : ""));
        url = a2a_py_str(ju);

        row = json_object();
        json_set(row, "slug", json_string(slug && *slug ? slug : "default"));
        name = json_obj_get(a, "name");
        json_set(row, "name", name ? json_copy(name) : json_null());
        json_set(row, "url", json_string(url ? url : ""));
        tenant = json_obj_get(a, "tenant");
        json_set(row, "tenant", a2a_truthy(tenant) ? json_copy(tenant) : json_null());
        profile = json_obj_get(a, "profile");
        json_set(row, "profile", profile ? json_copy(profile) : json_null());
        local = json_obj_get(a, "local");
        json_set(row, "local", json_bool(local ? a2a_truthy(local) : 0));
        json_append(out, row);

        json_free(ju);
        free(url);
        free(path);
        free(slug);
    }
    free(base_s);
    json_free(base);
    return out;
}

json_t *w29_adapter_0e0bda_scope_for_agent(const json_t *agents, const json_t *agent_slug) {
    const json_t *agent = a2a_agent_by_slug(agents, agent_slug);
    char *slug = a2a_key_or_empty(agent, "slug");
    char *tenant = a2a_key_or_empty(agent, "tenant");
    json_t *out = json_array();

    json_append(out, json_string(slug ? slug : ""));
    json_append(out, json_string(tenant ? tenant : ""));
    free(slug);
    free(tenant);
    return out;
}

json_t *w29_adapter_0e0bda_build_card(const json_t *agents, const json_t *agent_slug,
                                      const json_t *host, const json_t *port,
                                      const json_t *public_url, const json_t *agent_name,
                                      const json_t *localhost_only,
                                      const json_t *provider_org, const json_t *provider_url) {
    const json_t *agent = a2a_agent_by_slug(agents, agent_slug);
    json_t *base = w29_adapter_0e0bda_base_url(host, port, public_url);
    char *base_s = a2a_py_str(base);
    char *agent_path = a2a_key_or_empty(agent, "path");
    json_t *url_j = a2a_join_url_json(json_string(base_s ? base_s : ""),
                                      json_string(agent_path ? agent_path : ""));
    char *url = a2a_py_str(url_j);
    char *name = a2a_key_or_empty(agent, "name");
    char *desc = a2a_key_or_empty(agent, "description");
    char *tenant = a2a_key_or_empty(agent, "tenant");
    const json_t *local = agent ? json_obj_get(agent, "local") : NULL;
    int streaming = local ? a2a_truthy(local) : 1;
    int auth_required = !a2a_truthy(localhost_only);
    json_t *card, *iface, *ifaces, *caps, *provider, *skills;

    if (!url || !*url) { free(url); url = a2a_dup(base_s ? base_s : ""); }
    if (!name || !*name) { free(name); name = a2a_or_empty(agent_name); }
    if (!desc || !*desc) { free(desc); desc = a2a_dup(A2A_DEFAULT_DESCRIPTION); }

    skills = a2a_skills_from_toolsets(agent ? json_obj_get(agent, "advertised_toolsets") : NULL);

    card = json_object();
    json_set(card, "name", json_string(name));
    json_set(card, "description", json_string(desc));
    json_set(card, "url", json_string(url));
    json_set(card, "version", json_string("1.0.0"));

    provider = json_object();
    json_set(provider, "organization",
             json_string(provider_org && provider_org->type == JSON_STRING
                             ? (provider_org->str_val ? provider_org->str_val : "")
                             : "Hermes Agent"));
    json_set(provider, "url", json_string(a2a_truthy(provider_url)
                                              ? (provider_url->str_val ? provider_url->str_val : "")
                                              : url));
    json_set(card, "provider", provider);

    iface = json_object();
    json_set(iface, "url", json_string(url));
    json_set(iface, "protocolBinding", json_string("JSONRPC"));
    json_set(iface, "protocolVersion", json_string("1.0"));
    if (tenant && *tenant) json_set(iface, "tenant", json_string(tenant));
    ifaces = json_array();
    json_append(ifaces, iface);
    json_set(card, "supportedInterfaces", ifaces);

    caps = json_object();
    json_set(caps, "streaming", json_bool(streaming));
    json_set(caps, "pushNotifications", json_bool(1));
    json_set(caps, "stateTransitionHistory", json_bool(0));
    json_set(caps, "extendedAgentCard", json_bool(0));
    json_set(card, "capabilities", caps);

    {
        json_t *modes_in = json_array(), *modes_out = json_array();
        json_append(modes_in, json_string("text/plain"));
        json_append(modes_out, json_string("text/plain"));
        json_set(card, "defaultInputModes", modes_in);
        json_set(card, "defaultOutputModes", modes_out);
    }
    json_set(card, "skills", skills);

    if (auth_required) {
        json_t *schemes = json_object(), *scheme = json_object();
        json_t *security = json_array(), *entry = json_object();
        json_set(scheme, "type", json_string("http"));
        json_set(scheme, "scheme", json_string("bearer"));
        json_set(schemes, "bearer", scheme);
        json_set(card, "securitySchemes", schemes);
        json_set(entry, "bearer", json_array());
        json_append(security, entry);
        json_set(card, "security", security);
    }

    free(base_s);
    free(url);
    free(name);
    free(desc);
    free(tenant);
    free(agent_path);
    json_free(url_j);
    json_free(base);
    return card;
}

/* ======================= gate p3 ======================================== */

typedef struct { const char *method; const char *handler; int is_v1; } a2a_method_row;

/* adapter.py:_METHOD_TABLE flattened; the alias order fixes isV1. */
static const a2a_method_row A2A_METHODS[] = {
    {"SendMessage", "_rpc_message_send", 1},
    {"message/send", "_rpc_message_send", 0},
    {"SendStreamingMessage", "_rpc_message_stream", 1},
    {"message/stream", "_rpc_message_stream", 0},
    {"GetTask", "_rpc_tasks_get", 1},
    {"tasks/get", "_rpc_tasks_get", 0},
    {"ListTasks", "_rpc_tasks_list", 1},
    {"tasks/list", "_rpc_tasks_list", 0},
    {"CancelTask", "_rpc_tasks_cancel", 1},
    {"tasks/cancel", "_rpc_tasks_cancel", 0},
    {"SubscribeToTask", "_rpc_tasks_subscribe", 1},
    {"tasks/subscribe", "_rpc_tasks_subscribe", 0},
    {"CreateTaskPushNotificationConfig", "_rpc_push_config_create", 1},
    {"tasks/pushNotificationConfig/create", "_rpc_push_config_create", 0},
    {"tasks/pushNotificationConfig/set", "_rpc_push_config_create", 0},
    {"tasks/pushNotification/set", "_rpc_push_config_create", 0},
    {"GetTaskPushNotificationConfig", "_rpc_push_config_get", 1},
    {"tasks/pushNotificationConfig/get", "_rpc_push_config_get", 0},
    {"ListTaskPushNotificationConfigs", "_rpc_push_config_list", 1},
    {"tasks/pushNotificationConfig/list", "_rpc_push_config_list", 0},
    {"DeleteTaskPushNotificationConfig", "_rpc_push_config_delete", 1},
    {"tasks/pushNotificationConfig/delete", "_rpc_push_config_delete", 0},
};

json_t *w29_adapter_0e0bda_method_lookup(const json_t *method) {
    size_t i, n = sizeof A2A_METHODS / sizeof A2A_METHODS[0];
    char *want = a2a_or_empty(method);
    json_t *out = NULL;

    if (!want) return json_null();
    for (i = 0; i < n && !out; i++) {
        if (!strcmp(want, A2A_METHODS[i].method)) {
            out = json_object();
            json_set(out, "handler", json_string(A2A_METHODS[i].handler));
            json_set(out, "isV1", json_bool(A2A_METHODS[i].is_v1));
        }
    }
    free(want);
    return out ? out : json_null();
}

/* str(params.get("taskId") or params.get("id") or "") */
static char *a2a_task_id_of(const json_t *params) {
    const json_t *tid = params && params->type == JSON_OBJECT ? json_obj_get(params, "taskId") : NULL;
    const json_t *alt;

    if (a2a_truthy(tid)) return a2a_py_str(tid);
    alt = params && params->type == JSON_OBJECT ? json_obj_get(params, "id") : NULL;
    if (a2a_truthy(alt)) return a2a_py_str(alt);
    return a2a_dup("");
}

json_t *w29_adapter_0e0bda_find_task_error(const json_t *req_id, const json_t *params) {
    char *task_id = a2a_task_id_of(params);
    a2a_sbuf b;
    json_t *out;

    sb_init(&b);
    sb_add(&b, "task not found: ");
    sb_add(&b, task_id ? task_id : "");
    out = a2a_err(req_id, -32001, sb_take(&b));
    sb_drop(&b);
    free(task_id);
    return out;
}

json_t *w29_adapter_0e0bda_push_op_error(const json_t *req_id, const json_t *params) {
    char *task_id = a2a_key_or_empty(params, "taskId");
    json_t *out;

    if (!task_id || !*task_id) {
        out = a2a_err(req_id, -32602, "taskId required");
    } else {
        a2a_sbuf b;
        sb_init(&b);
        sb_add(&b, "push config not found for task: ");
        sb_add(&b, task_id);
        out = a2a_err(req_id, -32001, sb_take(&b));
        sb_drop(&b);
    }
    free(task_id);
    return out;
}

json_t *w29_adapter_0e0bda_push_create_error(const json_t *req_id, const json_t *params) {
    char *task_id = a2a_key_or_empty(params, "taskId");
    const json_t *cfg = params && params->type == JSON_OBJECT
                            ? json_obj_get(params, "pushNotificationConfig") : NULL;
    char *url;
    json_t *out;

    if (!a2a_truthy(cfg) && params && params->type == JSON_OBJECT)
        cfg = json_obj_get(params, "config");
    url = cfg && cfg->type == JSON_OBJECT ? a2a_or_empty(json_obj_get(cfg, "url")) : a2a_dup("");

    if (!task_id || !*task_id || !url || !*url) {
        out = a2a_err(req_id, -32602, "taskId and pushNotificationConfig.url required");
    } else {
        a2a_sbuf b;
        sb_init(&b);
        sb_add(&b, "task not found: ");
        sb_add(&b, task_id);
        out = a2a_err(req_id, -32001, sb_take(&b));
        sb_drop(&b);
    }
    free(url);
    free(task_id);
    return out;
}

json_t *w29_adapter_0e0bda_inline_push_url(const json_t *params) {
    const json_t *conf = params && params->type == JSON_OBJECT
                             ? json_obj_get(params, "configuration") : NULL;
    const json_t *cfg = a2a_truthy(conf) && conf->type == JSON_OBJECT
                            ? json_obj_get(conf, "taskPushNotificationConfig") : NULL;
    char *out;

    if (!a2a_truthy(cfg)) return json_string("");
    if (cfg->type != JSON_OBJECT) return json_string("");  /* live code raises here */
    out = a2a_or_empty(json_obj_get(cfg, "url"));
    if (!out || !*out) {
        const json_t *inner = json_obj_get(cfg, "pushNotificationConfig");
        free(out);
        out = a2a_or_empty(inner && inner->type == JSON_OBJECT ? json_obj_get(inner, "url") : NULL);
    }
    {
        json_t *res = json_string(out ? out : "");
        free(out);
        return res;
    }
}

json_t *w29_adapter_0e0bda_note_forwarded(const json_t *order, const json_t *input_id,
                                          const json_t *max_entries) {
    size_t n = order && order->type == JSON_ARRAY ? json_len(order) : 0;
    size_t i, count = 0, max = A2A_MAX_FORWARDED_INPUTS;
    char **items = (char **)calloc(n + 1, sizeof(char *));
    char *id = a2a_py_str(input_id);
    int seen = 0;
    json_t *out = json_object(), *arr = json_array();

    if (!items || !id) { free(items); free(id); json_free(arr); return out; }
    if (max_entries && max_entries->type == JSON_NUMBER && max_entries->num_val >= 0)
        max = (size_t)max_entries->num_val;

    for (i = 0; i < n; i++) {
        const json_t *v = json_get(order, i);
        char *s;
        if (!v || v->type == JSON_NULL) continue;
        s = a2a_py_str(v);
        if (!s) continue;
        if (!strcmp(s, id)) { seen = 1; free(s); continue; }  /* move_to_end == drop here */
        items[count++] = s;
    }
    items[count++] = a2a_dup(id);
    while (count > max) {
        free(items[0]);
        memmove(items, items + 1, (count - 1) * sizeof(char *));
        count--;
    }
    json_set(out, "seen", json_bool(seen));
    for (i = 0; i < count; i++) {
        json_append(arr, json_string(items[i]));
        free(items[i]);
    }
    json_set(out, "order", arr);
    free(items);
    free(id);
    return out;
}

json_t *w29_adapter_0e0bda_forget_forwarded(const json_t *order, const json_t *input_id) {
    size_t n = order && order->type == JSON_ARRAY ? json_len(order) : 0;
    size_t i, count = 0;
    char **items = (char **)calloc(n + 1, sizeof(char *));
    char *id = a2a_or_empty(input_id);
    json_t *out = json_object(), *arr = json_array();

    if (!items || !id) { free(items); free(id); json_free(arr); return out; }
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(order, i);
        char *s;
        if (!v || v->type == JSON_NULL) continue;
        s = a2a_py_str(v);
        if (!s) continue;
        if (!strcmp(s, id)) { free(s); continue; }
        items[count++] = s;
    }
    for (i = 0; i < count; i++) {
        json_append(arr, json_string(items[i]));
        free(items[i]);
    }
    json_set(out, "order", arr);
    free(items);
    free(id);
    return out;
}

json_t *w29_adapter_0e0bda_chat_info(const json_t *chat_id) {
    char *cid = a2a_py_str(chat_id);
    a2a_sbuf b;
    json_t *out = json_object();

    sb_init(&b);
    sb_add(&b, "a2a:");
    sb_add(&b, cid ? cid : "");
    json_set(out, "name", js_take(&b));
    json_set(out, "type", json_string("dm"));
    free(cid);
    return out;
}
