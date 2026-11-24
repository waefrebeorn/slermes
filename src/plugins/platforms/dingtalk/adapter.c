/*
 * adapter.c — C11 port of upstream plugins/platforms/dingtalk/adapter.py.
 *
 * Two differential gates share this ONE file:
 *   w29_adapter_91f75f_p1 — text/predicate value core
 *   w29_adapter_91f75f_p2 — payload shaping, retry math, credential keys
 *
 * PORTED (pure functions of values; no socket, no clock, no SDK, no thread):
 * PoP: w29_adapter_91f75f_constants @ plugins/platforms/dingtalk/adapter.py:<module budgets>
 * PoP: w29_adapter_91f75f_csv_set @ plugins/platforms/dingtalk/adapter.py:_csv_set
 * PoP: w29_adapter_91f75f_normalize_markdown @ plugins/platforms/dingtalk/adapter.py:DingTalkAdapter._normalize_markdown
 * PoP: w29_adapter_91f75f_require_mention @ plugins/platforms/dingtalk/adapter.py:DingTalkAdapter._dingtalk_require_mention
 * PoP: w29_adapter_91f75f_is_user_allowed @ plugins/platforms/dingtalk/adapter.py:DingTalkAdapter._is_user_allowed
 * PoP: w29_adapter_91f75f_should_process_message @ plugins/platforms/dingtalk/adapter.py:DingTalkAdapter._should_process_message
 * PoP: w29_adapter_91f75f_is_sdk_incompat @ plugins/platforms/dingtalk/adapter.py:_is_sdk_incompat
 * PoP: w29_adapter_91f75f_chat_info @ plugins/platforms/dingtalk/adapter.py:DingTalkAdapter.get_chat_info
 * PoP: w29_adapter_91f75f_webhook_payload @ plugins/platforms/dingtalk/adapter.py:DingTalkAdapter.send
 * PoP: w29_adapter_91f75f_standalone_payload @ plugins/platforms/dingtalk/adapter.py:_standalone_send
 * PoP: w29_adapter_91f75f_image_content @ plugins/platforms/dingtalk/adapter.py:DingTalkAdapter.send_image
 * PoP: w29_adapter_91f75f_no_local_upload @ plugins/platforms/dingtalk/adapter.py:_NO_LOCAL_UPLOAD
 * PoP: w29_adapter_91f75f_reconnect_plan @ plugins/platforms/dingtalk/adapter.py:DingTalkAdapter._run_stream_loop
 * PoP: w29_adapter_91f75f_webhook_valid @ plugins/platforms/dingtalk/adapter.py:DingTalkAdapter._get_valid_webhook
 * PoP: w29_adapter_91f75f_credentials @ plugins/platforms/dingtalk/adapter.py:_credentials
 * PoP: w29_adapter_91f75f_is_connected @ plugins/platforms/dingtalk/adapter.py:_is_connected
 * PoP: w29_adapter_91f75f_nested_allowed_users @ plugins/platforms/dingtalk/adapter.py:_nested_allowed_users
 * PoP: w29_adapter_91f75f_sdk_log_key @ plugins/platforms/dingtalk/adapter.py:_SdkLogGuard.filter
 *
 * SEAMED (values, not I/O, enter as fixture fields):
 *   - the dingtalk-stream / httpx / alibabacloud SDKs and every socket they open;
 *   - asyncio: the stream loop's sleeps and cancellation are decisions here
 *     (reconnect_plan), the awaits are not;
 *   - the scoped-secret reader (get_scoped_secret): its env rung arrives as
 *     env_id / env_secret inputs; extra_or_secret's "" default for a missing
 *     key is reproduced (a null setting reads as the empty string);
 *   - re.compile / Pattern.search for mention patterns: the compiled patterns
 *     arrive as one hit flag per pattern (pattern_hits); the composition of the
 *     group gate stays here;
 *   - record.getMessage() / time.monotonic(): the already-formatted record
 *     message and level arrive as fields; the 500-char key is shaped here;
 *   - datetime.now(tz=UTC): the millisecond clock arrives as now_ms; the
 *     5-minute expiry margin stays here;
 *   - apply_yaml_bridge, uuid ids, MessageDeduplicator, the HTTP send itself.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_adapter_91f75f_p1.h"
#include "noc_w29_adapter_91f75f_p2.h"

/* ------------------------------------------------------------------ */
/* shared Python-value helpers                                         */
/* ------------------------------------------------------------------ */

static char *dt_strdup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static char *dt_strndup(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* Python truthiness.  A NULL json_t* is None. */
static int dt_truthy(const json_t *v) {
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

/* Python's numeric view: int, float and bool are numbers; None is not. */
static int dt_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* str(): None -> "None", True/False, integral numbers without a ".0", strings
 * verbatim.  libjson carries one number type, so an integral JSON number is
 * printed the way Python prints an int.  Containers fall back to JSON text
 * (a documented seam: Python would print their repr). */
static char *dt_py_str(const json_t *v) {
    char buf[64];
    if (!v) return dt_strdup("None");
    switch (v->type) {
    case JSON_NULL:   return dt_strdup("None");
    case JSON_BOOL:   return dt_strdup(v->bool_val ? "True" : "False");
    case JSON_STRING: return dt_strdup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val &&
            v->num_val > -1e15 && v->num_val < 1e15)
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof buf, "%g", v->num_val);
        return dt_strdup(buf);
    default: {
        char *s = json_dumps(v, 0);
        char *r = dt_strdup(s ? s : "");
        if (s) free(s);
        return r;
    }
    }
}

/* `value or ""` : a falsy value reads as the empty string. */
static char *dt_or_empty(const json_t *v) {
    if (!dt_truthy(v)) return dt_strdup("");
    return dt_py_str(v);
}

static int dt_ws(char c) {
    /* Python str.strip()/lstrip() whitespace; unicode spaces are not fixtured. */
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static void dt_trim(const char *s, size_t n, size_t *off, size_t *len) {
    size_t a = 0, b = n;
    while (a < b && dt_ws(s[a])) a++;
    while (b > a && dt_ws(s[b - 1])) b--;
    *off = a;
    *len = b - a;
}

static char *dt_lower(const char *s) {
    char *p = dt_strdup(s), *q;
    if (!p) return NULL;
    for (q = p; *q; q++)
        if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
    return p;
}

/* Python slice [:n] over CODE POINTS (not bytes). */
static char *dt_cp_trunc(const char *s, size_t n) {
    size_t i = 0, cp = 0;
    if (!s) s = "";
    while (s[i] && cp < n) {
        i++;
        while (s[i] && (((unsigned char)s[i] & 0xC0) == 0x80)) i++;
        cp++;
    }
    return dt_strndup(s, i);
}

/* growable byte buffer */
typedef struct {
    char  *p;
    size_t len, cap;
} dtbuf;

static int dtbuf_add(dtbuf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *np;
        while (cap < b->len + n + 1) cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np) return 0;
        b->p = np;
        b->cap = cap;
    }
    if (n) memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
    return 1;
}

static void dtbuf_free(dtbuf *b) {
    free(b->p);
    b->p = NULL;
    b->len = b->cap = 0;
}

/* _NUMBERED_RE = ^\d+\.\s  (ASCII digits; matched against a stripped line) */
static int dt_numbered(const char *s, size_t n) {
    size_t k = 0;
    while (k < n && s[k] >= '0' && s[k] <= '9') k++;
    if (k == 0 || k >= n || s[k] != '.') return 0;
    k++;
    if (k >= n) return 0;
    return dt_ws(s[k]) ? 1 : 0;
}

/* _normalize_markdown: blank line before a numbered list, dedent ``` fences. */
static char *dt_normalize(const char *text) {
    size_t n, i, li = 0, nlines = 1;
    struct dt_line { size_t off, len; } *lines;
    dtbuf out;
    int first = 1;

    if (!text) return dt_strdup("");
    n = strlen(text);
    for (i = 0; i < n; i++)
        if (text[i] == '\n') nlines++;
    lines = (struct dt_line *)malloc(nlines * sizeof *lines);
    if (!lines) return dt_strdup(text);
    lines[0].off = 0;
    for (i = 0; i < n; i++) {
        if (text[i] == '\n') {
            lines[li].len = i - lines[li].off;
            li++;
            lines[li].off = i + 1;
        }
    }
    lines[li].len = n - lines[li].off;

    memset(&out, 0, sizeof out);
    if (!dtbuf_add(&out, "", 0)) { free(lines); return dt_strdup(text); }
    for (i = 0; i < nlines; i++) {
        const char *line = text + lines[i].off;
        size_t ll = lines[i].len, tb = 0, tl = 0, pb = 0, pl = 0;
        int prev_nonempty = 0, prev_numbered = 0;

        if (i > 0) {
            dt_trim(text + lines[i - 1].off, lines[i - 1].len, &pb, &pl);
            prev_nonempty = pl > 0;
            prev_numbered = prev_nonempty &&
                dt_numbered(text + lines[i - 1].off + pb, pl);
        }
        dt_trim(line, ll, &tb, &tl);
        if (prev_nonempty && dt_numbered(line + tb, tl) && !prev_numbered) {
            /* an extra "" element => "\n\n" in the join */
            if (!first && !dtbuf_add(&out, "\n", 1)) break;
            first = 0;
        }
        if (!first && !dtbuf_add(&out, "\n", 1)) break;
        first = 0;
        if (tl >= 3 && strncmp(line + tb, "```", 3) == 0)
            dtbuf_add(&out, line + tb, ll - tb);
        else
            dtbuf_add(&out, line, ll);
    }
    free(lines);
    if (!out.p) return dt_strdup("");
    return out.p;
}

/* ------------------------------------------------------------------ */
/* csv / list helpers                                                  */
/* ------------------------------------------------------------------ */

static int dt_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void dt_add_trimmed(char ***items, size_t *n, size_t *cap,
                           const char *s, size_t len) {
    size_t off, tl;
    char *copy;
    dt_trim(s, len, &off, &tl);
    if (tl == 0) return;
    copy = dt_strndup(s + off, tl);
    if (!copy) return;
    if (*n == *cap) {
        size_t nc = *cap ? *cap * 2 : 8;
        char **ni = (char **)realloc(*items, nc * sizeof(char *));
        if (!ni) { free(copy); return; }
        *items = ni;
        *cap = nc;
    }
    (*items)[(*n)++] = copy;
}

static int dt_has(const json_t *arr, const char *s) {
    size_t i;
    if (!arr || !s) return 0;
    for (i = 0; i < json_len(arr); i++) {
        const json_t *e = json_get(arr, i);
        if (e && e->type == JSON_STRING && e->str_val && !strcmp(e->str_val, s)) return 1;
    }
    return 0;
}

json_t *w29_adapter_91f75f_csv_set(const json_t *raw) {
    json_t *out = json_array();
    json_t *parsed = NULL;
    const json_t *src = NULL;
    char **items = NULL;
    size_t n = 0, cap = 0, i;

    /* _decode_json_list_literal: a list stays a list; a string starting with
     * '[' that parses to a list is decoded. */
    if (raw && raw->type == JSON_ARRAY) {
        src = raw;
    } else if (raw && raw->type == JSON_STRING && raw->str_val) {
        const char *s = raw->str_val;
        size_t off = 0;
        while (s[off] && dt_ws(s[off])) off++;
        if (s[off] == '[') {
            parsed = json_parse(s, NULL);
            if (parsed && parsed->type == JSON_ARRAY) src = parsed;
        }
    }
    if (src) {
        for (i = 0; i < json_len(src); i++) {
            char *t = dt_py_str(json_get(src, i));
            if (!t) continue;
            dt_add_trimmed(&items, &n, &cap, t, strlen(t));
            free(t);
        }
    } else {
        char *s = dt_py_str(raw);
        if (s) {
            const char *start = s, *p;
            for (p = s; ; p++) {
                if (*p == ',' || *p == '\0') {
                    dt_add_trimmed(&items, &n, &cap, start, (size_t)(p - start));
                    if (*p == '\0') break;
                    start = p + 1;
                }
            }
            free(s);
        }
    }
    if (parsed) json_free(parsed);

    if (n > 1) qsort(items, n, sizeof(char *), dt_cmp_str);
    for (i = 0; i < n; i++) {
        if (i == 0 || strcmp(items[i], items[i - 1]) != 0) {
            json_t *sv = json_string(items[i]);
            if (sv) json_append(out, sv);
        }
    }
    for (i = 0; i < n; i++) free(items[i]);
    free(items);
    return out;
}

/* extra_or_secret(..., default "") for the *_CHATS keys: a missing or null
 * setting is the empty string, an explicit blank stays blank. */
static json_t *dt_csv_setting(const json_t *v) {
    if (!v || v->type == JSON_NULL) return json_array();
    return w29_adapter_91f75f_csv_set(v);
}

/* ------------------------------------------------------------------ */
/* p1: text + predicates                                               */
/* ------------------------------------------------------------------ */

json_t *w29_adapter_91f75f_normalize_markdown(const json_t *text) {
    char *s, *md;
    json_t *out;

    if (!text || text->type != JSON_STRING) return json_null();
    s = text->str_val ? text->str_val : "";
    md = dt_normalize(s);
    if (!md) return json_null();
    out = json_string(md);
    free(md);
    return out;
}

json_t *w29_adapter_91f75f_require_mention(const json_t *configured) {
    /* configured.lower() in {"true","1","yes","on"} if str else bool(configured) */
    if (configured && configured->type == JSON_STRING) {
        char *low = dt_lower(configured->str_val ? configured->str_val : "");
        int hit = 0;
        if (low) {
            hit = !strcmp(low, "true") || !strcmp(low, "1") ||
                  !strcmp(low, "yes") || !strcmp(low, "on");
            free(low);
        }
        return json_bool(hit);
    }
    return json_bool(dt_truthy(configured));
}

json_t *w29_adapter_91f75f_is_user_allowed(const json_t *allowed_raw,
                                           const json_t *sender_id,
                                           const json_t *sender_staff_id) {
    json_t *set = dt_csv_setting(allowed_raw);
    char *sid = dt_or_empty(sender_id);
    char *stf = dt_or_empty(sender_staff_id);
    char *lsid = dt_lower(sid ? sid : "");
    char *lstf = dt_lower(stf ? stf : "");
    int allowed = 0;
    size_t i;

    if (json_len(set) == 0 || dt_has(set, "*")) {
        allowed = 1;
    } else {
        for (i = 0; i < json_len(set); i++) {
            const json_t *e = json_get(set, i);
            char *low;
            if (!e || e->type != JSON_STRING) continue;
            low = dt_lower(e->str_val ? e->str_val : "");
            if (!low) continue;
            /* the "" element is dropped by the set difference: it matches nothing */
            if (lsid && lsid[0] && !strcmp(low, lsid)) allowed = 1;
            if (lstf && lstf[0] && !strcmp(low, lstf)) allowed = 1;
            free(low);
            if (allowed) break;
        }
    }
    free(sid);
    free(stf);
    free(lsid);
    free(lstf);
    json_free(set);
    return json_bool(allowed);
}

json_t *w29_adapter_91f75f_should_process_message(const json_t *req) {
    const json_t *is_group, *chat, *text, *in_at, *rm, *hits;
    json_t *allowed, *free_set;
    char *chat_s, *text_s;
    int rm_on = 0, mention_hit = 0, out;

    if (!req || req->type != JSON_OBJECT) return json_bool(1);
    is_group = json_obj_get(req, "is_group");
    if (!dt_truthy(is_group)) return json_bool(1);      /* DMs always pass */

    chat = json_obj_get(req, "chat_id");
    text = json_obj_get(req, "text");
    in_at = json_obj_get(req, "is_in_at_list");
    rm = json_obj_get(req, "require_mention");
    chat_s = dt_or_empty(chat);
    text_s = dt_or_empty(text);

    allowed = dt_csv_setting(json_obj_get(req, "allowed_chats"));
    if (json_len(allowed) > 0 && chat_s && chat_s[0] && !dt_has(allowed, chat_s)) {
        free(chat_s);
        free(text_s);
        json_free(allowed);
        return json_bool(0);
    }
    free(allowed);

    /* extra_or_secret(..., "false") => a missing setting is the string "false" */
    if (rm && rm->type != JSON_NULL) {
        json_t *r = w29_adapter_91f75f_require_mention(rm);
        rm_on = dt_truthy(r);
        json_free(r);
    }

    hits = json_obj_get(req, "pattern_hits");
    if (text_s && text_s[0] && hits && hits->type == JSON_ARRAY && json_len(hits) > 0) {
        size_t i;
        for (i = 0; i < json_len(hits); i++)
            if (dt_truthy(json_get(hits, i))) { mention_hit = 1; break; }
    }

    free_set = dt_csv_setting(json_obj_get(req, "free_response_chats"));
    out = ((chat_s && chat_s[0] && dt_has(free_set, chat_s)) || !rm_on ||
           dt_truthy(in_at) || mention_hit) ? 1 : 0;
    free(free_set);
    free(chat_s);
    free(text_s);
    return json_bool(out);
}

json_t *w29_adapter_91f75f_is_sdk_incompat(const json_t *chain) {
    const json_t *cur;
    int hops;

    if (!chain || chain->type != JSON_ARRAY || json_len(chain) == 0) return json_bool(0);
    cur = json_get(chain, 0);
    for (hops = 0; hops < 4; hops++) {          /* the live loop is range(4) */
        const json_t *type, *msg, *cause, *ctx, *te;
        double d;
        int is_te = 0, next = -1;

        if (!cur || cur->type == JSON_NULL) return json_bool(0);   /* exc is None */
        if (cur->type != JSON_OBJECT) return json_bool(0);

        type = json_obj_get(cur, "type");
        msg = json_obj_get(cur, "msg");
        te = json_obj_get(cur, "te");
        if (dt_number(te, &d)) is_te = d != 0.0;      /* isinstance(exc, TypeError) */
        else if (type && type->type == JSON_STRING && type->str_val &&
                 !strcmp(type->str_val, "TypeError")) is_te = 1;
        if (is_te && msg && msg->type == JSON_STRING && msg->str_val &&
            strstr(msg->str_val, "asynchronous context manager"))
            return json_bool(1);

        cause = json_obj_get(cur, "cause");
        ctx = json_obj_get(cur, "context");
        if (cause && cause->type == JSON_NUMBER) next = (int)cause->num_val;
        else if (ctx && ctx->type == JSON_NUMBER) next = (int)ctx->num_val;
        else return json_bool(0);                     /* __cause__ or __context__ is None */
        if (next < 0 || (size_t)next >= json_len(chain)) return json_bool(0);
        cur = json_get(chain, (size_t)next);
    }
    return json_bool(0);
}

json_t *w29_adapter_91f75f_chat_info(const json_t *chat_id) {
    char *id = dt_or_empty(chat_id);
    char *low = dt_lower(id ? id : "");
    json_t *out = json_object();
    int is_group = 0;

    if (low && strstr(low, "group")) is_group = 1;
    free(low);
    json_set(out, "name", json_string(id ? id : ""));
    json_set(out, "type", json_string(is_group ? "group" : "dm"));
    free(id);
    return out;
}

json_t *w29_adapter_91f75f_constants(void) {
    static const int BACKOFF[5] = {2, 5, 10, 30, 60};
    static const char *const TRUTHY[4] = {"1", "on", "true", "yes"};
    json_t *o = json_object();
    json_t *truthy = json_array();
    json_t *backoff = json_array();
    int i;

    json_set(o, "max_message_length", json_number(20000.0));
    for (i = 0; i < 5; i++) json_append(backoff, json_number((double)BACKOFF[i]));
    json_set(o, "reconnect_backoff", backoff);
    json_set(o, "reconnect_circuit_breaker_trips", json_number(5.0));
    json_set(o, "reconnect_circuit_breaker_delay", json_number(300.0));
    json_set(o, "sdk_log_repeat_window", json_number(300.0));
    json_set(o, "session_webhooks_max", json_number(500.0));
    json_set(o, "emotion_id", json_string("2659900"));
    json_set(o, "emotion_bg", json_string("im_bg_1"));
    for (i = 0; i < 4; i++) json_append(truthy, json_string(TRUTHY[i]));
    json_set(o, "truthy", truthy);
    json_set(o, "numbered_re", json_string("^\\d+\\.\\s"));
    json_set(o, "webhook_re", json_string("^https://(?:api|oapi)\\.dingtalk\\.com/"));
    json_set(o, "no_local_upload", json_string(
        "DingTalk session webhook replies do not support local %s. "
        "Only markdown/text replies are supported without OpenAPI %s."));
    return o;
}

/* ------------------------------------------------------------------ */
/* p2: payload shaping                                                 */
/* ------------------------------------------------------------------ */

/* send(): content[:MAX_MESSAGE_LENGTH] -> _normalize_markdown -> markdown payload */
json_t *w29_adapter_91f75f_webhook_payload(const json_t *content) {
    char *raw, *cut, *md;
    json_t *out, *body;

    if (!content || content->type != JSON_STRING) return json_null();
    raw = content->str_val ? content->str_val : "";
    cut = dt_cp_trunc(raw, 20000);
    if (!cut) return json_null();
    md = dt_normalize(cut);
    free(cut);
    if (!md) return json_null();

    out = json_object();
    body = json_object();
    json_set(out, "msgtype", json_string("markdown"));
    json_set(body, "title", json_string("Hermes"));
    json_set(body, "text", json_string(md));
    json_set(out, "markdown", body);
    free(md);
    return out;
}

/* _standalone_send: {"msgtype": "text", "text": {"content": message}} */
json_t *w29_adapter_91f75f_standalone_payload(const json_t *message) {
    char *s = dt_py_str(message);
    json_t *out = json_object();
    json_t *body = json_object();

    json_set(out, "msgtype", json_string("text"));
    json_set(body, "content", json_string(s ? s : ""));
    json_set(out, "text", body);
    free(s);
    return out;
}

/* send_image: "![image](url)", prefixed by "{caption}\n\n" when caption is truthy */
json_t *w29_adapter_91f75f_image_content(const json_t *caption,
                                         const json_t *image_url) {
    char *url = dt_py_str(image_url);
    char *block, *out;
    size_t bl;
    json_t *res;
    dtbuf b;

    if (!url) return json_null();
    bl = strlen(url) + 16;
    block = (char *)malloc(bl);
    if (!block) { free(url); return json_null(); }
    snprintf(block, bl, "![image](%s)", url);
    free(url);

    memset(&b, 0, sizeof b);
    if (dt_truthy(caption)) {
        char *cap = dt_py_str(caption);
        dtbuf_add(&b, cap ? cap : "", cap ? strlen(cap) : 0);
        dtbuf_add(&b, "\n\n", 2);
        free(cap);
    }
    dtbuf_add(&b, block, strlen(block));
    free(block);
    out = b.p;
    if (!out) return json_null();
    res = json_string(out);
    free(out);
    return res;
}

/* _NO_LOCAL_UPLOAD % (what, openapi) */
json_t *w29_adapter_91f75f_no_local_upload(const json_t *kind) {
    const char *k = (kind && kind->type == JSON_STRING && kind->str_val) ? kind->str_val : "";
    const char *what, *openapi;
    char buf[512];

    if (!strcmp(k, "image")) {
        what = "image uploads";
        openapi = "media upload";
    } else if (!strcmp(k, "file")) {
        what = "file attachments";
        openapi = "message send";
    } else {
        return json_null();
    }
    snprintf(buf, sizeof buf,
             "DingTalk session webhook replies do not support local %s. "
             "Only markdown/text replies are supported without OpenAPI %s.",
             what, openapi);
    return json_string(buf);
}

/* _run_stream_loop: consecutive-error counter, circuit breaker, backoff index */
json_t *w29_adapter_91f75f_reconnect_plan(const json_t *consecutive,
                                          const json_t *same_type,
                                          const json_t *backoff_idx) {
    static const int BACKOFF[5] = {2, 5, 10, 30, 60};
    double c = 0.0, idx = 0.0;
    long n, i, delay;
    int tripped;
    json_t *o;

    dt_number(consecutive, &c);
    dt_number(backoff_idx, &idx);
    n = (long)c;
    i = (long)idx;
    if (dt_truthy(same_type)) n += 1;
    else n = 1;

    tripped = n > 5;                       /* RECONNECT_CIRCUIT_BREAKER_TRIPS */
    if (tripped) {
        delay = 300;                       /* RECONNECT_CIRCUIT_BREAKER_DELAY */
    } else {
        long slot = i < 0 ? 0 : (i < 4 ? i : 4);
        delay = BACKOFF[slot];
        i += 1;
    }

    o = json_object();
    json_set(o, "consecutive_same_error", json_number((double)n));
    json_set(o, "delay", json_number((double)delay));
    json_set(o, "tripped", json_bool(tripped));
    json_set(o, "backoff_idx", json_number((double)i));
    return o;
}

/* _get_valid_webhook: a session webhook is stale 5 minutes before it expires.
 * info: {"webhook": str, "expired_ms": num|null} — the dict entry. */
json_t *w29_adapter_91f75f_webhook_valid(const json_t *info, const json_t *now_ms) {
    const json_t *expired;
    double e = 0.0, nowv = 0.0;

    if (!info || info->type != JSON_OBJECT) return json_null();
    expired = json_obj_get(info, "expired_ms");
    if (dt_number(expired, &e) && e != 0.0 && e > 0.0 && dt_number(now_ms, &nowv)) {
        /* int(now.timestamp() * 1000) — same IEEE ops as the live expression */
        double nowi = (double)(long long)(nowv / 1000.0 * 1000.0);
        if (nowi + 300000.0 >= e) return json_null();
    }
    return json_copy(info);
}

/* _credentials: PlatformConfig.extra first, then the scoped secret (seam input). */
json_t *w29_adapter_91f75f_credentials(const json_t *extra,
                                       const json_t *env_id,
                                       const json_t *env_secret) {
    const json_t *base = (extra && extra->type == JSON_OBJECT) ? extra : NULL;
    const json_t *id = base ? json_obj_get(base, "client_id") : NULL;
    const json_t *sec = base ? json_obj_get(base, "client_secret") : NULL;
    json_t *out = json_array();

    if (dt_truthy(id)) json_append(out, json_copy(id));
    else if (env_id && env_id->type != JSON_NULL) json_append(out, json_copy(env_id));
    else json_append(out, json_string(""));

    if (dt_truthy(sec)) json_append(out, json_copy(sec));
    else if (env_secret && env_secret->type != JSON_NULL) json_append(out, json_copy(env_secret));
    else json_append(out, json_string(""));
    return out;
}

/* _is_connected: all(_credentials(...)) */
json_t *w29_adapter_91f75f_is_connected(const json_t *extra,
                                        const json_t *env_id,
                                        const json_t *env_secret) {
    json_t *c = w29_adapter_91f75f_credentials(extra, env_id, env_secret);
    int ok = dt_truthy(json_get(c, 0)) && dt_truthy(json_get(c, 1));
    json_free(c);
    return json_bool(ok);
}

/* _nested_allowed_users: extra.allowed_users, this block first, then the
 * gateway.platforms.dingtalk / platforms.dingtalk containers. */
json_t *w29_adapter_91f75f_nested_allowed_users(const json_t *yaml_cfg,
                                                const json_t *dingtalk_cfg) {
    const json_t *gw, *containers[2], *holders[3];
    size_t i;

    if (!yaml_cfg || yaml_cfg->type != JSON_OBJECT) return json_null();
    gw = json_obj_get(yaml_cfg, "gateway");
    containers[0] = (gw && gw->type == JSON_OBJECT) ? json_obj_get(gw, "platforms") : NULL;
    containers[1] = json_obj_get(yaml_cfg, "platforms");

    holders[0] = dingtalk_cfg;
    holders[1] = (containers[0] && containers[0]->type == JSON_OBJECT)
                 ? json_obj_get(containers[0], "dingtalk") : NULL;
    holders[2] = (containers[1] && containers[1]->type == JSON_OBJECT)
                 ? json_obj_get(containers[1], "dingtalk") : NULL;

    for (i = 0; i < 3; i++) {
        const json_t *ex, *au;
        if (!holders[i] || holders[i]->type != JSON_OBJECT) continue;
        ex = json_obj_get(holders[i], "extra");
        if (!ex || ex->type != JSON_OBJECT) continue;
        au = json_obj_get(ex, "allowed_users");
        if (au && au->type != JSON_NULL) return json_copy(au);
    }
    return json_null();
}

/* _SdkLogGuard.filter: key = (levelno, msg[:500], type(exc).__name__ or None) */
json_t *w29_adapter_91f75f_sdk_log_key(const json_t *level,
                                       const json_t *msg,
                                       const json_t *exc_type) {
    char *m = dt_py_str(msg), *cut;
    double lv = 0.0;
    json_t *out = json_array();

    if (!m) return json_null();
    cut = dt_cp_trunc(m, 500);
    free(m);
    if (!cut) return json_null();
    dt_number(level, &lv);
    json_append(out, json_number(lv));
    json_append(out, json_string(cut));
    free(cut);
    if (exc_type && exc_type->type != JSON_NULL) json_append(out, json_copy(exc_type));
    else json_append(out, json_null());
    return out;
}
