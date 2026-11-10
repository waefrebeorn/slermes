/*
 * port_fast_mode.c — pure service-tier helpers from agent/fast_mode.py.
 *
 * The lookup tables mirror the Python module exactly:
 *   NORMAL_TIER_WORDS = {"", "normal", "default", "standard", "off", "none"}
 *   SERVICE_TIER_WORDS = {"fast":"priority","priority":"priority","on":"priority",
 *                         "ultrafast":"ultrafast","ultra":"ultrafast",
 *                         "auto":"auto","cold":"cold"}
 */

#include "port_fast_mode.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *fm_strdup(const char *s, size_t n)
{
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* Treat NULL and "" the same way Python's `str(raw or "")` yields "" for
 * None/"" — i.e. the pre-strip value is "". */

static const char *fm_service_tier_value(const char *word)
{
    /* word is already stripped+lowered. */
    static const struct {
        const char *word;
        const char *tier;
    } TABLE[] = {
        {"fast", "priority"}, {"priority", "priority"}, {"on", "priority"},
        {"ultrafast", "ultrafast"}, {"ultra", "ultrafast"},
        {"auto", "auto"}, {"cold", "cold"},
    };
    size_t i;
    for (i = 0; i < sizeof(TABLE) / sizeof(TABLE[0]); i++)
        if (strcmp(word, TABLE[i].word) == 0)
            return TABLE[i].tier;
    return NULL;
}

static int fm_is_normal_word(const char *word)
{
    static const char *const NORMAL[] = {
        "", "default", "none", "normal", "off", "standard"
    };
    size_t i;
    /* Python checks membership of the empty string against NORMAL_TIER_WORDS
     * AFTER the strip, so the empty string in the table catches blank input. */
    for (i = 0; i < sizeof(NORMAL) / sizeof(NORMAL[0]); i++)
        if (strcmp(word, NORMAL[i]) == 0)
            return 1;
    return 0;
}

/* str(raw or "").strip().lower() into a malloc'd buffer. */
static char *fm_normalized_word(const char *raw)
{
    static const size_t CAP = 256;
    char buf[256];
    const char *p = raw ? raw : "";
    size_t len = strlen(p), s = 0, e = len;
    while (s < len && isspace((unsigned char)p[s]))
        s++;
    while (e > s && isspace((unsigned char)p[e - 1]))
        e--;
    if (e - s > CAP)
        e = s + CAP;
    for (size_t i = 0; i < e - s; i++)
        buf[i] = (char)tolower((unsigned char)p[s + i]);
    return fm_strdup(buf, e - s);
}

char *fm_parse_service_tier(const char *raw)
{
    char *word = fm_normalized_word(raw);
    const char *tier;
    char *out;
    if (!word) return NULL;
    if (fm_is_normal_word(word)) {
        free(word);
        return NULL; /* None */
    }
    tier = fm_service_tier_value(word);
    free(word);
    if (!tier) return NULL; /* unknown word -> None */
    out = fm_strdup(tier, strlen(tier));
    return out;
}

int fm_parse_exact_service_tier(const char *raw, char **out)
{
    char *word = fm_normalized_word(raw);
    const char *tier;
    char *v;
    *out = NULL;
    if (!word) return -1;
    if (fm_is_normal_word(word)) {
        free(word);
        *out = fm_strdup("", 0);
        return *out ? 0 : -1;
    }
    tier = fm_service_tier_value(word);
    if (!tier) {
        free(word);
        return -1; /* ValueError */
    }
    v = fm_strdup(tier, strlen(tier));
    free(word);
    if (!v) return -1;
    *out = v;
    return 0;
}

char *fm_service_tier_word(const char *tier)
{
    const char *p = tier ? tier : "";
    /* {"priority":"fast", None:"normal", "":"normal"}.get(tier, tier) */
    if (strcmp(p, "priority") == 0)
        return fm_strdup("fast", 4);
    if (p[0] == '\0')  /* NULL (None) or "" both -> "normal" */
        return fm_strdup("normal", 6);
    return fm_strdup(p, strlen(p));
}
