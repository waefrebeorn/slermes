#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/kanban_watchers.py: 4 missing functions ── */
/* ── rest_revive3 seam infrastructure (kw) ───────────────────────────────
 * fn(kw_io_t *io, json_t *in); kw_seam records {ev,args,ret} (args
 * adopted), raise-specs model Python exceptions, NULL return == None. */
typedef struct kw_io {
    json_t *(*call)(struct kw_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} kw_io_t;

static int kw_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int kw_truthy(const json_t *v) {
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

static json_t *kw_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *kw_seam(kw_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static char *kw_strndup(const char *s, size_t n) {
    size_t len = 0;
    while (len < n && s[len]) len++;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}
static char *kw_str_of(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
            else snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return strdup(buf);
        default: return strdup("");
    }
}

/* PoP: wake_scope_id @ gateway/kanban_watchers_notifier.py:_wake_scope_id */
json_t *wake_scope_id(kw_io_t *io, json_t *in) {
    const json_t *sub = json_obj_get(in, "sub");
    const json_t *meta = json_obj_get(sub, "delivery_metadata");
    if (meta && meta->type == JSON_OBJECT) {
        static const char *keys[] = {"scope_id", "guild_id", "slack_team_id", "team_id"};
        for (int i = 0; i < 4; i++) {
            const json_t *value = json_obj_get(meta, keys[i]);
            if (kw_truthy(value)) {
                char *s = kw_str_of(value);
                json_t *out = json_string(s);
                free(s);
                return out;
            }
        }
    }
    /* adapter.scope_id_for_chat: callable seam, noncallable, or absent */
    const json_t *adapter = json_obj_get(in, "adapter");
    const char *kind = json_get_str(adapter, "resolver", "absent");
    if (strcmp(kind, "callable") != 0) return json_null();
    const json_t *chat = json_obj_get(sub, "chat_id");
    char *chat_s = kw_truthy(chat) ? kw_str_of(chat) : strdup("");
    json_t *args = json_object();
    json_set_hdr(args, "chat_id", chat_s);
    free(chat_s);
    json_t *resolved = kw_seam(io, "scope_id_for_chat", args);
    if (kw_is_raise(resolved)) { json_free(resolved); return json_null(); }  /* except -> None (logger noop) */
    if (!kw_truthy(resolved)) { json_free(resolved); return json_null(); }
    char *s = kw_str_of(resolved);
    json_free(resolved);
    json_t *out = json_string(s);
    free(s);
    return out;
}

/* _LOCAL_PATH_RE.sub("[local path]", s): unix dirs + windows drive paths,
 * with the (?<![\w:/]) lookbehind. Returns malloc'd result. */
static int kw_is_word(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}
static int kw_path_char(unsigned char c) {
    return c != 0 && !isspace(c) && c != ',' && c != ';';
}
static char *kw_local_path_sub(const char *s) {
    static const char *dirs[] = {"Users", "home", "private", "tmp", "var", "etc", "workspace"};
    size_t n = strlen(s);
    size_t cap = n * 2 + 16;
    char *out = malloc(cap);
    size_t o = 0;
    size_t i = 0;
    while (i < n) {
        int lookbehind_ok = (i == 0) || !(kw_is_word((unsigned char)s[i-1]) || s[i-1] == ':' || s[i-1] == '/');
        size_t match_end = 0;
        if (lookbehind_ok) {
            if (s[i] == '/') {
                for (int d = 0; d < 7 && !match_end; d++) {
                    size_t dl = strlen(dirs[d]);
                    if (strncmp(s + i + 1, dirs[d], dl) == 0 && s[i+1+dl] == '/') {
                        size_t j = i + 1 + dl + 1;
                        size_t start = j;
                        while (j < n && kw_path_char((unsigned char)s[j])) j++;
                        if (j > start) match_end = j;
                    }
                }
            } else if (isalpha((unsigned char)s[i]) && i + 2 < n && s[i+1] == ':' && s[i+2] == '\\') {
                size_t j = i + 3;
                size_t start = j;
                while (j < n && kw_path_char((unsigned char)s[j])) j++;
                if (j > start) match_end = j;
            }
        }
        if (match_end) {
            const char *rep = "[local path]";
            size_t rl = strlen(rep);
            if (o + rl + 1 >= cap) { cap = (o + rl + 1) * 2; out = realloc(out, cap); }
            memcpy(out + o, rep, rl); o += rl;
            i = match_end;
        } else {
            if (o + 2 >= cap) { cap *= 2; out = realloc(out, cap); }
            out[o++] = s[i++];
        }
    }
    out[o] = '\0';
    return out;
}

/* " ".join(s.split()): collapse all whitespace runs to single spaces, trim. */
static char *kw_collapse_ws(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(n + 1);
    size_t o = 0;
    int in_ws = 1;  /* leading trim */
    for (size_t i = 0; i < n; i++) {
        if (isspace((unsigned char)s[i])) { in_ws = 1; continue; }
        if (in_ws && o > 0) out[o++] = ' ';
        in_ws = 0;
        out[o++] = s[i];
    }
    out[o] = '\0';
    return out;
}

/* PoP: safe_review_reason @ gateway/kanban_watchers_notifier.py:_safe_review_reason */
json_t *safe_review_reason(kw_io_t *io, json_t *in) {
    const json_t *value = json_obj_get(in, "value");
    long long limit = (long long)json_get_num(in, "limit", 160);
    /* from agent.redact import redact_sensitive_text */
    json_t *imp = kw_seam(io, "import_redact_sensitive_text", json_object());
    if (kw_is_raise(imp)) return imp;
    json_free(imp);
    char *text = (!value || value->type == JSON_NULL) ? strdup("") : kw_str_of(value);
    json_t *args = json_object();
    json_set_hdr(args, "text", text);
    free(text);
    json_set(args, "force", json_bool(true));
    json_set(args, "redact_url_credentials", json_bool(true));
    json_t *reason = kw_seam(io, "redact_sensitive_text", args);
    if (kw_is_raise(reason)) return reason;
    const char *rs = reason && reason->type == JSON_STRING && reason->str_val ? reason->str_val : "";
    char *subbed = kw_local_path_sub(rs);
    char *collapsed = kw_collapse_ws(subbed);
    free(subbed);
    json_free(reason);
    /* if len(reason) > limit: reason[:limit-1].rstrip() + "…" */
    size_t len = strlen(collapsed);
    char *result;
    if ((long long)len > limit) {
        size_t cut = limit > 0 ? (size_t)(limit - 1) : 0;
        if (cut > len) cut = len;
        while (cut > 0 && isspace((unsigned char)collapsed[cut-1])) cut--;
        result = malloc(cut + 4);
        memcpy(result, collapsed, cut);
        memcpy(result + cut, "\xe2\x80\xa6", 3);  /* … */
        result[cut+3] = '\0';
    } else {
        result = strdup(collapsed);
    }
    free(collapsed);
    json_t *out = json_string(result);
    free(result);
    return out;
}

/* deferred (rest_revive3): async thread service — tag removed */
char *to_thread_process_service(void) { return strdup(""); }
