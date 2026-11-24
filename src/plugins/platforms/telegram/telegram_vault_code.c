/*
 * telegram_vault_code.c - real C11 port of upstream Hermes
 *   plugins/platforms/telegram/telegram_vault_code.py.
 *
 * The code card's decisions: which inbound message answers which card (the author fence and
 * the ended-card swallow), what popping a reply does to the pending counter and the bounded
 * retired map, the ForceReply prompt text, and the debug-log drop while a code is pending.
 *
 * PoP: w19_telegramva_544569_match_card @ plugins/platforms/telegram/telegram_vault_code.py:_match_vault_code_card
 * PoP: w19_telegramva_544569_pop_reply @ plugins/platforms/telegram/telegram_vault_code.py:_pop_vault_code_reply
 * PoP: w19_telegramva_544569_prompt_text @ plugins/platforms/telegram/telegram_vault_code.py:send_vault_code_prompt
 * PoP: w19_telegramva_544569_filter_allows @ plugins/platforms/telegram/telegram_vault_code.py:_DropPendingCodeUpdates.filter
 */
#include "hermes_json.h"
#include "noc_w19_telegramva_544569.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HINT_LIMIT        300
#define PLACEHOLDER_LIMIT 64
#define RETIRED_MAX       512

static const char *jstr(json_t *v) { return (v && v->type == JSON_STRING) ? v->str_val : NULL; }

static int jtruthy(json_t *v) {
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

/* Python str() of a scalar id (int chat ids arrive as strings upstream; either way str()). */
static const char *pystr(json_t *v, char *buf, size_t n) {
    buf[0] = '\0';
    if (!v || v->type == JSON_NULL) return buf;
    if (v->type == JSON_STRING) { snprintf(buf, n, "%s", v->str_val ? v->str_val : ""); return buf; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, n, "%lld", (long long)d);
        else snprintf(buf, n, "%g", d);
        return buf;
    }
    if (v->type == JSON_BOOL) { snprintf(buf, n, "%s", v->bool_val ? "True" : "False"); return buf; }
    return buf;
}

/* html.escape(s) with quote=True. */
static char *html_escape_dup(const char *s) {
    if (!s) s = "";
    size_t cap = strlen(s) * 6 + 1;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; s[i]; i++) {
        const char *rep = NULL;
        switch (s[i]) {
            case '&': rep = "&amp;"; break;
            case '<': rep = "&lt;"; break;
            case '>': rep = "&gt;"; break;
            case '"': rep = "&quot;"; break;
            case '\'': rep = "&#x27;"; break;
            default: break;
        }
        if (rep) {
            size_t rl = strlen(rep);
            memcpy(out + o, rep, rl);
            o += rl;
        } else {
            out[o++] = s[i];
        }
    }
    out[o] = '\0';
    return out;
}

/* "{site}".format-style substitution for one keyword (the i18n seam). */
static void format_site(const char *tmpl, const char *site, char *out, size_t n) {
    size_t o = 0;
    const char *p = tmpl ? tmpl : "";
    while (*p && o + 1 < n) {
        if (!strncmp(p, "{site}", 6)) {
            const char *v = site ? site : "";
            size_t vl = strlen(v);
            if (o + vl + 1 > n) break;
            memcpy(out + o, v, vl);
            o += vl;
            p += 6;
        } else {
            out[o++] = *p++;
        }
    }
    out[o] = '\0';
}

static json_t *key_json(const char *chat, const char *msg) {
    json_t *k = json_array();
    json_append(k, json_string(chat ? chat : ""));
    json_append(k, json_string(msg ? msg : ""));
    return k;
}

/* retired rows carry the key as a [chat, message] pair */
static int rkey_eq(json_t *entry, const char *chat, const char *msg) {
    json_t *k = json_obj_get(entry, "key");
    if (!k || k->type != JSON_ARRAY || json_len(k) != 2) return 0;
    const char *c = jstr(json_get(k, 0));
    const char *m = jstr(json_get(k, 1));
    return c && m && !strcmp(c, chat) && !strcmp(m, msg);
}

static int key_eq(json_t *entry, const char *chat, const char *msg) {
    const char *ec = jstr(json_obj_get(entry, "chat_id"));
    const char *em = jstr(json_obj_get(entry, "message_id"));
    return ec && em && !strcmp(ec, chat) && !strcmp(em, msg);
}

/* _match_vault_code_card: (live key or None, is the author's reply to an ended card) */
json_t *w19_telegramva_544569_match_card(json_t *chat_id, json_t *user_id, json_t *replied_to,
                                         json_t *chat_type, double now, json_t *replies,
                                         json_t *retired) {
    json_t *out = json_object();
    char chat[256], uid[256];
    pystr(chat_id, chat, sizeof chat);
    pystr(user_id, uid, sizeof uid);
    json_t *jkey = NULL;
    int to_ended = 0;
    int replied = jtruthy(replied_to);

    if (replied) {
        char msg_id[64];
        pystr(replied_to, msg_id, sizeof msg_id);
        json_t *reply = NULL;
        if (replies && replies->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(replies); i++) {
                json_t *e = json_get(replies, i);
                if (e && e->type == JSON_OBJECT && key_eq(e, chat, msg_id)) { reply = e; break; }
            }
        }
        if (reply) {
            const char *owner = jstr(json_obj_get(reply, "user_id"));
            if (owner && !strcmp(owner, uid)) {
                double exp = json_get_num(reply, "expires_at", 0.0);
                if (now < exp) jkey = key_json(chat, msg_id);
                to_ended = (now >= exp) ? 1 : 0;
                if (jkey || to_ended) goto done;
            }
        }
        if (retired && retired->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(retired); i++) {
                json_t *e = json_get(retired, i);
                if (!e || e->type != JSON_OBJECT) continue;
                if (!rkey_eq(e, chat, msg_id)) continue;
                const char *owner = jstr(json_obj_get(e, "user_id"));
                if (owner && !strcmp(owner, uid)) { to_ended = 1; goto done; }
            }
        }
    }
    {
        const char *ctype = jstr(chat_type);
        if (!ctype || strcmp(ctype, "private")) goto done;
    }
    /* any other private-chat text from the author answers their NEWEST live card */
    if (replies && replies->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(replies); i++) {
            json_t *e = json_get(replies, i);
            if (!e || e->type != JSON_OBJECT) continue;
            const char *ec = jstr(json_obj_get(e, "chat_id"));
            const char *owner = jstr(json_obj_get(e, "user_id"));
            double exp = json_get_num(e, "expires_at", 0.0);
            if (ec && !strcmp(ec, chat) && owner && !strcmp(owner, uid) && now < exp) {
                json_free(jkey);
                jkey = key_json(ec, jstr(json_obj_get(e, "message_id")));
            }
        }
    }
done:
    json_set(out, "key", jkey ? jkey : json_null());
    json_set(out, "to_ended", json_bool(to_ended ? true : false));
    return out;
}

/* _pop_vault_code_reply: drop the live card, spend one pending, remember the author (bounded). */
json_t *w19_telegramva_544569_pop_reply(json_t *replies, json_t *retired, int pending,
                                        const char *chat_id, const char *message_id) {
    json_t *out = json_object();
    json_t *kept = json_array();
    json_t *ret = json_array();
    int found = 0;
    const char *chat = chat_id ? chat_id : "";
    const char *msg = message_id ? message_id : "";
    size_t n = (replies && replies->type == JSON_ARRAY) ? json_len(replies) : 0;

    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(replies, i);
        if (!found && e && e->type == JSON_OBJECT && key_eq(e, chat, msg)) { found = 1; continue; }
        json_append(kept, json_copy(e));
    }
    size_t rn = (retired && retired->type == JSON_ARRAY) ? json_len(retired) : 0;
    for (size_t i = 0; i < rn; i++) json_append(ret, json_copy(json_get(retired, i)));

    if (found) {
        const char *owner = NULL;
        for (size_t i = 0; i < n; i++) {
            json_t *e = json_get(replies, i);
            if (e && e->type == JSON_OBJECT && key_eq(e, chat, msg)) { owner = jstr(json_obj_get(e, "user_id")); break; }
        }
        if (pending > 0) pending -= 1;                 /* max(0, pending - 1) */
        /* retired[key] = user_id: an existing key keeps its position, a new one appends */
        json_t *ret2 = json_array();
        int replaced = 0;
        for (size_t i = 0; i < json_len(ret); i++) {
            json_t *e = json_get(ret, i);
            if (!replaced && e && e->type == JSON_OBJECT && rkey_eq(e, chat, msg)) {
                json_t *entry = json_object();
                json_set(entry, "key", key_json(chat, msg));
                json_set(entry, "user_id", owner ? json_string(owner) : json_null());
                json_append(ret2, entry);
                replaced = 1;
            } else {
                json_append(ret2, json_copy(e));
            }
        }
        if (!replaced) {
            json_t *entry = json_object();
            json_set(entry, "key", key_json(chat, msg));
            json_set(entry, "user_id", owner ? json_string(owner) : json_null());
            json_append(ret2, entry);
        }
        json_free(ret);
        ret = ret2;
        while (json_len(ret) > RETIRED_MAX) {          /* oldest dropped first */
            json_t *rest = json_array();
            for (size_t i = 1; i < json_len(ret); i++) json_append(rest, json_copy(json_get(ret, i)));
            json_free(ret);
            ret = rest;
        }
    }
    json_set(out, "found", json_bool(found ? true : false));
    json_set(out, "pending", json_number((double)pending));
    json_set(out, "replies", kept);
    json_set(out, "retired", ret);
    return out;
}

/* send_vault_code_prompt's ForceReply text. */
json_t *w19_telegramva_544569_prompt_text(json_t *chat_id, json_t *user_id, json_t *site,
                                          json_t *hint, json_t *title_template,
                                          json_t *body_template, json_t *label_template) {
    json_t *out = json_object();
    char chat[256], uid[256];
    char title_raw[4096], body_raw[8192], label_raw[1024], text[32768];
    pystr(chat_id, chat, sizeof chat);
    pystr(user_id, uid, sizeof uid);
    const char *raw_site = jstr(site) ? jstr(site) : "";
    format_site(jstr(title_template), raw_site, title_raw, sizeof title_raw);
    format_site(jstr(body_template), raw_site, body_raw, sizeof body_raw);
    {
        char *et = html_escape_dup(title_raw);
        char *eb = html_escape_dup(body_raw);
        snprintf(text, sizeof text, "<b>%s</b>\n%s", et ? et : "", eb ? eb : "");
        free(et);
        free(eb);
    }
    if (jtruthy(hint)) {
        const char *h = jstr(hint) ? jstr(hint) : "";
        size_t hlen = strlen(h);
        if (hlen > HINT_LIMIT) hlen = HINT_LIMIT;
        char *head = (char *)malloc(hlen + 1);
        if (head) {
            memcpy(head, h, hlen);
            head[hlen] = '\0';
            char *esc = html_escape_dup(head);
            free(head);
            size_t tl = strlen(text);
            if (esc && tl + strlen(esc) + 2 < sizeof text)
                snprintf(text + tl, sizeof text - tl, "\n%s", esc);
            free(esc);
        }
    }
    if (chat[0] == '-') {
        /* in a group the reply field opens only for the mentioned user */
        char prefix[128];
        int plen = snprintf(prefix, sizeof prefix,
                            "<a href=\"tg://user?id=%lld\">\xe2\x81\xa3</a>", (long long)strtoll(uid, NULL, 10));
        if (plen > 0 && (size_t)plen < sizeof prefix) {
            size_t tl = strlen(text);
            if ((size_t)plen + tl + 1 < sizeof text) {
                memmove(text + plen, text, tl + 1);
                memcpy(text, prefix, (size_t)plen);
            }
        }
    }
    format_site(jstr(label_template), raw_site, label_raw, sizeof label_raw);
    if (strlen(label_raw) > PLACEHOLDER_LIMIT) label_raw[PLACEHOLDER_LIMIT] = '\0';
    json_set(out, "text", json_string(text));
    json_set(out, "placeholder", json_string(label_raw));
    return out;
}

/* _DropPendingCodeUpdates.filter */
json_t *w19_telegramva_544569_filter_allows(int pending, json_t *message_text) {
    if (pending == 0) return json_bool(true);
    const char *s = jstr(message_text) ? jstr(message_text) : "";
    int drop = strstr(s, "getUpdates") != NULL || strstr(s, "Webhook received data") != NULL;
    return json_bool(drop ? false : true);
}
