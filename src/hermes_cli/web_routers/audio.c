/*
 * audio.c - C11 port of upstream hermes_cli/web_routers/audio.py
 * (gates w32_audio_a418b2_p1 and w32_audio_a418b2_p2).
 *
 * STATUS: behavior port.  Every HTTP cycle, temp file, provider round-trip and
 * socket in that module is a seam; this file owns only what the values those
 * seams return decide.
 *
 * PoP: w32_audio_a418b2_p1_extension_for_mime @ hermes_cli/web_routers/audio.py:_audio_extension_for_mime
 * PoP: w32_audio_a418b2_p1_transcribe         @ hermes_cli/web_routers/audio.py:transcribe_audio_upload
 * PoP: w32_audio_a418b2_p1_speak              @ hermes_cli/web_routers/audio.py:speak_text
 * PoP: w32_audio_a418b2_p2_voice_label        @ hermes_cli/web_routers/audio.py:_elevenlabs_voice_label
 * PoP: w32_audio_a418b2_p2_voices             @ hermes_cli/web_routers/audio.py:get_elevenlabs_voices
 * PoP: w32_audio_a418b2_p2_sdp_session        @ hermes_cli/web_routers/audio.py:create_voice_live_session
 * PoP: w32_audio_a418b2_p2_lease              @ hermes_cli/web_routers/audio.py:tts_lease
 *
 * Seams (supplied as fixture inputs, never ported): the temp file write and
 * its unlink, transcription/TTS provider calls, os.path.isfile, the upload
 * budget boundary, the ElevenLabs urllib fetch and its HTTP status, the
 * WebRTC session call and its ValueError/RuntimeError signal, the TTS
 * acquire/release call, the profile scope, the pydantic request-model default
 * (active=True) and client_error credential redaction (identity over the plain
 * messages these fixtures carry).
 *
 * Contract convention used throughout: a post-validation payload shape the
 * live route would have raised on answers {"status": 0, "error": "<ExcName>"}
 * -- the exception is the same seam an HTTP 500 would have been.
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_audio_a418b2_p1.h"
#include "noc_w32_audio_a418b2_p2.h"

/* ------------------------------------------------------------------ */
/* Python value semantics over a json_t tree                          */
/* ------------------------------------------------------------------ */

static char *au_dup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static int au_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static int au_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL)   { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* The string a JSON value carries, or NULL when it is not a string. */
static const char *au_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static int au_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip() over ASCII whitespace; never NULL. */
static char *au_strip_dup(const char *s) {
    size_t a = 0, b;
    char *p;
    if (!s) return au_dup("");
    b = strlen(s);
    while (a < b && au_is_space(s[a])) a++;
    while (b > a && au_is_space(s[b - 1])) b--;
    p = (char *)malloc(b - a + 1);
    if (!p) return au_dup("");
    if (b > a) memcpy(p, s + a, b - a);
    p[b - a] = '\0';
    return p;
}

/* Python str(value) for the scalar shapes these payloads carry.  Collections
 * are outside the fixtures' contract and answer "". */
static char *au_py_str(const json_t *v) {
    char buf[64];
    if (!v) return au_dup("");
    switch (v->type) {
    case JSON_STRING: return au_dup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return au_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == (double)(long long)d && d > -1e15 && d < 1e15)
            snprintf(buf, sizeof buf, "%lld", (long long)d);
        else
            snprintf(buf, sizeof buf, "%.15g", d);
        return au_dup(buf);
    }
    case JSON_NULL: return au_dup("");
    default: return au_dup("");
    }
}

static char *au_lower_dup(const char *s) {
    char *p = au_dup(s);
    size_t i;
    if (!p) return NULL;
    for (i = 0; p[i]; i++)
        if (p[i] >= 'A' && p[i] <= 'Z') p[i] = (char)(p[i] - 'A' + 'a');
    return p;
}

static int au_starts_with(const char *s, const char *prefix) {
    size_t n = strlen(prefix);
    return s && strncmp(s, prefix, n) == 0;
}

static int au_mem_contains(const char *hay, size_t hlen, const char *needle) {
    size_t n = strlen(needle), i;
    if (n == 0) return 1;
    if (!hay || hlen < n) return 0;
    for (i = 0; i + n <= hlen; i++)
        if (memcmp(hay + i, needle, n) == 0) return 1;
    return 0;
}

static char *au_cat(const char *a, const char *b) {
    size_t na = strlen(a ? a : ""), nb = strlen(b ? b : "");
    char *p = (char *)malloc(na + nb + 1);
    if (!p) return NULL;
    memcpy(p, a ? a : "", na);
    memcpy(p + na, b ? b : "", nb);
    p[na + nb] = '\0';
    return p;
}

/* {"status": 400|413|500, "detail": ...} */
static json_t *au_status(long code, const char *detail) {
    json_t *o = json_object();
    json_set(o, "status", json_number((double)code));
    json_set(o, "detail", json_string(detail ? detail : ""));
    return o;
}

/* {"status": 0, "error": "<ExceptionName>"} */
static json_t *au_raised(const char *exc) {
    json_t *o = json_object();
    json_set(o, "status", json_number(0.0));
    json_set(o, "error", json_string(exc));
    return o;
}

/* Copy an optional value into an object key, null when absent. */
static void au_set(json_t *obj, const char *key, const json_t *v) {
    json_set(obj, key, v ? json_copy(v) : json_null());
}

/* ------------------------------------------------------------------ */
/* base64 - Python's b64decode(validate=True) / b64encode             */
/* ------------------------------------------------------------------ */

static int au_b64_rank(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* Strict form: multiple of four, alphabet only, '=' solely as trailing pad. */
static int au_b64_check(const char *s, size_t n, size_t *out_len) {
    size_t i, pad = 0, data;
    if (n % 4) return 0;
    for (i = 0; i < n; i++) {
        if (s[i] == '=') { pad++; continue; }
        if (au_b64_rank(s[i]) < 0) return 0;
        if (pad) return 0;                    /* data after padding */
    }
    if (pad > 2) return 0;
    data = n - pad;
    if (pad && (data % 4) != (4 - pad)) return 0;
    *out_len = (n / 4) * 3 - pad;
    return 1;
}

static size_t au_b64_decode(const char *s, size_t n, unsigned char *out) {
    size_t i, o = 0;
    unsigned acc = 0, bits = 0;
    for (i = 0; i < n; i++) {
        int r;
        if (s[i] == '=') break;
        r = au_b64_rank(s[i]);
        if (r < 0) break;
        acc = (acc << 6) | (unsigned)r;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[o++] = (unsigned char)((acc >> bits) & 0xFFu);
        }
    }
    return o;
}

static const char AU_B64_ALPHA[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *au_b64_encode(const unsigned char *d, size_t n) {
    size_t olen = ((n + 2) / 3) * 4, i, o = 0;
    char *out = (char *)malloc(olen + 1);
    if (!out) return NULL;
    for (i = 0; i + 2 < n; i += 3) {
        unsigned v = ((unsigned)d[i] << 16) | ((unsigned)d[i + 1] << 8) | d[i + 2];
        out[o++] = AU_B64_ALPHA[(v >> 18) & 63];
        out[o++] = AU_B64_ALPHA[(v >> 12) & 63];
        out[o++] = AU_B64_ALPHA[(v >> 6) & 63];
        out[o++] = AU_B64_ALPHA[v & 63];
    }
    if (i < n) {
        unsigned v = (unsigned)d[i] << 16;
        int rem = (int)(n - i);
        if (rem == 2) v |= (unsigned)d[i + 1] << 8;
        out[o++] = AU_B64_ALPHA[(v >> 18) & 63];
        out[o++] = AU_B64_ALPHA[(v >> 12) & 63];
        out[o++] = rem == 2 ? AU_B64_ALPHA[(v >> 6) & 63] : '=';
        out[o++] = '=';
    }
    out[o] = '\0';
    return out;
}

/* ------------------------------------------------------------------ */
/* p1 - payload shaping, suffix derivation, size budget               */
/* ------------------------------------------------------------------ */

static const struct { const char *mime; const char *ext; } AU_MIME_EXT[] = {
    {"audio/aac", ".aac"},   {"audio/flac", ".flac"},  {"audio/m4a", ".m4a"},
    {"audio/mp3", ".mp3"},   {"audio/mp4", ".mp4"},    {"audio/mpeg", ".mp3"},
    {"audio/ogg", ".ogg"},   {"audio/wav", ".wav"},    {"audio/wave", ".wav"},
    {"audio/webm", ".webm"}, {"audio/x-m4a", ".m4a"},  {"audio/x-wav", ".wav"},
    {"video/webm", ".webm"},
};

json_t *w32_audio_a418b2_p1_extension_for_mime(const json_t *mime_type) {
    const char *raw;
    char *head = NULL, *trimmed = NULL, *norm = NULL;
    size_t i;
    json_t *out;

    if (au_truthy(mime_type) && mime_type->type != JSON_STRING)
        return au_raised("AttributeError");
    raw = au_str(mime_type);
    if (!raw) raw = "";

    /* (mime_type or "").split(";", 1)[0].strip().lower() */
    {
        const char *semi = strchr(raw, ';');
        size_t n = semi ? (size_t)(semi - raw) : strlen(raw);
        head = (char *)malloc(n + 1);
        if (!head) return json_string(".webm");
        memcpy(head, raw, n);
        head[n] = '\0';
    }
    trimmed = au_strip_dup(head);
    norm = au_lower_dup(trimmed);
    free(head);
    free(trimmed);

    out = json_string(".webm");
    for (i = 0; i < sizeof AU_MIME_EXT / sizeof AU_MIME_EXT[0]; i++) {
        if (norm && strcmp(norm, AU_MIME_EXT[i].mime) == 0) {
            json_free(out);
            out = json_string(AU_MIME_EXT[i].ext);
            break;
        }
    }
    free(norm);
    return out;
}

json_t *w32_audio_a418b2_p1_transcribe(const json_t *req) {
    const json_t *raw_url = json_obj_get(req, "data_url");
    const json_t *raw_mime = json_obj_get(req, "mime_type");
    const json_t *result = json_obj_get(req, "result");
    const json_t *raw_max = json_obj_get(req, "max_bytes");
    char *data_url = NULL, *sel = NULL, *mime = NULL, *norm = NULL;
    const char *comma;
    size_t dlen = 0, limit = (size_t)W32_AUDIO_A418B2_MAX_UPLOAD_BYTES;
    double dnum = 0.0;
    json_t *out = NULL;

    if (au_truthy(raw_url) && raw_url->type != JSON_STRING)
        return au_raised("AttributeError");
    data_url = au_strip_dup(au_str(raw_url));

    if (!au_starts_with(data_url, "data:")) {
        out = au_status(400, "Invalid audio payload");
        goto done;
    }
    comma = strchr(data_url, ',');
    if (!comma) {
        out = au_status(400, "Invalid audio payload");
        goto done;
    }
    if (!au_mem_contains(data_url, (size_t)(comma - data_url), ";base64")) {
        out = au_status(400, "Audio payload must be base64 encoded");
        goto done;
    }

    /* mime_type = (payload.mime_type or header[5:].split(";")[0] or "audio/webm").strip() */
    if (au_truthy(raw_mime)) {
        if (raw_mime->type != JSON_STRING) {
            out = au_raised("AttributeError");
            goto done;
        }
        sel = au_dup(raw_mime->str_val);
    } else {
        const char *p = data_url + 5;
        size_t plen = (size_t)(comma - p);
        const char *semi = (const char *)memchr(p, ';', plen);
        size_t part = semi ? (size_t)(semi - p) : plen;
        if (part == 0) {
            sel = au_dup("audio/webm");
        } else {
            sel = (char *)malloc(part + 1);
            if (sel) {
                memcpy(sel, p, part);
                sel[part] = '\0';
            }
        }
    }
    mime = au_strip_dup(sel);
    if (!mime) mime = au_dup("");
    free(sel);
    sel = NULL;

    /* normalized = mime_type.split(";", 1)[0].lower() -- no strip after split */
    {
        const char *semi = strchr(mime, ';');
        size_t n = semi ? (size_t)(semi - mime) : strlen(mime);
        norm = (char *)malloc(n + 1);
        if (norm) {
            memcpy(norm, mime, n);
            norm[n] = '\0';
            for (n = 0; norm[n]; n++)
                if (norm[n] >= 'A' && norm[n] <= 'Z') norm[n] = (char)(norm[n] - 'A' + 'a');
        }
    }
    if (!norm || !(au_starts_with(norm, "audio/") || strcmp(norm, "video/webm") == 0)) {
        out = au_status(400, "Payload must be an audio recording");
        goto done;
    }

    {
        const char *encoded = comma + 1;
        size_t elen = strlen(encoded);

        if (!au_b64_check(encoded, elen, &dlen)) {
            out = au_status(400, "Audio payload is not valid base64");
            goto done;
        }
        if (dlen == 0) {
            out = au_status(400, "Audio recording is empty");
            goto done;
        }
        if (raw_max && au_number(raw_max, &dnum) && dnum >= 0.0)
            limit = (size_t)dnum;
        if (dlen > limit) {
            out = au_status(413, "Audio recording is too large");
            goto done;
        }
    }

    if (!result || result->type != JSON_OBJECT) {
        out = au_raised("AttributeError");
        goto done;
    }

    if (!au_truthy(json_obj_get(result, "success"))) {
        const json_t *errv = json_obj_get(result, "error");
        char *err, *lower;
        if (au_truthy(errv) && errv->type != JSON_STRING) {
            out = au_raised("AttributeError");
            goto done;
        }
        err = au_dup(au_str(errv) ? au_str(errv) : "Transcription failed");
        lower = au_lower_dup(err);
        if (lower && strstr(lower, "empty transcript")) {
            out = json_object();
            json_set(out, "status", json_number(200.0));
            json_set(out, "ok", json_bool(1));
            json_set(out, "transcript", json_string(""));
            au_set(out, "provider", json_obj_get(result, "provider"));
        } else {
            out = au_status(400, err);
        }
        free(lower);
        free(err);
        goto done;
    }

    {
        const json_t *tv = json_obj_get(result, "transcript");
        char *raw_t = au_truthy(tv) ? au_py_str(tv) : au_dup("");
        char *transcript = au_strip_dup(raw_t);
        out = json_object();
        json_set(out, "status", json_number(200.0));
        json_set(out, "ok", json_bool(1));
        json_set(out, "transcript", json_string(transcript ? transcript : ""));
        au_set(out, "provider", json_obj_get(result, "provider"));
        free(raw_t);
        free(transcript);
    }

done:
    free(data_url);
    free(mime);
    free(norm);
    return out;
}

/* os.path.splitext(path)[1] for the shapes the TTS tool returns. */
static char *au_splitext_ext(const char *path) {
    const char *base = path, *p, *dot = NULL;
    for (p = path; *p; p++)
        if (*p == '/' || *p == '\\') base = p + 1;
    for (p = base; *p; p++)
        if (*p == '.') dot = p;
    if (!dot) return au_dup("");
    if (dot == base) return au_dup("");   /* ".bashrc" carries no extension */
    return au_dup(dot);
}

static const struct { const char *ext; const char *mime; } AU_SPEAK_MIME[] = {
    {".mp3", "audio/mpeg"}, {".ogg", "audio/ogg"}, {".opus", "audio/ogg"},
    {".wav", "audio/wav"},  {".flac", "audio/flac"},
};

json_t *w32_audio_a418b2_p1_speak(const json_t *req) {
    const json_t *raw_text = json_obj_get(req, "text");
    const json_t *tts = json_obj_get(req, "tts_result");
    const json_t *exists = json_obj_get(req, "file_exists");
    const json_t *raw_b64 = json_obj_get(req, "audio_b64");
    json_t *result = NULL, *out = NULL;
    int parsed = 0;
    char *text = NULL;

    if (au_truthy(raw_text) && raw_text->type != JSON_STRING)
        return au_raised("AttributeError");
    text = au_strip_dup(au_str(raw_text));
    if (!text[0]) {
        free(text);
        return au_status(400, "Text is required");
    }
    free(text);

    if (tts && tts->type == JSON_STRING) {
        result = json_parse(tts->str_val, NULL);
        parsed = 1;
        if (!result) return au_status(500, "Invalid TTS response");
    } else {
        result = (json_t *)tts;
    }
    if (!result || result->type != JSON_OBJECT) {
        if (parsed) json_free(result);
        return au_raised("AttributeError");
    }

    if (!au_truthy(json_obj_get(result, "success"))) {
        const json_t *errv = json_obj_get(result, "error");
        char *err = au_truthy(errv) ? au_py_str(errv) : NULL;
        out = au_status(400, err ? err : "Speech synthesis failed");
        free(err);
        goto done;
    }

    {
        const json_t *fpv = json_obj_get(result, "file_path");
        char *ext, *ext_low, *mime = NULL, *enc = NULL, *prefix = NULL, *data_url = NULL;
        const char *fp;
        unsigned char *bytes = NULL;
        size_t nbytes = 0, i;

        if (au_truthy(fpv) && (fpv->type == JSON_ARRAY || fpv->type == JSON_OBJECT)) {
            out = au_raised("TypeError");   /* os.path.isfile(<not a path>) */
            goto done;
        }
        fp = au_str(fpv);
        if (!fp || !fp[0] || !au_truthy(exists)) {
            out = au_status(500, "Audio file missing");
            goto done;
        }

        ext = au_splitext_ext(fp);
        ext_low = au_lower_dup(ext ? ext : "");
        free(ext);
        for (i = 0; i < sizeof AU_SPEAK_MIME / sizeof AU_SPEAK_MIME[0]; i++) {
            if (ext_low && strcmp(ext_low, AU_SPEAK_MIME[i].ext) == 0) {
                mime = au_dup(AU_SPEAK_MIME[i].mime);
                break;
            }
        }
        if (!mime) mime = au_dup("audio/mpeg");
        free(ext_low);

        /* File bytes are a fixture; base64-encode them as the route does. */
        {
            const char *b = au_str(raw_b64);
            size_t blen = b ? strlen(b) : 0;
            if (b && blen) {
                size_t dl = 0;
                if (au_b64_check(b, blen, &dl) && dl > 0) {
                    bytes = (unsigned char *)malloc(dl);
                    if (bytes) nbytes = au_b64_decode(b, blen, bytes);
                }
            }
        }
        enc = au_b64_encode(bytes, nbytes);
        prefix = au_cat("data:", mime ? mime : "");
        data_url = au_cat(prefix ? prefix : "", ";base64,");
        {
            char *full = au_cat(data_url ? data_url : "", enc ? enc : "");
            free(data_url);
            data_url = full;
        }

        out = json_object();
        json_set(out, "status", json_number(200.0));
        json_set(out, "ok", json_bool(1));
        json_set(out, "data_url", json_string(data_url ? data_url : ""));
        json_set(out, "mime_type", json_string(mime ? mime : ""));
        au_set(out, "provider", json_obj_get(result, "provider"));
        free(prefix);
        free(data_url);
        free(enc);
        free(mime);
        free(bytes);
    }

done:
    if (parsed && result) json_free(result);
    return out;
}

/* ------------------------------------------------------------------ */
/* p2 - contract: labels, voice list, SDP verdict, lease answer        */
/* ------------------------------------------------------------------ */

/* _elevenlabs_voice_label: "Name (category)" when a category is present. */
static char *au_voice_label_str(const json_t *voice) {
    const json_t *namev = json_obj_get(voice, "name");
    const json_t *idv = json_obj_get(voice, "voice_id");
    const json_t *catv = json_obj_get(voice, "category");
    char *name_raw, *name, *cat_raw, *cat, *out;

    if (au_truthy(namev)) name_raw = au_py_str(namev);
    else if (au_truthy(idv)) name_raw = au_py_str(idv);
    else name_raw = au_dup("Voice");
    name = au_strip_dup(name_raw);
    free(name_raw);

    cat_raw = au_truthy(catv) ? au_py_str(catv) : au_dup("");
    cat = au_strip_dup(cat_raw);
    free(cat_raw);

    if (cat && cat[0]) {
        char *mid = au_cat(name ? name : "", " (");
        char *tail = au_cat(mid ? mid : "", cat);
        out = au_cat(tail ? tail : "", ")");
        free(mid);
        free(tail);
    } else {
        out = au_dup(name ? name : "");
    }
    free(name);
    free(cat);
    return out;
}

json_t *w32_audio_a418b2_p2_voice_label(const json_t *voice) {
    char *label;
    json_t *out;
    if (!voice || voice->type != JSON_OBJECT) return au_raised("AttributeError");
    label = au_voice_label_str(voice);
    out = json_string(label ? label : "");
    free(label);
    return out;
}

json_t *w32_audio_a418b2_p2_voices(const json_t *req) {
    const json_t *payload = json_obj_get(req, "payload");
    const json_t *http = json_obj_get(req, "http_status");
    json_t *out, *voices;
    const json_t *raw;
    double code = 0.0;

    if (http && au_number(http, &code)) {
        if ((long)code == 401 || (long)code == 403) {
            out = json_object();
            json_set(out, "status", json_number(200.0));
            json_set(out, "available", json_bool(0));
            json_set(out, "voices", json_array());
            json_set(out, "error", json_string("unauthorized"));
            return out;
        }
        return au_status(502, "Could not load ElevenLabs voices");
    }

    if (!payload || payload->type != JSON_OBJECT) return au_raised("AttributeError");

    voices = json_array();
    raw = json_obj_get(payload, "voices");
    if (au_truthy(raw)) {
        if (raw->type == JSON_NUMBER || raw->type == JSON_BOOL) {
            json_free(voices);
            return au_raised("TypeError");   /* for voice in <number> */
        }
        if (raw->type == JSON_ARRAY) {
            size_t n = json_len(raw), i;
            json_t **items = (json_t **)calloc(n ? n : 1, sizeof(json_t *));
            char **keys = (char **)calloc(n ? n : 1, sizeof(char *));
            size_t have = 0;
            if (!items || !keys) {
                free(items);
                free(keys);
                json_free(voices);
                return au_raised("MemoryError");
            }
            for (i = 0; i < n; i++) {
                const json_t *voice = json_get(raw, i);
                const json_t *idv;
                char *id_raw, *id, *name_raw, *label, *lkey;
                json_t *item;
                if (!voice || voice->type != JSON_OBJECT) continue;
                idv = json_obj_get(voice, "voice_id");
                id_raw = au_truthy(idv) ? au_py_str(idv) : au_dup("");
                id = au_strip_dup(id_raw);
                free(id_raw);
                if (!id[0]) { free(id); continue; }

                name_raw = au_truthy(json_obj_get(voice, "name"))
                               ? au_py_str(json_obj_get(voice, "name"))
                               : au_dup(id);
                label = au_voice_label_str(voice);
                lkey = au_lower_dup(label ? label : "");

                item = json_object();
                json_set(item, "voice_id", json_string(id));
                json_set(item, "name", json_string(name_raw ? name_raw : ""));
                json_set(item, "label", json_string(label ? label : ""));
                items[have] = item;
                keys[have] = lkey;
                have++;
                free(id);
                free(name_raw);
                free(label);
            }
            /* Python's sort is stable: insertion sort keeps ties in input order. */
            for (i = 1; i < have; i++) {
                size_t j = i;
                while (j > 0 && keys[j - 1] && keys[j] && strcmp(keys[j - 1], keys[j]) > 0) {
                    json_t *ti = items[j - 1]; items[j - 1] = items[j]; items[j] = ti;
                    char *tk = keys[j - 1]; keys[j - 1] = keys[j]; keys[j] = tk;
                    j--;
                }
            }
            for (i = 0; i < have; i++) {
                json_append(voices, items[i]);
                free(keys[i]);
            }
            free(items);
            free(keys);
        }
        /* A string iterates to chars and an object to its keys: no dicts, none kept. */
    }

    out = json_object();
    json_set(out, "status", json_number(200.0));
    json_set(out, "available", json_bool(1));
    json_set(out, "voices", voices);
    return out;
}

json_t *w32_audio_a418b2_p2_sdp_session(const json_t *req) {
    const json_t *raw_sdp = json_obj_get(req, "sdp");
    const json_t *kind = json_obj_get(req, "error_kind");
    const json_t *result = json_obj_get(req, "result");
    char *sdp, *trimmed;
    json_t *out;

    if (au_truthy(raw_sdp) && raw_sdp->type != JSON_STRING) return au_raised("AttributeError");
    sdp = au_dup(au_str(raw_sdp) ? au_str(raw_sdp) : "");
    trimmed = au_strip_dup(sdp);
    if (!trimmed[0]) {
        free(sdp);
        free(trimmed);
        return au_status(400, "An SDP offer is required");
    }
    free(sdp);
    free(trimmed);

    if (kind && kind->type == JSON_STRING) {
        if (strcmp(kind->str_val, "value") == 0)
            return au_status(503, au_str(json_obj_get(req, "error_text")));
        if (strcmp(kind->str_val, "runtime") == 0)
            return au_status(502, au_str(json_obj_get(req, "error_text")));
    }

    if (!result || result->type != JSON_OBJECT) return au_raised("TypeError");

    out = json_object();
    json_set(out, "status", json_number(200.0));
    json_set(out, "ok", json_bool(1));
    {
        size_t i, n = json_object_size(result);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(result, i);
            if (!k) continue;
            json_set(out, k, json_copy(json_object_get_at(result, i)));
        }
    }
    return out;
}

json_t *w32_audio_a418b2_p2_lease(const json_t *req) {
    const json_t *raw_lease = json_obj_get(req, "lease");
    const json_t *active = json_obj_get(req, "active");
    const json_t *result = json_obj_get(req, "result");
    char *lease, *trimmed;
    json_t *out, *merged = NULL;

    if (au_truthy(raw_lease) && raw_lease->type != JSON_STRING) return au_raised("AttributeError");
    lease = au_dup(au_str(raw_lease) ? au_str(raw_lease) : "");
    trimmed = au_strip_dup(lease);
    free(lease);
    if (!trimmed[0]) {
        free(trimmed);
        return au_status(400, "lease is required");
    }

    if (!result || result->type == JSON_NULL) {
        /* Warm-up failure seam: the tool raised, the route reports it in-body. */
        merged = json_object();
        json_set(merged, "leases", json_null());
        json_set(merged, "action", json_string("error"));
        {
            const char *msg = au_str(json_obj_get(req, "error_text"));
            json_set(merged, "error", json_string(msg ? msg : ""));
        }
    } else if (result->type != JSON_OBJECT) {
        free(trimmed);
        return au_raised("AttributeError");
    } else {
        merged = json_copy(result);
        {
            const json_t *errv = json_obj_get(result, "error");
            if (au_truthy(errv)) {
                char *s = au_py_str(errv);   /* client_error() == str() + redaction */
                json_set(merged, "error", json_string(s ? s : ""));
                free(s);
            }
        }
    }

    out = json_object();
    json_set(out, "status", json_number(200.0));
    json_set(out, "ok", json_bool(1));
    json_set(out, "lease", json_string(trimmed));
    json_set(out, "active", active ? json_copy(active) : json_bool(1));
    {
        size_t i, n = json_object_size(merged);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(merged, i);
            if (!k) continue;
            json_set(out, k, json_copy(json_object_get_at(merged, i)));
        }
    }
    free(merged);
    free(trimmed);
    return out;
}
