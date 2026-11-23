/*
 * voice_mode_transcript.c — faithful C11 port of tools/voice_mode_transcript.py.
 * PoP: w15_voicemode_7ad9c5_is_whisper_hallucination @ tools/voice_mode_transcript.py:is_whisper_hallucination
 * PoP: w15_voicemode_7ad9c5_normalize_for_echo_compare @ tools/voice_mode_transcript.py:_normalize_for_echo_compare
 * PoP: w15_voicemode_7ad9c5_is_tts_echo @ tools/voice_mode_transcript.py:is_tts_echo
 * PoP: w15_voicemode_7ad9c5_is_voice_stop_phrase @ tools/voice_mode_transcript.py:is_voice_stop_phrase
 * PoP: w15_voicemode_7ad9c5_voice_stop_hint @ tools/voice_mode_transcript.py:voice_stop_hint
 */
#include "noc_w15_voicemode_7ad9c5.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

/* Python str.strip() for ASCII input. */
static char *py_strip(const char *s) {
    if (!s) return dupstr("");
    size_t a = 0, b = strlen(s);
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *p = (char *)malloc(b - a + 1);
    if (p) { memcpy(p, s + a, b - a); p[b - a] = '\0'; }
    return p;
}

static void ascii_lower(char *s) {
    for (; *s; ++s) *s = (char)tolower((unsigned char)*s);
}

/* re.sub(r"\s+", " ", text).strip().lower() */
static char *normalize_for_echo(const char *text) {
    const char *src = text ? text : "";
    size_t n = strlen(src);
    char *tmp = (char *)malloc(n + 2);
    if (!tmp) return dupstr("");
    size_t o = 0;
    int inws = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)src[i];
        if (isspace(c)) {
            if (!inws) { tmp[o++] = ' '; inws = 1; }
        } else { tmp[o++] = src[i]; inws = 0; }
    }
    tmp[o] = '\0';
    char *out = py_strip(tmp);
    free(tmp);
    ascii_lower(out);
    return out;
}

/* WHISPER_HALLUCINATIONS (mirror of the live set, UTF-8 literals). */
static const char *const HALLUCINATIONS[] = {
    "thank you", "thanks for watching", "subscribe to my channel", "like and subscribe",
    "please subscribe", "thank you for watching", "bye", "you", "the end",
    "продолжение следует", "sous-titres", "sous-titres réalisés par la communauté d'amara.org",
    "sottotitoli creati dalla comunità amara.org", "untertitel von stephanie geiges",
    "amara.org", "www.mooji.org", "ご視聴ありがとうございました",
    NULL
};

/* _HALLUCINATION_REPEAT_RE: ^(?:thank you|thanks|bye|you|ok|okay|the end|\.|\s|,|!)+$ (IGNORECASE) */
static int repeat_match(const char *s) {
    const char *p = s;
    int count = 0;
    while (*p) {
        size_t r = strlen(p);
        int k = 0;
        if (r >= 9 && !memcmp(p, "thank you", 9)) k = 9;
        else if (r >= 6 && !memcmp(p, "thanks", 6)) k = 6;
        else if (r >= 7 && !memcmp(p, "the end", 7)) k = 7;
        else if (r >= 4 && !memcmp(p, "okay", 4)) k = 4;
        else if (r >= 2 && !memcmp(p, "ok", 2)) k = 2;
        else if (r >= 3 && !memcmp(p, "bye", 3)) k = 3;
        else if (r >= 3 && !memcmp(p, "you", 3)) k = 3;
        else if (*p == '.' || *p == ',' || *p == '!' || isspace((unsigned char)*p)) k = 1;
        if (!k) return 0;
        p += k;
        count++;
    }
    return count > 0;
}

int w15_voicemode_7ad9c5_is_whisper_hallucination(const char *transcript) {
    char *cl = py_strip(transcript ? transcript : "");
    ascii_lower(cl);
    char *bare = dupstr(cl);
    size_t n = strlen(bare);
    while (n > 0 && (bare[n - 1] == '.' || bare[n - 1] == '!')) bare[--n] = '\0';
    int res = (*cl == '\0');
    if (!res) {
        for (int i = 0; HALLUCINATIONS[i]; i++) {
            if (!strcmp(bare, HALLUCINATIONS[i])) { res = 1; break; }
        }
        if (!res && repeat_match(cl)) res = 1;
    }
    free(bare);
    free(cl);
    return res;
}

char *w15_voicemode_7ad9c5_normalize_for_echo_compare(const char *text) {
    return normalize_for_echo(text);
}

/* difflib.SequenceMatcher(None, a, b).ratio() -- Ratcliff/Obershelp.
 * Fixtures keep len(b) < 200 so autojunk never triggers (matching live). */
typedef struct { int alo, ahi, blo, bhi; } sm_range;

static int sm_find_longest(const char *a, int alo, int ahi,
                           const char *b, int blo, int bhi, int *oi, int *oj) {
    int nb = bhi + 1;
    int *j2len = (int *)calloc((size_t)nb, sizeof(int));
    int *newj2len = (int *)calloc((size_t)nb, sizeof(int));
    int besti = alo, bestj = blo, bestsize = 0;
    if (j2len && newj2len) {
        for (int i = alo; i < ahi; i++) {
            memset(newj2len, 0, (size_t)nb * sizeof(int));
            for (int j = blo; j < bhi; j++) {
                if (a[i] != b[j]) continue;
                int k = (j > blo ? j2len[j - 1] : 0) + 1;
                newj2len[j] = k;
                if (k > bestsize) { bestsize = k; besti = i - k + 1; bestj = j - k + 1; }
            }
            int *t = j2len; j2len = newj2len; newj2len = t;
        }
    }
    free(j2len);
    free(newj2len);
    while (besti > alo && bestj > blo && a[besti - 1] == b[bestj - 1]) {
        besti--; bestj--; bestsize++;
    }
    while (besti + bestsize < ahi && bestj + bestsize < bhi &&
           a[besti + bestsize] == b[bestj + bestsize]) bestsize++;
    *oi = besti; *oj = bestj;
    return bestsize;
}

static int sm_matching_size(const char *a, int la, const char *b, int lb) {
    sm_range stack[512];
    int top = 0, total = 0;
    stack[top++] = (sm_range){0, la, 0, lb};
    while (top > 0) {
        sm_range r = stack[--top];
        int i = 0, j = 0;
        int k = sm_find_longest(a, r.alo, r.ahi, b, r.blo, r.bhi, &i, &j);
        if (k > 0) {
            total += k;
            if (r.alo < i && r.blo < j) stack[top++] = (sm_range){r.alo, i, r.blo, j};
            if (i + k < r.ahi && j + k < r.bhi)
                stack[top++] = (sm_range){i + k, r.ahi, j + k, r.bhi};
        }
    }
    return total;
}

static double sm_ratio(const char *a, const char *b) {
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la + lb == 0) return 1.0;
    int m = sm_matching_size(a, la, b, lb);
    return 2.0 * (double)m / (double)(la + lb);
}

int w15_voicemode_7ad9c5_is_tts_echo(const char *transcript, const char *spoken_text,
                                     double threshold) {
    char *a = normalize_for_echo(transcript ? transcript : "");
    char *b = normalize_for_echo(spoken_text ? spoken_text : "");
    int res = 0;
    if (!*a || !*b) goto done;
    if (sm_ratio(a, b) >= threshold) { res = 1; goto done; }
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la < 10 || la >= lb) goto done;
    for (int start = 0; start <= lb - la; start++) {
        char *win = (char *)malloc((size_t)la + 1);
        if (!win) break;
        memcpy(win, b + start, (size_t)la);
        win[la] = '\0';
        int hit = sm_ratio(a, win) >= threshold;
        free(win);
        if (hit) { res = 1; break; }
    }
done:
    free(a);
    free(b);
    return res;
}

int w15_voicemode_7ad9c5_is_voice_stop_phrase(const char *transcript, const json_t *stop_phrases) {
    char *c;
    if (transcript && *transcript) {
        char *s = py_strip(transcript);
        ascii_lower(s);
        size_t a = 0, b = strlen(s);
        while (a < b && strchr(".,!?;: \t\n\r\f\v\"'", s[a])) a++;
        while (b > a && strchr(".,!?;: \t\n\r\f\v\"'", s[b - 1])) b--;
        c = (char *)malloc(b - a + 1);
        if (c) { memcpy(c, s + a, b - a); c[b - a] = '\0'; }
        free(s);
    } else {
        c = dupstr("");
    }
    int res = 0;
    if (*c && stop_phrases && stop_phrases->type == JSON_ARRAY) {
        for (size_t i = 0; i < stop_phrases->c.count; i++) {
            json_t *v = stop_phrases->c.items[i];
            if (v && v->type == JSON_STRING && v->str_val && !strcmp(c, v->str_val)) { res = 1; break; }
        }
    }
    free(c);
    return res;
}

char *w15_voicemode_7ad9c5_voice_stop_hint(const json_t *stop_phrases) {
    const char *first = NULL;
    if (stop_phrases && stop_phrases->type == JSON_ARRAY && stop_phrases->c.count > 0) {
        json_t *v = stop_phrases->c.items[0];
        if (v && v->type == JSON_STRING) first = v->str_val;
    }
    if (!first) return dupstr("");
    size_t n = strlen(first) + 64;
    char *out = (char *)malloc(n);
    if (out) snprintf(out, n, "Say \"%s\" to end the voice chat.", first);
    return out;
}
