/*
 * wake_word_engines.c -- faithful C11 port of upstream Hermes
 * tools/wake_word_engines.py (gate w10_wakeworde_ebd387).
 *
 * PoP: w10_wakeworde_ebd387_sub               @ tools/wake_word_engines.py:_sub
 * PoP: w10_wakeworde_ebd387_looks_like_path   @ tools/wake_word_engines.py:_looks_like_path
 * PoP: w10_wakeworde_ebd387_sherpa_model_root @ tools/wake_word_engines.py:_sherpa_model_root
 * PoP: w10_wakeworde_ebd387_openwake_process  @ tools/wake_word_engines.py:_OpenWakeWordEngine.process
 * PoP: w10_wakeworde_ebd387_sherpa_process    @ tools/wake_word_engines.py:_SherpaKwsEngine.process
 * PoP: w10_wakeworde_ebd387_porcupine_process @ tools/wake_word_engines.py:_PorcupineEngine.process
 *
 * Model downloads / audio I / vendor SDKs are not portable; the deterministic
 * helpers and the three engines' decision logic are ported verbatim.
 */
#include "noc_w10_wakeworde_ebd387.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *jstr(const json_t *v) { return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : ""; }
static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "") + 1; char *o = (char *)malloc(n);
    if (o) strcpy(o, s ? s : ""); return o;
}

json_t *w10_wakeworde_ebd387_sub(const json_t *cfg, const char *key) {
    if (!cfg || cfg->type != JSON_OBJECT) {
        json_t *e = json_object();
        json_set(e, "__raise__", json_string("AttributeError"));
        return e;  /* Python: cfg.get() on a non-mapping raises */
    }
    json_t *v = json_obj_get(cfg, key);
    return (v && v->type == JSON_OBJECT) ? json_copy(v) : json_object();
}

json_t *w10_wakeworde_ebd387_looks_like_path(const char *value, int exists, const char *sep) {
    const char *v = value ? value : "";
    const char *sp = sep ? sep : "\\";
    int like = 0;
    if (*sp && strstr(v, sp)) like = 1;
    size_t l = strlen(v);
    static const char *const SUFFIX[] = {".onnx", ".tflite", ".ppn", NULL};
    for (int i = 0; SUFFIX[i]; i++) {
        size_t sl = strlen(SUFFIX[i]);
        if (l >= sl && !strcmp(v + l - sl, SUFFIX[i])) like = 1;
    }
    if (exists) like = 1;
    return json_bool(like);
}

json_t *w10_wakeworde_ebd387_sherpa_model_root(const char *home) {
    const char *h = home ? home : "";
    size_t n = strlen(h) + strlen("/cache/wakewords") + 1;
    char *raw = (char *)malloc(n);
    snprintf(raw, n, "%s/cache/wakewords", h);
    /* pathlib / on Windows normalizes separators to os.sep ("\\"). */
    char *out = dupstr(raw);
    for (char *p = out; *p; p++) if (*p == '/') *p = '\\';
    json_t *res = json_string(out);
    free(raw); free(out);
    return res;
}

/* _OpenWakeWordEngine.process: N consecutive over-threshold frames fire. */
json_t *w10_wakeworde_ebd387_openwake_process(const json_t *scores, double threshold,
                                              int confirm_needed, int initial_streak) {
    json_t *results = json_array();
    int streak = initial_streak;
    size_t nf = (scores && scores->type == JSON_ARRAY) ? json_len(scores) : 0;
    for (size_t f = 0; f < nf; f++) {
        json_t *frame = json_get(scores, f);
        int over = 0;
        size_t ns = (frame && frame->type == JSON_ARRAY) ? json_len(frame) : 0;
        for (size_t i = 0; i < ns; i++) {
            json_t *s = json_get(frame, i);
            if (s && s->type == JSON_NUMBER && s->num_val >= threshold) { over = 1; break; }
        }
        int fired = 0;
        if (over) {
            streak++;
            if (streak >= confirm_needed) { streak = 0; fired = 1; }
        } else streak = 0;
        json_append(results, json_bool(fired));
    }
    json_t *out = json_object();
    json_set(out, "results", results);
    json_set(out, "streak", json_number((double)streak));
    return out;
}

/* _SherpaKwsEngine.process: last result in a frame wins; display -> profile. */
json_t *w10_wakeworde_ebd387_sherpa_process(const json_t *results, const json_t *display_to_profile) {
    json_t *fired_arr = json_array();
    json_t *last = NULL;
    size_t nf = (results && results->type == JSON_ARRAY) ? json_len(results) : 0;
    for (size_t f = 0; f < nf; f++) {
        json_t *frame = json_get(results, f);
        int fired = 0;
        size_t nr = (frame && frame->type == JSON_ARRAY) ? json_len(frame) : 0;
        for (size_t i = 0; i < nr; i++) {
            const char *display = jstr(json_get(frame, i));
            fired = 1;
            /* display.replace("_", " ").lower() */
            char *phrase = dupstr(display);
            for (char *p = phrase; *p; p++) {
                if (*p == '_') *p = ' ';
                else if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
            }
            const char *prof = (display_to_profile && display_to_profile->type == JSON_OBJECT)
                               ? json_get_str(display_to_profile, display, "") : "";
            if (last) json_free(last);
            last = json_array();
            json_append(last, json_string(phrase));
            json_append(last, json_string(prof));
            free(phrase);
        }
        json_append(fired_arr, json_bool(fired));
    }
    json_t *out = json_object();
    json_set(out, "results", fired_arr);
    json_set(out, "last_match", last ? last : json_null());
    return out;
}

json_t *w10_wakeworde_ebd387_porcupine_process(const json_t *values) {
    json_t *out = json_array();
    size_t n = (values && values->type == JSON_ARRAY) ? json_len(values) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(values, i);
        int hit = (v && v->type == JSON_NUMBER && v->num_val >= 0);
        json_append(out, json_bool(hit));
    }
    return out;
}
