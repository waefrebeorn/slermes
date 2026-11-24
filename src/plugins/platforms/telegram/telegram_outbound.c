/*
 * telegram_outbound.c - C11 port of upstream
 * plugins/platforms/telegram/telegram_outbound.py (gate w24_telegramou_2e8580).
 *
 * PoP: w24_telegramou_2e8580_is_visually_empty @ plugins/platforms/telegram/telegram_outbound.py:is_visually_empty
 * PoP: w24_telegramou_2e8580_notification_kwargs @ plugins/platforms/telegram/telegram_outbound.py:notification_kwargs
 * PoP: w24_telegramou_2e8580_metadata @ plugins/platforms/telegram/telegram_outbound.py:_metadata
 * PoP: w24_telegramou_2e8580_resolve_mode @ plugins/platforms/telegram/telegram_outbound.py:resolve_notifications_mode
 * PoP: w24_telegramou_2e8580_constants @ plugins/platforms/telegram/telegram_outbound.py:_NOTIFICATION_MODES,_INVISIBLE
 *
 * What is ported is the send policy both Telegram lanes share: the
 * visually-empty guard (a message made only of zero-width joiners, BOMs, soft
 * hyphens or bidi marks lands as an empty bubble and must be refused), the
 * notifications mode resolution, the silent-send kwargs and the per-send
 * metadata dict.
 *
 * Seams, deliberately not ported: the bot object, the TelegramAdapter, the
 * httpx client and the Python process environment. resolve_notifications_mode
 * reads os.getenv and hermes_cli.config; here those two reads are the arguments
 * ``env`` and ``cfg``, and the unknown-value logger.warning becomes the same
 * "important" answer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_telegramou_2e8580.h"

/* _NOTIFICATION_MODES */
static const char *const TG_NOTIFICATION_MODES[2] = { "all", "important" };

/* _INVISIBLE: the code points str.maketrans("", "", ...) deletes. */
static const unsigned int TG_INVISIBLE[W24_TELEGRAMOU_2E8580_INVISIBLE_COUNT] = {
    0x200b, 0x200c, 0x200d, 0x2060, 0x2061, 0x2062, 0x2063, 0x2064,
    0xfeff, 0x00ad, 0x180e, 0x200e, 0x200f, 0x202a, 0x202b, 0x202c,
    0x202d, 0x202e, 0x2066, 0x2067, 0x2068, 0x2069,
};

static int tg_truthy(const json_t *v) {
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

/* str.isspace() over the code points Python strips: ASCII whitespace, the
 * file/group/record separators, NEL, NBSP, OGHAM SPACE MARK, the EN QUAD..HAIR
 * SPACE run, LINE/PARAGRAPH SEPARATOR, NARROW NBSP, MEDIUM MATHEMATICAL SPACE
 * and IDEOGRAPHIC SPACE. */
static int tg_space(unsigned int cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0d)) return 1;
    if (cp >= 0x1c && cp <= 0x1f) return 1;
    if (cp == 0x85 || cp == 0xa0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200a) return 1;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000) return 1;
    return 0;
}

static int tg_invisible(unsigned int cp) {
    size_t i;
    for (i = 0; i < W24_TELEGRAMOU_2E8580_INVISIBLE_COUNT; i++)
        if (TG_INVISIBLE[i] == cp) return 1;
    return 0;
}

/* One UTF-8 code point; a malformed byte decodes as U+FFFD one byte at a time
 * (never a visible glyph, which is all the guard asks). */
static unsigned int tg_next_cp(const unsigned char *s, size_t len, size_t *i) {
    unsigned int cp;
    size_t need, k;
    unsigned char c = s[*i];

    if (c < 0x80) { (*i)++; return c; }
    if ((c & 0xe0) == 0xc0) { cp = c & 0x1fu; need = 1; }
    else if ((c & 0xf0) == 0xe0) { cp = c & 0x0fu; need = 2; }
    else if ((c & 0xf8) == 0xf0) { cp = c & 0x07u; need = 3; }
    else { (*i)++; return 0xfffd; }
    if (*i + need >= len) { (*i)++; return 0xfffd; }
    for (k = 1; k <= need; k++) {
        if ((s[*i + k] & 0xc0) != 0x80) { (*i)++; return 0xfffd; }
        cp = (cp << 6) | (unsigned int)(s[*i + k] & 0x3f);
    }
    *i += need + 1;
    return cp;
}

/* Python str.strip() with no argument over the translated text: true when
 * nothing visible is left. */
static int tg_strips_empty(const char *text) {
    const unsigned char *s = (const unsigned char *)text;
    size_t i = 0, len = strlen(text);

    while (i < len) {
        unsigned int cp = tg_next_cp(s, len, &i);
        if (tg_invisible(cp)) continue;
        if (tg_space(cp)) continue;
        return 0;
    }
    return 1;
}

json_t *w24_telegramou_2e8580_is_visually_empty(const json_t *text) {
    /* ``not text``: None, "", 0, False, [], {} all answer True. */
    if (!tg_truthy(text)) return json_bool(1);
    if (text->type != JSON_STRING) return json_null();  /* AttributeError seam */
    return json_bool(tg_strips_empty(text->str_val ? text->str_val : ""));
}

json_t *w24_telegramou_2e8580_notification_kwargs(const json_t *mode, const json_t *notify) {
    json_t *out;
    if (!mode || mode->type != JSON_STRING || strcmp(mode->str_val ? mode->str_val : "", "important") != 0)
        return json_object();
    if (tg_truthy(notify)) return json_object();
    out = json_object();
    json_set(out, "disable_notification", json_bool(1));
    return out;
}

/* Python str() for the scalar shapes a thread id holds. A shape this does not
 * cover (a list, a dict) answers NULL, the seam's error value. */
static char *tg_str(const json_t *v) {
    char *buf;
    double d;

    if (!v) return NULL;
    if (v->type == JSON_STRING) {
        size_t n = strlen(v->str_val ? v->str_val : "");
        buf = (char *)malloc(n + 1);
        if (buf) memcpy(buf, v->str_val ? v->str_val : "", n + 1);
        return buf;
    }
    if (v->type == JSON_BOOL) {
        const char *t = v->bool_val ? "True" : "False";
        buf = (char *)malloc(strlen(t) + 1);
        if (buf) strcpy(buf, t);
        return buf;
    }
    if (v->type == JSON_NUMBER) {
        buf = (char *)malloc(40);
        if (!buf) return NULL;
        d = v->num_val;
        if (d == (double)(long long)d && d > -1e17 && d < 1e17) snprintf(buf, 40, "%.0f", d);
        else snprintf(buf, 40, "%.15g", d);
        return buf;
    }
    return NULL;
}

json_t *w24_telegramou_2e8580_metadata(const json_t *thread_id, const json_t *notify) {
    json_t *out = json_object();

    if (thread_id && thread_id->type != JSON_NULL) {
        char *s = tg_str(thread_id);
        if (!s) { json_free(out); return json_null(); }  /* str() seam */
        json_set(out, "thread_id", json_string(s));
        free(s);
    }
    if (tg_truthy(notify)) json_set(out, "notify", json_bool(1));
    return out;
}

/* str.strip() over ASCII whitespace, in place, plus .lower(). */
static void tg_normalize(char *s) {
    size_t n, i;
    char *p = s;

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    n = strlen(s);
    while (n > 0) {
        char c = s[n - 1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') s[--n] = '\0';
        else break;
    }
    for (i = 0; i < n; i++)
        if (s[i] >= 'A' && s[i] <= 'Z') s[i] = (char)(s[i] - 'A' + 'a');
}

/* str(raw).strip().lower() where raw is the config value. */
static char *tg_mode_from_cfg(const json_t *cfg) {
    char *s;
    if (!cfg || cfg->type == JSON_NULL) {
        s = (char *)malloc(1);
        if (s) s[0] = '\0';
        return s;
    }
    s = tg_str(cfg);
    if (!s) return NULL;
    tg_normalize(s);
    return s;
}

json_t *w24_telegramou_2e8580_resolve_mode(const json_t *env, const json_t *cfg) {
    char *mode = NULL;
    json_t *out;

    if (env && env->type == JSON_STRING) {
        mode = tg_str(env);
        if (!mode) return json_null();
        tg_normalize(mode);
    }
    if (!mode || mode[0] == '\0') {
        char *from_cfg;
        free(mode);
        from_cfg = tg_mode_from_cfg(cfg);
        if (!from_cfg) return json_null();
        mode = from_cfg;
    }
    if (mode[0] == '\0') { free(mode); return json_string("important"); }
    if (strcmp(mode, TG_NOTIFICATION_MODES[0]) != 0 && strcmp(mode, TG_NOTIFICATION_MODES[1]) != 0) {
        free(mode);            /* logger.warning("Unknown ... defaulting to 'important'") */
        return json_string("important");
    }
    out = json_string(mode);
    free(mode);
    return out;
}

json_t *w24_telegramou_2e8580_constants(void) {
    json_t *o = json_object();
    json_t *modes = json_array();
    json_t *invis = json_array();
    size_t i;

    for (i = 0; i < 2; i++) json_append(modes, json_string(TG_NOTIFICATION_MODES[i]));
    for (i = 0; i < W24_TELEGRAMOU_2E8580_INVISIBLE_COUNT; i++)
        json_append(invis, json_number((double)TG_INVISIBLE[i]));
    json_set(o, "notification_modes", modes);
    json_set(o, "invisible_codepoints", invis);
    return o;
}
