/*
 * telegram_audio_meta.c - C11 port of upstream
 * plugins/platforms/telegram/telegram_audio_meta.py
 * (gate w26_telegramau_5c2119).
 *
 * Ported value->value: the media facts a Telegram send carries. Proven by the
 * differential oracle gate w26_telegramau_5c2119.
 * PoP: w26_telegramau_5c2119_coerce_duration @ plugins/platforms/telegram/telegram_audio_meta.py:_coerce_duration_seconds
 * PoP: w26_telegramau_5c2119_audio_tags @ plugins/platforms/telegram/telegram_audio_meta.py:audio_player_fields
 * PoP: w26_telegramau_5c2119_has_cover @ plugins/platforms/telegram/telegram_audio_meta.py:audio_player_fields
 * PoP: w26_telegramau_5c2119_video_geometry @ plugins/platforms/telegram/telegram_audio_meta.py:_probe_video_geometry
 * PoP: w26_telegramau_5c2119_video_thumb_seek @ plugins/platforms/telegram/telegram_audio_meta.py:_video_thumbnail_jpeg
 * PoP: w26_telegramau_5c2119_wav_duration @ plugins/platforms/telegram/telegram_audio_meta.py:_probe_voice_duration_seconds
 *
 * Seams: wave/mutagen/ffprobe/ffmpeg.  What arrives here is the value a probe
 * would have returned (its JSON blob, or a frame count); what is ported is the
 * rounding, the tag scrubbing, the geometry validation and Telegram's own
 * 256-character limit.
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <strings.h>
#include "noc_w26_telegramau_5c2119.h"

/* ---------------------------------------------------------------- helpers */

static int tg_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static int tg_is_digit(char c) { return c >= '0' && c <= '9'; }

/* Python float(<str>): the whole string must be a number. */
static int tg_float_str(const char *s, double *out) {
    const char *p = s;
    const char *start;
    const char *end;
    int digits = 0;
    char *buf;
    size_t n;

    if (!s) return 0;
    while (tg_is_space(*p)) p++;
    start = p;
    if (*p == '+' || *p == '-') p++;
    if (strncasecmp(p, "infinity", 8) == 0) { p += 8; }
    else if (strncasecmp(p, "inf", 3) == 0) { p += 3; }
    else if (strncasecmp(p, "nan", 3) == 0) { p += 3; }
    else {
        while (tg_is_digit(*p)) { p++; digits++; }
        if (*p == '.') {
            p++;
            while (tg_is_digit(*p)) { p++; digits++; }
        }
        if (digits == 0) return 0;
        if (*p == 'e' || *p == 'E') {
            const char *q = p + 1;
            if (*q == '+' || *q == '-') q++;
            if (!tg_is_digit(*q)) return 0;
            while (tg_is_digit(*q)) q++;
            p = q;
        }
    }
    end = p;
    while (tg_is_space(*p)) p++;
    if (*p != '\0') return 0;
    n = (size_t)(end - start);
    buf = (char *)malloc(n + 1);
    if (!buf) return 0;
    memcpy(buf, start, n);
    buf[n] = '\0';
    *out = strtod(buf, NULL);
    free(buf);
    return 1;
}

/* Python float(value) for a JSON value.  None/list/dict raise -> 0. */
static int tg_py_float(const json_t *v, double *out) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_BOOL:   *out = v->bool_val ? 1.0 : 0.0; return 1;
    case JSON_NUMBER: *out = v->num_val; return 1;
    case JSON_STRING: return tg_float_str(v->str_val ? v->str_val : "", out);
    default: return 0;
    }
}

/* Python int(value) for a JSON value (used by the video geometry). */
static int tg_py_int(const json_t *v, long long *out) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_BOOL:   *out = v->bool_val ? 1 : 0; return 1;
    case JSON_NUMBER:
        if (v->num_val != v->num_val) return 0;
        if (v->num_val >= 9.2233720368547758e18) { *out = LLONG_MAX; return 1; }
        if (v->num_val <= -9.2233720368547758e18) { *out = LLONG_MIN; return 1; }
        *out = (long long)v->num_val; return 1;
    case JSON_STRING: {
        const char *s = v->str_val ? v->str_val : "";
        char *e = NULL;
        long long n;
        while (tg_is_space(*s)) s++;
        if (!*s) return 0;
        n = strtoll(s, &e, 10);
        if (!e) return 0;
        while (tg_is_space(*e)) e++;
        if (*e != '\0') return 0;
        *out = n;
        return 1;
    }
    default: return 0;
    }
}

/* Python round(float) -- half to even, returning a whole number. */
static long long tg_py_round(double x) {
    if (x != x) return 0;
    if (x >= 9.2233720368547758e18) return LLONG_MAX;
    if (x <= -9.2233720368547758e18) return LLONG_MIN;
    return (long long)nearbyint(x);
}

/* Python str.strip() over ASCII whitespace.  Caller frees. */
static char *tg_strip(const char *s) {
    const char *b = s ? s : "";
    const char *e;
    size_t n;
    char *out;
    while (tg_is_space(*b)) b++;
    e = b + strlen(b);
    while (e > b && tg_is_space(e[-1])) e--;
    n = (size_t)(e - b);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}

/* str[:n] by code point, as Python slices a str.  Caller frees. */
static char *tg_truncate_cp(const char *s, size_t n_cp) {
    const char *base = s ? s : "";
    const char *p = base;
    size_t cp = 0, n;
    char *out;
    while (*p && cp < n_cp) {
        p++;
        while ((*p & 0xC0) == 0x80) p++;
        cp++;
    }
    n = (size_t)(p - base);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, base, n);
    out[n] = '\0';
    return out;
}

static void tg_ascii_lower(char *s) {
    for (; *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

static const json_t *tg_blob(const json_t *in) {
    const json_t *blob = json_obj_get(in, "blob");
    return (blob && blob->type == JSON_OBJECT) ? blob : NULL;
}

static const json_t *tg_object_field(const json_t *obj, const char *key) {
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

static const json_t *tg_array_field(const json_t *obj, const char *key) {
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    return (v && v->type == JSON_ARRAY) ? v : NULL;
}

/* ---------------------------------------------------------------- entries */

json_t *w26_telegramau_5c2119_coerce_duration(const json_t *in) {
    double f;
    long long secs;
    if (!tg_py_float(json_obj_get(in, "value"), &f)) return json_null();
    secs = tg_py_round(f);
    return secs > 0 ? json_number((double)secs) : json_null();
}

json_t *w26_telegramau_5c2119_audio_tags(const json_t *in) {
    const json_t *blob = tg_blob(in);
    const json_t *tags = tg_object_field(tg_object_field(blob, "format"), "tags");
    json_t *out = json_object();
    char *artist = NULL, *title = NULL;
    size_t i;

    for (i = 0; tags && i < json_object_size(tags); i++) {
        const char *k = json_object_get_key_at(tags, i);
        const json_t *v = json_object_get_at(tags, i);
        char *lower, *val;
        if (!k || !v || v->type != JSON_STRING) continue;
        lower = tg_strip(k);
        val = tg_strip(v->str_val);
        if (!lower || !val) { free(lower); free(val); continue; }
        tg_ascii_lower(lower);
        if (!strcmp(lower, "artist")) { free(artist); artist = val; val = NULL; }
        else if (!strcmp(lower, "title")) { free(title); title = val; val = NULL; }
        free(lower);
        free(val);
    }
    if (artist && artist[0]) {
        char *cut = tg_truncate_cp(artist, W26_TELEGRAMAU_5C2119_TAG_MAX_CHARS);
        json_set(out, "performer", json_string(cut ? cut : ""));
        free(cut);
    }
    if (title && title[0]) {
        char *cut = tg_truncate_cp(title, W26_TELEGRAMAU_5C2119_TAG_MAX_CHARS);
        json_set(out, "title", json_string(cut ? cut : ""));
        free(cut);
    }
    free(artist);
    free(title);
    return out;
}

json_t *w26_telegramau_5c2119_has_cover(const json_t *in) {
    const json_t *streams = tg_array_field(tg_blob(in), "streams");
    size_t i, n = streams ? json_len(streams) : 0;
    for (i = 0; i < n; i++) {
        const json_t *s = json_get(streams, i);
        const json_t *ct;
        if (!s || s->type != JSON_OBJECT) continue;
        ct = json_obj_get(s, "codec_type");
        if (ct && ct->type == JSON_STRING && ct->str_val && !strcmp(ct->str_val, "video"))
            return json_bool(1);
    }
    return json_bool(0);
}

json_t *w26_telegramau_5c2119_video_geometry(const json_t *in) {
    const json_t *blob = tg_blob(in);
    const json_t *streams = tg_array_field(blob, "streams");
    json_t *out = json_object();
    const json_t *first;
    long long w, h;
    double fdur;

    if (!streams || json_len(streams) == 0) return out;
    first = json_get(streams, 0);
    if (!first || first->type != JSON_OBJECT) return out;
    if (!tg_py_int(json_obj_get(first, "width"), &w)) return out;
    if (!tg_py_int(json_obj_get(first, "height"), &h)) return out;
    json_set(out, "width", json_number((double)w));
    json_set(out, "height", json_number((double)h));
    if (json_obj_get(first, "width") && json_obj_get(first, "height") &&
        tg_py_float(json_obj_get(tg_object_field(blob, "format"), "duration"), &fdur)) {
        long long dur = tg_py_round(fdur);
        if (dur > 0) json_set(out, "duration", json_number((double)dur));
    }
    return out;
}

json_t *w26_telegramau_5c2119_video_thumb_seek(const json_t *in) {
    const json_t *v = json_obj_get(in, "duration");
    long long dur;
    long long seek;

    if (!v || v->type == JSON_NULL) dur = 0;
    else if (!tg_py_int(v, &dur)) return json_null();
    if (!dur) dur = 3;                              /* (duration or 3) */
    seek = (long long)((double)dur * 0.25);         /* int(...) truncates */
    return json_number((double)(seek > 1 ? seek : 1));
}

json_t *w26_telegramau_5c2119_wav_duration(const json_t *in) {
    const json_t *rate_v = json_obj_get(in, "rate");
    const json_t *frames_v = json_obj_get(in, "nframes");
    long long rate, frames;
    double secs;
    long long whole;

    if (!rate_v || rate_v->type == JSON_NULL) rate = 0;
    else if (!tg_py_int(rate_v, &rate)) return json_null();
    if (!rate) return json_null();                  /* `... if rate else None` */
    if (!frames_v || frames_v->type == JSON_NULL) return json_null();
    if (!tg_py_int(frames_v, &frames)) return json_null();
    secs = (double)frames / (double)rate;
    whole = tg_py_round(secs);
    return whole > 0 ? json_number((double)whole) : json_null();
}
