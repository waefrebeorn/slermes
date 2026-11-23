/*
 * transcription_local.c — C port of upstream Hermes tools/transcription_local.py.
 *
 * PoP: w13_transcripti_f10ae7_normalize_local_stt_language @ tools/transcription_local.py:_normalize_local_stt_language
 * PoP: w13_transcripti_f10ae7_normalize_local_model @ tools/transcription_local.py:_normalize_local_model
 * PoP: w13_transcripti_f10ae7_get_idle_unload_seconds @ tools/transcription_local.py:_get_idle_unload_seconds
 * PoP: w13_transcripti_f10ae7_looks_like_cuda_lib_error @ tools/transcription_local.py:_looks_like_cuda_lib_error
 * PoP: w13_transcripti_f10ae7_confidence_thresholds @ tools/transcription_local.py:_confidence_thresholds
 *
 * The live module's imported helpers are injected as faithful stubs by the oracle:
 *   tools.transcription_common._config_number (mirrored by config_number below).
 */
#include "noc_w13_transcripti_f10ae7.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* str.strip() into a caller buffer; NUL written at the trimmed length. */
static const char *trim_copy(const char *s, char *buf, size_t n) {
    while (*s && isspace((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    size_t len = (size_t)(e - s);
    if (len >= n) len = n - 1;
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

/* _config_number(cfg, key, default, cast): cast(cfg.get(key, default)) or default. */
static double config_number(const struct json_t *cfg, const char *key, double dflt, int as_int) {
    const struct json_t *v = cfg ? json_obj_get(cfg, key) : NULL;
    if (!v) return dflt;
    if (v->type == JSON_NUMBER) {
        double x = v->num_val;
        return as_int ? (double)(long long)x : x;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_STRING) {
        char *end = NULL;
        if (as_int) {
            long long iv = strtoll(v->str_val, &end, 10);
            if (end == v->str_val) return dflt;
            while (*end && isspace((unsigned char)*end)) end++;
            if (*end) return dflt;
            return (double)iv;
        }
        double x = strtod(v->str_val, &end);
        if (end == v->str_val) return dflt;
        while (*end && isspace((unsigned char)*end)) end++;
        if (*end) return dflt;
        return x;
    }
    return dflt;
}

static const char *py_str(const struct json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, n, "None"); return buf; }
    if (v->type == JSON_STRING) return v->str_val;
    if (v->type == JSON_BOOL) { snprintf(buf, n, "%s", v->bool_val ? "True" : "False"); return buf; }
    if (v->type == JSON_NUMBER) {
        double x = v->num_val;
        if (x == (double)(long long)x) snprintf(buf, n, "%lld", (long long)x);
        else snprintf(buf, n, "%g", x);
        return buf;
    }
    snprintf(buf, n, "");
    return buf;
}

struct json_t *w13_transcripti_f10ae7_normalize_local_stt_language(
        const struct json_t *language, const struct json_t *supported) {
    static const char *alias_keys[] = {"\xe7\xb9\x81\xe9\xab\x94\xe4\xb8\xad\xe6\x96\x87",
                                       "\xe7\xb9\x81\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87",
                                       "\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87",
                                       "\xe7\xae\x80\xe9\xab\x94\xe4\xb8\xad\xe6\x96\x87", NULL};
    if (!language || language->type != JSON_STRING) return json_null();
    char raw[512];
    trim_copy(language->str_val, raw, sizeof raw);
    if (!raw[0]) return json_null();

    char cand[512];
    int aliased = 0;
    for (int i = 0; alias_keys[i]; i++) {
        if (strcmp(raw, alias_keys[i]) == 0) {
            snprintf(cand, sizeof cand, "zh");
            aliased = 1;
            break;
        }
    }
    if (!aliased) {
        /* folded = raw.casefold().replace("_", "-"); candidate = folded.split("-",1)[0] */
        size_t n = strlen(raw);
        if (n >= sizeof cand) n = sizeof cand - 1;
        for (size_t i = 0; i < n; i++) {
            char c = raw[i];
            cand[i] = (c == '_') ? '-' : (char)tolower((unsigned char)c);
        }
        cand[n] = '\0';
        char *dash = strchr(cand, '-');
        if (dash) *dash = '\0';
    }

    /* candidate.isascii() and candidate.isalpha() and (len == 2 or in {haw,yue}) */
    size_t clen = strlen(cand);
    int shape = (clen == 2) || !strcmp(cand, "haw") || !strcmp(cand, "yue");
    int ok = shape && clen > 0;
    for (size_t i = 0; i < clen && ok; i++)
        if ((unsigned char)cand[i] >= 128 || !isalpha((unsigned char)cand[i])) ok = 0;
    if (!ok) return json_null();

    if (supported && supported->type == JSON_ARRAY) {
        int found = 0;
        for (size_t i = 0; i < json_len(supported); i++) {
            const struct json_t *e = json_get(supported, i);
            char vbuf[512], folded[512];
            const char *s = py_str(e, vbuf, sizeof vbuf);
            size_t m = strlen(s);
            if (m >= sizeof folded) m = sizeof folded - 1;
            for (size_t j = 0; j < m; j++) folded[j] = (char)tolower((unsigned char)s[j]);
            folded[m] = '\0';
            if (!strcmp(cand, folded)) { found = 1; break; }
        }
        if (!found) return json_null();
    }
    return json_string(cand);
}

struct json_t *w13_transcripti_f10ae7_normalize_local_model(const struct json_t *model_name) {
    if (!model_name || model_name->type != JSON_STRING || !model_name->str_val[0])
        return json_string("base"); /* DEFAULT_LOCAL_MODEL */
    const char *m = model_name->str_val;
    static const char *cloud_only[] = {
        "whisper-1", "gpt-4o-mini-transcribe", "gpt-4o-transcribe", "gpt-transcribe",
        "whisper-large-v3-turbo", "whisper-large-v3", "distil-whisper-large-v3-en", NULL};
    for (int i = 0; cloud_only[i]; i++)
        if (!strcmp(m, cloud_only[i])) return json_string("base");
    return json_string(m);
}

struct json_t *w13_transcripti_f10ae7_get_idle_unload_seconds(const struct json_t *cfg) {
    double v = config_number(cfg, "unload_after_idle_seconds", 0.0, 1);
    if (v < 0) v = 0;
    return json_number(v);
}

struct json_t *w13_transcripti_f10ae7_looks_like_cuda_lib_error(const struct json_t *exc_msg) {
    static const char *markers[] = {
        "libcublas", "libcudnn", "libcudart", "cannot be loaded", "cannot open shared object",
        "no kernel image is available", "CUBLAS_STATUS_NOT_SUPPORTED", "no CUDA-capable device",
        "CUDA driver version is insufficient", NULL};
    char buf[4096];
    const char *s = py_str(exc_msg, buf, sizeof buf);
    for (int i = 0; markers[i]; i++)
        if (strstr(s, markers[i])) return json_bool(true);
    return json_bool(false);
}

struct json_t *w13_transcripti_f10ae7_confidence_thresholds(const struct json_t *cfg) {
    double no_speech = config_number(cfg, "no_speech_prob_threshold", 0.6, 0);
    double logprob = config_number(cfg, "logprob_threshold", -1.0, 0);
    struct json_t *out = json_array();
    json_append(out, json_number(no_speech));
    json_append(out, json_number(logprob));
    return out;
}
