/*
 * adapter.c - C11 port of upstream plugins/platforms/mattermost/adapter.py
 * (gates w30_adapter_7683aa_p1 and w30_adapter_7683aa_p2).
 *
 * PoP: w30_mm_p1_format_message          @ plugins/platforms/mattermost/adapter.py:MattermostAdapter.format_message
 * PoP: w30_mm_p1_with_mentions_disabled  @ plugins/platforms/mattermost/adapter.py:_with_mentions_disabled
 * PoP: w30_mm_p1_channel_id_set          @ plugins/platforms/mattermost/adapter.py:_channel_id_set
 * PoP: w30_mm_p1_post_result             @ plugins/platforms/mattermost/adapter.py:_post_result
 * PoP: w30_mm_p1_url_filename            @ plugins/platforms/mattermost/adapter.py:_url_filename
 * PoP: w30_mm_p1_channel_info            @ plugins/platforms/mattermost/adapter.py:MattermostAdapter.get_chat_info
 * PoP: w30_mm_p1_api_request_url         @ plugins/platforms/mattermost/adapter.py:MattermostAdapter._api
 * PoP: w30_mm_p1_auth_header             @ plugins/platforms/mattermost/adapter.py:MattermostAdapter._auth_header
 * PoP: w30_mm_p1_request_headers         @ plugins/platforms/mattermost/adapter.py:MattermostAdapter._headers
 * PoP: w30_mm_p1_broken_thread_root      @ plugins/platforms/mattermost/adapter.py:MattermostAdapter._last_post_failure_is_broken_thread_root
 * PoP: w30_mm_p1_reconnect_step          @ plugins/platforms/mattermost/adapter.py:MattermostAdapter._ws_loop
 * PoP: w30_mm_p1_url_and_token           @ plugins/platforms/mattermost/adapter.py:_url_and_token
 * PoP: w30_mm_p1_validate_config         @ plugins/platforms/mattermost/adapter.py:validate_mattermost_config
 * PoP: w30_mm_p1_constants               @ plugins/platforms/mattermost/adapter.py:<module constants>
 * PoP: w30_mm_p2_apply_channel_gating    @ plugins/platforms/mattermost/adapter.py:MattermostAdapter._apply_channel_gating
 * PoP: w30_mm_p2_is_configured_bot_user  @ plugins/platforms/mattermost/adapter.py:MattermostAdapter._is_configured_bot_user
 * PoP: w30_mm_p2_post_payload            @ plugins/platforms/mattermost/adapter.py:MattermostAdapter._post_message
 * PoP: w30_mm_p2_broken_root_fallback    @ plugins/platforms/mattermost/adapter.py:MattermostAdapter._post_preserving_thread
 * PoP: w30_mm_p2_ws_event                @ plugins/platforms/mattermost/adapter.py:MattermostAdapter._handle_ws_event
 * PoP: w30_mm_p2_resolved_root_id        @ plugins/platforms/mattermost/adapter.py:MattermostAdapter._resolve_root_id
 * PoP: w30_mm_p2_typing_request          @ plugins/platforms/mattermost/adapter.py:MattermostAdapter.send_typing
 * PoP: w30_mm_p2_multi_image_empty       @ plugins/platforms/mattermost/adapter.py:MattermostAdapter.send_multiple_images
 * PoP: w30_mm_p2_post_with_file_result   @ plugins/platforms/mattermost/adapter.py:MattermostAdapter._post_with_file
 *
 * Seams (never ported here): the aiohttp ClientSession/WebSocket, the REST
 * response bodies, the deduplicator's clock, the media cache and the
 * filesystem.  Two gates share this one translation unit: p1 is the outbound
 * value helpers, p2 the inbound admission/gating/payload logic.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "noc_w30_adapter_7683aa_p1.h"
#include "noc_w30_adapter_7683aa_p2.h"

/* --- module constants (adapter.py:37-49) --- */
#define MM_MAX_POST_LENGTH        4000.0
#define MM_RECONNECT_BASE_DELAY   2.0
#define MM_RECONNECT_MAX_DELAY    60.0
#define MM_RECONNECT_JITTER       0.2
#define MM_POST_WITH_FILE_ERROR   "Failed to post with file"
#define MM_PLATFORM_HINT \
    "You are in a Mattermost workspace communicating with your user. Mattermost renders standard " \
    "Markdown \xE2\x80\x94 headings, bold, italic, code blocks, and tables all work. You can send media files " \
    "natively: include MEDIA:/absolute/path/to/file in your response. Images (.jpg, .png, .webp) are " \
    "uploaded as photo attachments, audio and video as file attachments. Image URLs in markdown format " \
    "![alt](url) are rendered as inline previews automatically."

/* ---------------------------------------------------------------- helpers */

static char *mm_dup(const char *s) {
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
static int mm_truthy(const json_t *v) {
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

/* Python's numeric view: int/float/bool; anything else is not a number. */
static int mm_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL)   { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Shortest decimal that strtod()s back to the same double (repr-like). */
static void mm_num_repr(char *out, size_t cap, double v) {
    int p;
    if (v == (double)(long long)v && v > -1e15 && v < 1e15) {
        snprintf(out, cap, "%lld", (long long)v);
        return;
    }
    for (p = 1; p <= 17; p++) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%.*g", p, v);
        if (strtod(tmp, NULL) == v) { snprintf(out, cap, "%s", tmp); return; }
    }
    snprintf(out, cap, "%.17g", v);
}

/* Python str(value) for the shapes a channel list / user id can hold. */
static char *mm_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return mm_dup("None");
    switch (v->type) {
    case JSON_BOOL:   return mm_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return mm_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER: mm_num_repr(buf, sizeof(buf), v->num_val); return mm_dup(buf);
    default: break;
    }
    /* Empty containers are part of the value surface: str({}) is "{}" and str([])
     * is "[]", which is what a comma-split then produces as a channel id. Only a
     * NON-empty container would need Python's single-quoted repr, and that stays a
     * documented gap. */
    if (v->type == JSON_OBJECT) return mm_dup(json_object_size(v) ? "<container>" : "{}");
    if (v->type == JSON_ARRAY) return mm_dup(json_len(v) ? "<container>" : "[]");
    return mm_dup("<container>");
}

static int mm_isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python str.strip() with the default ASCII whitespace set. */
static char *mm_strip(const char *s) {
    size_t a = 0, b;
    char *p;
    if (!s) return mm_dup("");
    b = strlen(s);
    while (a < b && mm_isspace(s[a])) a++;
    while (b > a && mm_isspace(s[b - 1])) b--;
    p = (char *)malloc(b - a + 1);
    if (!p) return NULL;
    memcpy(p, s + a, b - a);
    p[b - a] = '\0';
    return p;
}

/* Python str.lstrip("@") */
static char *mm_lstrip_at(const char *s) {
    if (!s) s = "";
    while (*s == '@') s++;
    return mm_dup(s);
}

static void mm_lower_ascii(char *s) {
    for (; s && *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

/* re.sub(re.escape(needle), "", hay, flags=IGNORECASE) for a literal needle. */
static void mm_ci_remove_all(char *hay, const char *needle) {
    size_t nl = strlen(needle), hl = strlen(hay), r = 0, w = 0;
    if (!nl) return;
    while (r < hl) {
        size_t i;
        for (i = 0; i < nl && r + i < hl; i++) {
            char a = hay[r + i], b = needle[i];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
        }
        if (i == nl) { r += nl; continue; }
        hay[w++] = hay[r++];
    }
    hay[w] = '\0';
}

static int mm_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* _CHANNEL_TYPE_MAP lookup: {"D":"dm","G":"group","P":"group","O":"channel"} */
static const char *mm_channel_type(const char *code) {
    if (!code || !code[0] || code[1] != '\0') return "channel";
    switch (code[0]) {
    case 'D': return "dm";
    case 'G': return "group";
    case 'P': return "group";
    case 'O': return "channel";
    default:  return "channel";
    }
}

/* sorted, de-duplicated string set -> JSON array (consumes items) */
static json_t *mm_set_to_array(char **items, size_t n) {
    json_t *arr = json_array();
    size_t i, out = 0;
    if (!items) return arr;
    qsort(items, n, sizeof(char *), mm_cmp_str);
    for (i = 0; i < n; i++) {
        if (out && strcmp(items[i], items[out - 1]) == 0) { free(items[i]); continue; }
        items[out++] = items[i];
    }
    for (i = 0; i < out; i++) {
        json_append(arr, json_string(items[i]));
        free(items[i]);
    }
    free(items);
    return arr;
}

/* _channel_id_set: list (or comma-separated str(...)) -> stripped string set */
static json_t *mm_channel_id_set(const json_t *raw) {
    char **items = NULL;
    size_t n = 0, cap = 0;
    if (raw && raw->type == JSON_ARRAY) {
        size_t i;
        for (i = 0; i < json_len(raw); i++) {
            char *s = mm_pystr(json_get(raw, i));
            char *t = mm_strip(s);
            free(s);
            if (!t || !t[0]) { free(t); continue; }
            if (n == cap) {
                char **ni;
                cap = cap ? cap * 2 : 8;
                ni = (char **)realloc(items, cap * sizeof(char *));
                if (!ni) { free(t); break; }
                items = ni;
            }
            items[n++] = t;
        }
    } else {
        char *whole = mm_pystr(raw);
        char *cur = whole;
        for (;;) {
            char *comma = strchr(cur, ',');
            char *piece;
            if (comma) *comma = '\0';
            piece = mm_strip(cur);
            if (piece && piece[0]) {
                if (n == cap) {
                    char **ni;
                    cap = cap ? cap * 2 : 8;
                    ni = (char **)realloc(items, cap * sizeof(char *));
                    if (!ni) { free(piece); break; }
                    items = ni;
                }
                items[n++] = piece;
            } else {
                free(piece);
            }
            if (!comma) break;
            cur = comma + 1;
        }
        free(whole);
    }
    return mm_set_to_array(items, n);
}

/* _with_mentions_disabled: props merge, disable_mentions always true. */
static json_t *mm_mentions_disabled(const json_t *payload) {
    json_t *out = (payload && payload->type == JSON_OBJECT) ? json_copy(payload) : json_object();
    const json_t *props = json_obj_get(out, "props");
    json_t *np = (props && props->type == JSON_OBJECT) ? json_copy(props) : json_object();
    json_set(np, "disable_mentions", json_bool(true));
    json_set(out, "props", np);
    return out;
}

/* SendResult subset: {"success","message_id","error"} */
static json_t *mm_send_result(int ok, json_t *message_id, const char *error) {
    json_t *o = json_object();
    json_set(o, "success", json_bool(ok ? true : false));
    json_set(o, "message_id", message_id ? message_id : json_null());
    json_set(o, "error", error ? json_string(error) : json_null());
    return o;
}

/* _post_result */
static json_t *mm_post_result(const json_t *data, const char *error) {
    const json_t *id = NULL;
    if (data && data->type == JSON_OBJECT) id = json_obj_get(data, "id");
    else if (data && data->type == JSON_ARRAY) {
        size_t i;
        for (i = 0; i < json_len(data); i++) {
            const json_t *e = json_get(data, i);
            if (e && e->type == JSON_STRING && e->str_val && !strcmp(e->str_val, "id")) id = e;
        }
    }
    if (!id) return mm_send_result(0, NULL, error);
    return mm_send_result(1, json_copy(id), NULL);
}

/* _last_post_failure_is_broken_thread_root */
static int mm_broken_thread_root(const json_t *status, const char *error) {
    double st = 0.0;
    char *body;
    int hit_root, hit_invalid;
    if (!mm_number(status, &st) || (st != 400.0 && st != 404.0)) return 0;
    if (!error || !error[0]) return 0;
    body = mm_dup(error);
    if (!body) return 0;
    mm_lower_ascii(body);
    hit_root = strstr(body, "root_id") || strstr(body, "rootid") || strstr(body, "root id")
               || strstr(body, "thread") || strstr(body, "post");
    hit_invalid = strstr(body, "invalid") || strstr(body, "not found")
                  || strstr(body, "does not exist") || strstr(body, "missing");
    free(body);
    return (hit_root && hit_invalid) ? 1 : 0;
}

/* gateway.platforms._shared.extra_or_secret, string view (env rung included). */
static char *mm_setting_str(const json_t *extra, const char *key, const char *env,
                            const char *def, int blank_is_unset) {
    const char *envv = env ? getenv(env) : NULL;
    const json_t *v;
    if (envv) {
        char *t = mm_strip(envv);
        if (t && t[0]) return t;
        free(t);
    }
    v = (extra && extra->type == JSON_OBJECT) ? json_obj_get(extra, key) : NULL;
    if (!v || v->type == JSON_NULL) return mm_dup(def);
    if (blank_is_unset && v->type == JSON_STRING) {
        char *t = mm_strip(v->str_val);
        int blank = !t[0];
        free(t);
        if (blank) return mm_dup(def);
    }
    return mm_pystr(v);
}

/* extra_or_secret, JSON view: the value keeps its type (a YAML list stays a
 * list, which is what _channel_id_set consumes). */
static json_t *mm_setting_json(const json_t *extra, const char *key, const char *env,
                               const char *def, int blank_is_unset) {
    const char *envv = env ? getenv(env) : NULL;
    const json_t *v;
    if (envv) {
        char *t = mm_strip(envv);
        int nonblank = t && t[0];
        free(t);
        if (nonblank) return json_string(envv);
    }
    v = (extra && extra->type == JSON_OBJECT) ? json_obj_get(extra, key) : NULL;
    if (!v || v->type == JSON_NULL) return json_string(def);
    if (blank_is_unset && v->type == JSON_STRING) {
        char *t = mm_strip(v->str_val);
        int blank = !t[0];
        free(t);
        if (blank) return json_string(def);
    }
    return json_copy(v);
}

/* ------------------------------------------------------------------- p1 */

json_t *w30_mm_p1_format_message(const char *content) {
    size_t L, i = 0, n = 0;
    char *out;
    json_t *r;
    if (!content) content = "";
    L = strlen(content);
    out = (char *)malloc(L + 1);
    if (!out) return json_string("");
    while (i < L) {
        if (content[i] == '!' && i + 1 < L && content[i + 1] == '[') {
            size_t j = i + 2;
            while (j < L && content[j] != ']') j++;
            if (j < L && content[j] == ']' && j + 1 < L && content[j + 1] == '(') {
                size_t k = j + 2;
                while (k < L && content[k] != ')') k++;
                if (k < L && k > j + 2) {          /* [^)]+ needs >= 1 char */
                    size_t t;
                    for (t = j + 2; t < k; t++) out[n++] = content[t];
                    i = k + 1;
                    continue;
                }
            }
        }
        out[n++] = content[i++];
    }
    out[n] = '\0';
    r = json_string(out);
    free(out);
    return r;
}

json_t *w30_mm_p1_with_mentions_disabled(const json_t *payload) {
    return mm_mentions_disabled(payload);
}

json_t *w30_mm_p1_channel_id_set(const json_t *raw) {
    return mm_channel_id_set(raw);
}

json_t *w30_mm_p1_post_result(const json_t *data, const char *error) {
    return mm_post_result(data, error);
}

json_t *w30_mm_p1_url_filename(const char *url, const char *fallback) {
    const char *slash, *q;
    size_t len;
    char *buf;
    json_t *r;
    if (!url) url = "";
    slash = strrchr(url, '/');
    slash = slash ? slash + 1 : url;
    q = strchr(slash, '?');
    len = q ? (size_t)(q - slash) : strlen(slash);
    if (len == 0) return json_string(fallback ? fallback : "");
    buf = (char *)malloc(len + 1);
    if (!buf) return json_string(fallback ? fallback : "");
    memcpy(buf, slash, len);
    buf[len] = '\0';
    r = json_string(buf);
    free(buf);
    return r;
}

json_t *w30_mm_p1_channel_info(const json_t *data, const char *chat_id) {
    json_t *o = json_object();
    const json_t *dn, *nm, *ty;
    const char *name = NULL, *type;
    if (!mm_truthy(data)) {
        json_set(o, "name", json_string(chat_id ? chat_id : ""));
        json_set(o, "type", json_string("channel"));
        return o;
    }
    dn = json_obj_get(data, "display_name");
    nm = json_obj_get(data, "name");
    ty = json_obj_get(data, "type");
    if (mm_truthy(dn)) name = dn->type == JSON_STRING ? dn->str_val : (chat_id ? chat_id : "");
    else if (mm_truthy(nm)) name = nm->type == JSON_STRING ? nm->str_val : (chat_id ? chat_id : "");
    else name = chat_id ? chat_id : "";
    if (!ty) type = "channel";                       /* key absent -> .get default "O" */
    else if (ty->type == JSON_STRING) type = mm_channel_type(ty->str_val);
    else type = "channel";                           /* null/other -> map miss */
    json_set(o, "name", json_string(name));
    json_set(o, "type", json_string(type));
    return o;
}

json_t *w30_mm_p1_api_request_url(const char *base_url, const char *path) {
    json_t *o = json_object();
    const char *p;
    char *url;
    size_t bl, pl;
    if (!base_url) base_url = "";
    if (!path) path = "";
    if (strstr(path, "..")) {
        json_set(o, "blocked", json_bool(true));
        json_set(o, "url", json_null());
        return o;
    }
    p = path;
    while (*p == '/') p++;              /* path.lstrip("/") */
    bl = strlen(base_url);
    pl = strlen(p);
    url = (char *)malloc(bl + pl + 16);
    if (!url) {
        json_set(o, "blocked", json_bool(false));
        json_set(o, "url", json_null());
        return o;
    }
    snprintf(url, bl + pl + 16, "%s/api/v4/%s", base_url, p);
    json_set(o, "blocked", json_bool(false));
    json_set(o, "url", json_string(url));
    free(url);
    return o;
}

json_t *w30_mm_p1_auth_header(const char *token) {
    json_t *o = json_object();
    const char *t = token ? token : "";
    char *v = (char *)malloc(strlen(t) + 8);
    if (!v) return o;
    sprintf(v, "Bearer %s", t);
    json_set(o, "Authorization", json_string(v));
    free(v);
    return o;
}

json_t *w30_mm_p1_request_headers(const char *token) {
    json_t *o = w30_mm_p1_auth_header(token);
    json_set(o, "Content-Type", json_string("application/json"));
    return o;
}

json_t *w30_mm_p1_broken_thread_root(const json_t *status, const char *error) {
    return json_bool(mm_broken_thread_root(status, error) ? true : false);
}

json_t *w30_mm_p1_reconnect_step(const json_t *delay, const json_t *rnd) {
    double d = 0.0, r = 0.0, sleep_s, next;
    json_t *o = json_object();
    mm_number(delay, &d);
    mm_number(rnd, &r);
    sleep_s = d + d * MM_RECONNECT_JITTER * r;
    next = d * 2.0;
    if (next > MM_RECONNECT_MAX_DELAY) next = MM_RECONNECT_MAX_DELAY;
    json_set(o, "sleep", json_number(sleep_s));
    json_set(o, "next", json_number(next));
    return o;
}

json_t *w30_mm_p1_url_and_token(const json_t *config) {
    json_t *o = json_object();
    const json_t *extra = (config && config->type == JSON_OBJECT) ? json_obj_get(config, "extra") : NULL;
    const json_t *u, *tok;
    const char *env_url = getenv("MATTERMOST_URL");
    const char *env_tok = getenv("MATTERMOST_TOKEN");
    if (extra && extra->type != JSON_OBJECT) extra = NULL;
    u = extra ? json_obj_get(extra, "url") : NULL;
    /* _get_scoped_secret(name, "") never yields None: an unset env is "". */
    u = mm_truthy(u) ? json_copy(u) : json_string(env_url ? env_url : "");
    tok = (config && config->type == JSON_OBJECT) ? json_obj_get(config, "token") : NULL;
    tok = mm_truthy(tok) ? json_copy(tok) : json_string(env_tok ? env_tok : "");
    json_set(o, "url", (json_t *)u);
    json_set(o, "token", (json_t *)tok);
    return o;
}

json_t *w30_mm_p1_validate_config(const json_t *config) {
    json_t *pair = w30_mm_p1_url_and_token(config);
    const json_t *u = json_obj_get(pair, "url");
    const json_t *t = json_obj_get(pair, "token");
    char *ts = mm_strip(t && t->type == JSON_STRING ? t->str_val : "");
    char *us = mm_strip(u && u->type == JSON_STRING ? u->str_val : "");
    int ok = ts[0] && us[0];
    free(ts);
    free(us);
    json_free(pair);
    return json_bool(ok ? true : false);
}

json_t *w30_mm_p1_constants(void) {
    json_t *o = json_object();
    json_t *map = json_object();
    json_t *props = json_object();
    json_t *media = json_array();
    json_t *pair;
    json_t *ext = json_object();
    json_set(o, "max_post_length", json_number(MM_MAX_POST_LENGTH));
    json_set(o, "reconnect_base_delay", json_number(MM_RECONNECT_BASE_DELAY));
    json_set(o, "reconnect_max_delay", json_number(MM_RECONNECT_MAX_DELAY));
    json_set(o, "reconnect_jitter", json_number(MM_RECONNECT_JITTER));
    json_set(o, "post_with_file_error", json_string(MM_POST_WITH_FILE_ERROR));
    json_set(map, "D", json_string("dm"));
    json_set(map, "G", json_string("group"));
    json_set(map, "P", json_string("group"));
    json_set(map, "O", json_string("channel"));
    json_set(o, "channel_type_map", map);
    json_set(props, "disable_mentions", json_bool(true));
    json_set(o, "disable_mentions_props", props);
    pair = json_array();
    json_append(pair, json_string("image/"));
    json_append(pair, json_string("photo"));
    json_append(media, pair);
    pair = json_array();
    json_append(pair, json_string("audio/"));
    json_append(pair, json_string("voice"));
    json_append(media, pair);
    json_set(o, "media_msg_types", media);
    json_set(ext, "image/", json_string(".png"));
    json_set(ext, "audio/", json_string(".ogg"));
    json_set(o, "inbound_cache_ext", ext);
    json_set(o, "splits_long_messages", json_bool(true));
    json_set(o, "serial_receive_loop", json_bool(true));
    json_set(o, "platform_hint", json_string(MM_PLATFORM_HINT));
    return o;
}

/* ------------------------------------------------------------------- p2 */

/* _apply_channel_gating over fixture-supplied config.extra + env. */
static json_t *mm_apply_channel_gating(const json_t *req) {
    const json_t *extra = json_obj_get(req, "extra");
    const char *channel_id = json_get_str(req, "channel_id", "");
    const char *bot_username = json_get_str(req, "bot_username", "");
    const char *bot_user_id = json_get_str(req, "bot_user_id", "");
    char *text = mm_dup(json_get_str(req, "message_text", ""));
    char *allowed_raw, *free_raw, *req_raw, *p1, *p2;
    json_t *allowed, *free_channels;
    int require_mention, has_mention = 0;
    size_t i;

    if (extra && extra->type != JSON_OBJECT) extra = NULL;

    allowed_raw = mm_setting_str(extra, "allowed_channels", "MATTERMOST_ALLOWED_CHANNELS", "", 0);
    {
        json_t *allowed_v = mm_setting_json(extra, "allowed_channels",
                                            "MATTERMOST_ALLOWED_CHANNELS", "", 0);
        allowed = mm_channel_id_set(allowed_v);
        json_free(allowed_v);
    }
    if (json_len(allowed) > 0) {
        int found = 0;
        for (i = 0; i < json_len(allowed); i++) {
            const json_t *e = json_get(allowed, i);
            if (e && e->type == JSON_STRING && !strcmp(e->str_val, channel_id)) found = 1;
        }
        if (!found) {
            json_free(allowed); free(allowed_raw); free(text);
            return json_null();
        }
    }
    json_free(allowed);

    req_raw = mm_setting_str(extra, "require_mention", "MATTERMOST_REQUIRE_MENTION", "true", 0);
    mm_lower_ascii(req_raw);
    require_mention = !(strcmp(req_raw, "false") == 0 || strcmp(req_raw, "0") == 0
                        || strcmp(req_raw, "no") == 0);

    free_raw = mm_setting_str(extra, "free_response_channels", "MATTERMOST_FREE_RESPONSE_CHANNELS", "", 0);
    {
        json_t *free_v = mm_setting_json(extra, "free_response_channels",
                                         "MATTERMOST_FREE_RESPONSE_CHANNELS", "", 0);
        free_channels = mm_channel_id_set(free_v);
        json_free(free_v);
    }

    p1 = (char *)malloc(strlen(bot_username) + 2);
    p2 = (char *)malloc(strlen(bot_user_id) + 2);
    if (!p1 || !p2) {
        free(p1); free(p2); free(text); free(allowed_raw); free(free_raw);
        free(req_raw); json_free(free_channels);
        return json_null();
    }
    sprintf(p1, "@%s", bot_username);
    sprintf(p2, "@%s", bot_user_id);

    {
        char *lt = mm_dup(text), *l1 = mm_dup(p1), *l2 = mm_dup(p2);
        mm_lower_ascii(lt); mm_lower_ascii(l1); mm_lower_ascii(l2);
        has_mention = strstr(lt, l1) != NULL || strstr(lt, l2) != NULL;
        free(lt); free(l1); free(l2);
    }

    if (require_mention) {
        int in_free = 0;
        for (i = 0; i < json_len(free_channels); i++) {
            const json_t *e = json_get(free_channels, i);
            if (e && e->type == JSON_STRING && !strcmp(e->str_val, channel_id)) in_free = 1;
        }
        if (!in_free && !has_mention) {
            free(p1); free(p2); free(text); free(allowed_raw); free(free_raw); free(req_raw);
            json_free(free_channels);
            return json_null();
        }
    }
    if (has_mention) {
        char *t;
        mm_ci_remove_all(text, p1);
        t = mm_strip(text); free(text); text = t;
        mm_ci_remove_all(text, p2);
        t = mm_strip(text); free(text); text = t;
    }
    free(p1); free(p2); free(allowed_raw); free(free_raw); free(req_raw);
    json_free(free_channels);
    {
        json_t *r = json_string(text);
        free(text);
        return r;
    }
}

json_t *w30_mm_p2_apply_channel_gating(const json_t *req) {
    return mm_apply_channel_gating(req);
}

/* _is_configured_bot_user */
static int mm_is_bot_user(const char *user_id) {
    const char *envv = getenv("MATTERMOST_BOT_USERS");
    char *raw = envv ? mm_dup(envv) : mm_dup("");
    json_t *set = mm_channel_id_set(json_string(raw));
    int found = 0;
    size_t i;
    for (i = 0; i < json_len(set); i++) {
        const json_t *e = json_get(set, i);
        if (e && e->type == JSON_STRING && !strcmp(e->str_val, user_id)) found = 1;
    }
    json_free(set);
    free(raw);
    return found;
}

json_t *w30_mm_p2_is_configured_bot_user(const json_t *req) {
    char *uid = mm_pystr(json_obj_get(req, "user_id"));
    int found = mm_is_bot_user(uid);
    free(uid);
    return json_bool(found ? true : false);
}

/* thread-root candidate derivation shared by _post_message/_post_preserving_thread */
static const json_t *mm_thread_candidate(const json_t *req) {
    const json_t *cand = json_obj_get(req, "reply_to");
    if (!mm_truthy(cand)) {
        const json_t *md = json_obj_get(req, "metadata");
        cand = NULL;
        if (md && md->type == JSON_OBJECT) {
            const json_t *t = json_obj_get(md, "thread_id");
            if (!mm_truthy(t)) t = json_obj_get(md, "root_id");
            cand = t;
        }
    }
    return mm_truthy(cand) ? cand : NULL;
}

static json_t *mm_post_payload(const json_t *req) {
    json_t *payload = json_object();
    const json_t *file_ids = json_obj_get(req, "file_ids");
    const json_t *cid = json_obj_get(req, "chat_id");
    const json_t *msg = json_obj_get(req, "message");
    json_set(payload, "channel_id", cid ? json_copy(cid) : json_null());
    json_set(payload, "message", msg ? json_copy(msg) : json_null());
    if (file_ids && file_ids->type != JSON_NULL) json_set(payload, "file_ids", json_copy(file_ids));
    {
        json_t *with_props = mm_mentions_disabled(payload);
        json_free(payload);
        payload = with_props;
    }
    if (!strcmp(json_get_str(req, "reply_mode", ""), "thread")) {
        const json_t *cand = mm_thread_candidate(req);
        if (cand) {
            const json_t *res = json_obj_get(req, "resolved_root");
            if (res && res->type == JSON_STRING) {
                json_set(payload, "root_id", json_copy(res));
            } else {
                char *s = mm_pystr(cand);
                json_set(payload, "root_id", json_string(s));
                free(s);
            }
        }
    }
    return payload;
}

json_t *w30_mm_p2_post_payload(const json_t *req) {
    return mm_post_payload(req);
}

json_t *w30_mm_p2_broken_root_fallback(const json_t *req) {
    json_t *payload = mm_post_payload(req);
    json_t *o = json_object();
    const json_t *md = json_obj_get(req, "metadata");
    int posted_ok = json_get_bool(req, "posted_ok", true);
    int notify = (md && md->type == JSON_OBJECT) && mm_truthy(json_obj_get(md, "notify"));
    int has_root = json_obj_get(payload, "root_id") != NULL;
    int broken = mm_broken_thread_root(json_obj_get(req, "status"), json_get_str(req, "error", ""));
    int flat = (!posted_ok) && has_root && notify && broken;
    json_set(o, "flat_fallback", json_bool(flat ? true : false));
    json_set(o, "posts", json_number(flat ? 2 : 1));
    if (flat) {
        json_t *keys = json_array();
        size_t i, n = json_object_size(payload), k = 0;
        char **names = (char **)malloc((n ? n : 1) * sizeof(char *));
        for (i = 0; i < n; i++) {
            const char *key = json_object_get_key_at(payload, i);
            if (key && strcmp(key, "root_id") != 0) names[k++] = mm_dup(key);
        }
        qsort(names, k, sizeof(char *), mm_cmp_str);
        for (i = 0; i < k; i++) { json_append(keys, json_string(names[i])); free(names[i]); }
        free(names);
        json_set(o, "flat_keys", keys);
    } else {
        json_set(o, "flat_keys", json_null());
    }
    json_free(payload);
    return o;
}

/* media message type from the downloaded mime list */
static const char *mm_msg_type_for(const char *text, const json_t *media_types) {
    size_t i;
    if (text && text[0] == '/') return "command";
    if (media_types && media_types->type == JSON_ARRAY && json_len(media_types) > 0) {
        static const char *const prefixes[2] = {"image/", "audio/"};
        static const char *const kinds[2] = {"photo", "voice"};
        int p;
        for (p = 0; p < 2; p++) {
            for (i = 0; i < json_len(media_types); i++) {
                const json_t *m = json_get(media_types, i);
                if (m && m->type == JSON_STRING && m->str_val
                    && !strncmp(m->str_val, prefixes[p], strlen(prefixes[p])))
                    return kinds[p];
            }
        }
        return "document";
    }
    return "text";
}

json_t *w30_mm_p2_ws_event(const json_t *req) {
    const json_t *event = json_obj_get(req, "event");
    const json_t *data, *post_text;
    json_t *post = NULL;
    const char *bot_user_id = json_get_str(req, "bot_user_id", "");
    const char *reply_mode = json_get_str(req, "reply_mode", "");
    json_t *out = json_object();
    char *text = NULL;
    json_t *media_urls = json_array(), *media_types = json_array();
    int drop = 0;

    json_set(out, "dispatched", json_bool(false));
    if (!event || event->type != JSON_OBJECT) return out;
    if (strcmp(json_get_str(event, "event", ""), "posted") != 0) return out;
    data = json_obj_get(event, "data");
    if (!data || data->type != JSON_OBJECT) return out;
    post_text = json_obj_get(data, "post");
    if (!post_text || post_text->type != JSON_STRING) return out;
    post = json_parse(post_text->str_val ? post_text->str_val : "", NULL);
    if (!post || post->type != JSON_OBJECT) { json_free(post); return out; }

    {
        const char *sender_id = json_get_str(post, "user_id", "");
        const char *post_id = json_get_str(post, "id", "");
        const json_t *type = json_obj_get(post, "type");
        const json_t *seen = json_obj_get(req, "seen");
        if (!strcmp(sender_id, bot_user_id)) drop = 1;
        if (!drop && mm_truthy(type)) drop = 1;
        if (!drop && post_id[0] && seen && seen->type == JSON_ARRAY) {
            size_t i;
            for (i = 0; i < json_len(seen); i++) {
                const json_t *s = json_get(seen, i);
                if (s && s->type == JSON_STRING && !strcmp(s->str_val, post_id)) drop = 1;
            }
        }

        if (!drop) {
            const char *channel_id = json_get_str(post, "channel_id", "");
            const char *ctype = json_get_str(data, "channel_type", "O");
            int is_dm = !strcmp(ctype, "D");
            text = mm_dup(json_get_str(post, "message", ""));

            if (!is_dm) {
                json_t *greq = json_object();
                const json_t *ex = json_obj_get(req, "extra");
                json_t *gated;
                json_set(greq, "channel_id", json_string(channel_id));
                json_set(greq, "message_text", json_string(text));
                json_set(greq, "bot_username", json_string(json_get_str(req, "bot_username", "")));
                json_set(greq, "bot_user_id", json_string(bot_user_id));
                json_set(greq, "extra", ex ? json_copy(ex) : json_object());
                gated = mm_apply_channel_gating(greq);
                if (!gated || gated->type == JSON_NULL) {
                    drop = 1;
                } else {
                    free(text);
                    text = mm_dup(gated->str_val);
                }
                json_free(gated);
                json_free(greq);
            }

            if (!drop) {
                const json_t *root = json_obj_get(post, "root_id");
                const char *post_id2 = json_get_str(post, "id", "");
                char *thread_id = mm_truthy(root) ? mm_pystr(root) : NULL;
                const char *chat_type = mm_channel_type(ctype);
                int is_bot = 0;
                const json_t *file_ids;

                if (!thread_id && !strcmp(reply_mode, "thread") && !is_dm && post_id2[0])
                    thread_id = mm_dup(post_id2);
                if (text[0] && mm_isspace(text[0])) {
                    char *t = text;
                    while (*t && mm_isspace(*t)) t++;
                    if (t[0] == '/') {
                        char *ls = mm_dup(t);
                        free(text);
                        text = ls;
                    }
                }

                {
                    const json_t *props = json_obj_get(post, "props");
                    const json_t *fb = (props && props->type == JSON_OBJECT)
                                       ? json_obj_get(props, "from_bot") : NULL;
                    char *fbs = mm_pystr(fb);
                    char *lower = mm_dup(fbs);
                    mm_lower_ascii(lower);
                    is_bot = !strcmp(lower, "true");
                    free(lower);
                    free(fbs);
                }
                if (!is_bot) is_bot = mm_is_bot_user(json_get_str(post, "user_id", ""));

                file_ids = json_obj_get(post, "file_ids");
                if (mm_truthy(file_ids) && json_get_bool(req, "authorized", false)) {
                    const json_t *mu = json_obj_get(req, "media_urls");
                    const json_t *mt = json_obj_get(req, "media_types");
                    json_free(media_urls);
                    json_free(media_types);
                    media_urls = (mu && mu->type == JSON_ARRAY) ? json_copy(mu) : json_array();
                    media_types = (mt && mt->type == JSON_ARRAY) ? json_copy(mt) : json_array();
                }

                {
                    const char *mtype = mm_msg_type_for(text, media_types);
                    char *uname = mm_lstrip_at(json_get_str(data, "sender_name", ""));
                    const char *sender2 = json_get_str(post, "user_id", "");

                    json_set(out, "dispatched", json_bool(true));
                    json_set(out, "text", json_string(text));
                    json_set(out, "message_type", json_string(mtype));
                    json_set(out, "chat_id", json_string(channel_id));
                    json_set(out, "chat_type", json_string(chat_type));
                    json_set(out, "user_id", json_string(sender2));
                    json_set(out, "user_name", json_string(uname[0] ? uname : sender2));
                    json_set(out, "thread_id", thread_id ? json_string(thread_id) : json_null());
                    json_set(out, "message_id", json_string(post_id2));
                    json_set(out, "is_bot", json_bool(is_bot ? true : false));
                    json_set(out, "media_urls", media_urls);
                    json_set(out, "media_types", media_types);
                    media_urls = NULL;
                    media_types = NULL;
                    free(uname);
                }
                free(thread_id);
            }
        }
    }
    free(text);
    json_free(media_urls);
    json_free(media_types);
    json_free(post);
    return out;
}

json_t *w30_mm_p2_resolved_root_id(const json_t *req) {
    const char *post_id = json_get_str(req, "post_id", "");
    const json_t *data = json_obj_get(req, "api_data");
    const json_t *root;
    if (!post_id[0]) return json_string(post_id);
    if (!mm_truthy(data)) return json_string(post_id);
    root = json_obj_get(data, "root_id");
    if (!mm_truthy(root)) return json_string(post_id);
    return json_copy(root);
}

json_t *w30_mm_p2_typing_request(const json_t *req) {
    const char *chat_id = json_get_str(req, "chat_id", "");
    const char *bot_user_id = json_get_str(req, "bot_user_id", "");
    json_t *o = json_object();
    json_t *payload = json_object();
    char *path = (char *)malloc(strlen(bot_user_id) + 16);
    json_set(payload, "channel_id", json_string(chat_id));
    if (!path) return o;
    sprintf(path, "users/%s/typing", bot_user_id);
    json_set(o, "path", json_string(path));
    json_set(o, "payload", payload);
    free(path);
    return o;
}

json_t *w30_mm_p2_multi_image_empty(const json_t *req) {
    const json_t *images = json_obj_get(req, "images");
    if (mm_truthy(images)) return json_object();   /* non-empty batch is not a pure seam */
    return mm_send_result(0, NULL, "no images to send");
}

json_t *w30_mm_p2_post_with_file_result(const json_t *req) {
    const json_t *data = json_obj_get(req, "data");
    const json_t *caption = json_obj_get(req, "caption");
    json_t *msg = (caption && caption->type != JSON_NULL) ? json_copy(caption) : json_string("");
    json_t *file_ids = json_array();
    json_t *o;
    json_append(file_ids, json_copy(json_obj_get(req, "file_id")));
    o = mm_post_result(data, MM_POST_WITH_FILE_ERROR);
    json_set(o, "message", msg);
    json_set(o, "file_ids", file_ids);
    return o;
}
