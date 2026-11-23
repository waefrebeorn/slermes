/*
 * methods_vault.c — C home for upstream Hermes tui_gateway/methods_vault.py.
 *
 * PoP: methodsvau_remove @ tui_gateway/methods_vault.py:vault.remove
 * PoP: methodsvau_lock @ tui_gateway/methods_vault.py:vault.lock
 * PoP: methodsvau_add @ tui_gateway/methods_vault.py:vault.add
 * PoP: methodsvau_unlock @ tui_gateway/methods_vault.py:vault.unlock
 *
 * Faithful guard/result logic of the JSON-RPC handlers. External services
 * (vault store, backend registry, config, lock) are explicit fixture seams.
 * The stubbed _ok is identity; _err is {code, error}.
 */
#include "noc_w13_methodsvau_26674a.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupn(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static char *dupstr(const char *s) { return dupn(s ? s : "", strlen(s ? s : "")); }
static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return 0;
}
/* str(v or "") for scalar JSON values. */
static char *str_or_empty(const json_t *v) {
    char buf[64];
    if (!v || !truthy(v)) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val);
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%g", v->num_val);
        return dupstr(buf);
    }
    return dupstr("");
}
static json_t *err(int code, const char *msg) {
    json_t *o = json_object();
    json_set(o, "code", json_number(code));
    json_set_hdr(o, "error", msg);
    return o;
}
static char *replace_all(const char *s, const char *needle, const char *rep) {
    if (!needle || !*needle) return dupstr(s);
    size_t nl = strlen(needle), rl = strlen(rep);
    size_t cap = strlen(s) * 2 + 64, len = 0;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    const char *p = s;
    while (*p) {
        const char *q = strstr(p, needle);
        if (!q) { size_t r = strlen(p); if (len + r + 1 > cap) { cap = len + r + 1; out = (char *)realloc(out, cap); } memcpy(out + len, p, r); len += r; break; }
        size_t pre = (size_t)(q - p);
        if (len + pre + rl + 1 > cap) { cap = (len + pre + rl + 1) * 2; out = (char *)realloc(out, cap); }
        memcpy(out + len, p, pre); len += pre;
        memcpy(out + len, rep, rl); len += rl;
        p = q + nl;
    }
    out[len] = '\0';
    return out;
}

/* scrub_secret_from_text: exact substring scrub for str secrets (len>=3), then
 * password['"]?\s*[:=]\s*\S+ -> \1[REDACTED] (case-sensitive). */
static char *scrub_secret(const char *text, const json_t *secret) {
    char *cur = dupstr(text);
    if (secret && secret->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(secret); ++i) {
            json_t *v = json_object_get_at(secret, i);
            if (v && v->type == JSON_STRING && v->str_val && strlen(v->str_val) >= 3) {
                char *nxt = replace_all(cur, v->str_val, "[REDACTED]");
                free(cur); cur = nxt;
            }
        }
    }
    size_t len = strlen(cur), cap = len * 2 + 64, olen = 0;
    char *out = (char *)malloc(cap);
    if (!out) return cur;
    size_t i = 0;
    while (i < len) {
        int matched = 0;
        if (len - i >= 8 && !memcmp(cur + i, "password", 8)) {
            size_t j = i + 8;
            if (j < len && (cur[j] == '\'' || cur[j] == '"')) ++j;
            while (j < len && isspace((unsigned char)cur[j])) ++j;
            if (j < len && (cur[j] == ':' || cur[j] == '=')) {
                size_t k = j + 1;
                while (k < len && isspace((unsigned char)cur[k])) ++k;
                if (k < len && !isspace((unsigned char)cur[k])) {
                    size_t m = k;
                    while (m < len && !isspace((unsigned char)cur[m])) ++m;
                    size_t grp = k - i;               /* keep group 1 verbatim */
                    size_t need = olen + grp + 10 + 1;
                    if (need > cap) { cap = need * 2; out = (char *)realloc(out, cap); }
                    memcpy(out + olen, cur + i, grp); olen += grp;
                    memcpy(out + olen, "[REDACTED]", 10); olen += 10;
                    i = m; matched = 1;   /* resume AFTER the consumed token */
                }
            }
        }
        if (!matched) {
            if (olen + 1 > cap) { cap *= 2; out = (char *)realloc(out, cap); }
            out[olen++] = cur[i++];
        }
    }
    out[olen] = '\0';
    free(cur);
    return out;
}

struct json_t *methodsvau_remove(const struct json_t *params, const struct json_t *removed) {
    char *id = str_or_empty(json_obj_get(params, "id"));
    if (!id[0]) { free(id); return err(5095, "id is required"); }
    free(id);
    json_t *o = json_object();
    json_set(o, "removed", removed ? json_copy(removed) : json_null());
    return o;
}

struct json_t *methodsvau_lock(void) {
    json_t *o = json_object();
    json_set(o, "locked", json_bool(true));
    return o;
}

struct json_t *methodsvau_add(const struct json_t *params, const struct json_t *add_result) {
    json_t *secret = json_obj_get(params, "secret");
    if (!secret || secret->type != JSON_OBJECT || json_object_size(secret) == 0)
        return err(5095, "secret payload is required");
    json_t *emsg = add_result ? json_obj_get(add_result, "error") : NULL;
    if (emsg && emsg->type == JSON_STRING) {
        char *sc = scrub_secret(emsg->str_val, secret);
        json_t *o = err(5095, sc);
        free(sc);
        return o;
    }
    json_t *o = json_object();
    json_t *idv = add_result ? json_obj_get(add_result, "id") : NULL;
    json_set(o, "id", idv ? json_copy(idv) : json_null());
    return o;
}

struct json_t *methodsvau_unlock(const struct json_t *params, const struct json_t *backends,
                                 const struct json_t *exc_msg) {
    char *name = str_or_empty(json_obj_get(params, "name"));
    char *pw = str_or_empty(json_obj_get(params, "password"));
    int found = 0;
    if (backends && backends->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(backends); ++i) {
            json_t *b = json_get(backends, i);
            const char *bn = b ? json_get_str(b, "name", NULL) : NULL;
            if (bn && !strcmp(bn, name)) {
                json_t *nu = json_obj_get(b, "needs_unlock");
                if (nu && nu->type == JSON_BOOL && nu->bool_val) { found = 1; break; }
            }
        }
    }
    if (!found) {
        char msg[256];
        snprintf(msg, sizeof(msg), "%s is not an enabled password manager", name);
        free(name); free(pw);
        return err(5095, msg);
    }
    if (!pw[0]) { free(name); free(pw); return err(5095, "master password is required"); }
    if (exc_msg && exc_msg->type == JSON_STRING) {
        char *red = replace_all(exc_msg->str_val, pw, "[REDACTED]");
        json_t *o = err(5095, red);
        free(red); free(name); free(pw);
        return o;
    }
    json_t *o = json_object();
    json_set_hdr(o, "name", name);
    json_set(o, "unlocked", json_bool(true));
    free(name); free(pw);
    return o;
}
