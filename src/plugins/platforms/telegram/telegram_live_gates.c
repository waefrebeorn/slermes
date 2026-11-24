/*
 * telegram_live_gates.c -- real C11 port of the pure decisions of upstream
 * Hermes plugins/platforms/telegram/telegram_live_gates.py.
 *
 * PoP: w22_telegramli_e4312e_keys @ plugins/platforms/telegram/telegram_live_gates.py:_LIVE_GATE_EXTRA_KEYS
 * PoP: w22_telegramli_e4312e_allowlist_decision @ plugins/platforms/telegram/telegram_live_gates.py:env_allowlist_decision
 * PoP: w22_telegramli_e4312e_refresh_needed @ plugins/platforms/telegram/telegram_live_gates.py:refresh_telegram_gates
 * PoP: w22_telegramli_e4312e_merge_extra @ plugins/platforms/telegram/telegram_live_gates.py:refresh_telegram_gates
 *
 * Host seams, injected by the caller: the owning profile's home directory, the
 * two config.yaml change signatures, the freshly loaded platform extras and the
 * TELEGRAM_ALLOWED_USERS value. `refresh_live_gate_extra`'s lock, scope bind and
 * `load_gateway_config()` call, and the adapter's live gate set re-derivation,
 * stay upstream because they need a connected gateway process.
 */
#include "hermes_json.h"
#include "noc_w22_telegramli_e4312e.h"

#include <stdlib.h>
#include <string.h>

static const char *const TG_LIVE_KEYS[] = {
    "allow_from",
    "group_allow_from",
    "allowed_chats",
    "group_allowed_chats",
    "allowed_topics",
    "allow_bots",
};

#define TG_LIVE_KEY_COUNT ((int)(sizeof(TG_LIVE_KEYS) / sizeof(TG_LIVE_KEYS[0])))

/* One UTF-8 whitespace character at *p, as Python's str.strip() spells it. */
static int tg_space_width(const char *p) {
    unsigned char c = (unsigned char)p[0];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f')
        return 1;
    if (c >= 0x1c && c <= 0x1f) return 1;                    /* Python str.isspace() */
    if (c == 0xc2 && (unsigned char)p[1] == 0xa0) return 2;  /* NBSP */
    if (c == 0xe2 && (unsigned char)p[1] == 0x80 &&
        ((unsigned char)p[2] == 0x80 || (unsigned char)p[2] == 0x81 ||
         (unsigned char)p[2] == 0x82 || (unsigned char)p[2] == 0x83 ||
         (unsigned char)p[2] == 0x87 || (unsigned char)p[2] == 0x8b)) return 3;
    if (c == 0xe2 && (unsigned char)p[1] == 0x81 && (unsigned char)p[2] == 0x9f) return 3;
    if (c == 0xe3 && (unsigned char)p[1] == 0x80 && (unsigned char)p[2] == 0x80) return 3;
    return 0;
}

/* str.strip() copy: the caller frees. Never NULL for a NULL input. */
static char *tg_strip_dup(const char *s) {
    const char *start;
    const char *end;
    size_t n;
    char *out;

    if (!s) s = "";
    start = s;
    while (*start) {
        int w = tg_space_width(start);
        if (!w) break;
        start += w;
    }
    end = start + strlen(start);
    while (end > start) {
        /* Walk back over one trailing character at a time. */
        const char *cut = end - 1;
        while (cut > start && ((unsigned char)*cut & 0xc0) == 0x80) cut--;
        if (!tg_space_width(cut)) break;
        end = cut;
    }
    n = (size_t)(end - start);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, start, n);
    out[n] = '\0';
    return out;
}

json_t *w22_telegramli_e4312e_keys(void) {
    json_t *out = json_array();
    int i;
    if (!out) return NULL;
    for (i = 0; i < TG_LIVE_KEY_COUNT; i++)
        json_append(out, json_string(TG_LIVE_KEYS[i]));
    return out;
}

json_t *w22_telegramli_e4312e_allowlist_decision(const char *user_id, const char *env_csv) {
    char *csv = tg_strip_dup(env_csv);
    char *walk;
    int star = 0;
    int match = 0;

    if (!csv) return json_null();
    if (!csv[0]) {  /* `if not allowed_csv: return None` */
        free(csv);
        return json_null();
    }
    walk = csv;
    while (walk) {
        char *comma = strchr(walk, ',');
        char *piece;
        if (comma) *comma = '\0';
        piece = tg_strip_dup(walk);
        if (piece && piece[0]) {
            if (!strcmp(piece, "*")) star = 1;
            else if (user_id && !strcmp(piece, user_id)) match = 1;
        }
        free(piece);
        walk = comma ? comma + 1 : NULL;
    }
    free(csv);
    return json_bool(star || match ? 1 : 0);
}

/* Tuple-element equality where an absent version part is None on both sides. */
static int tg_same_part(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    return !strcmp(a, b);
}

int w22_telegramli_e4312e_refresh_needed(const char *baseline_home,
                                         const char *baseline_tag,
                                         const char *current_tag) {
    /* `if not baseline or not baseline[0]: return False` */
    if (!baseline_home || !baseline_home[0]) return 0;
    /* `if version == adapter._live_gate_version: return False` -- the baseline
     * tuple carries the home as its first element, so compare both parts. */
    if (tg_same_part(baseline_tag, current_tag)) return 0;
    return 1;
}

json_t *w22_telegramli_e4312e_merge_extra(const json_t *extra,
                                          const json_t *platforms,
                                          const char *platform) {
    json_t *out;
    json_t *record;
    const json_t *loaded;
    int i;

    if (!extra || extra->type != JSON_OBJECT) out = json_object();
    else out = json_copy(extra);
    if (!out) return NULL;

    record = platform ? json_obj_get(platforms, platform) : NULL;
    /* `loaded.platforms.get(adapter.platform) or PlatformConfig()`: a falsy
     * record (absent, null, empty object) reads as a config with no extras. */
    loaded = NULL;
    if (record && record->type == JSON_OBJECT && record->c.count > 0) {
        json_t *inner = json_obj_get(record, "extra");
        if (inner && inner->type == JSON_OBJECT) loaded = inner;
    }
    for (i = 0; i < TG_LIVE_KEY_COUNT; i++) {
        const char *key = TG_LIVE_KEYS[i];
        json_t *value = loaded ? json_obj_get(loaded, key) : NULL;
        if (value) json_set(out, key, json_copy(value));
        else json_obj_del(out, key);
    }
    return out;
}
