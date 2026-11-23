/*
 * tts_tool_delivery.c -- faithful C11 port of upstream Hermes
 * tools/tts_tool_delivery.py (gate w10_ttstoolde_5650bd).
 *
 * PoP: w10_ttstoolde_5650bd_section              @ tools/tts_tool_delivery.py:_section
 * PoP: w10_ttstoolde_5650bd_positive_int         @ tools/tts_tool_delivery.py:_positive_int
 * PoP: w10_ttstoolde_5650bd_resolve_max_text_length @ tools/tts_tool_delivery.py:_resolve_max_text_length
 * PoP: w10_ttstoolde_5650bd_resolve_audio_delivery_profile @ tools/tts_tool_delivery.py:_resolve_audio_delivery_profile
 * PoP: w10_ttstoolde_5650bd_pack_under_cap       @ tools/tts_tool_delivery.py:_pack_under_cap
 * PoP: w10_ttstoolde_5650bd_split_oversized_sentence @ tools/tts_tool_delivery.py:_split_oversized_sentence
 * PoP: w10_ttstoolde_5650bd_split_text_for_tts   @ tools/tts_tool_delivery.py:_split_text_for_tts
 * PoP: w10_ttstoolde_5650bd_wav_sidecar_path     @ tools/tts_tool_delivery.py:_wav_sidecar_path
 * PoP: w10_ttstoolde_5650bd_wrap_pcm_as_wav      @ tools/tts_tool_delivery.py:_wrap_pcm_as_wav
 *
 * ffmpeg/filesystem/network surfaces are not portable; the nine deterministic
 * helpers above are ported verbatim, including literal provider caps.
 */
#include "noc_w10_ttstoolde_5650bd.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FALLBACK_MAX_TEXT_LENGTH 4000
#define DEFAULT_COMMAND_TTS_MAX_TEXT_LENGTH 5000

static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "") + 1; char *o = (char *)malloc(n);
    if (o) strcpy(o, s ? s : ""); return o;
}
static const char *jstr(const json_t *v) { return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : ""; }
static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count != 0;
    return 1;
}
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v) return dupstr("");
    switch (v->type) {
        case JSON_STRING: return dupstr(v->str_val ? v->str_val : "");
        case JSON_NULL: return dupstr("None");
        case JSON_BOOL: return dupstr(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e15)
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
            else snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return dupstr(buf);
        default: return dupstr("");
    }
}
static char *lower_str(const char *s) {
    char *o = dupstr(s);
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *strip_ws(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    size_t l = strlen(s);
    while (l && isspace((unsigned char)s[l-1])) l--;
    char *o = (char *)malloc(l + 1); memcpy(o, s, l); o[l] = '\0'; return o;
}
static char *str_concat(const char *a, const char *b) {
    size_t n = strlen(a) + strlen(b) + 1; char *o = (char *)malloc(n);
    snprintf(o, n, "%s%s", a, b); return o;
}

json_t *w10_ttstoolde_5650bd_section(const json_t *tts_config, const char *key) {
    json_t *v = (tts_config && tts_config->type == JSON_OBJECT) ? json_obj_get(tts_config, key) : NULL;
    return (v && v->type == JSON_OBJECT) ? json_copy(v) : json_object();
}

json_t *w10_ttstoolde_5650bd_positive_int(const json_t *value) {
    if (value && value->type == JSON_NUMBER && value->num_val > 0)
        return json_number(value->num_val);
    return json_null();
}

static long positive_int_val(const json_t *v) {
    return (v && v->type == JSON_NUMBER && v->num_val > 0) ? (long)v->num_val : 0L;
}

/* PROVIDER_MAX_TEXT_LENGTH */
static long provider_cap(const char *key) {
    static const char *const K[] = {"edge","openai","xai","minimax","mistral","gemini","elevenlabs","neutts","kittentts","piper",NULL};
    static const long V[] = {5000,4096,15000,10000,4000,32000,10000,2000,2000,5000};
    for (int i = 0; K[i]; i++) if (!strcmp(key, K[i])) return V[i];
    return 0;
}
static long eleven_cap(const char *model) {
    static const char *const K[] = {"eleven_v3","eleven_ttv_v3","eleven_multilingual_v2","eleven_multilingual_v1",
        "eleven_english_sts_v2","eleven_english_sts_v1","eleven_flash_v2","eleven_flash_v2_5",NULL};
    static const long V[] = {5000,5000,10000,10000,10000,10000,30000,40000};
    for (int i = 0; K[i]; i++) if (!strcmp(model, K[i])) return V[i];
    return 0;
}
static int is_builtin(const char *key) {
    static const char *const B[] = {"edge","elevenlabs","openai","minimax","xai","mistral","gemini",
        "neutts","kittentts","piper","deepinfra",NULL};
    for (int i = 0; B[i]; i++) if (!strcmp(key, B[i])) return 1;
    return 0;
}

json_t *w10_ttstoolde_5650bd_resolve_max_text_length(const char *provider, const json_t *tts_config) {
    if (!provider || !*provider) return json_number(FALLBACK_MAX_TEXT_LENGTH);
    char *k0 = strip_ws(provider);
    char *key = lower_str(k0); free(k0);
    const json_t *cfg = (tts_config && tts_config->type == JSON_OBJECT) ? tts_config : NULL;
    json_t *prov_cfg = w10_ttstoolde_5650bd_section(cfg, key);
    long result = 0;

    long over = positive_int_val(json_obj_get(prov_cfg, "max_text_length"));
    if (over) { result = over; goto done; }

    if (!strcmp(key, "elevenlabs")) {
        char *mid = dupstr("eleven_multilingual_v2");
        json_t *m = json_obj_get(prov_cfg, "model_id");
        if (truthy(m)) { char *ms = py_str(m); free(mid); mid = strip_ws(ms); free(ms); }
        long mapped = eleven_cap(mid);
        free(mid);
        if (mapped) { result = mapped; goto done; }
    }
    long cap = provider_cap(key);
    if (cap) { result = cap; goto done; }

    if (!is_builtin(key)) {
        json_t *named = NULL;
        json_t *providers = cfg ? json_obj_get(cfg, "providers") : NULL;
        if (providers && providers->type == JSON_OBJECT) {
            json_t *sec = json_obj_get(providers, key);
            if (sec && sec->type == JSON_OBJECT) named = sec;
        }
        if (!named && cfg) {
            json_t *sec = json_obj_get(cfg, key);
            if (sec && sec->type == JSON_OBJECT) named = sec;
        }
        int cmd = 0;
        if (named) {
            json_t *typev = json_obj_get(named, "type");
            char *ts = typev ? py_str(typev) : dupstr("");
            char *tt = strip_ws(ts); char *tl = lower_str(tt);
            json_t *cmdv = json_obj_get(named, "command");
            cmd = (!strcmp(tl, "") || !strcmp(tl, "command")) && cmdv && cmdv->type == JSON_STRING
                  && *jstr(cmdv);
            free(ts); free(tt); free(tl);
        }
        if (cmd) {
            long mc = positive_int_val(json_obj_get(named, "max_text_length"));
            result = mc ? mc : DEFAULT_COMMAND_TTS_MAX_TEXT_LENGTH;
            goto done;
        }
    }
    result = FALLBACK_MAX_TEXT_LENGTH;

done:
    json_free(prov_cfg);
    free(key);
    return json_number(result);
}

json_t *w10_ttstoolde_5650bd_resolve_audio_delivery_profile(const char *platform, const json_t *tts_config) {
    char *k0 = strip_ws(platform ? platform : "default");
    char *key = lower_str(*k0 ? k0 : "default"); free(k0);

    long max_bytes = 10L * 1024 * 1024;
    if (!strcmp(key, "discord")) max_bytes = 10L * 1024 * 1024;
    else if (!strcmp(key, "telegram")) max_bytes = 50L * 1024 * 1024;
    double ratio = 0.85;

    json_t *dp = w10_ttstoolde_5650bd_section(tts_config, "delivery_profiles");
    json_t *ov = w10_ttstoolde_5650bd_section(dp, key);
    json_free(dp);
    size_t no = json_object_size(ov);
    for (size_t i = 0; i < no; i++) {
        const char *k = json_object_get_key_at(ov, i);
        json_t *v = json_object_get_at(ov, i);
        if (!k || !v || v->type == JSON_NULL) continue;
        if (!strcmp(k, "max_file_bytes")) {
            long po = positive_int_val(v);
            if (po) max_bytes = po;
        } else if (!strcmp(k, "safety_ratio")) {
            if (v->type == JSON_NUMBER && v->num_val > 0 && v->num_val <= 1) ratio = v->num_val;
            else ratio = 0.85;
        }
    }
    json_free(ov);

    long target = (long)(max_bytes * ratio);
    if (target < 1) target = 1;
    json_t *out = json_object();
    json_set(out, "platform", json_string(key));
    json_set(out, "max_file_bytes", json_number((double)max_bytes));
    json_set(out, "safety_ratio", json_number(ratio));
    json_set(out, "target_file_bytes", json_number((double)target));
    free(key);
    return out;
}

/* ---- greedy packing ---- */
static char *joined_trim(const char *cur, const char *piece) {
    char *raw = str_concat(cur, " ");
    char *raw2 = str_concat(raw, piece); free(raw);
    char *o = strip_ws(raw2); free(raw2);
    return o;
}
static size_t str_len(const char *s) { return strlen(s); }

json_t *w10_ttstoolde_5650bd_pack_under_cap(const json_t *pieces, int max_chars, int slice_oversized) {
    json_t *out = json_array();
    if (max_chars <= 0) max_chars = 1;
    char *current = dupstr("");
    size_t n = (pieces && pieces->type == JSON_ARRAY) ? json_len(pieces) : 0;
    for (size_t i = 0; i < n; i++) {
        const char *piece = jstr(json_get(pieces, i));
        size_t plen = str_len(piece);
        if (slice_oversized && plen > (size_t)max_chars) {
            if (*current) { json_append(out, json_string(current)); free(current); current = dupstr(""); }
            for (size_t off = 0; off < plen; off += (size_t)max_chars) {
                size_t take = plen - off < (size_t)max_chars ? plen - off : (size_t)max_chars;
                char *sl = (char *)malloc(take + 1);
                memcpy(sl, piece + off, take); sl[take] = '\0';
                json_append(out, json_string(sl)); free(sl);
            }
            continue;
        }
        char *cand = joined_trim(current, piece);
        if (*current && str_len(cand) > (size_t)max_chars) {
            json_append(out, json_string(current));
            free(current); current = dupstr(piece);
        } else { free(current); current = cand; cand = NULL; }
        free(cand);
    }
    if (*current) json_append(out, json_string(current));
    free(current);
    return out;
}

json_t *w10_ttstoolde_5650bd_split_oversized_sentence(const char *sentence, int max_chars) {
    json_t *words = json_array();
    const char *p = sentence ? sentence : "";
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *s = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        size_t l = (size_t)(p - s);
        char *w = (char *)malloc(l + 1); memcpy(w, s, l); w[l] = '\0';
        json_append(words, json_string(w)); free(w);
    }
    json_t *out = w10_ttstoolde_5650bd_pack_under_cap(words, max_chars, 1);
    json_free(words);
    return out;
}

json_t *w10_ttstoolde_5650bd_split_text_for_tts(const char *text, int max_chars) {
    if (max_chars <= 0) max_chars = FALLBACK_MAX_TEXT_LENGTH;
    char *norm = strip_ws(text ? text : "");
    /* collapse all internal whitespace runs to a single space */
    size_t n = strlen(norm); char *col = (char *)malloc(n + 1);
    size_t j = 0; int was_space = 1;
    for (size_t i = 0; i < n; i++) {
        if (isspace((unsigned char)norm[i])) { if (!was_space) { col[j++] = ' '; was_space = 1; } }
        else { col[j++] = norm[i]; was_space = 0; }
    }
    if (j && col[j-1] == ' ') j--;
    col[j] = '\0'; free(norm);
    if (j == 0) { free(col); return json_array(); }
    if (strlen(col) <= (size_t)max_chars) { json_t *o = json_array(); json_append(o, json_string(col)); free(col); return o; }

    json_t *expanded = json_array();
    const char *p = col;
    while (*p) {
        const char *start = p;
        int split = 0;
        for (const char *q = p; *q; q++) {
            if ((*q=='.'||*q=='!'||*q=='?'||*q==';'||*q==':'||*q==',') && q[1] && isspace((unsigned char)q[1])) {
                size_t l = (size_t)(q + 1 - start);
                char *sn = (char *)malloc(l + 1); memcpy(sn, start, l); sn[l] = '\0';
                char *st = strip_ws(sn); free(sn);
                if (*st) json_append(expanded, json_string(st));
                free(st);
                p = q + 1; while (*p && isspace((unsigned char)*p)) p++;
                split = 1; break;
            }
        }
        if (split) continue;
        /* no split point: rest of string */
        char *st = strip_ws(start);
        if (*st) json_append(expanded, json_string(st));
        free(st);
        break;
    }
    free(col);
    json_t *out = json_array();
    size_t ne = json_len(expanded);
    for (size_t i = 0; i < ne; i++) {
        const char *s = jstr(json_get(expanded, i));
        if (strlen(s) <= (size_t)max_chars) json_append(out, json_string(s));
        else {
            json_t *parts = w10_ttstoolde_5650bd_split_oversized_sentence(s, max_chars);
            size_t np = json_len(parts);
            for (size_t k = 0; k < np; k++) json_append(out, json_copy(json_get(parts, k)));
            json_free(parts);
        }
    }
    json_free(expanded);
    json_t *res = w10_ttstoolde_5650bd_pack_under_cap(out, max_chars, 0);
    json_free(out);
    return res;
}

json_t *w10_ttstoolde_5650bd_wav_sidecar_path(const char *output_path) {
    const char *p = output_path ? output_path : "";
    size_t l = strlen(p);
    if (l >= 4 && !strcmp(p + l - 4, ".wav")) return json_string(p);
    const char *dot = strrchr(p, '.');
    if (!dot) return json_string(".wav");
    size_t n = (size_t)(dot - p);
    char *o = (char *)malloc(n + 5);
    memcpy(o, p, n); memcpy(o + n, ".wav", 5);
    json_t *out = json_string(o); free(o);
    return out;
}

static void put_u16(json_t *a, unsigned v) {
    json_append(a, json_number((double)(v & 0xff)));
    json_append(a, json_number((double)((v >> 8) & 0xff)));
}
static void put_u32(json_t *a, unsigned long v) {
    for (int i = 0; i < 4; i++) json_append(a, json_number((double)((v >> (8*i)) & 0xff)));
}
json_t *w10_ttstoolde_5650bd_wrap_pcm_as_wav(const json_t *pcm_bytes, int sample_rate, int channels, int sample_width) {
    json_t *out = json_array();
    if (channels <= 0) channels = 1;
    if (sample_width <= 0) sample_width = 2;
    unsigned long pcm_n = (pcm_bytes && pcm_bytes->type == JSON_ARRAY) ? json_len(pcm_bytes) : 0;
    unsigned long block_align = (unsigned long)channels * (unsigned long)sample_width;
    /* "RIFF" + u32(size) + "WAVE" */
    const unsigned char riff[4] = {'R','I','F','F'}, wave[4] = {'W','A','V','E'};
    for (int i = 0; i < 4; i++) json_append(out, json_number(riff[i]));
    put_u32(out, 4UL + 24UL + 8UL + pcm_n);
    for (int i = 0; i < 4; i++) json_append(out, json_number(wave[i]));
    /* fmt chunk */
    const unsigned char fmt[4] = {'f','m','t',' '};
    for (int i = 0; i < 4; i++) json_append(out, json_number(fmt[i]));
    put_u32(out, 16);
    put_u16(out, 1);                       /* PCM */
    put_u16(out, (unsigned)channels);
    put_u32(out, (unsigned long)sample_rate);
    put_u32(out, (unsigned long)sample_rate * block_align);
    put_u16(out, (unsigned)block_align);
    put_u16(out, (unsigned)sample_width * 8);
    /* data header */
    const unsigned char dat[4] = {'d','a','t','a'};
    for (int i = 0; i < 4; i++) json_append(out, json_number(dat[i]));
    put_u32(out, pcm_n);
    for (unsigned long i = 0; i < pcm_n; i++) {
        json_t *b = pcm_bytes ? json_get(pcm_bytes, (size_t)i) : NULL;
        int v = (b && b->type == JSON_NUMBER) ? (int)b->num_val : 0;
        json_append(out, json_number((double)(v & 0xff)));
    }
    return out;
}
