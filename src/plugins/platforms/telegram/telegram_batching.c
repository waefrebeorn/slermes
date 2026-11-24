/*
 * telegram_batching.c - C11 port of upstream
 * plugins/platforms/telegram/telegram_batching.py (gate w26_telegramba_f62536).
 *
 * PoP: w26_telegramba_f62536_pad_inline_flags @ plugins/platforms/telegram/telegram_batching.py:pad_inline_flags
 * PoP: w26_telegramba_f62536_command_first @ plugins/platforms/telegram/telegram_batching.py:command_first
 * PoP: w26_telegramba_f62536_merged_text @ plugins/platforms/telegram/telegram_batching.py:merged_text
 * PoP: w26_telegramba_f62536_constants @ plugins/platforms/telegram/telegram_batching.py:CAPTION_TEXT_ATTR/ITEM_NOTES_ATTR
 * PoP: w26_telegramba_f62536_caption_is_command @ gateway/platforms/helpers.py:caption_is_command
 *
 * What an album merge decides is a pure function of the values on the events:
 * how many inline flags to pad, which caption leads, and how one item's text
 * and notes join the accumulated one.  The events, the lane dict, the routing
 * adapter and the caption join are seams; only the values are ported.
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_telegramba_f62536.h"

static char *tb_strdup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (n) memcpy(out, s, n);
    out[n] = 0;
    return out;
}

static int tb_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int tb_starts_ci(const char *p, const char *pre) {
    while (*pre) {
        char a = *p, b = *pre;
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a != b) return 0;
        p++;
        pre++;
    }
    return 1;
}

/* caption_is_command: the caption's one-line text, with Desktop attachment refs
 * stripped, starts a slash command whose name has no "/" in it. */
int w26_telegramba_f62536_caption_is_command(const char *caption) {
    const char *p = caption ? caption : "", *e, *at;
    size_t n, i;

    while (*p && tb_ws(*p)) p++;
    for (;;) {                                   /* ^(?:@(?:image|file|url):[^\n]+\n?)+ */
        const char *q = NULL;
        if (tb_starts_ci(p, "@image:")) q = p + 7;
        else if (tb_starts_ci(p, "@file:")) q = p + 6;
        else if (tb_starts_ci(p, "@url:")) q = p + 5;
        if (!q) break;
        if (!*q || *q == '\n') break;
        while (*q && *q != '\n') q++;
        if (*q == '\n') q++;
        p = q;
    }
    while (*p && tb_ws(*p)) p++;
    if (*p != '/') return 0;
    e = p + 1;
    while (*e && !tb_ws(*e)) e++;
    at = (const char *)memchr(p + 1, '@', (size_t)(e - (p + 1)));
    n = at ? (size_t)(at - (p + 1)) : (size_t)(e - (p + 1));
    for (i = 0; i < n; i++)
        if (p[1 + i] == '/') return 0;
    return 1;
}

json_t *w26_telegramba_f62536_pad_inline_flags(const json_t *inlined, const json_t *urls) {
    size_t nflags = 0, nuris = 0, i;
    json_t *out = json_array();

    if (inlined && inlined->type == JSON_ARRAY) nflags = json_len(inlined);
    if (urls && urls->type == JSON_ARRAY) nuris = json_len(urls);
    for (i = 0; i < nflags; i++) json_append(out, json_copy(json_get(inlined, i)));
    while (nflags < nuris) {                     /* [None] * (len(urls) - len(flags)) */
        json_append(out, json_null());
        nflags++;
    }
    return out;
}

/* merge_caption, the fixed join: (a + "\n\n" + b) if a else b. */
static char *tb_merge(const char *a, const char *b) {
    size_t an = a ? strlen(a) : 0, bn = b ? strlen(b) : 0;
    char *out;

    if (!an) return b ? tb_strdup(b) : tb_strdup("");
    out = (char *)malloc(an + bn + 3);
    if (!out) return NULL;
    memcpy(out, a, an);
    memcpy(out + an, "\n\n", 2);
    if (bn) memcpy(out + an + 2, b, bn);
    out[an + 2 + bn] = '\0';
    return out;
}

json_t *w26_telegramba_f62536_command_first(const json_t *captions, const json_t *incoming) {
    const char *cap = NULL, *inc = NULL;
    char *joined;
    json_t *out;

    if (captions && captions->type == JSON_STRING) cap = captions->str_val;
    if (incoming && incoming->type == JSON_STRING) inc = incoming->str_val;
    if (!cap || !*cap || !inc) return json_null();
    if (!w26_telegramba_f62536_caption_is_command(inc)) return json_null();
    if (w26_telegramba_f62536_caption_is_command(cap)) return json_null();
    joined = tb_merge(inc, cap);
    if (!joined) return json_null();
    out = json_string(joined);
    free(joined);
    return out;
}

json_t *w26_telegramba_f62536_merged_text(const json_t *text, const json_t *incoming,
                                          const json_t *notes) {
    const char *src = text && text->type == JSON_STRING ? text->str_val : NULL;
    const char *inc = incoming && incoming->type == JSON_STRING ? incoming->str_val : NULL;
    size_t nnotes = notes && notes->type == JSON_ARRAY ? json_len(notes) : 0;
    char *caption = tb_strdup(inc ? inc : "");
    char **kept = (char **)calloc(nnotes ? nnotes : 1, sizeof(char *));
    size_t nkept = 0, i;
    json_t *out;

    if (!caption || !kept) { free(caption); free(kept); return json_null(); }

    for (i = nnotes; i-- > 0;) {                 /* for note in reversed(item_notes(event)) */
        const json_t *nv = json_get(notes, i);
        const char *note = nv && nv->type == JSON_STRING ? nv->str_val : NULL;
        size_t cl, nl;
        if (!note) continue;
        cl = strlen(caption);
        nl = strlen(note);
        if (nl > cl) continue;
        if (strcmp(caption + (cl - nl), note) != 0) continue;
        if (nl == 0) caption[0] = '\0';          /* caption[:-0] is empty */
        else caption[cl - nl] = '\0';
        while (*caption && caption[strlen(caption) - 1] == '\n')
            caption[strlen(caption) - 1] = '\0';  /* rstrip("\n") */
        kept[nkept++] = (char *)note;
    }

    out = NULL;
    if (*caption) {
        char *joined = tb_merge(src, caption);
        if (!joined) { free(caption); free(kept); return json_null(); }
        out = json_string(joined);
        free(joined);
    } else {
        /* `text` is returned unchanged when there is nothing to merge, whatever
         * its type: upstream hands back the same object, so a caller that passed
         * [] gets [] back, not None. */
        out = src ? json_string(src) : (text ? json_copy(text) : json_null());
    }
    /* notes were prepended while walking backwards: restore the original order */
    for (i = nkept; i-- > 0;) {
        size_t tl;
        char *joined;
        const char *base = out && out->type == JSON_STRING ? out->str_val : NULL;
        const char *note = kept[i];
        if (base && *base) {
            tl = strlen(base) + strlen(note) + 3;
            joined = (char *)malloc(tl);
            if (!joined) break;
            snprintf(joined, tl, "%s\n\n%s", base, note);
        } else {
            size_t nl2 = strlen(note);
            joined = (char *)malloc(nl2 + 1);
            if (!joined) break;
            memcpy(joined, note, nl2 + 1);
        }
        json_free(out);
        out = json_string(joined);
        free(joined);
    }
    free(caption);
    free(kept);
    return out;
}

json_t *w26_telegramba_f62536_constants(void) {
    json_t *o = json_object();
    json_set(o, "caption_text_attr", json_string("_telegram_caption_text"));
    json_set(o, "item_notes_attr", json_string("_telegram_item_notes"));
    return o;
}
