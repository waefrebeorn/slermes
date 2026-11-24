/*
 * telegram_typing_gate.c -- real C11 port of the per-chat Telegram typing
 * admission gate, upstream plugins/platforms/telegram/telegram_typing_gate.py.
 *
 * PoP: w21_telegramty_a78c8c_typing_in_cooldown @ plugins/platforms/telegram/telegram_typing_gate.py:_typing_in_cooldown
 * PoP: w21_telegramty_a78c8c_typing_may_fire @ plugins/platforms/telegram/telegram_typing_gate.py:typing_may_fire
 *
 * The two live collaborators are pure-math helpers ported inline here rather
 * than called:
 *   normalize_telegram_chat_id  (telegram_ids.py) -- int() when the stripped
 *     string parses as a base-10 int, else the stripped string.
 *   claim_ai_action             (telegram_draft_budget.py) -- the shared
 *     20-per-5s / 40-per-30s per-chat budget, whose slot is claimed only when
 *     no window is full.
 * The adapter's three dicts plus its loop clock are the state seam; everything
 * else in the decision is computed here.
 *
 * ASCII source: this module carries no literal text.
 */
#include "hermes_json.h"
#include "noc_w21_telegramty_a78c8c.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TTY_MIN_INTERVAL 2.0   /* TYPING_CHAT_MIN_INTERVAL_SECS */
#define TTY_AI_LONGEST   30.0  /* max(seconds for seconds, _ in _WINDOWS) */
#define TTY_AI_WINDOW_A  5.0   /* _WINDOWS[0] */
#define TTY_AI_LIMIT_A   20
#define TTY_AI_WINDOW_B  30.0  /* _WINDOWS[1] */
#define TTY_AI_LIMIT_B   40

static char *tty_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Python repr of a float for str(): shortest round-tripping decimal. */
static void tty_fmt_double(double d, char *buf, size_t cap) {
    int prec;
    for (prec = 15; prec <= 17; prec++) {
        double back;
        snprintf(buf, cap, "%.*g", prec, d);
        back = strtod(buf, NULL);
        if (back == d) return;
    }
}

/* Python str(value) for the scalar shapes a chat_id can carry. */
static char *tty_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return tty_dup("None");
    switch (v->type) {
    case JSON_BOOL:   return tty_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return tty_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if (isfinite(v->num_val) && floor(v->num_val) == v->num_val &&
            v->num_val >= -9.007199254740992e15 && v->num_val <= 9.007199254740992e15) {
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        } else {
            tty_fmt_double(v->num_val, buf, sizeof(buf));
        }
        return tty_dup(buf);
    default: {
        char *dumped = v ? json_dumps(v, 0) : NULL;
        char *out = tty_dup(dumped ? dumped : "");
        free(dumped);
        return out;
    }
    }
}

/* str(value).strip() */
static char *tty_str_strip(const json_t *v) {
    char *s = tty_str(v);
    char *start, *end;
    size_t n;
    if (!s) return NULL;
    start = s;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r' ||
           *start == '\f' || *start == '\v') start++;
    end = start + strlen(start);
    while (end > start) {
        char c = end[-1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') end--;
        else break;
    }
    n = (size_t)(end - start);
    memmove(s, start, n);
    s[n] = '\0';
    return s;
}

/* int(text) for base 10: optional sign, digits, single underscores between
 * digits (Python's int() literal grammar), whole string must be consumed. */
static int tty_parse_int(const char *s, long long *out) {
    const char *p = s;
    int neg = 0, digits = 0, after_digit = 0;
    long long acc = 0;
    if (!p) return 0;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (!*p) return 0;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            int d = *p - '0';
            if (acc > (9223372036854775807LL - d) / 10) return 0; /* beyond int64 */
            acc = acc * 10 + d;
            digits++;
            after_digit = 1;
        } else if (*p == '_') {
            if (!after_digit || !(p[1] >= '0' && p[1] <= '9')) return 0;
            after_digit = 0;
        } else {
            return 0;
        }
        p++;
    }
    if (!digits) return 0;
    *out = neg ? -acc : acc;
    return 1;
}

/* str(normalize_telegram_chat_id(chat_id)) */
static char *tty_norm_key(const json_t *chat_id) {
    char *raw = tty_str_strip(chat_id);
    char buf[64];
    long long v;
    if (!raw) return NULL;
    if (tty_parse_int(raw, &v)) {
        free(raw);
        snprintf(buf, sizeof(buf), "%lld", v);
        return tty_dup(buf);
    }
    return raw;
}

static json_t *tty_sub(const json_t *state, const char *key) {
    const json_t *v = (state && state->type == JSON_OBJECT) ? json_obj_get(state, key) : NULL;
    return (v && v->type == JSON_OBJECT) ? json_copy(v) : json_object();
}

static json_t *tty_state_out(json_t *cooldown, json_t *last, json_t *ai) {
    json_t *state = json_object();
    json_set(state, "cooldown_until", cooldown);
    json_set(state, "last_sent", last);
    json_set(state, "ai_calls", ai);
    return state;
}

static json_t *tty_result(int result, json_t *state) {
    json_t *out = json_object();
    json_set(out, "result", json_bool(result != 0));
    json_set(out, "state", state);
    return out;
}

/* --- _typing_in_cooldown ------------------------------------------------ */
static int tty_in_cooldown(json_t *cooldown, const json_t *chat_id, double now) {
    char *s = tty_str(chat_id);
    json_t *until;
    int active = 0;
    if (!s) return 0;
    until = json_obj_get(cooldown, s);
    if (until && until->type == JSON_NUMBER) {
        if (now < until->num_val) {
            active = 1;
        } else {
            json_obj_del(cooldown, s);   /* until_by_chat.pop(str(chat_id), None) */
        }
    }
    free(s);
    return active;
}

json_t *w21_telegramty_a78c8c_typing_in_cooldown(const json_t *state,
                                                 const json_t *chat_id, double now) {
    json_t *cooldown = tty_sub(state, "cooldown_until");
    json_t *last = tty_sub(state, "last_sent");
    json_t *ai = tty_sub(state, "ai_calls");
    int active = tty_in_cooldown(cooldown, chat_id, now);
    return tty_result(active, tty_state_out(cooldown, last, ai));
}

/* --- claim_ai_action ---------------------------------------------------- */
/* 0.0 => claimed; > 0 => seconds until a slot frees (nothing reserved). */
static double tty_claim_ai(json_t *ai, const json_t *chat_id, double now) {
    char *key = tty_norm_key(chat_id);
    json_t *arr;
    double times[512];
    size_t n = 0, i, w;
    double delay = 0.0;

    if (!key) return 0.0;
    arr = json_obj_get(ai, key);
    if (!arr || arr->type != JSON_ARRAY) {
        json_t *fresh = json_array();
        if (arr) json_obj_del(ai, key);
        json_set(ai, key, fresh);   /* setdefault(key, deque()) */
        arr = fresh;
    }
    for (i = 0; i < json_len(arr) && n < 512; i++) {
        json_t *it = json_get(arr, i);
        if (it && it->type == JSON_NUMBER) times[n++] = it->num_val;
    }
    /* while calls and now - calls[0] >= 30.0: popleft() */
    while (n > 0 && now - times[0] >= TTY_AI_LONGEST) {
        for (w = 0; w + 1 < n; w++) times[w] = times[w + 1];
        n--;
    }
    /* delay = max(calls[-limit] + seconds - now for each full window) */
    if (n >= (size_t)TTY_AI_LIMIT_A) {
        double d = times[n - (size_t)TTY_AI_LIMIT_A] + TTY_AI_WINDOW_A - now;
        if (d > delay) delay = d;
    }
    if (n >= (size_t)TTY_AI_LIMIT_B) {
        double d = times[n - (size_t)TTY_AI_LIMIT_B] + TTY_AI_WINDOW_B - now;
        if (d > delay) delay = d;
    }
    {
        json_t *rebuilt = json_array();
        for (i = 0; i < n; i++) json_append(rebuilt, json_number(times[i]));
        if (delay <= 0.0) json_append(rebuilt, json_number(now)); /* calls.append(now) */
        json_set(ai, key, rebuilt);
    }
    if (delay > 0.0) {
        /* Nothing reserved, but the front-prune already happened. */
        free(key);
        return delay;
    }
    /* Bounded dict: drop every chat whose newest call is older than the window
     * (an empty deque counts as stale too). */
    {
        size_t count = json_object_size(ai);
        char **doomed = (char **)calloc(count ? count : 1, sizeof(char *));
        size_t nd = 0;
        for (i = 0; i < count; i++) {
            const char *k = json_object_get_key_at(ai, i);
            json_t *q = json_object_get_at(ai, i);
            double newest;
            int stale = 0;
            if (!k) continue;
            if (!q || q->type != JSON_ARRAY || json_len(q) == 0) {
                stale = 1;
            } else {
                json_t *tail = json_get(q, json_len(q) - 1);
                newest = (tail && tail->type == JSON_NUMBER) ? tail->num_val : 0.0;
                if (now - newest >= TTY_AI_LONGEST) stale = 1;
            }
            if (stale) doomed[nd++] = (char *)k;
        }
        for (i = 0; i < nd; i++) json_obj_del(ai, doomed[i]);
        free(doomed);
    }
    free(key);
    return 0.0;
}

json_t *w21_telegramty_a78c8c_typing_may_fire(const json_t *state,
                                              const json_t *chat_id, double now) {
    json_t *cooldown = tty_sub(state, "cooldown_until");
    json_t *last = tty_sub(state, "last_sent");
    json_t *ai = tty_sub(state, "ai_calls");
    const json_t *flood = (state && state->type == JSON_OBJECT) ? json_obj_get(state, "flood_remaining") : NULL;
    double slot = (state && state->type == JSON_OBJECT) ? json_get_num(state, "slot_remaining", 0.0) : 0.0;
    char *key = NULL;
    double previous;

    if (tty_in_cooldown(cooldown, chat_id, now)) return tty_result(0, tty_state_out(cooldown, last, ai));
    /* `_send_flood_cooldown_remaining(chat_id) is not None` -- only a null or
     * missing seam value means "no flood window". */
    if (flood && flood->type != JSON_NULL) return tty_result(0, tty_state_out(cooldown, last, ai));
    if (slot > 0.0) return tty_result(0, tty_state_out(cooldown, last, ai));

    key = tty_norm_key(chat_id);
    if (!key) return tty_result(0, tty_state_out(cooldown, last, ai));
    {
        json_t *sent = json_obj_get(last, key);
        previous = (sent && sent->type == JSON_NUMBER) ? sent->num_val : -INFINITY;
    }
    if (now - previous < TTY_MIN_INTERVAL) {
        free(key);
        return tty_result(0, tty_state_out(cooldown, last, ai));
    }
    if (tty_claim_ai(ai, chat_id, now) > 0.0) {
        /* Drafts and typing share one per-chat budget; the draft frame shows
         * activity, so the typing tick is skipped. */
        free(key);
        return tty_result(0, tty_state_out(cooldown, last, ai));
    }
    /* for stale in [k for k, t in last.items() if now - t >= 2.0]: del last[stale] */
    {
        size_t count = json_object_size(last);
        char **doomed = (char **)calloc(count ? count : 1, sizeof(char *));
        size_t nd = 0, i;
        for (i = 0; i < count; i++) {
            const char *k = json_object_get_key_at(last, i);
            json_t *t = json_object_get_at(last, i);
            double when = (t && t->type == JSON_NUMBER) ? t->num_val : 0.0;
            if (!k) continue;
            if (now - when >= TTY_MIN_INTERVAL) doomed[nd++] = (char *)k;
        }
        for (i = 0; i < nd; i++) json_obj_del(last, doomed[i]);
        free(doomed);
    }
    json_set(last, key, json_number(now));
    free(key);
    return tty_result(1, tty_state_out(cooldown, last, ai));
}
