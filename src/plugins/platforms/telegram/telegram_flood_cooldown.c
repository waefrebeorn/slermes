/*
 * telegram_flood_cooldown.c -- real C11 port of the per-chat Telegram send
 * flood window of upstream Hermes
 * plugins/platforms/telegram/telegram_flood_cooldown.py: the persisted-window
 * seeding a boot performs, the arming a 429 retry_after does, and the remaining
 * seconds a send checks before it fires. Clock and loop are injected, so the
 * file body and the two clocks are the only inputs.
 *
 * PoP: w20_telegramfl_e6bdc8_bot_id @ plugins/platforms/telegram/telegram_flood_cooldown.py:_bot_id
 * PoP: w20_telegramfl_e6bdc8_store_name @ plugins/platforms/telegram/telegram_flood_cooldown.py:_store_path
 * PoP: w20_telegramfl_e6bdc8_seed_windows @ plugins/platforms/telegram/telegram_flood_cooldown.py:_windows
 * PoP: w20_telegramfl_e6bdc8_record @ plugins/platforms/telegram/telegram_flood_cooldown.py:_record_send_flood_cooldown
 * PoP: w20_telegramfl_e6bdc8_remaining @ plugins/platforms/telegram/telegram_flood_cooldown.py:_send_flood_cooldown_remaining
 *
 * The write side (_schedule_persist's coalesced flush, land_persisted_state)
 * stays upstream: it needs the adapter's live loop and the atomic writer.
 */
#include "hermes_json.h"
#include "noc_w20_telegramfl_e6bdc8.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Longest window armed by one refusal (module constant). */
#define TF6_MAX_WINDOW_SECONDS 300.0

static char *tf6_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Python str()/repr() of a float: shortest round-tripping digits, positional
 * inside (-4, 16] decimal points, scientific outside it, always with a float
 * marker ("30.0", "1e+16", "1e-05"). */
static void tf6_float_text(char *dst, size_t cap, double d) {
    char sci[64], digits[32];
    int precision, decpt, ndigits = 0, exp10 = 0, i, negative = 0;
    const char *p;
    char *q = dst;

    if (isnan(d)) { snprintf(dst, cap, "nan"); return; }
    if (isinf(d)) { snprintf(dst, cap, d < 0 ? "-inf" : "inf"); return; }
    if (d == 0.0) { snprintf(dst, cap, signbit(d) ? "-0.0" : "0.0"); return; }
    for (precision = 0; precision <= 16; precision++) {
        snprintf(sci, sizeof(sci), "%.*e", precision, d);
        if (strtod(sci, NULL) == d) break;
    }
    p = sci;
    if (*p == '-') { negative = 1; p++; }
    for (; *p && *p != 'e' && *p != 'E'; p++) {
        if (*p == '.') continue;
        if (*p < '0' || *p > '9') break;
        if (ndigits < (int)sizeof(digits) - 1) digits[ndigits++] = *p;
    }
    if (*p == 'e' || *p == 'E') exp10 = (int)strtol(p + 1, NULL, 10);
    digits[ndigits] = '\0';
    while (ndigits > 1 && digits[ndigits - 1] == '0') digits[--ndigits] = '\0';
    decpt = exp10 + 1;

    if (negative) *q++ = '-';
    if (decpt > -4 && decpt <= 16) {
        if (decpt <= 0) {
            *q++ = '0';
            *q++ = '.';
            for (i = 0; i < -decpt; i++) *q++ = '0';
            for (i = 0; i < ndigits; i++) *q++ = digits[i];
        } else if (decpt >= ndigits) {
            for (i = 0; i < ndigits; i++) *q++ = digits[i];
            for (i = ndigits; i < decpt; i++) *q++ = '0';
            *q++ = '.';
            *q++ = '0';
        } else {
            for (i = 0; i < decpt; i++) *q++ = digits[i];
            *q++ = '.';
            for (i = decpt; i < ndigits; i++) *q++ = digits[i];
        }
    } else {
        int e = decpt - 1;
        char exp_buf[8];
        *q++ = digits[0];
        if (ndigits > 1) {
            *q++ = '.';
            for (i = 1; i < ndigits; i++) *q++ = digits[i];
        }
        *q++ = 'e';
        *q++ = e < 0 ? '-' : '+';
        if (e < 0) e = -e;
        snprintf(exp_buf, sizeof(exp_buf), "%02d", e);
        memcpy(q, exp_buf, strlen(exp_buf));
        q += strlen(exp_buf);
    }
    *q = '\0';
}

static int tf6_is_space(unsigned char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

/* int(text) for a whole signed decimal literal (single underscores allowed
 * between digits), canonicalised the way str(int(text)) prints it. */
static int tf6_int_literal(const char *s, char *out, size_t cap) {
    const char *p = s;
    int negative = 0;
    char raw[512];
    size_t n = 0, first = 0;

    if (!s || !*s) return 0;
    if (*p == '+' || *p == '-') { negative = (*p == '-'); p++; }
    if (!*p) return 0;
    for (; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            if (n < sizeof(raw) - 1) raw[n++] = *p;
            continue;
        }
        if (*p == '_') {
            int prev_digit = (p > s) && (p[-1] >= '0' && p[-1] <= '9');
            int next_digit = (p[1] >= '0' && p[1] <= '9');
            if (!prev_digit || !next_digit) return 0;
            continue;
        }
        return 0;
    }
    raw[n] = '\0';
    while (first + 1 < n && raw[first] == '0') first++;
    if (first == n - 1 && raw[first] == '0') negative = 0;
    if (negative) {
        if (cap < 3) return 0;
        out[0] = '-';
        snprintf(out + 1, cap - 1, "%s", raw + first);
    } else {
        snprintf(out, cap, "%s", raw + first);
    }
    return 1;
}

/* str(chat_id) for the scalar shapes a Bot API chat id can carry. */
static char *tf6_chat_text(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tf6_dup("None");
    switch (v->type) {
    case JSON_STRING: return tf6_dup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return tf6_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER: {
        char buf[64];
        char text[80];
        tf6_float_text(buf, sizeof(buf), v->num_val);
        /* an integral chat id is an int id: str(42), not str(42.0) */
        if (floor(v->num_val) == v->num_val && fabs(v->num_val) <= 9007199254740992.0) {
            snprintf(text, sizeof(text), "%lld", (long long)v->num_val);
            return tf6_dup(text);
        }
        return tf6_dup(buf);
    }
    default: return tf6_dup("None");
    }
}

/* str(chat_id).strip() then int() when possible -- never raises. */
static char *tf6_normalize(const char *chat) {
    char stripped[512];
    char numeric[512];
    size_t len, start = 0, end;

    if (!chat) return tf6_dup("None");
    len = strlen(chat);
    while (start < len && tf6_is_space((unsigned char)chat[start])) start++;
    end = len;
    while (end > start && tf6_is_space((unsigned char)chat[end - 1])) end--;
    if (end - start >= sizeof(stripped)) end = start + sizeof(stripped) - 1;
    memcpy(stripped, chat + start, end - start);
    stripped[end - start] = '\0';
    if (tf6_int_literal(stripped, numeric, sizeof(numeric))) return tf6_dup(numeric);
    return tf6_dup(stripped);
}

struct json_t *w20_telegramfl_e6bdc8_bot_id(const char *token) {
    char head[256];
    const char *colon;
    size_t n, i;

    if (!token) return json_null();
    colon = strchr(token, ':');
    n = colon ? (size_t)(colon - token) : strlen(token);
    if (n >= sizeof(head)) n = sizeof(head) - 1;
    memcpy(head, token, n);
    head[n] = '\0';
    if (!n) return json_null();
    for (i = 0; i < n; i++) {
        if (head[i] < '0' || head[i] > '9') return json_null();
    }
    return json_string(head);
}

struct json_t *w20_telegramfl_e6bdc8_store_name(const char *token, int has_receipt_dir) {
    json_t *bot = w20_telegramfl_e6bdc8_bot_id(token);
    char name[384];
    json_t *out;

    if (!has_receipt_dir || !bot || bot->type != JSON_STRING) {
        json_free(bot);
        return json_null();
    }
    snprintf(name, sizeof(name), "telegram_flood_cooldown_%s.json", bot->str_val);
    out = json_string(name);
    json_free(bot);
    return out;
}

static int tf6_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static json_t *tf6_obj_copy(const json_t *src) {
    json_t *out = json_object();
    size_t i;
    if (!src || src->type != JSON_OBJECT) return out;
    for (i = 0; i < src->c.count; i++) {
        if (!src->c.keys[i]) continue;
        json_set(out, src->c.keys[i], json_copy(src->c.items[i]));
    }
    return out;
}

/* json.loads(read_text(utf-8-sig)).get("until") or {} -- NULL on the empty path. */
static json_t *tf6_parse_saved(const char *text) {
    const char *body = text;
    json_t *doc, *until, *kept;

    if (!body) return NULL;
    if ((unsigned char)body[0] == 0xef && (unsigned char)body[1] == 0xbb &&
        (unsigned char)body[2] == 0xbf) {
        body += 3;
    }
    doc = json_parse(body, NULL);
    if (!doc) return NULL;                                          /* ValueError */
    if (doc->type != JSON_OBJECT) { json_free(doc); return NULL; }   /* AttributeError */
    until = json_obj_get(doc, "until");
    if (!tf6_truthy(until)) { json_free(doc); return NULL; }
    kept = json_copy(until);
    json_free(doc);
    return kept;
}

struct json_t *w20_telegramfl_e6bdc8_seed_windows(const char *token, int has_receipt_dir,
                                                  const char *saved_text, const struct json_t *preset,
                                                  int loaded, double wall_now, double loop_now) {
    json_t *until = tf6_obj_copy(preset);
    json_t *saved;
    size_t i;

    if (loaded) return until;
    {
        json_t *bot = w20_telegramfl_e6bdc8_bot_id(token);
        int has_store = (has_receipt_dir && bot && bot->type == JSON_STRING) ? 1 : 0;
        json_free(bot);
        if (!has_store) return until;                    /* _store_path() is None */
    }
    saved = tf6_parse_saved(saved_text);
    if (!saved) return until;
    if (saved->type != JSON_OBJECT) { json_free(saved); return until; }
    for (i = 0; i < saved->c.count; i++) {
        const char *chat = saved->c.keys[i];
        json_t *value = saved->c.items[i];
        double deadline, resumed, existing, left;
        if (!chat || !value) continue;
        if (value->type == JSON_BOOL) deadline = value->bool_val ? 1.0 : 0.0;
        else if (value->type == JSON_NUMBER) deadline = value->num_val;
        else continue;                                   /* not a number: dropped */
        if (deadline <= wall_now) continue;              /* already expired */
        left = deadline - wall_now;
        resumed = loop_now + (left < TF6_MAX_WINDOW_SECONDS ? left : TF6_MAX_WINDOW_SECONDS);
        existing = json_get_num(until, chat, 0.0);
        json_set(until, chat, json_number(existing > resumed ? existing : resumed));
    }
    json_free(saved);
    return until;
}

struct json_t *w20_telegramfl_e6bdc8_record(const struct json_t *chat, double wait,
                                            double loop_now, const struct json_t *preset) {
    json_t *until = tf6_obj_copy(preset);
    char *chat_text = tf6_chat_text(chat);
    char *key = tf6_normalize(chat_text);
    json_t *out = json_object();
    json_t *result = json_object();
    char error[128];
    char wait_text[64];
    double capped, deadline;

    capped = wait < TF6_MAX_WINDOW_SECONDS ? wait : TF6_MAX_WINDOW_SECONDS;
    if (capped < 1.0) capped = 1.0;
    deadline = loop_now + capped;
    json_set(until, key ? key : "", json_number(deadline));
    tf6_float_text(wait_text, sizeof(wait_text), wait);
    snprintf(error, sizeof(error), "flood_control:%s", wait_text);

    json_set(out, "key", json_string(key ? key : ""));
    json_set(out, "deadline", json_number(deadline));
    json_set(out, "until", until);
    json_set(result, "success", json_bool(0));
    json_set(result, "error", json_string(error));
    json_set(result, "retry_after", json_number(wait));
    json_set(out, "result", result);
    free(key);
    free(chat_text);
    return out;
}

struct json_t *w20_telegramfl_e6bdc8_remaining(const json_t *until, const struct json_t *chat,
                                               double loop_now) {
    json_t *map = tf6_obj_copy(until);
    char *chat_text = tf6_chat_text(chat);
    char *key = tf6_normalize(chat_text);
    json_t *out = json_object();
    json_t *deadline = json_obj_get(map, key ? key : "");
    double remaining;

    if (!deadline) {
        json_set(out, "remaining", json_null());
        json_set(out, "until", map);
        free(key);
        free(chat_text);
        return out;
    }
    remaining = deadline->num_val - loop_now;
    if (remaining > 0.0) {
        json_set(out, "remaining", json_number(remaining));
    } else {
        json_obj_del(map, key ? key : "");
        json_set(out, "remaining", json_null());
    }
    json_set(out, "until", map);
    free(key);
    free(chat_text);
    return out;
}
