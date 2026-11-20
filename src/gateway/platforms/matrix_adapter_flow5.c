/*
 * matrix_adapter_flow5.c -- FLOW port of UNCLAIMED helpers of
 * plugins/platforms/matrix/adapter.py (hermes-agent): outbound/inbound mention
 * handling, media classification, device-key lookup, media-tool invocation,
 * YAML-config bridging, connection state and one-shot recovery-key path.
 *
 * The flow1/flow2/flow3/flow4 siblings and their claimed funcs are NOT
 * re-ported. Every collaborator below (ported, claimed, module or stdlib)
 * enters through the ONE universal seam as a bare module-function name --
 * none is re-ported. All symbols are static and ma5_-prefixed; libjson only.
 *
 * Decision sequences only: every live I/O / injected dependency enters through
 * ma5_call(ctx, name, args). The harness wires it to a recording stub fed the
 * case's scripted results (in.io); the oracle installs recording stubs emitting
 * the identical event shape {"name","args","ret"} from the SAME scripts.
 *
 * Seam name -> live call:
 *   subprocess.run                       -> subprocess.run(cmd, capture_output=..., ...)
 *   _apply_yaml_bridge                   -> _apply_yaml_bridge(matrix_cfg, _YAML_BRIDGE)
 *   get_env_value                        -> gateway_mod.get_env_value(name)
 *   _get_scoped_secret                   -> _get_scoped_secret(name, default)
 *   expanduser                           -> Path(output_file).expanduser()
 *   _is_bot_mentioned                    -> self._is_bot_mentioned(body, formatted_body, user_ids)
 *   _user_localpart                      -> self._user_localpart()
 *   _protect_outbound_mention_regions    -> self._protect_outbound_mention_regions(text)
 *   is_voice_event                       -> is_voice_event(content)
 *   _thread_root                         -> _thread_root(relates_to)
 *   _content_mentions_bot                -> self._content_mentions_bot(body, content)
 *   has_voice_marker                     -> has_voice_marker(source_content)
 *   _resolve_room_identity               -> self._resolve_room_identity(room_id)
 *   query_keys                           -> client.query_keys({mxid: [device_id]})
 *
 * Representation contract (shared with tests/oracle_matrix_adapter_flow5.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; scripted raises
 *   round-trip verbatim when they escape a drive. These funcs have NO try/
 *   except, so every seam raise (Exception or BaseException) propagates.
 * - Attribute carriers are {"__attr__": {attrs}} (config, client, identity,
 *   event). Sets return as sorted arrays (Python set -> canon sorted list).
 * - logger calls are silent. Fail-closed: a missing/exhausted seam script is
 *   a RuntimeError that propagates out of the drive.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- universal seam context -- */
typedef struct ma5_ctx {
    json_t *state;      /* owned; emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} ma5_ctx_t;

/* -- exception / value helpers -- */
static bool ma5_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static json_t *ma5_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type ? type : "RuntimeError");
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *ma5_make_exc(const json_t *spec) {
    return ma5_err(json_get_str(spec, "type", "RuntimeError"),
                   json_get_str(spec, "message", ""));
}
static bool ma5_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool ma5_truthy(const json_t *v) {
    if (ma5_is_null(v)) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *ma5_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *ma5_copy_or_null(const json_t *v) { return v ? json_copy(v) : json_null(); }
/* json_set_hdr is a silent no-op on a NULL value; Python keeps the key as None. */
static void ma5_set_str(json_t *obj, const char *key, const char *s) {
    json_set(obj, key, s ? json_string(s) : json_null());
}
static const json_t *ma5_attr(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    const json_t *in = json_obj_get(obj, "__attr__");
    if (in && in->type == JSON_OBJECT) return json_obj_get(in, key);
    return json_obj_get(obj, key);
}

/* -- growable string buffer -- */
typedef struct { char *b; size_t n, cap; } ma5_buf_t;
static void mb_reserve(ma5_buf_t *b, size_t need) {
    if (b->b && b->n + need + 1 <= b->cap) return;
    size_t nc = b->cap ? b->cap : 32;
    while (nc < b->n + need + 1) nc *= 2;
    char *nb = realloc(b->b, nc);
    if (!nb) return;
    b->b = nb; b->cap = nc;
    if (b->n == 0) b->b[0] = '\0';
}
static void mb_addn(ma5_buf_t *b, const char *s, size_t n) {
    if (!s || n == 0) return;
    mb_reserve(b, n);
    if (!b->b) return;
    memcpy(b->b + b->n, s, n);
    b->n += n; b->b[b->n] = '\0';
}
static void mb_add(ma5_buf_t *b, const char *s) { if (s) mb_addn(b, s, strlen(s)); }
static void mb_addc(ma5_buf_t *b, char c) { mb_reserve(b, 1); if (b->b) { b->b[b->n++] = c; b->b[b->n] = '\0'; } }
static char *mb_take(ma5_buf_t *b) {
    if (!b->b) { char *e = malloc(1); if (e) e[0] = '\0'; return e; }
    b->b[b->n] = '\0';
    char *r = b->b; b->b = NULL; b->n = b->cap = 0; return r;
}
static char *ma5_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s); char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* -- ASCII Python-ish string helpers (fixtures are ASCII) -- */
static bool ma5_is_ws(unsigned char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'||c=='\v';
}
static char *ma5_strip(const char *s) {
    if (!s) return ma5_strdup("");
    while (*s && ma5_is_ws((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && ma5_is_ws((unsigned char)s[n - 1])) n--;
    char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}
static char ma5_tolower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }
static bool ma5_is_word(unsigned char c) {
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';
}
static const char *ma5_strcasestr(const char *h, const char *n) {
    if (!h || !n || !*n) return NULL;
    size_t nl = strlen(n);
    for (const char *p = h; *p; p++) {
        size_t i = 0;
        for (; i < nl; i++) {
            if (!p[i] || ma5_tolower(p[i]) != ma5_tolower(n[i])) break;
        }
        if (i == nl) return p;
    }
    return NULL;
}
static bool ma5_word_search(const char *body, const char *word) {
    if (!body || !word || !*word) return false;
    const char *p = body;
    while ((p = ma5_strcasestr(p, word)) != NULL) {
        bool before = (p == body) ? true : !ma5_is_word((unsigned char)p[-1]);
        size_t wl = strlen(word);
        bool after = (p[wl] == '\0') ? true : !ma5_is_word((unsigned char)p[wl]);
        if (before && after) return true;
        p += 1;
    }
    return false;
}
static char *ma5_replace_all(const char *s, const char *needle, const char *repl) {
    if (!s) return ma5_strdup("");
    if (!needle || !*needle) return ma5_strdup(s);
    ma5_buf_t b = {0};
    const char *p = s;
    while (*p) {
        const char *q = strstr(p, needle);
        if (!q) { mb_add(&b, p); break; }
        mb_addn(&b, p, (size_t)(q - p));
        mb_add(&b, repl);
        p = q + strlen(needle);
    }
    return mb_take(&b);
}
static char *ma5_collapse_spaces(const char *s) {
    ma5_buf_t b = {0}; size_t i = 0, n = strlen(s);
    while (i < n) {
        if (s[i]==' '||s[i]=='\t') { size_t j=i; while(j<n&&(s[j]==' '||s[j]=='\t')) j++; mb_addc(&b,' '); i=j; }
        else { mb_addc(&b, s[i]); i++; }
    }
    return mb_take(&b);
}
static char *ma5_strip_punct_ws(const char *s) {
    ma5_buf_t b = {0}; size_t i = 0, n = strlen(s);
    while (i < n) {
        if (ma5_is_ws((unsigned char)s[i])) {
            size_t j = i; while (j<n && ma5_is_ws((unsigned char)s[j])) j++;
            if (j < n && strchr(",.;:!?", s[j])) { i = j; continue; }
            mb_addn(&b, s + i, j - i); i = j;
        } else { mb_addc(&b, s[i]); i++; }
    }
    return mb_take(&b);
}
static bool ma5_arr_has_str(const json_t *arr, const char *s) {
    if (!s || !arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *v = ma5_str(json_get(arr, i));
        if (v && strcmp(v, s) == 0) return true;
    }
    return false;
}

/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *ma5_call(ma5_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    char msg[256];
    if (!entry) {
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ma5_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ma5_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) ret = ma5_make_exc(raise);
            else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}

/* ===================== ported helpers ===================== */

/* PoP: ma5_run_media_tool @ plugins/platforms/matrix/adapter.py:_run_media_tool */
/* _run_media_tool(cmd, *, timeout, text=False) */
static json_t *ma5_run_media_tool(ma5_ctx_t *ctx, const json_t *args) {
    const json_t *cmd = json_obj_get(args, "cmd");
    json_t *a = json_object();
    json_set(a, "cmd", cmd ? json_copy(cmd) : json_array());
    json_set(a, "timeout", json_number(json_get_num(args, "timeout", 0)));
    json_set(a, "text", json_bool(json_get_bool(args, "text", false)));
    return ma5_call(ctx, "subprocess.run", a);
}

/* PoP: ma5_apply_yaml_config @ plugins/platforms/matrix/adapter.py:_apply_yaml_config */
/* _apply_yaml_config(yaml_cfg, matrix_cfg) -> _apply_yaml_bridge(matrix_cfg, _YAML_BRIDGE) */
static json_t *ma5_apply_yaml_config(ma5_ctx_t *ctx, const json_t *args) {
    const json_t *mc = json_obj_get(args, "matrix_cfg");
    json_t *a = json_object();
    json_set(a, "matrix_cfg", mc ? json_copy(mc) : json_null());
    json_set_hdr(a, "bridge", "YAML_BRIDGE");
    return ma5_call(ctx, "_apply_yaml_bridge", a);
}

/* PoP: ma5_is_connected @ plugins/platforms/matrix/adapter.py:_is_connected */
/* _is_connected(config) */
static json_t *ma5_is_connected(ma5_ctx_t *ctx, const json_t *args) {
    const json_t *cfg = json_obj_get(args, "config");
    const json_t *extra = ma5_attr(cfg, "extra");
    const char *hs = NULL;
    if (extra && extra->type == JSON_OBJECT) hs = ma5_str(json_obj_get(extra, "homeserver"));
    char *hs_owned = NULL;
    if (!hs || !*hs) {
        json_t *a = json_object();
        json_set_hdr(a, "name", "MATRIX_HOMESERVER");
        json_t *r = ma5_call(ctx, "get_env_value", a);
        if (ma5_is_raise(r)) return r;
        hs_owned = ma5_strdup(ma5_str(r));
        hs = hs_owned;
    }
    const char *token = ma5_str(ma5_attr(cfg, "token"));
    char *tok_owned = NULL;
    if (!token || !*token) {
        json_t *a = json_object();
        json_set_hdr(a, "name", "MATRIX_ACCESS_TOKEN");
        json_t *r = ma5_call(ctx, "get_env_value", a);
        if (ma5_is_raise(r)) { free(hs_owned); return r; }
        tok_owned = ma5_strdup(ma5_str(r));
        token = tok_owned;
    }
    if (!token || !*token) {
        json_t *a = json_object();
        json_set_hdr(a, "name", "MATRIX_PASSWORD");
        json_t *r = ma5_call(ctx, "get_env_value", a);
        if (ma5_is_raise(r)) { free(hs_owned); free(tok_owned); return r; }
        free(tok_owned); tok_owned = ma5_strdup(ma5_str(r));
        token = tok_owned;
    }
    char *hs_s = ma5_strip(hs ? hs : "");
    char *tk_s = ma5_strip(token ? token : "");
    bool ok = hs_s[0] && tk_s[0];
    free(hs_s); free(tk_s); free(hs_owned); free(tok_owned);
    return json_bool(ok);
}

/* PoP: ma5_recovery_key_output_path @ plugins/platforms/matrix/adapter.py:_recovery_key_output_path */
/* _recovery_key_output_path() */
static json_t *ma5_recovery_key_output_path(ma5_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = json_object();
    json_set_hdr(a, "name", "MATRIX_RECOVERY_KEY_OUTPUT_FILE");
    json_set_hdr(a, "default", "");
    json_t *r = ma5_call(ctx, "_get_scoped_secret", a);
    if (ma5_is_raise(r)) return r;
    if (ma5_is_null(r)) return ma5_err("AttributeError", "'NoneType' object has no attribute 'strip'");
    char *s = ma5_strip(ma5_str(r));
    if (!s[0]) { free(s); return json_null(); }
    json_t *e = json_object();
    json_set_hdr(e, "path", s);
    free(s);
    return ma5_call(ctx, "expanduser", e);
}

/* PoP: ma5_content_mentions_bot @ plugins/platforms/matrix/adapter.py:_content_mentions_bot */
/* _content_mentions_bot(body, content) -> self._is_bot_mentioned(...) */
static json_t *ma5_content_mentions_bot(ma5_ctx_t *ctx, const json_t *args) {
    const json_t *content = json_obj_get(args, "content");
    json_t *a = json_object();
    ma5_set_str(a, "body", ma5_str(json_obj_get(args, "body")));
    const json_t *fb = (content && content->type == JSON_OBJECT) ? json_obj_get(content, "formatted_body") : NULL;
    json_set(a, "formatted_body", ma5_copy_or_null(fb));
    const json_t *mentions = (content && content->type == JSON_OBJECT) ? json_obj_get(content, "m.mentions") : NULL;
    const json_t *uids = (mentions && mentions->type == JSON_OBJECT) ? json_obj_get(mentions, "user_ids") : NULL;
    json_set(a, "user_ids", ma5_copy_or_null(uids));
    return ma5_call(ctx, "_is_bot_mentioned", a);
}

/* PoP: ma5_is_bot_mentioned @ plugins/platforms/matrix/adapter.py:_is_bot_mentioned */
/* _is_bot_mentioned(body, formatted_body=None, mention_user_ids=None) */
static json_t *ma5_is_bot_mentioned(ma5_ctx_t *ctx, const json_t *args) {
    const json_t *self = ctx->state;
    const char *uid = ma5_str(json_obj_get(self, "_user_id"));
    const char *body = ma5_str(json_obj_get(args, "body"));
    const char *fb = ma5_str(json_obj_get(args, "formatted_body"));
    const json_t *uids = json_obj_get(args, "user_ids");
    if (ma5_truthy(uids) && uid && uid[0] && ma5_arr_has_str(uids, uid)) return json_bool(true);
    if ((!body || !*body) && (!fb || !*fb)) return json_bool(false);
    if (uid && uid[0] && body && strstr(body, uid)) return json_bool(true);
    json_t *a = json_object();
    json_t *r = ma5_call(ctx, "_user_localpart", a);
    if (ma5_is_raise(r)) return r;
    const char *lp = ma5_str(r);
    if (lp && *lp && ma5_word_search(body, lp)) { json_free(r); return json_bool(true); }
    json_free(r);
    if (fb && *fb && uid && *uid) {
        size_t n = strlen(uid) + 32;
        char *needle = malloc(n);
        if (needle) {
            snprintf(needle, n, "matrix.to/#/%s", uid);
            bool found = strstr(fb, needle) != NULL;
            free(needle);
            return json_bool(found);
        }
    }
    return json_bool(false);
}

/* _OUTBOUND_MENTION_RE scanner: (?<![\w/])(@[0-9A-Za-z._=/-]+:[0-9A-Za-z.-]+(?::\d+)?) */
static bool ma5_is_lpart(unsigned char c) {
    return (c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||
           c=='.'||c=='_'||c=='='||c=='/'||c=='-';
}
static bool ma5_is_server(unsigned char c) {
    return (c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='.'||c=='-';
}
/* p points at '@'. On success sets *mlen = match length, returns true. */
static bool ma5_mention_match(const char *p, size_t *mlen) {
    if (*p != '@') return false;
    const char *q = p + 1;
    while (ma5_is_lpart((unsigned char)*q)) q++;
    if (q == p + 1 || *q != ':') return false;
    q++;
    const char *r = q;
    while (ma5_is_server((unsigned char)*r)) r++;
    if (r == q) return false;
    if (*r == ':') {
        const char *s = r + 1;
        if (*s >= '0' && *s <= '9') {
            while (*s >= '0' && *s <= '9') s++;
            r = s;
        }
    }
    *mlen = (size_t)(r - p);
    return true;
}

/* PoP: ma5_has_outbound_room_mention @ plugins/platforms/matrix/adapter.py:_has_outbound_room_mention */
/* _has_outbound_room_mention(text) */
static json_t *ma5_has_outbound_room_mention(ma5_ctx_t *ctx, const json_t *args) {
    const char *text = ma5_str(json_obj_get(args, "text"));
    json_t *a = json_object();
    json_set_hdr(a, "text", text);
    json_t *r = ma5_call(ctx, "_protect_outbound_mention_regions", a);
    if (ma5_is_raise(r)) return r;
    const char *prot = (r && r->type == JSON_ARRAY && json_len(r) >= 1) ? ma5_str(json_get(r, 0)) : NULL;
    bool found = false;
    if (prot) {
        const char *p = prot;
        while ((p = strstr(p, "@room")) != NULL) {
            bool before = (p == prot) || (!ma5_is_word((unsigned char)p[-1]) && p[-1] != '/');
            bool after = (p[5]=='\0') || (!ma5_is_word((unsigned char)p[5]) && p[5]!=':' && p[5]!='.' && p[5]!='-');
            if (before && after) { found = true; break; }
            p += 1;
        }
    }
    json_free(r);
    return json_bool(found);
}

/* PoP: ma5_extract_outbound_mentions @ plugins/platforms/matrix/adapter.py:_extract_outbound_mentions */
/* _extract_outbound_mentions(text) */
static json_t *ma5_extract_outbound_mentions(ma5_ctx_t *ctx, const json_t *args) {
    const char *text = ma5_str(json_obj_get(args, "text"));
    json_t *a = json_object();
    json_set_hdr(a, "text", text);
    json_t *r = ma5_call(ctx, "_protect_outbound_mention_regions", a);
    if (ma5_is_raise(r)) return r;
    const char *prot = (r && r->type == JSON_ARRAY && json_len(r) >= 1) ? ma5_str(json_get(r, 0)) : NULL;
    json_t *out = json_array();
    if (prot) {
        size_t i = 0, n = strlen(prot);
        while (i < n) {
            size_t mlen = 0;
            if (prot[i]=='@' && (i==0 || (!ma5_is_word((unsigned char)prot[i-1]) && prot[i-1]!='/')) &&
                ma5_mention_match(prot + i, &mlen)) {
                char *m = malloc(mlen + 1);
                if (m) {
                    memcpy(m, prot + i, mlen); m[mlen] = '\0';
                    if (!ma5_arr_has_str(out, m)) json_append(out, json_string(m));
                    free(m);
                }
                i += mlen;
                continue;
            }
            i++;
        }
    }
    json_free(r);
    return out;
}

/* PoP: ma5_inject_outbound_mention_links @ plugins/platforms/matrix/adapter.py:_inject_outbound_mention_links */
/* _inject_outbound_mention_links(text) */
static json_t *ma5_inject_outbound_mention_links(ma5_ctx_t *ctx, const json_t *args) {
    const char *text = ma5_str(json_obj_get(args, "text"));
    if (!text || !*text) return json_string(text ? text : "");
    json_t *a = json_object();
    json_set_hdr(a, "text", text);
    json_t *r = ma5_call(ctx, "_protect_outbound_mention_regions", a);
    if (ma5_is_raise(r)) return r;
    const char *prot = (r && r->type == JSON_ARRAY && json_len(r) >= 1) ? ma5_str(json_get(r, 0)) : NULL;
    const json_t *phs = (r && r->type == JSON_ARRAY && json_len(r) >= 2) ? json_get(r, 1) : NULL;
    ma5_buf_t out = {0};
    if (prot) {
        size_t i = 0, n = strlen(prot);
        while (i < n) {
            size_t mlen = 0;
            if (prot[i]=='@' && (i==0 || (!ma5_is_word((unsigned char)prot[i-1]) && prot[i-1]!='/')) &&
                ma5_mention_match(prot + i, &mlen)) {
                mb_addc(&out, '[');
                mb_addn(&out, prot + i, mlen);
                mb_add(&out, "](https://matrix.to/#/");
                mb_addn(&out, prot + i, mlen);
                mb_addc(&out, ')');
                i += mlen;
                continue;
            }
            mb_addc(&out, prot[i]);
            i++;
        }
    }
    char *linked = mb_take(&out);
    if (phs && phs->type == JSON_ARRAY) {
        for (size_t k = 0; k < json_len(phs); k++) {
            const char *orig = ma5_str(json_get(phs, k));
            if (!orig) continue;
            char tok[80];
            snprintf(tok, sizeof tok, "\x00MENTION_PROTECTED%zu\x00", k);
            char *nl = ma5_replace_all(linked, tok, orig);
            free(linked); linked = nl;
        }
    }
    json_t *res = json_string(linked);
    free(linked);
    json_free(r);
    return res;
}

/* PoP: ma5_voice_may_park @ plugins/platforms/matrix/adapter.py:_voice_may_park */
/* _voice_may_park(room_id, body, content, relates_to, mention_claimed) */
static json_t *ma5_voice_may_park(ma5_ctx_t *ctx, const json_t *args) {
    const json_t *self = ctx->state;
    const char *room = ma5_str(json_obj_get(args, "room_id"));
    const char *body = ma5_str(json_obj_get(args, "body"));
    const json_t *content = json_obj_get(args, "content");
    const json_t *rel = json_obj_get(args, "relates_to");
    bool claimed = json_get_bool(args, "mention_claimed", false);
    if (claimed || !json_get_bool(self, "_require_mention", false)) return json_bool(false);
    {
        json_t *a = json_object();
        json_set(a, "content", ma5_copy_or_null(content));
        json_t *r = ma5_call(ctx, "is_voice_event", a);
        if (ma5_is_raise(r)) return r;
        bool ok = ma5_truthy(r); json_free(r);
        if (!ok) return json_bool(false);
    }
    const json_t *free = json_obj_get(self, "_free_rooms");
    const json_t *allowed = json_obj_get(self, "_allowed_rooms");
    if (ma5_arr_has_str(free, room)) return json_bool(false);
    if (ma5_truthy(allowed) && !ma5_arr_has_str(allowed, room)) return json_bool(false);
    {
        json_t *a = json_object();
        json_set(a, "relates_to", ma5_copy_or_null(rel));
        json_t *r = ma5_call(ctx, "_thread_root", a);
        if (ma5_is_raise(r)) return r;
        const char *tid = ma5_str(r);
        const json_t *threads = json_obj_get(self, "_threads");
        bool hit = tid && *tid && ma5_arr_has_str(threads, tid);
        json_free(r);
        if (hit) return json_bool(false);
    }
    if (body && body[0] == '/') return json_bool(false);
    {
        json_t *a = json_object();
        ma5_set_str(a, "body", body);
        json_set(a, "content", ma5_copy_or_null(content));
        json_t *r = ma5_call(ctx, "_content_mentions_bot", a);
        if (ma5_is_raise(r)) return r;
        bool mentioned = ma5_truthy(r); json_free(r);
        return json_bool(!mentioned);
    }
}

/* PoP: ma5_strip_mention @ plugins/platforms/matrix/adapter.py:_strip_mention */
/* _strip_mention(body) */
static json_t *ma5_strip_mention(ma5_ctx_t *ctx, const json_t *args) {
    const char *body = ma5_str(json_obj_get(args, "body"));
    if (!body || !*body) return json_string("");
    const json_t *self = ctx->state;
    const char *uid = ma5_str(json_obj_get(self, "_user_id"));
    char *cur = ma5_strdup(body);
    if (uid && *uid) { char *n = ma5_replace_all(cur, uid, ""); free(cur); cur = n; }
    json_t *a = json_object();
    json_t *r = ma5_call(ctx, "_user_localpart", a);
    if (ma5_is_raise(r)) { free(cur); return r; }
    const char *lp = ma5_str(r);
    if (lp && *lp) {
        ma5_buf_t out = {0};
        size_t i = 0, n = strlen(cur), lpl = strlen(lp);
        while (i < n) {
            bool matched = false;
            if (cur[i]=='@' && (i==0 || !ma5_is_word((unsigned char)cur[i-1])) && i + 1 + lpl <= n) {
                size_t k = 0;
                for (; k < lpl; k++) if (ma5_tolower(cur[i+1+k]) != ma5_tolower(lp[k])) break;
                if (k == lpl) {
                    size_t end = i + 1 + lpl;
                    if (end >= n || !ma5_is_word((unsigned char)cur[end])) { i = end; matched = true; }
                }
            }
            if (!matched) { mb_addc(&out, cur[i]); i++; }
        }
        free(cur); cur = mb_take(&out);
    }
    json_free(r);
    char *s1 = ma5_collapse_spaces(cur); free(cur); cur = s1;
    char *s2 = ma5_strip_punct_ws(cur); free(cur); cur = s2;
    char *s3 = ma5_strip(cur); free(cur); cur = s3;
    json_t *res = json_string(cur); free(cur);
    return res;
}

/* PoP: ma5_classify_inbound_media @ plugins/platforms/matrix/adapter.py:_classify_inbound_media */
/* _classify_inbound_media(msgtype, event_mimetype, source_content) */
static json_t *ma5_classify_inbound_media(ma5_ctx_t *ctx, const json_t *args) {
    const char *mt = ma5_str(json_obj_get(args, "msgtype"));
    const char *em = ma5_str(json_obj_get(args, "event_mimetype"));
    const json_t *sc = json_obj_get(args, "source_content");
    const char *kind, *def;
    bool voice = false;
    if (mt && strcmp(mt, "m.image") == 0) { kind = "photo"; def = "image/png"; }
    else if (mt && strcmp(mt, "m.audio") == 0) {
        json_t *a = json_object();
        json_set(a, "content", ma5_copy_or_null(sc));
        json_t *r = ma5_call(ctx, "has_voice_marker", a);
        if (ma5_is_raise(r)) return r;
        voice = ma5_truthy(r); json_free(r);
        kind = voice ? "voice" : "audio"; def = "audio/ogg";
    }
    else if (mt && strcmp(mt, "m.video") == 0) { kind = "video"; def = "video/mp4"; }
    else { kind = "document"; def = "application/octet-stream"; }
    const char *mime = (em && *em) ? em : def;
    json_t *arr = json_array();
    json_append(arr, json_string(kind));
    json_append(arr, json_string(mime));
    json_append(arr, json_bool(voice));
    return arr;
}

/* PoP: ma5_get_chat_info @ plugins/platforms/matrix/adapter.py:get_chat_info */
/* get_chat_info(chat_id) */
static json_t *ma5_get_chat_info(ma5_ctx_t *ctx, const json_t *args) {
    const char *room = ma5_str(json_obj_get(args, "chat_id"));
    json_t *a = json_object();
    json_set_hdr(a, "room_id", room);
    json_t *r = ma5_call(ctx, "_resolve_room_identity", a);
    if (ma5_is_raise(r)) return r;
    const char *name = ma5_str(ma5_attr(r, "display_name"));
    const char *ct = ma5_str(ma5_attr(r, "chat_type"));
    json_t *out = json_object();
    ma5_set_str(out, "name", name);
    json_set_hdr(out, "type", (ct && strcmp(ct, "dm") == 0) ? "dm" : "group");
    json_free(r);
    return out;
}

/* PoP: ma5_query_own_device_keys @ plugins/platforms/matrix/adapter.py:_query_own_device_keys */
/* _query_own_device_keys(client) */
static json_t *ma5_query_own_device_keys(ma5_ctx_t *ctx, const json_t *args) {
    const json_t *client = json_obj_get(args, "client");
    const char *mxid = ma5_str(ma5_attr(client, "mxid"));
    const char *dev = ma5_str(ma5_attr(client, "device_id"));
    json_t *arr = json_array();
    json_append(arr, json_string(dev ? dev : ""));
    json_t *payload = json_object();
    json_set(payload, mxid ? mxid : "", arr);
    json_t *a = json_object();
    json_set(a, "payload", payload);
    json_t *r = ma5_call(ctx, "query_keys", a);
    if (ma5_is_raise(r)) return r;
    const json_t *dk = ma5_attr(r, "device_keys");
    const json_t *ud = (dk && dk->type == JSON_OBJECT) ? json_obj_get(dk, mxid ? mxid : "") : NULL;
    if (!ma5_truthy(ud)) ud = NULL;
    const json_t *res = (ud && ud->type == JSON_OBJECT) ? json_obj_get(ud, dev ? dev : "") : NULL;
    json_t *out = ma5_copy_or_null(res);
    json_free(r);
    return out;
}
