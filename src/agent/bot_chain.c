/*
 * bot_chain.c — real C11 port of upstream Hermes agent/bot_chain.py.
 *
 * PoP: botchain_hops @ agent/bot_chain.py:_hops
 * PoP: botchain_envelope @ agent/bot_chain.py:Admission.envelope
 * PoP: botchain_carry @ agent/bot_chain.py:carry_chain
 * PoP: botchain_admit @ agent/bot_chain.py:admit_send
 *
 * admit_send is ported with its runtime state/settings resolved in `facts`.
 * The live sha256 ask digest is an injective function of the normalized body,
 * so the ask identity here IS the normalized body (" ".join(str(body).split())).
 */
#include "noc_w5_botchain_6aa11f.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MSG_MAXHops \
    "This bot-to-bot exchange already reached its limit of %d messages " \
    "(gateway.bot_loop_guard.max_hops), so nothing was sent. Do not retry; " \
    "finish your turn and tell your user where it stands."
#define MSG_NOANSWER \
    "That bot sent you a reply that needs no answer, so this exchange is over. " \
    "Do not message them again in this turn; tell your user what you learned instead."
#define MSG_DUPLICATE \
    "You already sent this exact message to that bot in this exchange, so it was " \
    "not sent again. Wait for their answer or say something new."

static json_t *jget(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT ? json_obj_get(obj, key) : NULL;
}

static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

/* " ".join(str(value).split()) for a JSON scalar string/value. */
static char *normalize(const char *s) {
    const char *p;
    size_t cap, len = 0;
    int inword = 0;
    char *out;
    s = s ? s : "";
    cap = strlen(s) + 2;
    out = (char *)malloc(cap);
    if (!out) return strdup("");
    for (p = s; *p; p++) {
        if (isspace((unsigned char)*p)) { inword = 0; continue; }
        if (!inword && len > 0) out[len++] = ' ';
        out[len++] = *p;
        inword = 1;
    }
    out[len] = '\0';
    return out;
}

json_t *botchain_hops(const json_t *author) {
    json_t *v = jget(author, "hops");
    if (v && v->type == JSON_NUMBER && v->num_val > 0.0 &&
        floor(v->num_val) == v->num_val)
        return json_number(v->num_val);
    return json_number(0);
}

json_t *botchain_envelope(double hop) {
    json_t *out = json_object();
    if (hop > 1.0) json_set(out, "hops", json_number(hop));
    return out;
}

json_t *botchain_carry(const char *old_id, const char *new_id) {
    int ok = old_id && old_id[0] && new_id && new_id[0] && strcmp(old_id, new_id) != 0;
    return json_bool(ok ? true : false);
}

typedef struct { const char *r; const char *n; } Ask;

static int ask_in_state(const json_t *asks, const char *r, const char *n) {
    size_t i, cnt;
    if (!asks || asks->type != JSON_ARRAY) return 0;
    cnt = asks->c.count;
    for (i = 0; i < cnt; i++) {
        json_t *pair = json_get(asks, i);
        json_t *a, *b;
        if (!pair || pair->type != JSON_ARRAY || pair->c.count < 2) continue;
        a = json_get(pair, 0);
        b = json_get(pair, 1);
        if (a && a->type == JSON_STRING && a->str_val &&
            b && b->type == JSON_STRING && b->str_val &&
            strcmp(a->str_val, r) == 0 && strcmp(b->str_val, n) == 0)
            return 1;
    }
    return 0;
}

static int cmp_str(const void *pa, const void *pb) {
    const char *const *a = (const char *const *)pa;
    const char *const *b = (const char *const *)pb;
    return strcmp(*a, *b);
}

static int cmp_ask(const void *pa, const void *pb) {
    const Ask *a = (const Ask *)pa;
    const Ask *b = (const Ask *)pb;
    int c = strcmp(a->r, b->r);
    if (c) return c;
    return strcmp(a->n, b->n);
}

static json_t *default_verdict(void) {
    json_t *out = json_object();
    json_set(out, "refusal", json_null());
    json_set(out, "refused", json_object());
    json_set(out, "notify_only", json_array());
    json_set(out, "hop", json_number(0));
    json_set(out, "asks", json_array());
    json_set(out, "prior_sent", json_number(0));
    return out;
}

json_t *botchain_admit(const json_t *facts) {
    int enabled, max_hops, expect, bot_turn, hop;
    const char *body, *aid;
    char *norm, aid_buf[128];
    json_t *author, *recipients, *state_asks, *no_reply;
    json_t *out, *refused, *notify, *asks_out;
    int state_hops, state_sent;
    size_t i, nrecip;
    char **notify_names;
    size_t nnotify = 0;
    Ask *admitted;
    size_t nadmitted = 0;

    if (!facts || facts->type != JSON_OBJECT) return default_verdict();
    enabled = json_get_bool(facts, "enabled", true) ? 1 : 0;
    if (!enabled) return default_verdict();

    max_hops = (int)json_get_num(facts, "max_hops", 4);
    expect = json_get_bool(facts, "bot_replies_expect_reply", false) ? 1 : 0;
    author = jget(facts, "author");
    recipients = jget(facts, "recipients");
    body = json_get_str(facts, "body", "");
    state_hops = (int)json_get_num(facts, "hops", 0);
    state_sent = (int)json_get_num(facts, "sent", 0);
    state_asks = jget(facts, "asks");
    no_reply = jget(facts, "no_reply_from");

    /* author_id = str(author.get("id") or "") if author.get("is_bot") else "" */
    aid_buf[0] = '\0';
    aid = aid_buf;
    if (author && py_truthy(jget(author, "is_bot"))) {
        json_t *idv = jget(author, "id");
        if (idv && idv->type == JSON_STRING && idv->str_val) {
            snprintf(aid_buf, sizeof(aid_buf), "%s", idv->str_val);
        } else if (idv && idv->type == JSON_NUMBER && idv->num_val != 0.0) {
            snprintf(aid_buf, sizeof(aid_buf), "%lld", (long long)idv->num_val);
        }
    }

    norm = normalize(body);
    bot_turn = state_hops > 0 ? 1 : 0;
    hop = bot_turn ? state_sent + 1 : 1;

    out = json_object();
    if (hop > max_hops) {
        char msg[512];
        snprintf(msg, sizeof(msg), MSG_MAXHops, max_hops);
        json_set(out, "refusal", json_string(msg));
        json_set(out, "refused", json_object());
        json_set(out, "notify_only", json_array());
        json_set(out, "hop", json_number(0));
        json_set(out, "asks", json_array());
        json_set(out, "prior_sent", json_number(0));
        free(norm);
        return out;
    }

    refused = json_object();
    notify = json_array();
    asks_out = json_array();
    nrecip = (recipients && recipients->type == JSON_ARRAY) ? recipients->c.count : 0;
    notify_names = nrecip ? (char **)malloc(sizeof(char *) * (nrecip + 1)) : NULL;
    admitted = nrecip ? (Ask *)malloc(sizeof(Ask) * (nrecip + 1)) : NULL;

    for (i = 0; i < nrecip; i++) {
        json_t *rv = json_get(recipients, i);
        const char *r = (rv && rv->type == JSON_STRING && rv->str_val) ? rv->str_val : "";
        int replying = aid[0] && strcmp(r, aid) == 0;
        int no_answer = replying && no_reply && no_reply->type == JSON_STRING &&
                        no_reply->str_val && strcmp(no_reply->str_val, aid) == 0;
        int duplicate = bot_turn && ask_in_state(state_asks, r, norm);
        if (no_answer) {
            json_set(refused, r, json_string(MSG_NOANSWER));
        } else if (duplicate) {
            json_set(refused, r, json_string(MSG_DUPLICATE));
        } else {
            admitted[nadmitted].r = r;
            admitted[nadmitted].n = norm;
            nadmitted++;
            if (replying && !expect) {
                notify_names[nnotify++] = (char *)r;
            }
        }
    }

    if (nnotify > 1) qsort(notify_names, nnotify, sizeof(char *), cmp_str);
    for (i = 0; i < nnotify; i++) json_append(notify, json_string(notify_names[i]));
    if (nadmitted > 1) qsort(admitted, nadmitted, sizeof(Ask), cmp_ask);
    for (i = 0; i < nadmitted; i++) {
        json_t *pair = json_array();
        json_append(pair, json_string(admitted[i].r));
        json_append(pair, json_string(admitted[i].n));
        json_append(asks_out, pair);
    }

    json_set(out, "refusal", json_null());
    json_set(out, "refused", refused);
    json_set(out, "notify_only", notify);
    json_set(out, "hop", json_number(hop));
    json_set(out, "asks", asks_out);
    json_set(out, "prior_sent", json_number(state_sent));

    free(norm);
    free(notify_names);
    free(admitted);
    return out;
}
