/*
 * telegram_media_flood.c - real C11 port of upstream Hermes
 *   plugins/platforms/telegram/telegram_media_flood.py pure decision helpers.
 * The rest of that module is asyncio/network plumbing (Bot API calls, deferred tasks);
 * what is portable is the flood-window arithmetic and the recovery branch lattice.
 * PoP: w19_telegramme_82fdeb_flood_wait_seconds @ plugins/platforms/telegram/telegram_media_flood.py:flood_wait_seconds
 * PoP: w19_telegramme_82fdeb_known_window_message @ plugins/platforms/telegram/telegram_media_flood.py:_KnownFloodWindow
 * PoP: w19_telegramme_82fdeb_recover_action @ plugins/platforms/telegram/telegram_media_flood.py:TelegramMediaFloodMixin._media_flood_recover
 */
#include "hermes_json.h"
#include "noc_w19_telegramme_82fdeb.h"
#include <math.h>
#include <stdio.h>

static double lower_ch(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }

static int ci_at(const char *s, const char *word) {
    for (size_t i = 0; word[i]; i++) {
        if (!s[i] || lower_ch(s[i]) != word[i]) return 0;
    }
    return 1;
}

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* float(s) for a JSON scalar: 1 and *out set, or 0 when float() would raise. */
static int py_float(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        if (!s) return 0;
        while (*s && is_space(*s)) s++;
        if (!*s) return 0;
        char *end = NULL;
        double d = strtod(s, &end);
        if (end == s) return 0;
        while (*end && is_space(*end)) end++;
        if (*end != '\0') return 0;
        *out = d;
        return 1;
    }
    return 0;
}

/* _FLOOD_TEXT_RE = re.compile(r"retry\s+(?:in|after)\s+(\d+(?:\.\d+)?)") searched over str(error). */
static int scan_retry_text(const char *text, double *out) {
    if (!text) return 0;
    for (const char *p = text; *p; p++) {
        if (!ci_at(p, "retry")) continue;
        const char *q = p + 5;
        if (!is_space(*q)) continue;
        while (is_space(*q)) q++;
        const char *kw = NULL;
        if (ci_at(q, "after")) kw = q + 5;
        else if (ci_at(q, "in")) kw = q + 2;
        if (!kw) continue;
        if (!is_space(*kw)) continue;
        while (is_space(*kw)) kw++;
        if (*kw < '0' || *kw > '9') continue;
        char *end = NULL;
        double d = strtod(kw, &end);
        if (end == kw) continue;
        *out = d;
        return 1;
    }
    return 0;
}

json_t *w19_telegramme_82fdeb_flood_wait_seconds(int has_retry_after,
                                                 json_t *retry_after, const char *text) {
    if (has_retry_after && retry_after && retry_after->type != JSON_NULL) {
        json_t *value = retry_after;
        if (retry_after->type == JSON_OBJECT) {
            json_t *ts = json_obj_get(retry_after, "total_seconds");  /* newer PTB: a timedelta */
            if (ts && ts->type == JSON_OBJECT) ts = NULL;
            if (ts) value = ts;
        }
        double d = 0.0;
        if (py_float(value, &d)) return json_number(d);
        return json_null();   /* float() raised: no wait */
    }
    double d = 0.0;
    if (scan_retry_text(text, &d)) return json_number(d);
    return json_null();
}

static char g_msg[128];

const char *w19_telegramme_82fdeb_known_window_message(double wait) {
    snprintf(g_msg, sizeof(g_msg), "Telegram flood window open; retry in %.1f seconds", wait);
    return g_msg;
}

json_t *w19_telegramme_82fdeb_recover_action(int wait_present, double wait, int stage,
                                             int known_window, double inline_cap) {
    const char *action;
    int cooldown = 0;
    double sleep = 0.0;
    if (!wait_present) {
        action = "none";
    } else if (stage >= 2) {
        /* Refused again (or out of window waits), already armed: wait it out, caller reports. */
        action = "sleep_none";
        cooldown = known_window ? 0 : 1;
        sleep = wait;
    } else if (known_window) {
        /* Refused locally: the window is already armed, so only defer. */
        action = "defer";
    } else if (stage == 0 && wait <= inline_cap) {
        action = "inline_retry";
        cooldown = wait > 0 ? 1 : 0;
        sleep = wait;
    } else {
        action = "defer";
        cooldown = 1;
    }
    json_t *out = json_object();
    json_set(out, "action", json_string(action));
    json_set(out, "cooldown", json_bool(cooldown ? true : false));
    json_set(out, "sleep", json_number(sleep));
    return out;
}
