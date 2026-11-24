/*
 * sandbox_conformance.c - C11 port of upstream
 * tools/environments/sandbox_conformance.py, gates w33_sandboxcon_be5b73_p1
 * and w33_sandboxcon_be5b73_p2.
 *
 * The Python module runs ten isolation checks against a hosted backend: it
 * binds a scope, drives the real tools through the registry and asserts one
 * invariant each time.  Every one of those checks is an interaction: a
 * provider that records calls, a tool registry, a filesystem, subprocesses, a
 * clock and a completion queue.  What is ported here is the part that is a
 * pure function of values: the identity/scope shaping a run is planned with
 * (p1), the ordered check table and the literal capability table (p1), and the
 * host-observation predicates plus the verdict of the two "never touch the
 * host" invariants (p2).
 *
 * Seams (fixture-fed, deliberately not ported): RecordingSandboxProvider /
 * RecordingSandboxEnvironment (subprocess.Popen, uuid4 planting, a uid echo),
 * the tool registry dispatch and bind_state, ProcessSession and the
 * completion-queue drains, the idle reaper, tool_result_storage and the image
 * resolver.  sys.addaudithook and the os.stat/lstat/readlink wrapping are the
 * observation seam: the reports they would emit are fixture inputs here.
 *
 * PoP: w33_sandboxcon_be5b73_p1_hosted_scope @ tools/environments/sandbox_conformance.py:hosted_scope_for
 * PoP: w33_sandboxcon_be5b73_p1_marker @ tools/environments/sandbox_conformance.py:_marker
 * PoP: w33_sandboxcon_be5b73_p1_identity_key @ tools/environments/sandbox_conformance.py:_identity (SandboxIdentity.key)
 * PoP: w33_sandboxcon_be5b73_p1_checks_table @ tools/environments/sandbox_conformance.py:checks
 * PoP: w33_sandboxcon_be5b73_p1_constants @ tools/environments/sandbox_conformance.py:STUB_BACKEND/PERSISTED_MARK/_METADATA_CALLS
 * PoP: w33_sandboxcon_be5b73_p2_watched @ tools/environments/sandbox_conformance.py:_watched
 * PoP: w33_sandboxcon_be5b73_p2_audit @ tools/environments/sandbox_conformance.py:_audit
 * PoP: w33_sandboxcon_be5b73_p2_audit_events @ tools/environments/sandbox_conformance.py:_audit (event test)
 * PoP: w33_sandboxcon_be5b73_p2_host_touch_verdict @ tools/environments/sandbox_conformance.py:check_file_tools_never_touch_host + check_host_backend_refused
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w33_sandboxcon_be5b73_p1.h"
#include "noc_w33_sandboxcon_be5b73_p2.h"

/* ------------------------------------------------------------------ */
/* small helpers                                                       */

static int sc_truthy(const json_t *v)
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

/* ------------------------------------------------------------------ */
/* SHA-1, for uuid5 (upstream _marker)                                 */

typedef struct {
    uint32_t h[5];
    uint64_t total;
    unsigned char buf[64];
    size_t used;
} sc_sha1;

static uint32_t sc_rol(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

static void sc_sha1_block(sc_sha1 *s, const unsigned char *p)
{
    uint32_t w[80];
    uint32_t a, b, c, d, e;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (i = 16; i < 80; i++)
        w[i] = sc_rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4];
    for (i = 0; i < 80; i++) {
        uint32_t f, k, t;
        if (i < 20)      { f = (b & c) | ((~b) & d);            k = 0x5A827999u; }
        else if (i < 40) { f = b ^ c ^ d;                        k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d);      k = 0x8F1BBCDCu; }
        else             { f = b ^ c ^ d;                        k = 0xCA62C1D6u; }
        t = sc_rol(a, 5) + f + e + k + w[i];
        e = d; d = c; c = sc_rol(b, 30); b = a; a = t;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e;
}

static void sc_sha1_init(sc_sha1 *s)
{
    s->h[0] = 0x67452301u; s->h[1] = 0xEFCDAB89u; s->h[2] = 0x98BADCFEu;
    s->h[3] = 0x10325476u; s->h[4] = 0xC3D2E1F0u;
    s->total = 0; s->used = 0;
}

static void sc_sha1_update(sc_sha1 *s, const unsigned char *p, size_t n)
{
    s->total += (uint64_t)n;
    while (n > 0) {
        size_t take = 64 - s->used;
        if (take > n) take = n;
        memcpy(s->buf + s->used, p, take);
        s->used += take; p += take; n -= take;
        if (s->used == 64) { sc_sha1_block(s, s->buf); s->used = 0; }
    }
}

static void sc_sha1_final(sc_sha1 *s, unsigned char out[20])
{
    uint64_t bits = s->total * 8u;
    unsigned char pad[72];
    size_t padlen;
    int i;
    pad[0] = 0x80;
    padlen = (s->used < 56) ? (56 - s->used) : (120 - s->used);
    memset(pad + 1, 0, padlen - 1);
    for (i = 0; i < 8; i++) pad[padlen + (size_t)i] = (unsigned char)(bits >> (56 - 8 * i));
    sc_sha1_update(s, pad, padlen + 8);
    for (i = 0; i < 5; i++) {
        out[i * 4 + 0] = (unsigned char)(s->h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(s->h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(s->h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)(s->h[i]);
    }
}

/* uuid.uuid5(uuid.NAMESPACE_URL, name).hex[:12] - the first 12 hex digits are
 * digest bytes 0..5, so no version/variant bit surgery is reachable. */
static void sc_uuid5_hex12(const char *name, char out[13])
{
    static const unsigned char ns[16] = {
        0x6b, 0xa7, 0xb8, 0x11, 0x9d, 0xad, 0x11, 0xd1,
        0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8,
    };
    static const char hex[] = "0123456789abcdef";
    sc_sha1 s;
    unsigned char d[20];
    size_t i;
    sc_sha1_init(&s);
    sc_sha1_update(&s, ns, sizeof(ns));
    sc_sha1_update(&s, (const unsigned char *)name, strlen(name));
    sc_sha1_final(&s, d);
    for (i = 0; i < 6; i++) {
        out[i * 2] = hex[(d[i] >> 4) & 0xf];
        out[i * 2 + 1] = hex[d[i] & 0xf];
    }
    out[12] = '\0';
}

/* urllib.parse.quote(s, safe=''): unreserved bytes kept, the rest %XX. */
static char *sc_quote(const char *s)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t n = strlen(s), i;
    char *out = (char *)malloc(n * 3 + 1);
    size_t o = 0;
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-' || c == '~') {
            out[o++] = (char)c;
        } else {
            out[o++] = '%';
            out[o++] = hex[(c >> 4) & 0xf];
            out[o++] = hex[c & 0xf];
        }
    }
    out[o] = '\0';
    return out;
}

/* ================================================================== */
/* p1: identities and the plan                                         */

/* hosted_scope_for(agent, *, tenant="acme", user="u1") */
json_t *w33_sandboxcon_be5b73_p1_hosted_scope(const json_t *req)
{
    const char *agent = json_get_str(req, "agent", "");
    const char *tenant = json_get_str(req, "tenant", "acme");
    const char *user = json_get_str(req, "user", "u1");
    json_t *o = json_object();
    json_set(o, "tenant", json_string(tenant));
    json_set(o, "agent", json_string(agent));
    json_set(o, "actor_id", json_string(user));
    json_set(o, "owner_id", json_string(user));
    json_set(o, "can_administer", json_bool(1));
    return o;
}

/* _marker: f"mark{uuid5(NAMESPACE_URL, f'{tenant}/{agent}').hex[:12]}" */
json_t *w33_sandboxcon_be5b73_p1_marker(const json_t *scope)
{
    const char *tenant = json_get_str(scope, "tenant", "");
    const char *agent = json_get_str(scope, "agent", "");
    size_t need = strlen(tenant) + strlen(agent) + 2;
    char *name = (char *)malloc(need);
    char hex[13];
    char mark[17];
    if (!name) return json_string("");
    snprintf(name, need, "%s/%s", tenant, agent);
    sc_uuid5_hex12(name, hex);
    free(name);
    snprintf(mark, sizeof(mark), "mark%s", hex);
    return json_string(mark);
}

/* _identity(scope).key -> f"sandbox:{quote(tenant, safe='')}/{quote(agent, safe='')}" */
json_t *w33_sandboxcon_be5b73_p1_identity_key(const json_t *scope)
{
    const char *tenant = json_get_str(scope, "tenant", "");
    const char *agent = json_get_str(scope, "agent", "");
    char *qt = sc_quote(tenant);
    char *qa = sc_quote(agent);
    size_t need = 8 + strlen(qt ? qt : "") + 1 + strlen(qa ? qa : "") + 1;
    char *key = (char *)malloc(need);
    json_t *out;
    if (!key) { free(qt); free(qa); return json_string(""); }
    snprintf(key, need, "sandbox:%s/%s", qt ? qt : "", qa ? qa : "");
    out = json_string(key);
    free(key);
    free(qt);
    free(qa);
    return out;
}

/* The ordered table upstream checks() yields, with each callable's parameter
 * names (inspect.signature): the two idle checks are wrapped in a lambda, so
 * their first parameter is "p", not "provider". */
struct sc_check {
    const char *name;
    const char *params[3];
    int arity;
};

static const struct sc_check SC_CHECKS[W33_SANDBOXCON_BE5B73_P1_CHECK_COUNT] = {
    { "tools_address_own_sandbox",            { "provider", "a", "b" },        3 },
    { "foreign_handle_refused",               { "provider", "a", "b" },        3 },
    { "file_tools_never_touch_host",          { "provider", "a", "host_dir" }, 3 },
    { "media_never_read_on_host",             { "provider", "a", "host_dir" }, 3 },
    { "background_process_invisible",         { "provider", "a", "b" },        3 },
    { "process_notifications_private",        { "provider", "a", "b" },        3 },
    { "host_tools_refused",                   { "provider", "a", "b" },        3 },
    { "oversized_results_stay_in_sandbox",    { "provider", "a", "b" },        3 },
    { "idle_sandbox_kept_while_working",      { "p", "a", "b" },               3 },
    { "idle_sandbox_suspended_not_destroyed", { "p", "a", "b" },               3 },
};

json_t *w33_sandboxcon_be5b73_p1_checks_table(void)
{
    json_t *arr = json_array();
    size_t i;
    for (i = 0; i < W33_SANDBOXCON_BE5B73_P1_CHECK_COUNT; i++) {
        json_t *row = json_object();
        json_t *params = json_array();
        int j;
        json_set(row, "index", json_number((double)i));
        json_set(row, "name", json_string(SC_CHECKS[i].name));
        for (j = 0; j < SC_CHECKS[i].arity; j++)
            json_append(params, json_string(SC_CHECKS[i].params[j]));
        json_set(row, "params", params);
        json_set(row, "arity", json_number((double)SC_CHECKS[i].arity));
        json_append(arr, row);
    }
    return arr;
}

json_t *w33_sandboxcon_be5b73_p1_constants(void)
{
    json_t *o = json_object();
    json_t *meta = json_array();
    json_set(o, "stub_backend", json_string("recording_sandbox"));
    json_set(o, "persisted_mark", json_string("Full output saved to:"));
    json_append(meta, json_string("stat"));
    json_append(meta, json_string("lstat"));
    json_append(meta, json_string("readlink"));
    json_set(o, "metadata_calls", meta);
    return o;
}

/* ================================================================== */
/* p2: host observation and the never-touch-host verdicts              */

/* A path value as the fixtures carry it: a str; {"bytes": s} for bytes;
 * {"pathlike": s} for an os.PathLike whose __fspath__ is s; {"fd": n} or null
 * for a value the predicate rejects.  text receives the os.fsdecode()d form. */
static int sc_path_text(const json_t *v, char **text)
{
    *text = NULL;
    if (!v) return 0;
    if (v->type == JSON_STRING) {
        *text = strdup(v->str_val ? v->str_val : "");
        return *text != NULL;
    }
    if (v->type == JSON_OBJECT) {
        const char *bs = json_get_str(v, "bytes", NULL);
        const char *pl = json_get_str(v, "pathlike", NULL);
        if (bs) { *text = strdup(bs); return *text != NULL; }
        if (pl) { *text = strdup(pl); return *text != NULL; }
        return 0; /* {"fd": n}: an int is not a path -> "" */
    }
    return 0; /* null / number / array: not a path -> "" */
}

/* Python str(path) as _audit records it: a str is itself, bytes render as
 * b'...'.  Returns malloc'd. */
static char *sc_path_repr(const json_t *v)
{
    if (v && v->type == JSON_STRING)
        return strdup(v->str_val ? v->str_val : "");
    if (v && v->type == JSON_OBJECT) {
        const char *bs = json_get_str(v, "bytes", NULL);
        const char *pl = json_get_str(v, "pathlike", NULL);
        if (bs) {
            size_t n = strlen(bs), i, o = 0;
            /* Python repr picks ' unless the value holds ' and no " */
            char quote = (strchr(bs, '\'') && !strchr(bs, '"')) ? '"' : '\'';
            char *out = (char *)malloc(n * 4 + 5);
            if (!out) return NULL;
            out[o++] = 'b'; out[o++] = quote;
            for (i = 0; i < n; i++) {
                unsigned char c = (unsigned char)bs[i];
                if (c == '\\' || (char)c == quote) { out[o++] = '\\'; out[o++] = (char)c; }
                else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
                else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
                else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
                else if (c >= 0x20 && c < 0x7f) { out[o++] = (char)c; }
                else {
                    static const char hx[] = "0123456789abcdef";
                    out[o++] = '\\'; out[o++] = 'x';
                    out[o++] = hx[(c >> 4) & 0xf];
                    out[o++] = hx[c & 0xf];
                }
            }
            out[o++] = quote; out[o] = '\0';
            return out;
        }
        if (pl) return strdup(pl); /* str() of a __fspath__-only object */
    }
    if (v && v->type == JSON_NULL) return strdup("None");
    if (v && v->type == JSON_NUMBER) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%g", v->num_val);
        return strdup(tmp);
    }
    return strdup("");
}

/* _watched(path): bool(text) and any(text.startswith(root) for root in _WATCHED) */
static int sc_path_watched(const json_t *path, const json_t *roots)
{
    char *text = NULL;
    size_t i, n;
    int hit = 0;
    if (!sc_path_text(path, &text)) {
        free(text);
        return 0;
    }
    if (!text[0]) { free(text); return 0; }
    n = json_len(roots);
    for (i = 0; i < n && !hit; i++) {
        const json_t *r = json_get(roots, i);
        const char *root = (r && r->type == JSON_STRING) ? r->str_val : NULL;
        if (root && !strncmp(text, root, strlen(root))) hit = 1;
    }
    free(text);
    return hit;
}

json_t *w33_sandboxcon_be5b73_p2_watched(const json_t *req)
{
    return json_bool(sc_path_watched(json_obj_get(req, "path"),
                                     json_obj_get(req, "roots")));
}

/* The audit event test: event in ("open", "os.listdir", "os.scandir"). */
static int sc_audited(const char *event)
{
    return !strcmp(event, "open") || !strcmp(event, "os.listdir") ||
           !strcmp(event, "os.scandir");
}

json_t *w33_sandboxcon_be5b73_p2_audit(const json_t *req)
{
    const json_t *roots = json_obj_get(req, "roots");
    const json_t *events = json_obj_get(req, "events");
    size_t n = json_len(events), i;
    json_t *out = json_array();
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(events, i);
        const char *event = json_get_str(e, "event", "");
        json_t *has_arg = json_obj_get(e, "arg");
        int watched = has_arg ? sc_path_watched(has_arg, roots) : 0;
        int recorded = (json_len(roots) > 0) && sc_audited(event) &&
                       has_arg != NULL && watched;
        json_t *row = json_object();
        json_set(row, "event", json_string(event));
        json_set(row, "watched", json_bool(watched));
        json_set(row, "recorded", json_bool(recorded));
        if (recorded) {
            char *repr = sc_path_repr(has_arg);
            size_t need = strlen(event) + strlen(repr ? repr : "") + 2;
            char *rec = (char *)malloc(need);
            if (rec) {
                snprintf(rec, need, "%s %s", event, repr ? repr : "");
                json_set(row, "record", json_string(rec));
                free(rec);
            } else {
                json_set(row, "record", json_null());
            }
            free(repr);
        } else {
            json_set(row, "record", json_null());
        }
        json_append(out, row);
    }
    return out;
}

json_t *w33_sandboxcon_be5b73_p2_audit_events(void)
{
    json_t *arr = json_array();
    json_append(arr, json_string("open"));
    json_append(arr, json_string("os.listdir"));
    json_append(arr, json_string("os.scandir"));
    return arr;
}

/* c.sandbox == _identity(a) */
static int sc_sandbox_eq(const json_t *call_sandbox, const json_t *identity)
{
    const char *ct = json_get_str(call_sandbox, "tenant", NULL);
    const char *ca = json_get_str(call_sandbox, "agent", NULL);
    const char *it = json_get_str(identity, "tenant", NULL);
    const char *ia = json_get_str(identity, "agent", NULL);
    if (!ct || !ca || !it || !ia) return 0;
    return !strcmp(ct, it) && !strcmp(ca, ia);
}

/* all(o.get("error") or o.get("success") is False or o.get("exit_code", 0) != 0) */
static int sc_refused(const json_t *o)
{
    const json_t *err = json_obj_get(o, "error");
    const json_t *succ = json_obj_get(o, "success");
    const json_t *code;
    if (sc_truthy(err)) return 1;
    if (succ && succ->type == JSON_BOOL && !succ->bool_val) return 1;
    code = json_obj_get(o, "exit_code");
    if (!code) return 0;                    /* .get("exit_code", 0) -> 0 */
    if (code->type == JSON_NULL) return 1;  /* None != 0 */
    if (code->type == JSON_BOOL) return code->bool_val ? 1 : 0;
    if (code->type == JSON_NUMBER) return code->num_val != 0.0;
    return 1;
}

static int sc_stat_eq(const json_t *before, const json_t *after)
{
    static const char *const fields[3] = { "mtime_ns", "size", "ino" };
    int i;
    if (!before || !after) return 0;
    for (i = 0; i < 3; i++) {
        double b = json_get_num(before, fields[i], -1.0);
        double a = json_get_num(after, fields[i], -2.0);
        if (a != b) return 0;
    }
    return 1;
}

json_t *w33_sandboxcon_be5b73_p2_host_touch_verdict(const json_t *obs)
{
    const char *mode = json_get_str(obs, "mode", "file_tools");
    const char *marker = json_get_str(obs, "marker", "");
    const json_t *touched = json_obj_get(obs, "touched");
    const json_t *outputs = json_obj_get(obs, "outputs");
    const json_t *calls = json_obj_get(obs, "calls");
    const json_t *paths = json_obj_get(obs, "paths");
    const json_t *identity = json_obj_get(obs, "agent");
    int file_mode = strcmp(mode, "host_backend") != 0;
    char *dump;
    size_t i, n;

    /* assert not touched, f"a hosted tool touched the host filesystem: ..." */
    if (json_len(touched) > 0) return json_bool(0);

    /* assert marker not in json.dumps(outputs) */
    dump = outputs ? json_dumps(outputs, 0) : NULL;
    if (dump) {
        int leaked = strstr(dump, marker) != NULL;
        free(dump);
        if (leaked) return json_bool(0);
    }

    if (file_mode) {
        /* assert planted.read_text(encoding="utf-8-sig") == f"{marker}\n" */
        const char *content = json_get_str(obs, "content_after", NULL);
        size_t need = strlen(marker) + 2;
        char *want = (char *)malloc(need);
        int same = 0;
        if (want) {
            snprintf(want, need, "%s\n", marker);
            same = content && !strcmp(content, want);
            free(want);
        }
        if (!same) return json_bool(0);

        /* assert (after.st_mtime_ns, after.st_size, after.st_ino) == (before...) */
        if (!sc_stat_eq(json_obj_get(obs, "stat_before"),
                        json_obj_get(obs, "stat_after"))) return json_bool(0);

        /* commands = [c.command for c in calls if c.sandbox == A and c.op == "execute"] */
        n = json_len(paths);
        for (i = 0; i < n; i++) {
            const json_t *p = json_get(paths, i);
            const char *path = (p && p->type == JSON_STRING) ? p->str_val : "";
            size_t k, m = json_len(calls);
            int found = 0;
            for (k = 0; k < m && !found; k++) {
                const json_t *c = json_get(calls, k);
                const char *op = json_get_str(c, "op", "");
                const char *cmd = json_get_str(c, "command", "");
                if (!strcmp(op, "execute") &&
                    sc_sandbox_eq(json_obj_get(c, "sandbox"), identity) &&
                    strstr(cmd, path)) found = 1;
            }
            if (!found) return json_bool(0);
        }

        /* assert any("sandbox copy" in c.stdin or "c2FuZGJveCBjb3B5" in c.stdin ...) */
        {
            size_t k, m = json_len(calls);
            int found = 0;
            for (k = 0; k < m && !found; k++) {
                const json_t *c = json_get(calls, k);
                const char *s = json_get_str(c, "stdin", "");
                if (strstr(s, "sandbox copy") || strstr(s, "c2FuZGJveCBjb3B5")) found = 1;
            }
            if (!found) return json_bool(0);
        }
    } else {
        /* assert all(o.get("error") or o.get("success") is False or
         *           o.get("exit_code", 0) != 0 for o in outputs), outputs */
        n = json_len(outputs);
        for (i = 0; i < n; i++) {
            const json_t *o = json_get(outputs, i);
            if (!o || o->type != JSON_OBJECT) return json_bool(0);
            if (!sc_refused(o)) return json_bool(0);
        }
        /* the planted file (and a symlink to it) is never opened, stat'd or changed */
        if (!sc_stat_eq(json_obj_get(obs, "stat_before"),
                        json_obj_get(obs, "stat_after"))) return json_bool(0);
    }
    return json_bool(1);
}
