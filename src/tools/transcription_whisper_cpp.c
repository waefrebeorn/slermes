/*
 * transcription_whisper_cpp.c - C11 port of upstream tools/transcription_whisper_cpp.py
 * (gate w25_transcript_064b5f).
 *
 * PoP: w25_transcript_064b5f_quote @ tools/transcription_whisper_cpp.py:whisper_cpp_command.<locals>.quote
 * PoP: w25_transcript_064b5f_model_dir @ tools/transcription_whisper_cpp.py:_model_dir
 * PoP: w25_transcript_064b5f_command @ tools/transcription_whisper_cpp.py:whisper_cpp_command
 * PoP: w25_transcript_064b5f_validate_model @ tools/transcription_whisper_cpp.py:ensure_whisper_cpp_models
 * PoP: w25_transcript_064b5f_model_files @ tools/transcription_whisper_cpp.py:_MODEL_FILES
 *
 * The pm package manager, the profile home and the download are seams: a fixed
 * installed binary path and model directory go in, the command string comes
 * out.  What is ported is exactly what those values decide.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_transcript_064b5f.h"

#define TX_MODEL_COUNT 2
#define TX_MODEL_FIELDS 3

/* _MODEL_FILES: (name, url, sha256) in declaration order. */
static const char *const TX_MODEL_FILES[TX_MODEL_COUNT][TX_MODEL_FIELDS] = {
    {"ggml-base.bin",
     "https://huggingface.co/ggerganov/whisper.cpp/resolve/"
     "5359861c739e955e79d9a303bcbc70fb988958b1/ggml-base.bin",
     "60ed5bc3dd14eea856493d334349b405782ddcaf0028d4b5df4088345fba2efe"},
    {"ggml-silero-v6.2.0.bin",
     "https://huggingface.co/ggml-org/whisper-vad/resolve/"
     "9ffd54a1e1ee413ddf265af9913beaf518d1639b/ggml-silero-v6.2.0.bin",
     "2aa269b785eeb53a82983a20501ddf7c1d9c48e33ab63a41391ac6c9f7fb6987"},
};

static char *tx_dup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (n) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* Path(p).as_posix(): backslashes become separators, '' and '.' segments
 * vanish, '..' and the leading '/' are kept. */
static char *tx_posix(const char *p) {
    size_t n, i = 0, oi = 0, start, len;
    int rooted;
    char *tmp, *out;

    if (!p) p = "";
    n = strlen(p);
    tmp = (char *)malloc(n + 1);
    if (!tmp) return NULL;
    for (i = 0; i < n; i++) tmp[i] = p[i] == '\\' ? '/' : p[i];
    tmp[n] = '\0';

    rooted = (n > 0 && tmp[0] == '/');
    out = (char *)malloc(n + 2);
    if (!out) { free(tmp); return NULL; }
    if (rooted) out[oi++] = '/';
    i = 0;
    while (i < n) {
        while (i < n && tmp[i] == '/') i++;
        start = i;
        while (i < n && tmp[i] != '/') i++;
        len = i - start;
        if (len == 0 || (len == 1 && tmp[start] == '.')) continue;
        if (oi > (rooted ? 1u : 0u)) out[oi++] = '/';
        memcpy(out + oi, tmp + start, len);
        oi += len;
    }
    if (oi == 0) out[oi++] = '.';
    out[oi] = '\0';
    free(tmp);
    return out;
}

/* Path(a) / b, rendered as_posix().  An empty a leaves no leading '/'. */
static char *tx_join(const char *a, const char *b) {
    size_t na = strlen(a), nb = strlen(b);
    char *raw, *out;
    raw = (char *)malloc(na + nb + 2);
    if (!raw) return NULL;
    if (na) snprintf(raw, na + nb + 2, "%s/%s", a, b);
    else snprintf(raw, na + nb + 2, "%s", b);
    out = tx_posix(raw);
    free(raw);
    return out;
}

/* pathlib's "/" operator applied twice. */
static char *tx_join3(const char *a, const char *b, const char *c) {
    char *mid = tx_join(a, b);
    char *out;
    if (!mid) return NULL;
    out = tx_join(mid, c);
    free(mid);
    return out;
}

/* shlex.quote: only [_A-Za-z0-9@%+=:,./-] is safe; the empty string is ''. */
static int tx_safe_char(char c) {
    return isalnum((unsigned char)c) || strchr("_@%+=:,./-", c) != NULL;
}

static char *tx_shell_quote(const char *s) {
    size_t n, i, quotes = 0, oi = 0;
    int safe = 1;
    char *out;

    if (!s) s = "";
    n = strlen(s);
    if (n == 0) return tx_dup("''");
    for (i = 0; i < n; i++) {
        if (!tx_safe_char(s[i])) { safe = 0; break; }
    }
    if (safe) return tx_dup(s);
    for (i = 0; i < n; i++) if (s[i] == '\'') quotes++;
    out = (char *)malloc(n + quotes * 4 + 3);
    if (!out) return NULL;
    out[oi++] = '\'';
    for (i = 0; i < n; i++) {
        if (s[i] == '\'') {
            /* shlex.quote: ' -> '"'"' (close, double-quoted quote, reopen). */
            memcpy(out + oi, "'\"'\"'", 5);
            oi += 5;
        } else {
            out[oi++] = s[i];
        }
    }
    out[oi++] = '\'';
    out[oi] = '\0';
    return out;
}

/* Python repr() of the shapes a model_name can hold. */
static char *tx_repr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return tx_dup("None");
    switch (v->type) {
    case JSON_BOOL:
        return tx_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val &&
            v->num_val < 1e15 && v->num_val > -1e15)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return tx_dup(buf);
    case JSON_STRING: {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = strlen(s), i, oi = 0;
        char *out = (char *)malloc(n * 2 + 3);
        if (!out) return NULL;
        out[oi++] = '\'';
        for (i = 0; i < n; i++) {
            char c = s[i];
            if (c == '\\' || c == '\'') { out[oi++] = '\\'; out[oi++] = c; }
            else if (c == '\n') { out[oi++] = '\\'; out[oi++] = 'n'; }
            else if (c == '\t') { out[oi++] = '\\'; out[oi++] = 't'; }
            else if (c == '\r') { out[oi++] = '\\'; out[oi++] = 'r'; }
            else out[oi++] = c;
        }
        out[oi++] = '\'';
        out[oi] = '\0';
        return out;
    }
    case JSON_ARRAY:
        return tx_dup("[...]");
    case JSON_OBJECT:
        return tx_dup("{...}");
    default:
        break;
    }
    return tx_dup("None");
}

json_t *w25_transcript_064b5f_quote(const json_t *req) {
    const json_t *path = json_obj_get(req, "path");
    char *posix, *quoted;
    json_t *out;

    if (!path || path->type != JSON_STRING) return json_null();
    posix = tx_posix(path->str_val);
    quoted = posix ? tx_shell_quote(posix) : NULL;
    free(posix);
    if (!quoted) return json_null();
    out = json_string(quoted);
    free(quoted);
    return out;
}

json_t *w25_transcript_064b5f_model_dir(const json_t *req) {
    const char *home = json_get_str(req, "home", "");
    char *joined = tx_join3(home, "cache", "whisper.cpp");
    json_t *out;
    if (!joined) return json_null();
    out = json_string(joined);
    free(joined);
    return out;
}

json_t *w25_transcript_064b5f_command(const json_t *req) {
    const json_t *binary = json_obj_get(req, "binary");
    const char *models;
    char *bin_posix, *base_path, *vad_path, *bq, *mq, *vq, *buf;
    size_t need;
    json_t *out;

    if (!binary || binary->type == JSON_NULL) return json_null();
    if (binary->type != JSON_STRING) return json_null();
    models = json_get_str(req, "models_dir", "");
    bin_posix = tx_posix(binary->str_val ? binary->str_val : "");
    base_path = tx_join(models, "ggml-base.bin");
    vad_path = tx_join(models, "ggml-silero-v6.2.0.bin");
    bq = bin_posix ? tx_shell_quote(bin_posix) : NULL;
    mq = base_path ? tx_shell_quote(base_path) : NULL;
    vq = vad_path ? tx_shell_quote(vad_path) : NULL;
    if (!bq || !mq || !vq) {
        free(bin_posix); free(base_path); free(vad_path); free(bq); free(mq); free(vq);
        return json_null();
    }
    /* The command adapter fills {input_path}/{language}/{output_dir}; upstream
     * leaves those three placeholders literal here. */
    need = strlen(bq) + strlen(mq) + strlen(vq) + 192;
    buf = (char *)malloc(need);
    if (!buf) {
        free(bin_posix); free(base_path); free(vad_path);
        free(bq); free(mq); free(vq);
        return json_null();
    }
    snprintf(buf, need,
             "%s -m %s -f {input_path} -l {language} -t 4 -bs 5 -bo 5 -ng -nt -otxt "
             "-of {output_dir}/transcript --vad -vm %s -vsd 500",
             bq, mq, vq);
    out = json_string(buf);
    free(bin_posix); free(base_path); free(vad_path);
    free(bq); free(mq); free(vq);
    free(buf);
    return out;
}

json_t *w25_transcript_064b5f_validate_model(const json_t *req) {
    const json_t *name = json_obj_get(req, "model_name");
    char *rep, *buf;
    size_t need;
    json_t *out;

    if (name && name->type == JSON_STRING && strcmp(name->str_val, "base") == 0)
        return json_null();
    rep = tx_repr(name);
    if (!rep) return json_null();
    need = strlen(rep) + 112;
    buf = (char *)malloc(need);
    if (!buf) { free(rep); return json_null(); }
    snprintf(buf, need,
             "Windows ARM64 whisper.cpp currently supports stt.local.model: base; requested %s",
             rep);
    out = json_string(buf);
    free(buf);
    free(rep);
    return out;
}

json_t *w25_transcript_064b5f_model_files(void) {
    size_t i;
    json_t *out = json_array();
    for (i = 0; i < TX_MODEL_COUNT; i++) {
        json_t *row = json_object();
        json_set(row, "name", json_string(TX_MODEL_FILES[i][0]));
        json_set(row, "url", json_string(TX_MODEL_FILES[i][1]));
        json_set(row, "sha256", json_string(TX_MODEL_FILES[i][2]));
        json_append(out, row);
    }
    return out;
}
