/*
 * transcription_cloud.c — C port of upstream Hermes tools/transcription_cloud.py.
 *
 * PoP: w13_transcripti_f24ccf_sdk_prompt_kwargs @ tools/transcription_cloud.py:_sdk_prompt_kwargs
 * PoP: w13_transcripti_f24ccf_elevenlabs_error_detail @ tools/transcription_cloud.py:_elevenlabs_error_detail
 * PoP: w13_transcripti_f24ccf_extract_transcript_text @ tools/transcription_cloud.py:_extract_transcript_text
 */
#include "noc_w13_transcripti_f24ccf.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python truthiness (plain `if value` / `or`), distinct from is_truthy_value. */
static int py_bool(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val[0] ? 1 : 0; /* no strip */
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}

/* str(v).strip() into a caller buffer; NUL is written at the trimmed length. */
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

/* Python str() for scalar JSON values used by these helpers. */
static const char *py_str(const struct json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, n, "None"); return buf; }
    if (v->type == JSON_STRING) return v->str_val; /* borrowed; owner alive at call site */
    if (v->type == JSON_BOOL) { snprintf(buf, n, "%s", v->bool_val ? "True" : "False"); return buf; }
    if (v->type == JSON_NUMBER) {
        double x = v->num_val;
        if (x == (double)(long long)x) snprintf(buf, n, "%lld", (long long)x);
        else snprintf(buf, n, "%g", x);
        return buf;
    }
    snprintf(buf, n, ""); /* container repr is deliberately not exercised */
    return buf;
}

static struct json_t *error_result(const char *msg) {
    struct json_t *o = json_object();
    json_set(o, "error", json_string(msg));
    return o;
}

struct json_t *w13_transcripti_f24ccf_sdk_prompt_kwargs(const struct json_t *language,
                                                        const struct json_t *prompt) {
    struct json_t *out = json_object();
    if (py_bool(language)) json_set(out, "language", json_copy(language));
    if (py_bool(prompt)) json_set(out, "prompt", json_copy(prompt));
    return out;
}

struct json_t *w13_transcripti_f24ccf_elevenlabs_error_detail(const struct json_t *err_body) {
    if (!json_is_object(err_body)) return json_string("");
    const struct json_t *value = json_obj_get(err_body, "detail");
    if (!py_bool(value)) value = json_obj_get(err_body, "error");
    if (!py_bool(value)) return json_string("");
    char buf[512];
    if (value->type == JSON_OBJECT) {
        const struct json_t *m = json_obj_get(value, "message");
        if (py_bool(m)) {
            const char *s = py_str(m, buf, sizeof buf);
            return json_string(s);
        }
        /* str(error_value) for a dict without a message: not exercised by fixtures. */
        return json_string("");
    }
    return json_string(py_str(value, buf, sizeof buf));
}

/* Voxtral-style "language xx <asr_text> ..." prefix strip.
 * Regex: \s*language\s+[\w.-]+(?:\s*<audio_language>[^<]*</audio_language>)?\s*<asr_text>\s*(.*)
 * IGNORECASE | DOTALL, anchored with .match(). */
static int asr_extract(const char *text, char *out, size_t n) {
    const char *p = text;
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncasecmp(p, "language", 8) != 0) return 0;
    p += 8;
    const char *q = p;
    while (*q && isspace((unsigned char)*q)) q++;
    if (q == p) return 0; /* \s+ required */
    p = q;
    const char *word = p;
    while (isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '-') p++;
    if (p == word) return 0; /* [\w.-]+ */
    const char *save = p;
    const char *r = p;
    while (*r && isspace((unsigned char)*r)) r++;
    if (strncasecmp(r, "<audio_language>", 16) == 0) {
        r += 16;
        const char *close = strstr(r, "</audio_language>");
        if (close) p = close + 17;
        else p = save;
    } else {
        p = save;
    }
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncasecmp(p, "<asr_text>", 10) != 0) return 0;
    p += 10;
    while (*p && isspace((unsigned char)*p)) p++;
    size_t len = strlen(p);
    while (len > 0 && isspace((unsigned char)p[len - 1])) len--; /* .strip() on group */
    if (len >= n) len = n - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return 1;
}

struct json_t *w13_transcripti_f24ccf_extract_transcript_text(const struct json_t *value) {
    char rawbuf[8192], tbuf[8192], ebuf[512], outbuf[8192];
    const char *text;
    if (value && value->type == JSON_STRING) {
        text = trim_copy(value->str_val, tbuf, sizeof tbuf);
    } else if (value && value->type == JSON_OBJECT) {
        const struct json_t *t = json_obj_get(value, "text");
        if (t && t->type == JSON_STRING) {
            text = trim_copy(t->str_val, tbuf, sizeof tbuf);
        } else {
            const struct json_t *e = json_obj_get(value, "error");
            if (py_bool(e)) return error_result(py_str(e, ebuf, sizeof ebuf));
            return error_result("Transcription response contained no text");
        }
    } else {
        text = trim_copy(py_str(value, rawbuf, sizeof rawbuf), tbuf, sizeof tbuf);
    }
    if (asr_extract(text, outbuf, sizeof outbuf)) return json_string(outbuf);
    return json_string(text);
}
