/*
 * telegram_context.c - C11 port of upstream
 * plugins/platforms/telegram/telegram_context.py (gate w24_telegramco_889ade).
 *
 * PoP: w24_telegramco_889ade_mentions_other @ plugins/platforms/telegram/telegram_context.py:mentions_other_participants
 * PoP: w24_telegramco_889ade_starts_with_other_mention @ plugins/platforms/telegram/telegram_context.py:starts_with_other_participant_mention
 * PoP: w24_telegramco_889ade_quoted_reply @ plugins/platforms/telegram/telegram_context.py:_quoted_reply
 * PoP: w24_telegramco_889ade_group_reply_expected @ plugins/platforms/telegram/telegram_context.py:group_reply_expected
 * PoP: w24_telegramco_889ade_group_trigger_text @ plugins/platforms/telegram/telegram_context.py:group_trigger_text
 * PoP: w24_telegramco_889ade_group_identity_prompt @ plugins/platforms/telegram/telegram_context.py:group_identity_prompt
 * PoP: w24_telegramco_889ade_constants @ plugins/platforms/telegram/telegram_context.py:<identity sentence>
 *
 * What is ported is the module's whole decision surface, including the text
 * cleanup it delegates to telegram_addressing.clean_bot_trigger_text (our own
 * @handle is stripped only as the sole addressee; a leading /cmd@us keeps one
 * space before its arguments).  The adapter is a seam: bot id/username and the
 * three per-message adapter verdicts arrive as values, and no Telegram client,
 * network call or entity object is created here.
 *
 * Note on the regexes: ``[,:-]`` in the upstream patterns is a character
 * *range* (',' through ':'), not three literals, while ``[,:\-]`` is literal.
 * Both are reproduced exactly -- the range is why "@bot1.9" separates.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_telegramco_889ade.h"

#define TC_SEP_RANGE 0   /* [,:-]  -> ',' .. ':' */
#define TC_SEP_LIT   1   /* [,:\-] -> ',', ':', '-' */

static const char tc_identity[] =
    "Your Telegram bot username in this group: @";
static const char tc_identity_tail[] =
    ". Mentions of other bots are not requests for you to relay the message.";

/* ---- value helpers --------------------------------------------------- */

static const char *tc_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static int tc_truthy(const json_t *v) {
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

/* Python == over the scalars a message id / thread id can hold. */
static int tc_equal(const json_t *a, const json_t *b) {
    int an = !a || a->type == JSON_NULL, bn = !b || b->type == JSON_NULL;
    if (an || bn) return an && bn;
    if (a->type == JSON_BOOL || b->type == JSON_BOOL) {
        int av = tc_truthy(a), bv = tc_truthy(b);
        return av == bv;
    }
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return 0;
}

static char *tc_dup(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

static char *tc_strndup(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* Python str.strip() (ASCII whitespace) over a malloc'd string, in place. */
static char *tc_strip_inplace(char *s) {
    char *b, *e;
    if (!s) return NULL;
    b = s;
    while (*b && isspace((unsigned char)*b)) b++;
    e = b + strlen(b);
    while (e > b && isspace((unsigned char)e[-1])) e--;
    memmove(s, b, (size_t)(e - b));
    s[e - b] = '\0';
    return s;
}

/* message.text or message.caption (addressing_text for text/caption messages). */
static const char *tc_text_of(const json_t *msg) {
    const char *text = tc_str(json_obj_get(msg, "text"));
    if (text && text[0]) return text;
    text = tc_str(json_obj_get(msg, "caption"));
    if (text && text[0]) return text;
    {
        const char *raw = tc_str(json_obj_get(msg, "text"));
        if (raw) return raw;
    }
    return tc_str(json_obj_get(msg, "caption"));
}

static int tc_is_group_chat(const json_t *msg) {
    const char *ct = tc_str(json_obj_get(msg, "chat_type"));
    if (!ct) return 0;
    return strcmp(ct, "group") == 0 || strcmp(ct, "supergroup") == 0
        || strcmp(ct, "channel") == 0;
}

/* Telegram entity offsets are code units; the fixtures stay BMP (1 cp == 1 unit). */
static size_t tc_byte_at(const char *s, size_t n, size_t cp) {
    size_t i = 0, c = 0;
    while (i < n && c < cp) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 0x80) i += 1;
        else if ((ch & 0xE0) == 0xC0) i += 2;
        else if ((ch & 0xF0) == 0xE0) i += 3;
        else if ((ch & 0xF8) == 0xF0) i += 4;
        else i += 1;
        if (i > n) i = n;
        c++;
    }
    return i;
}

/* adapter._entity_span(source_text, entity): the entity's text, or NULL. */
static char *tc_entity_span(const char *text, const json_t *entity) {
    const json_t *off = json_obj_get(entity, "offset");
    const json_t *len = json_obj_get(entity, "length");
    long long offset, length;
    size_t n, b0, b1;
    if (!text || !off || off->type != JSON_NUMBER) return NULL;
    if (!len || len->type != JSON_NUMBER) return NULL;
    offset = (long long)off->num_val;
    length = (long long)len->num_val;
    if (offset < 0 || length <= 0) return NULL;
    n = strlen(text);
    b0 = tc_byte_at(text, n, (size_t)offset);
    b1 = tc_byte_at(text, n, (size_t)(offset + length));
    if (b1 < b0) b1 = b0;
    return tc_strndup(text + b0, b1 - b0);
}

/* entity type: str(getattr(entity,"type","")).split(".")[-1].lower() */
static const char *tc_entity_type(const json_t *entity, char *buf, size_t cap) {
    const char *raw = tc_str(json_obj_get(entity, "type"));
    const char *dot;
    size_t i;
    if (!raw) raw = "";
    dot = strrchr(raw, '.');
    raw = dot ? dot + 1 : raw;
    snprintf(buf, cap, "%s", raw);
    for (i = 0; buf[i]; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
    return buf;
}

/* adapter._current_bot_username() as a value. */
static const char *tc_username(const json_t *adapter) {
    const char *u = tc_str(json_obj_get(adapter, "username"));
    return u ? u : "";
}

static int tc_has_bot(const json_t *adapter) {
    return tc_truthy(json_obj_get(adapter, "bot"));
}

/* adapter._bot.id, or NULL when there is no bot. */
static const json_t *tc_bot_id(const json_t *adapter) {
    if (!tc_has_bot(adapter)) return NULL;
    return json_obj_get(adapter, "bot_id");
}

/* ---- entity walks ---------------------------------------------------- */

typedef int (*tc_entity_fn)(const json_t *adapter, const char *source_text,
                            const json_t *entity, void *ctx);

typedef struct {
    int found;
} tc_walk;

static void tc_for_each_source(const json_t *msg, tc_entity_fn fn, const json_t *adapter,
                               void *ctx, int *stop) {
    static const char *const pairs[2][2] = {
        {"text", "entities"}, {"caption", "caption_entities"},
    };
    size_t s;
    for (s = 0; s < 2 && !*stop; s++) {
        const char *text = tc_str(json_obj_get(msg, pairs[s][0]));
        const json_t *ents = json_obj_get(msg, pairs[s][1]);
        size_t i, n;
        if (!ents || ents->type != JSON_ARRAY) continue;
        n = json_len(ents);
        for (i = 0; i < n; i++) {
            if (fn(adapter, text, json_get(ents, i), ctx)) { *stop = 1; break; }
        }
    }
}

/* _is_other_participant */
static int tc_is_other(const json_t *adapter, const char *source_text,
                       const json_t *entity, void *ctx) {
    char typebuf[64];
    const char *type = tc_entity_type(entity, typebuf, sizeof typebuf);
    (void)ctx;
    if (strcmp(type, "mention") != 0 && strcmp(type, "text_mention") != 0)
        return 0;                                  /* only mention / text_mention address */
    if (strcmp(type, "text_mention") == 0) {
        const json_t *user_id = json_obj_get(entity, "user_id");
        const json_t *bot_id = tc_bot_id(adapter);
        if (!user_id || user_id->type == JSON_NULL) return 0;   /* user is None */
        if (!bot_id || bot_id->type == JSON_NULL) return 1;
        return !tc_equal(user_id, bot_id);
    }
    {
        char *span = tc_entity_span(source_text, entity);
        char *handle;
        int other;
        if (!span) return 0;
        handle = tc_strip_inplace(span);
        while (*handle == '@') handle++;
        for (size_t i = 0; handle[i]; i++) handle[i] = (char)tolower((unsigned char)handle[i]);
        other = handle[0] != '\0' && strcmp(handle, tc_username(adapter)) != 0;
        free(span);
        return other;
    }
}

static int tc_is_mention_entity(const json_t *entity, int *offset_ok, long long *offset) {
    char typebuf[64];
    const char *type = tc_entity_type(entity, typebuf, sizeof typebuf);
    const json_t *off = json_obj_get(entity, "offset");
    *offset_ok = off && off->type == JSON_NUMBER;
    *offset = *offset_ok ? (long long)off->num_val : 0;
    return strcmp(type, "mention") == 0 || strcmp(type, "text_mention") == 0;
}

static int tc_any_other(const json_t *adapter, const json_t *msg) {
    int stop = 0;
    tc_for_each_source(msg, tc_is_other, adapter, NULL, &stop);
    return stop;
}

/* starts_with_other_participant_mention: offset == len(text) - len(text.lstrip()) */
static int tc_starts_other(const json_t *adapter, const json_t *msg) {
    static const char *const pairs[2][2] = {
        {"text", "entities"}, {"caption", "caption_entities"},
    };
    size_t s;
    for (s = 0; s < 2; s++) {
        const char *text = tc_str(json_obj_get(msg, pairs[s][0]));
        const json_t *ents = json_obj_get(msg, pairs[s][1]);
        size_t i, n;
        long long lead;
        if (!text) continue;
        lead = 0;
        while (text[lead] && isspace((unsigned char)text[lead])) lead++;
        if (!ents || ents->type != JSON_ARRAY) continue;
        n = json_len(ents);
        for (i = 0; i < n; i++) {
            const json_t *e = json_get(ents, i);
            int off_ok;
            long long off;
            if (!e || e->type != JSON_OBJECT) continue;
            if (!tc_is_mention_entity(e, &off_ok, &off)) continue;
            if (!off_ok || off != lead) continue;
            if (tc_is_other(adapter, text, e, NULL)) return 1;
        }
    }
    return 0;
}

/* ---- quoted reply --------------------------------------------------- */

static const json_t *tc_reply_of(const json_t *msg) {
    const json_t *reply = json_obj_get(msg, "reply");
    if (!reply || reply->type == JSON_NULL) return NULL;
    if (tc_truthy(json_obj_get(msg, "is_topic_message"))) {
        if (tc_equal(json_obj_get(reply, "message_id"),
                     json_obj_get(msg, "message_thread_id"))) return NULL;
    }
    return reply;
}

/* ---- trigger-text cleanup (telegram_addressing.clean_bot_trigger_text) */

static int tc_is_word(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
        || c == '_';
}

/* [,:-] is a range ',' .. ':' upstream, not three literals. */
static int tc_sep(char c, int mode) {
    if (mode == TC_SEP_RANGE) return c >= ',' && c <= ':';
    return c == ',' || c == ':' || c == '-';
}

/* "@<user>\b<seps>*\s*" at s -> length consumed, or 0. */
static size_t tc_handle_match(const char *s, const char *user, int mode) {
    size_t ulen = strlen(user), i;
    const char *p;
    if (s[0] != '@' || ulen == 0) return 0;
    if (strncasecmp(s + 1, user, ulen) != 0) return 0;
    p = s + 1 + ulen;
    if (tc_is_word(*p)) return 0;                      /* \b */
    while (*p && tc_sep(*p, mode)) p++;
    while (*p && isspace((unsigned char)*p)) p++;
    i = (size_t)(p - s);
    return i;
}

/* re.sub(r"(?i)@H\b<seps>*\s*", "", s) */
static char *tc_remove_handles(const char *s, const char *user, int mode) {
    size_t n = strlen(s), out = 0, i = 0;
    char *buf = (char *)malloc(n + 1);
    if (!buf) return NULL;
    while (i < n) {
        size_t m = tc_handle_match(s + i, user, mode);
        if (m) { i += m; continue; }
        buf[out++] = s[i++];
    }
    buf[out] = '\0';
    return buf;
}

/* ^(\s*)(?:@H\b<seps>*\s*)+  ->  \1 */
static char *tc_strip_leading_run(char *s, const char *user) {
    char *p = s;
    size_t ws, q, m, total = 0;
    char *out;
    while (*p && isspace((unsigned char)*p)) p++;
    ws = (size_t)(p - s);
    q = ws;
    while ((m = tc_handle_match(s + q, user, TC_SEP_RANGE)) != 0) { q += m; total += m; }
    if (!total) return s;
    out = (char *)malloc(ws + strlen(s + q) + 1);
    if (!out) return s;
    memcpy(out, s, ws);
    strcpy(out + ws, s + q);
    free(s);
    return out;
}

static int tc_is_dash_char(const char *p) {
    if (*p == '-') return 1;
    if ((unsigned char)p[0] == 0xE2 && (unsigned char)p[1] == 0x80
        && ((unsigned char)p[2] == 0x93 || (unsigned char)p[2] == 0x94)) return 3;
    return 0;
}

/* (?:@H\b<seps>*\s*)+(?=$|[-–—]+\w)  -> removed */
static char *tc_remove_runs_before_dash(const char *s, const char *user) {
    size_t n = strlen(s), i = 0, out = 0;
    char *buf = (char *)malloc(n + 1);
    if (!buf) return NULL;
    while (i < n) {
        if (s[i] == '@') {
            size_t q = i, m, total = 0;
            while ((m = tc_handle_match(s + q, user, TC_SEP_RANGE)) != 0) { q += m; total += m; }
            if (total) {
                const char *la = s + q;
                int ok = (*la == '\0');
                if (!ok) {
                    const char *d = la;
                    int nd = 0;
                    for (;;) {
                        int dl = tc_is_dash_char(d);
                        if (!dl) break;
                        d += dl;
                        nd = 1;
                    }
                    if (nd && tc_is_word(*d)) ok = 1;
                }
                if (ok) { i = q; continue; }
            }
        }
        buf[out++] = s[i++];
    }
    buf[out] = '\0';
    return buf;
}

/* ---- exported port -------------------------------------------------- */

json_t *w24_telegramco_889ade_mentions_other(const json_t *adapter, const json_t *msg) {
    if (!msg || msg->type != JSON_OBJECT) return json_bool(0);
    return json_bool(tc_any_other(adapter, msg));
}

json_t *w24_telegramco_889ade_starts_with_other_mention(const json_t *adapter,
                                                        const json_t *msg) {
    if (!msg || msg->type != JSON_OBJECT) return json_bool(0);
    return json_bool(tc_starts_other(adapter, msg));
}

json_t *w24_telegramco_889ade_quoted_reply(const json_t *msg) {
    const json_t *reply;
    if (!msg || msg->type != JSON_OBJECT) return json_null();
    reply = tc_reply_of(msg);
    if (!reply) return json_null();
    return json_copy(json_obj_get(reply, "message_id"));
}

/* /^\s*\/([A-Za-z0-9_-]+)... suffix; returns 1 when the text opens with "/" */
static int tc_opens_with_slash(const char *text) {
    const char *p = text ? text : "";
    while (*p && isspace((unsigned char)*p)) p++;
    return *p == '/';
}

json_t *w24_telegramco_889ade_group_reply_expected(const json_t *chat_type,
                                                   const json_t *adapter,
                                                   const json_t *msg) {
    const char *ct = tc_str(chat_type);
    const char *text;
    int addressed;

    if (!ct) ct = "";
    if (strcmp(ct, "group") != 0)                 /* dm -> True, anything else -> None */
        return strcmp(ct, "dm") == 0 ? json_bool(1) : json_null();

    if (!msg || msg->type != JSON_OBJECT) msg = NULL;
    text = tc_text_of(msg);
    addressed = tc_opens_with_slash(text)
        || tc_truthy(json_obj_get(msg, "reply_to_bot"))
        || tc_truthy(json_obj_get(msg, "mentions_bot"))
        || tc_truthy(json_obj_get(msg, "matches_patterns"));
    if (addressed) return json_bool(1);

    if (!tc_has_bot(adapter) || tc_username(adapter)[0] == '\0') return json_null();

    {
        const json_t *reply = tc_reply_of(msg);
        if (reply) {
            const json_t *from_user = json_obj_get(reply, "from_user_id");
            const json_t *sender_chat = json_obj_get(reply, "sender_chat");
            int unknown = (!from_user || from_user->type == JSON_NULL)
                || (sender_chat && sender_chat->type != JSON_NULL);
            return unknown ? json_null() : json_bool(0);
        }
    }
    if (tc_starts_other(adapter, msg)) return json_bool(0);

    {
        int empty_text = !text || text[0] == '\0';
        int spoken = empty_text && (tc_truthy(json_obj_get(msg, "voice"))
                                    || tc_truthy(json_obj_get(msg, "video_note"))
                                    || tc_truthy(json_obj_get(msg, "audio")));
        if ((text && strstr(text, "?")) || spoken) return json_null();
    }
    return json_bool(0);
}

static char *tc_clean_trigger(const char *text, const char *user) {
    size_t i = 0, ulen = strlen(user ? user : "");
    char *result = NULL;

    if (!text || !text[0] || ulen == 0) return tc_dup(text);

    while (isspace((unsigned char)text[i])) i++;
    for (;;) {                                        /* (?:@H<seps>*\s*)* */
        size_t m = tc_handle_match(text + i, user, TC_SEP_RANGE);
        if (!m) break;
        i += m;
    }
    if (text[i] == '/') {
        size_t cs = i, ce, j;
        const char *g2 = NULL;
        size_t g2len = 0;
        i++;
        j = i;
        while (tc_is_word(text[j]) || text[j] == '-') j++;
        if (j > i) {
            ce = j;
            i = j;
            if (text[i] == '@') {
                size_t k = i + 1, n = 0;
                while (n < 32 && tc_is_word(text[k])) { k++; n++; }
                if (n >= 1) { g2 = text + i + 1; g2len = n; i = k; }
            }
            {
                char la = text[i];
                int ok = la == '\0' || isspace((unsigned char)la) || la == ','
                    || la == ':' || la == ';' || la == '-';
                if (ok && (!g2 || (g2len == ulen && strncasecmp(g2, user, ulen) == 0))) {
                    char *rest;
                    const char *src = text + i;
                    if (g2) {
                        const char *p = src;
                        char *tmp;
                        while (*p && tc_sep(*p, TC_SEP_RANGE)) p++;
                        while (*p && isspace((unsigned char)*p)) p++;
                        tmp = (char *)malloc(strlen(p) + 2);
                        if (!tmp) return NULL;
                        tmp[0] = ' ';
                        strcpy(tmp + 1, p);
                        rest = tmp;
                    } else {
                        rest = tc_dup(src);
                    }
                    rest = tc_strip_leading_run(rest, user);
                    {
                        char *trimmed = tc_remove_runs_before_dash(rest, user);
                        free(rest);
                        rest = trimmed;
                    }
                    result = (char *)malloc((ce - cs) + strlen(rest ? rest : "") + 1);
                    if (result) {
                        memcpy(result, text + cs, ce - cs);
                        strcpy(result + (ce - cs), rest ? rest : "");
                        tc_strip_inplace(result);
                    }
                    free(rest);
                    return result;
                }
            }
        }
    }

    /* legacy: \s*(/[A-Za-z0-9_]+)@H\b[,:\-]*\s* */
    {
        size_t k = 0, cmd_start, cmd_end;
        char *rest, *joined;
        while (isspace((unsigned char)text[k])) k++;
        cmd_start = k + 1;
        if (text[k] == '/') {
            size_t e = cmd_start;
            while (tc_is_word(text[e])) e++;
            if (e > cmd_start && text[e] == '@'
                && strncasecmp(text + e + 1, user, ulen) == 0
                && !tc_is_word(text[e + 1 + ulen])) {
                const char *after = text + e + 1 + ulen;
                while (*after && tc_sep(*after, TC_SEP_LIT)) after++;
                while (*after && isspace((unsigned char)*after)) after++;
                cmd_end = e;
                rest = tc_remove_handles(after, user, TC_SEP_LIT);
                tc_strip_inplace(rest);
                if (rest && rest[0]) {
                    joined = (char *)malloc((cmd_end - cmd_start) + strlen(rest) + 2);
                    if (joined) {
                        memcpy(joined, text + cmd_start, cmd_end - cmd_start);
                        joined[cmd_end - cmd_start] = ' ';
                        strcpy(joined + (cmd_end - cmd_start) + 1, rest);
                    }
                    free(rest);
                    return joined;
                }
                free(rest);
                return tc_strndup(text + cmd_start, cmd_end - cmd_start);
            }
        }
    }

    {
        char *cleaned = tc_remove_handles(text, user, TC_SEP_LIT);
        if (!cleaned) return NULL;
        tc_strip_inplace(cleaned);
        if (!cleaned[0]) { free(cleaned); return tc_dup(text); }
        return cleaned;
    }
}

json_t *w24_telegramco_889ade_group_trigger_text(const json_t *adapter,
                                                 const json_t *msg,
                                                 const json_t *text) {
    const char *raw = tc_str(text);
    if (tc_is_group_chat(msg) && tc_any_other(adapter, msg))
        return text ? json_copy(text) : json_null();     /* leave other addressees intact */
    {
        char *cleaned = tc_clean_trigger(raw, tc_username(adapter));
        json_t *out;
        if (!raw) return json_null();
        out = cleaned ? json_string(cleaned) : json_null();
        free(cleaned);
        return out;
    }
}

json_t *w24_telegramco_889ade_group_identity_prompt(const json_t *adapter,
                                                    const json_t *msg,
                                                    const json_t *channel_prompt) {
    const char *prompt = tc_str(channel_prompt);
    const char *username;
    char *identity;
    json_t *out;

    if (!tc_is_group_chat(msg) || !tc_has_bot(adapter))
        return channel_prompt ? json_copy(channel_prompt) : json_null();
    username = tc_username(adapter);
    if (username[0] == '\0') return channel_prompt ? json_copy(channel_prompt) : json_null();

    identity = (char *)malloc(strlen(tc_identity) + strlen(username)
                              + strlen(tc_identity_tail) + 1);
    if (!identity) return json_null();
    sprintf(identity, "%s%s%s", tc_identity, username, tc_identity_tail);
    if (prompt && prompt[0]) {
        char *both = (char *)malloc(strlen(prompt) + strlen(identity) + 3);
        if (!both) { free(identity); return json_null(); }
        sprintf(both, "%s\n\n%s", prompt, identity);
        out = json_string(both);
        free(both);
    } else {
        out = json_string(identity);
    }
    free(identity);
    return out;
}

json_t *w24_telegramco_889ade_constants(void) {
    json_t *o = json_object();
    char *full = (char *)malloc(strlen(tc_identity) + strlen(tc_identity_tail) + 32);
    if (full) {
        sprintf(full, "%s{username}%s", tc_identity, tc_identity_tail);
        json_set(o, "identity_template", json_string(full));
        free(full);
    }
    {
        json_t *verdicts = json_array();
        json_append(verdicts, json_bool(1));
        json_append(verdicts, json_bool(0));
        json_append(verdicts, json_null());
        json_set(o, "reply_verdicts", verdicts);
    }
    return o;
}
