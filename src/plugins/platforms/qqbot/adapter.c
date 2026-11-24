/*
 * adapter.c - C home for plugins/platforms/qqbot/adapter.py.
 * Faithful port of the QQ Bot adapter's pure helpers: routing-key parsing and
 * authorization, inbound text assembly, message-type detection, media/voice sniffing,
 * REST paths, body construction, timestamp parsing and the STT config resolution.
 * PoP: w19_adapter_5d2c00_constants @ plugins/platforms/qqbot/adapter.py:<module>
 * PoP: w19_adapter_5d2c00_ws_proxy @ plugins/platforms/qqbot/adapter.py:_ws_proxy
 * PoP: w19_adapter_5d2c00_parse_json @ plugins/platforms/qqbot/adapter.py:QQAdapter._parse_json
 * PoP: w19_adapter_5d2c00_parse_gateway_session_key @ plugins/platforms/qqbot/adapter.py:QQAdapter._parse_gateway_session_key
 * PoP: w19_adapter_5d2c00_is_authorized_interaction @ plugins/platforms/qqbot/adapter.py:QQAdapter._is_authorized_interaction_for_session
 * PoP: w19_adapter_5d2c00_append_block @ plugins/platforms/qqbot/adapter.py:QQAdapter._append_block
 * PoP: w19_adapter_5d2c00_merge_quote_into @ plugins/platforms/qqbot/adapter.py:QQAdapter._merge_quote_into
 * PoP: w19_adapter_5d2c00_detect_message_type @ plugins/platforms/qqbot/adapter.py:QQAdapter._detect_message_type
 * PoP: w19_adapter_5d2c00_opt_str @ plugins/platforms/qqbot/adapter.py:QQAdapter._opt_str
 * PoP: w19_adapter_5d2c00_is_voice_content_type @ plugins/platforms/qqbot/adapter.py:QQAdapter._is_voice_content_type
 * PoP: w19_adapter_5d2c00_guess_ext_from_data @ plugins/platforms/qqbot/adapter.py:QQAdapter._guess_ext_from_data
 * PoP: w19_adapter_5d2c00_looks_like_silk @ plugins/platforms/qqbot/adapter.py:QQAdapter._looks_like_silk
 * PoP: w19_adapter_5d2c00_rest_path @ plugins/platforms/qqbot/adapter.py:QQAdapter._rest_path
 * PoP: w19_adapter_5d2c00_messages_path @ plugins/platforms/qqbot/adapter.py:QQAdapter._messages_path
 * PoP: w19_adapter_5d2c00_build_text_body @ plugins/platforms/qqbot/adapter.py:QQAdapter._build_text_body
 * PoP: w19_adapter_5d2c00_is_url @ plugins/platforms/qqbot/adapter.py:QQAdapter._is_url
 * PoP: w19_adapter_5d2c00_strip_at_mention @ plugins/platforms/qqbot/adapter.py:QQAdapter._strip_at_mention
 * PoP: w19_adapter_5d2c00_entry_matches @ plugins/platforms/qqbot/adapter.py:QQAdapter._entry_matches
 * PoP: w19_adapter_5d2c00_guess_chat_type @ plugins/platforms/qqbot/adapter.py:QQAdapter._guess_chat_type
 * PoP: w19_adapter_5d2c00_parse_qq_timestamp @ plugins/platforms/qqbot/adapter.py:QQAdapter._parse_qq_timestamp
 * PoP: w19_adapter_5d2c00_resolve_stt_config @ plugins/platforms/qqbot/adapter.py:QQAdapter._resolve_stt_config
 */
#include "noc_w19_adapter_5d2c00.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MSG_TYPE_TEXT 0
#define MSG_TYPE_MARKDOWN 2

static const char *VOICE_EXTENSIONS[] = {".silk", ".amr", ".mp3", ".wav", ".ogg",
                                         ".m4a", ".aac", ".speex", ".flac"};

static int py_isspace(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *dup_range(const char *s, size_t a, size_t b) {
    char *out;
    if (b < a) b = a;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *dup_str(const char *s) { return s ? dup_range(s, 0, strlen(s)) : dup_range("", 0, 0); }

static char *py_strip(const char *s) {
    size_t a = 0, b;
    if (!s) s = "";
    b = strlen(s);
    while (a < b && py_isspace((unsigned char)s[a])) a++;
    while (b > a && py_isspace((unsigned char)s[b - 1])) b--;
    return dup_range(s, a, b);
}

static void ascii_lower(char *s) {
    for (; s && *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

/* str(value) for the scalar shapes these helpers see. Caller frees. */
static char *py_str_value(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dup_str("");
    if (v->type == JSON_STRING) return dup_str(v->str_val);
    if (v->type == JSON_BOOL) return dup_str(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == (double)(long long)d) snprintf(buf, sizeof buf, "%lld", (long long)d);
        else snprintf(buf, sizeof buf, "%.17g", d);
        return dup_str(buf);
    }
    return dup_str("");
}

static int ends_with(const char *s, const char *suffix) {
    size_t a, b;
    if (!s || !suffix) return 0;
    a = strlen(s);
    b = strlen(suffix);
    return b <= a && !strcmp(s + a - b, suffix);
}

static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static unsigned char *hex2bin(const char *hex, size_t *out_len) {
    size_t n, i;
    unsigned char *buf;
    *out_len = 0;
    if (!hex) return NULL;
    n = strlen(hex);
    if (n % 2) return NULL;
    buf = (unsigned char *)calloc(n / 2 + 1, 1);
    if (!buf) return NULL;
    for (i = 0; i < n; i += 2) {
        int hi = hexval((unsigned char)hex[i]), lo = hexval((unsigned char)hex[i + 1]);
        if (hi < 0 || lo < 0) { free(buf); return NULL; }
        buf[i / 2] = (unsigned char)(hi * 16 + lo);
    }
    *out_len = n / 2;
    return buf;
}

json_t *w19_adapter_5d2c00_constants(void) {
    json_t *o = json_object();
    json_t *exts = json_array();
    json_t *urls = json_object();
    for (size_t i = 0; i < sizeof VOICE_EXTENSIONS / sizeof VOICE_EXTENSIONS[0]; i++)
        json_append(exts, json_string(VOICE_EXTENSIONS[i]));
    json_set(o, "voice_extensions", exts);
    json_set(urls, "zai", json_string("https://open.bigmodel.cn/api/coding/paas/v4"));
    json_set(urls, "openai", json_string("https://api.openai.com/v1"));
    json_set(urls, "glm", json_string("https://open.bigmodel.cn/api/coding/paas/v4"));
    json_set(o, "stt_provider_base_urls", urls);
    return o;
}

/* _ws_proxy: the first set proxy variable, in the adapter's own order. */
json_t *w19_adapter_5d2c00_ws_proxy(json_t *env) {
    static const char *keys[] = {"WSS_PROXY", "wss_proxy", "HTTPS_PROXY", "https_proxy",
                                 "ALL_PROXY", "all_proxy"};
    for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        json_t *v = env && env->type == JSON_OBJECT ? json_obj_get(env, keys[i]) : NULL;
        if (py_truthy(v) && v->type == JSON_STRING) return json_string(v->str_val);
    }
    return json_null();
}

/* _parse_json: json.loads(raw) when it yields a dict, else None. */
json_t *w19_adapter_5d2c00_parse_json(const char *raw) {
    json_t *parsed, *out;
    if (!raw) return json_null();
    parsed = json_parse(raw, NULL);
    if (!parsed) return json_null();
    if (parsed->type != JSON_OBJECT) {
        json_free(parsed);
        return json_null();
    }
    out = json_copy(parsed);
    json_free(parsed);
    return out;
}

/* agent:<namespace>:<platform>:<chat_type>:<chat_id>[:<user_id>] */
json_t *w19_adapter_5d2c00_parse_gateway_session_key(const char *session_key) {
    char *tmp = dup_str(session_key ? session_key : "");
    char *parts[16];
    int n = 0;
    char *p = tmp;
    json_t *out;
    parts[n++] = p;
    while (*p && n < 16) {
        if (*p == ':') { *p = '\0'; parts[n++] = p + 1; }
        p++;
    }
    if (n < 5 || strcmp(parts[0], "agent") || !parts[1][0]) {
        free(tmp);
        return json_null();
    }
    out = json_object();
    json_set(out, "platform", json_string(parts[2]));
    json_set(out, "chat_type", json_string(parts[3]));
    json_set(out, "chat_id", json_string(parts[4]));
    if (n > 5) json_set(out, "user_id", json_string(parts[5]));
    free(tmp);
    return out;
}

/* _is_authorized_interaction_for_session: the operator must own the key's chat. */
json_t *w19_adapter_5d2c00_is_authorized_interaction(json_t *event, const char *session_key) {
    json_t *parsed = w19_adapter_5d2c00_parse_gateway_session_key(session_key);
    char *operator_s = py_strip(py_str_value(event ? json_obj_get(event, "operator_openid") : NULL));
    char *platform = NULL, *chat_type = NULL, *chat_id = NULL;
    int allowed = 0;
    if (parsed && parsed->type == JSON_OBJECT) {
        json_t *p = json_obj_get(parsed, "platform");
        platform = p && p->type == JSON_STRING ? dup_str(p->str_val) : dup_str("");
    }
    if (!parsed || !platform || strcmp(platform, "qqbot") || !operator_s[0]) {
        free(operator_s); free(platform); json_free(parsed);
        return json_bool(false);
    }
    {
        json_t *ct = json_obj_get(parsed, "chat_type");
        json_t *ci = json_obj_get(parsed, "chat_id");
        chat_type = ct && ct->type == JSON_STRING ? dup_str(ct->str_val) : dup_str("");
        chat_id = ci && ci->type == JSON_STRING ? dup_str(ci->str_val) : dup_str("");
    }
    if (!strcmp(chat_type, "c2c") || !strcmp(chat_type, "dm")) {
        allowed = chat_id[0] && !strcmp(operator_s, chat_id);
    } else if (!strcmp(chat_type, "group") || !strcmp(chat_type, "guild")) {
        json_t *g = event ? json_obj_get(event, "group_openid") : NULL;
        json_t *gd = event ? json_obj_get(event, "guild_id") : NULL;
        char *event_chat = py_strip(py_str_value(py_truthy(g) ? g : gd));
        if (event_chat[0] && !strcmp(event_chat, chat_id)) {
            json_t *u = json_obj_get(parsed, "user_id");
            char *session_user = py_strip(py_str_value(u));
            allowed = session_user[0] && !strcmp(operator_s, session_user);
            free(session_user);
        }
        free(event_chat);
    }
    free(operator_s); free(platform); free(chat_type); free(chat_id);
    json_free(parsed);
    return json_bool(allowed ? true : false);
}

/* _append_block: the block after a blank line, or alone when the text is blank. */
json_t *w19_adapter_5d2c00_append_block(const char *text, const char *block) {
    const char *t = text ? text : "";
    const char *b = block ? block : "";
    char *stripped = py_strip(t);
    json_t *out;
    if (stripped[0]) {
        char *joined = (char *)malloc(strlen(t) + strlen(b) + 3);
        char *trimmed;
        snprintf(joined, strlen(t) + strlen(b) + 3, "%s\n\n%s", t, b);
        trimmed = py_strip(joined);
        out = json_string(trimmed);
        free(trimmed);
        free(joined);
    } else {
        out = json_string(b);
    }
    free(stripped);
    return out;
}

/* _merge_quote_into: the quote block before the text, a blank line between. */
json_t *w19_adapter_5d2c00_merge_quote_into(const char *text, const char *quote_block) {
    const char *t = text ? text : "";
    if (!quote_block || !quote_block[0]) return json_string(t);
    {
        char *stripped = py_strip(t);
        json_t *out;
        if (stripped[0]) {
            size_t n = strlen(quote_block) + strlen(t) + 3;
            char *joined = (char *)malloc(n);
            char *trimmed;
            snprintf(joined, n, "%s\n\n%s", quote_block, t);
            trimmed = py_strip(joined);
            out = json_string(trimmed);
            free(trimmed);
            free(joined);
        } else {
            out = json_string(quote_block);
        }
        free(stripped);
        return out;
    }
}

/* _detect_message_type: the first media type names the message kind. */
json_t *w19_adapter_5d2c00_detect_message_type(json_t *media_urls, json_t *media_types) {
    if (!media_urls || media_urls->type != JSON_ARRAY || json_len(media_urls) == 0)
        return json_string("text");
    if (!media_types || media_types->type != JSON_ARRAY || json_len(media_types) == 0)
        return json_string("photo");
    {
        json_t *first = json_get(media_types, 0);
        char *t = py_strip(py_str_value(first));
        json_t *out;
        ascii_lower(t);
        if (strstr(t, "audio") || strstr(t, "voice") || strstr(t, "silk")) out = json_string("voice");
        else if (strstr(t, "video")) out = json_string("video");
        else if (strstr(t, "image") || strstr(t, "photo")) out = json_string("photo");
        else out = json_string("text");
        free(t);
        return out;
    }
}

/* _opt_str: the stripped str, or None for a non-str and for whitespace. */
json_t *w19_adapter_5d2c00_opt_str(json_t *value) {
    char *s;
    json_t *out;
    if (!value || value->type != JSON_STRING) return json_null();
    s = py_strip(value->str_val);
    out = s[0] ? json_string(s) : json_null();
    free(s);
    return out;
}

/* _is_voice_content_type: voice/audio, never an explicit file upload, else by extension. */
json_t *w19_adapter_5d2c00_is_voice_content_type(const char *content_type, const char *filename) {
    char *ct = py_strip(content_type ? content_type : "");
    char *fn = py_strip(filename ? filename : "");
    int voice;
    ascii_lower(ct);
    ascii_lower(fn);
    if (!strcmp(ct, "voice") || !strncmp(ct, "audio/", 6)) {
        voice = 1;
    } else if (!strcmp(ct, "file")) {
        voice = 0;
    } else {
        voice = 0;
        for (size_t i = 0; i < sizeof VOICE_EXTENSIONS / sizeof VOICE_EXTENSIONS[0]; i++)
            if (ends_with(fn, VOICE_EXTENSIONS[i])) { voice = 1; break; }
    }
    free(ct);
    free(fn);
    return json_bool(voice ? true : false);
}

/* _MAGIC_EXTS: the first magic the data starts with names the extension. */
json_t *w19_adapter_5d2c00_guess_ext_from_data(const char *hex) {
    static const struct { const char *ext; size_t len; unsigned char magic[6]; } MAGIC[] = {
        {".silk", 6, {'#', '!', 'S', 'I', 'L', 'K'}},
        {".silk", 2, {0x02, 0x21}},
        {".wav",  4, {'R', 'I', 'F', 'F'}},
        {".flac", 4, {'f', 'L', 'a', 'C'}},
        {".mp3",  2, {0xff, 0xfb}},
        {".mp3",  2, {0xff, 0xf3}},
        {".mp3",  2, {0xff, 0xf2}},
        {".ogg",  4, {0x30, 0x26, 0xb2, 0x75}},
        {".ogg",  4, {'O', 'g', 'g', 'S'}},
    };
    size_t n = 0;
    unsigned char *data = hex2bin(hex, &n);
    const char *ext = ".amr";
    for (size_t i = 0; data && i < sizeof MAGIC / sizeof MAGIC[0]; i++) {
        if (n < MAGIC[i].len) continue;
        if (!memcmp(data, MAGIC[i].magic, MAGIC[i].len)) { ext = MAGIC[i].ext; break; }
    }
    free(data);
    return json_string(ext);
}

json_t *w19_adapter_5d2c00_looks_like_silk(const char *hex) {
    json_t *ext = w19_adapter_5d2c00_guess_ext_from_data(hex);
    int silk = ext && ext->type == JSON_STRING && !strcmp(ext->str_val, ".silk");
    json_free(ext);
    return json_bool(silk ? true : false);
}

/* _rest_path: /v2/{users|groups}/{id}/{endpoint} */
json_t *w19_adapter_5d2c00_rest_path(const char *chat_type, const char *target_id,
                                     const char *endpoint) {
    const char *kind = (chat_type && !strcmp(chat_type, "c2c")) ? "users" : "groups";
    size_t n = strlen(kind) + strlen(target_id ? target_id : "") + strlen(endpoint ? endpoint : "") + 12;
    char *buf = (char *)malloc(n);
    json_t *out;
    snprintf(buf, n, "/v2/%s/%s/%s", kind, target_id ? target_id : "", endpoint ? endpoint : "");
    out = json_string(buf);
    free(buf);
    return out;
}

json_t *w19_adapter_5d2c00_messages_path(const char *chat_type, const char *target_id) {
    return w19_adapter_5d2c00_rest_path(chat_type, target_id, "messages");
}

/* _build_text_body: markdown or plain text, truncated to the adapter's cap. */
json_t *w19_adapter_5d2c00_build_text_body(const char *content, const char *reply_to, int markdown,
                                           long max_len, long msg_seq) {
    const char *c = content ? content : "";
    size_t n = strlen(c);
    char *text;
    json_t *out;
    if (max_len >= 0 && n > (size_t)max_len) n = (size_t)max_len;
    text = dup_range(c, 0, n);
    if (markdown) {
        json_t *md = json_object();
        json_set(md, "content", json_string(text));
        out = json_object();
        json_set(out, "markdown", md);
        json_set(out, "msg_type", json_number(MSG_TYPE_MARKDOWN));
        json_set(out, "msg_seq", json_number((double)msg_seq));
    } else {
        out = json_object();
        json_set(out, "content", json_string(text));
        json_set(out, "msg_type", json_number(MSG_TYPE_TEXT));
        json_set(out, "msg_seq", json_number((double)msg_seq));
        if (reply_to && reply_to[0]) {
            json_t *ref = json_object();
            json_set(ref, "message_id", json_string(reply_to));
            json_set(out, "message_reference", ref);
        }
    }
    free(text);
    return out;
}

/* _is_url: urlparse(str(source)).scheme in {"http", "https"} */
json_t *w19_adapter_5d2c00_is_url(const char *source) {
    const char *s = source ? source : "";
    size_t i = 0;
    char scheme[16];
    if (!((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z'))) return json_bool(false);
    while (s[i] && i < sizeof scheme - 1) {
        char ch = s[i];
        int ok = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                 (ch >= '0' && ch <= '9') || ch == '+' || ch == '-' || ch == '.';
        if (!ok) break;
        scheme[i] = ch;
        i++;
    }
    scheme[i] = '\0';
    ascii_lower(scheme);
    if (s[i] != ':') return json_bool(false);
    return json_bool(!strcmp(scheme, "http") || !strcmp(scheme, "https"));
}

/* _strip_at_mention: re.sub(r"^@\S+\s*", "", content.strip()) */
json_t *w19_adapter_5d2c00_strip_at_mention(const char *content) {
    char *s = py_strip(content ? content : "");
    json_t *out;
    if (s[0] == '@') {
        size_t i = 1;
        while (s[i] && !py_isspace((unsigned char)s[i])) i++;
        while (s[i] && py_isspace((unsigned char)s[i])) i++;
        out = json_string(s + i);
    } else {
        out = json_string(s);
    }
    free(s);
    return out;
}

/* _entry_matches: any entry, stripped and lower-cased, is "*" or the target. */
json_t *w19_adapter_5d2c00_entry_matches(json_t *entries, const char *target) {
    char *norm = py_strip(target ? target : "");
    int hit = 0;
    ascii_lower(norm);
    if (entries && entries->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(entries); i++) {
            char *e = py_strip(py_str_value(json_get(entries, i)));
            ascii_lower(e);
            if (!strcmp(e, "*") || !strcmp(e, norm)) hit = 1;
            free(e);
            if (hit) break;
        }
    }
    free(norm);
    return json_bool(hit ? true : false);
}

/* _guess_chat_type: the stored inbound chat kind, "c2c" when unknown. */
json_t *w19_adapter_5d2c00_guess_chat_type(json_t *chat_type_map, const char *chat_id) {
    json_t *v = (chat_type_map && chat_type_map->type == JSON_OBJECT && chat_id)
                    ? json_obj_get(chat_type_map, chat_id)
                    : NULL;
    if (v && v->type == JSON_STRING) return json_string(v->str_val);
    return json_string("c2c");
}

/* Civil date from days since 1970-01-01 (Howard Hinnant's algorithm). */
static void civil_from_days(long long z, long long *y, long long *m, long long *d) {
    long long era, doe, yoe, doy, mp, yy;
    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    yy = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp < 10 ? mp + 3 : mp - 9;
    *y = *m <= 2 ? yy + 1 : yy;
}

static int parse_uint(const char *s, size_t n, long long *out) {
    long long v = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        v = v * 10 + (s[i] - '0');
    }
    *out = v;
    return 1;
}

/* _parse_qq_timestamp: ISO 8601 (current) or integer milliseconds (legacy, UTC). */
json_t *w19_adapter_5d2c00_parse_qq_timestamp(const char *raw) {
    json_t *out = json_object();
    const char *s = raw ? raw : "";
    long long y, mo, d, hh = 0, mi = 0, ss = 0;
    char frac[8] = "000000";
    char offset[8] = "";
    size_t i = 0;
    if (!s[0]) {
        json_set(out, "iso", json_null());
        json_set(out, "is_now", json_bool(true));
        return out;
    }
    /* YYYY-MM-DD */
    if (strlen(s) >= 10 && parse_uint(s, 4, &y) && s[4] == '-' && parse_uint(s + 5, 2, &mo) &&
        s[7] == '-' && parse_uint(s + 8, 2, &d)) {
        i = 10;
        if (s[i] == 'T' || s[i] == ' ') {
            i++;
            if (strlen(s + i) < 5 || !parse_uint(s + i, 2, &hh) || s[i + 2] != ':' ||
                !parse_uint(s + i + 3, 2, &mi))
                goto legacy;
            i += 5;
            if (s[i] == ':') {
                if (!parse_uint(s + i + 1, 2, &ss)) goto legacy;
                i += 3;
            }
            if (s[i] == '.') {
                size_t k = 0;
                i++;
                while (s[i + k] >= '0' && s[i + k] <= '9' && k < 6) { frac[k] = s[i + k]; k++; }
                while (k < 6) frac[k++] = '0';
                while (s[i] >= '0' && s[i] <= '9') i++;
            }
            if (s[i] == 'Z') {
                snprintf(offset, sizeof offset, "+00:00");
                i++;
            } else if ((s[i] == '+' || s[i] == '-') && strlen(s + i) >= 6) {
                char sign = s[i];
                long long oh, om;
                if (!parse_uint(s + i + 1, 2, &oh)) goto legacy;
                if (s[i + 3] == ':') {
                    if (!parse_uint(s + i + 4, 2, &om)) goto legacy;
                    i += 6;
                } else {
                    if (!parse_uint(s + i + 3, 2, &om)) goto legacy;
                    i += 5;
                }
                snprintf(offset, sizeof offset, "%c%02lld:%02lld", sign, oh, om);
            }
            if (s[i] != '\0') goto legacy;
        } else if (s[i] != '\0') {
            goto legacy;
        }
        {
            char buf[80];
            int has_frac = strcmp(frac, "000000") != 0;
            snprintf(buf, sizeof buf, "%04lld-%02lld-%02lldT%02lld:%02lld:%02lld%s%s%s", y, mo, d, hh,
                     mi, ss, has_frac ? "." : "", has_frac ? frac : "", offset);
            json_set(out, "iso", json_string(buf));
            json_set(out, "is_now", json_bool(false));
            return out;
        }
    }
legacy:
    {
        const char *p = s;
        int neg = 0;
        long long ms = 0;
        while (py_isspace((unsigned char)*p)) p++;
        if (*p == '+' || *p == '-') { neg = *p == '-'; p++; }
        if (*p < '0' || *p > '9') {
            json_set(out, "iso", json_null());
            json_set(out, "is_now", json_bool(true));
            return out;
        }
        while (*p >= '0' && *p <= '9') { ms = ms * 10 + (*p - '0'); p++; }
        while (py_isspace((unsigned char)*p)) p++;
        if (*p != '\0') {
            json_set(out, "iso", json_null());
            json_set(out, "is_now", json_bool(true));
            return out;
        }
        if (neg) ms = -ms;
        {
            long long secs = ms / 1000, rem = ms % 1000;
            long long days, sod, yy, mm, dd, h, m2, s2;
            if (rem < 0) { rem += 1000; secs -= 1; }
            days = secs / 86400;
            sod = secs % 86400;
            if (sod < 0) { sod += 86400; days -= 1; }
            civil_from_days(days, &yy, &mm, &dd);
            h = sod / 3600;
            m2 = (sod % 3600) / 60;
            s2 = sod % 60;
            {
                char buf[96];
                if (rem)
                    snprintf(buf, sizeof buf,
                             "%04lld-%02lld-%02lldT%02lld:%02lld:%02lld.%03lld000+00:00", yy, mm, dd,
                             h, m2, s2, rem);
                else
                    snprintf(buf, sizeof buf, "%04lld-%02lld-%02lldT%02lld:%02lld:%02lld+00:00", yy,
                             mm, dd, h, m2, s2);
                json_set(out, "iso", json_string(buf));
                json_set(out, "is_now", json_bool(false));
            }
        }
    }
    return out;
}

/* _config_number: cast(cfg.get(key, default)), the default on a bad value. */
static double config_number(json_t *cfg, const char *key, double def) {
    json_t *v = cfg && cfg->type == JSON_OBJECT ? json_obj_get(cfg, key) : NULL;
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_STRING) {
        char *end = NULL;
        double d;
        if (!v->str_val[0]) return def;
        d = strtod(v->str_val, &end);
        if (!end || *end != '\0') return def;
        return d;
    }
    return def;
}

/* _resolve_stt_config: extra["stt"] first, then the QQ_STT_* environment. */
json_t *w19_adapter_5d2c00_resolve_stt_config(json_t *extra, json_t *env) {
    json_t *stt_cfg = extra && extra->type == JSON_OBJECT ? json_obj_get(extra, "stt") : NULL;
    if (stt_cfg && stt_cfg->type == JSON_OBJECT) {
        json_t *enabled = json_obj_get(stt_cfg, "enabled");
        int is_disabled = enabled && enabled->type == JSON_BOOL && !enabled->bool_val;
        if (!is_disabled) {
            json_t *base = json_obj_get(stt_cfg, "baseUrl");
            json_t *api = json_obj_get(stt_cfg, "apiKey");
            json_t *model = json_obj_get(stt_cfg, "model");
            double timeout = config_number(stt_cfg, "timeout", 60.0);
            if (!py_truthy(base)) base = json_obj_get(stt_cfg, "base_url");
            if (!py_truthy(api)) api = json_obj_get(stt_cfg, "api_key");
            if (py_truthy(base) && py_truthy(api) && base->type == JSON_STRING &&
                api->type == JSON_STRING) {
                char *url = dup_str(base->str_val);
                json_t *out = json_object();
                while (strlen(url) > 1 && url[strlen(url) - 1] == '/') url[strlen(url) - 1] = '\0';
                json_set(out, "base_url", json_string(url));
                json_set(out, "api_key", json_string(api->str_val));
                json_set(out, "model", json_string(py_truthy(model) && model->type == JSON_STRING
                                                       ? model->str_val : "whisper-1"));
                json_set(out, "timeout", json_number(timeout));
                free(url);
                return out;
            }
            if (py_truthy(api) && api->type == JSON_STRING) {
                json_t *provider_v = json_obj_get(stt_cfg, "provider");
                const char *provider = py_truthy(provider_v) && provider_v->type == JSON_STRING
                                           ? provider_v->str_val : "zai";
                const char *base_url = NULL;
                if (!strcmp(provider, "zai") || !strcmp(provider, "glm"))
                    base_url = "https://open.bigmodel.cn/api/coding/paas/v4";
                else if (!strcmp(provider, "openai"))
                    base_url = "https://api.openai.com/v1";
                if (base_url) {
                    int glm = !strcmp(provider, "zai") || !strcmp(provider, "glm");
                    json_t *out = json_object();
                    json_set(out, "base_url", json_string(base_url));
                    json_set(out, "api_key", json_string(api->str_val));
                    json_set(out, "model", json_string(py_truthy(model) && model->type == JSON_STRING
                                                           ? model->str_val
                                                           : (glm ? "glm-asr" : "whisper-1")));
                    json_set(out, "timeout", json_number(timeout));
                    return out;
                }
            }
        }
    }
    {
        json_t *key = env && env->type == JSON_OBJECT ? json_obj_get(env, "QQ_STT_API_KEY") : NULL;
        if (py_truthy(key) && key->type == JSON_STRING) {
            json_t *base_v = json_obj_get(env, "QQ_STT_BASE_URL");
            json_t *model_v = json_obj_get(env, "QQ_STT_MODEL");
            char *url = dup_str(py_truthy(base_v) && base_v->type == JSON_STRING
                                    ? base_v->str_val
                                    : "https://open.bigmodel.cn/api/coding/paas/v4");
            json_t *out = json_object();
            while (strlen(url) > 1 && url[strlen(url) - 1] == '/') url[strlen(url) - 1] = '\0';
            json_set(out, "base_url", json_string(url));
            json_set(out, "api_key", json_string(key->str_val));
            json_set(out, "model", json_string(py_truthy(model_v) && model_v->type == JSON_STRING
                                                   ? model_v->str_val : "glm-asr"));
            json_set(out, "timeout", json_number(60.0));
            free(url);
            return out;
        }
    }
    return json_null();
}
