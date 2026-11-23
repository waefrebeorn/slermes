/*
 * session_images.c — C port of upstream Hermes hermes/gateway/session_images.py.
 *
 * PoP: w8_sessionimag_8b2bc8_attach @ gateway/session_images.py:attach_bytes
 * PoP: w8_sessionimag_8b2bc8_attach @ gateway/session_images.py:_stage
 *
 * Faithful port of the authority validation gate (param whitelist, string-only
 * values, subscription membership, authorize) and the pure staging core:
 * data-URL base64 decode with the 25 MiB cap, strict base64 decoding, image
 * extension sniff (filename hint else magic bytes incl. the RIFF/WEBP case),
 * and the extension->mime map. stage_image_bytes is injected with a
 * deterministic stand-in path/name so the result is reproducible.
 */
#include "noc_w8_sessionimag_8b2bc8.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMG_MAX_BYTES (25L * 1024L * 1024L)

static json_t *fail(const char *code) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "error", json_string(code));
    return o;
}

static json_t *success(json_t *result) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "result", result);
    return o;
}

static int b64val(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* Returns 1 ok, 0 invalid base64, -1 over the cap. */
static int decode_image_b64(const char *raw, unsigned char **out, size_t *outlen) {
    const char *s = raw ? raw : "";
    while (*s && isspace((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;

    const char *prefix = "data:image/";
    const char *p = s;
    size_t plen = strlen(prefix);
    if ((size_t)(e - s) >= plen && !memcmp(s, prefix, plen)) {
        const char *q = s + plen;
        while (q < e) {
            unsigned char c = (unsigned char)*q;
            if (isalnum(c) || c == '.' || c == '+' || c == '-') q++;
            else break;
        }
        if ((size_t)(e - q) >= 8 && !memcmp(q, ";base64,", 8)) p = q + 8;
        else p = s; /* regex did not match: use the whole trimmed string */
    }

    char *compact = (char *)malloc((size_t)(e - p) + 1);
    if (!compact) return 0;
    size_t m = 0;
    for (const char *q = p; q < e; q++)
        if (!isspace((unsigned char)*q)) compact[m++] = *q;
    compact[m] = '\0';

    size_t caplen = 4u * (size_t)((IMG_MAX_BYTES + 2) / 3);
    if (m > caplen) { free(compact); return -1; }
    if (m % 4 != 0) { free(compact); return 0; }
    size_t pad = 0;
    while (pad < m && compact[m - 1 - pad] == '=') pad++;
    if (pad > 2) { free(compact); return 0; }
    for (size_t i = 0; i < m - pad; i++) {
        if (compact[i] == '=' || b64val((unsigned char)compact[i]) < 0) {
            free(compact); return 0;
        }
    }

    size_t groups = m / 4;
    size_t olen = groups * 3 - pad;
    unsigned char *buf = (unsigned char *)malloc(olen ? olen : 1);
    if (!buf) { free(compact); return 0; }
    size_t oi = 0;
    for (size_t g = 0; g < groups; g++) {
        int v[4];
        for (int j = 0; j < 4; j++) {
            int x = b64val((unsigned char)compact[g * 4 + j]);
            v[j] = x < 0 ? 0 : x;
        }
        unsigned char b0 = (unsigned char)((v[0] << 2) | (v[1] >> 4));
        unsigned char b1 = (unsigned char)((v[1] << 4) | (v[2] >> 2));
        unsigned char b2 = (unsigned char)((v[2] << 6) | v[3]);
        if (g == groups - 1) {
            if (olen - oi >= 1) buf[oi++] = b0;
            if (olen - oi >= 1) buf[oi++] = b1;
            if (olen - oi >= 1) buf[oi++] = b2;
        } else {
            buf[oi++] = b0; buf[oi++] = b1; buf[oi++] = b2;
        }
    }
    free(compact);
    *out = buf; *outlen = olen;
    return 1;
}

/* Python: params.get('ext','').strip().lower().lstrip('.') */
static void norm_hint(const char *v, char *out, size_t n) {
    out[0] = '\0';
    if (!v) return;
    const char *s = v;
    while (*s && isspace((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    while (s < e && *s == '.') s++;
    size_t i = 0;
    for (; s < e && i + 1 < n; s++, i++) out[i] = (char)tolower((unsigned char)*s);
    out[i] = '\0';
}

/* Python _sniff_image_ext: filename suffix else magic bytes else ".png". */
static void sniff(const unsigned char *data, size_t dlen, const char *filename,
                  char *ext, size_t en) {
    const char *base = strrchr(filename, '/');
    base = base ? base + 1 : filename;
    const char *dot = strrchr(base, '.');
    if (dot && dot > base && dot[1]) {
        size_t i = 0;
        for (const char *p = dot; *p && i + 1 < en; p++, i++)
            ext[i] = (char)tolower((unsigned char)*p);
        ext[i] = '\0';
        return;
    }
    if (dlen >= 12 && !memcmp(data, "RIFF", 4) && !memcmp(data + 8, "WEBP", 4)) {
        snprintf(ext, en, ".webp"); return;
    }
    static const struct { const char *sig; size_t len; const char *ext; } M[] = {
        {"\x89PNG\r\n\x1a\n", 8, ".png"},
        {"\xff\xd8\xff", 3, ".jpg"},
        {"GIF87a", 6, ".gif"},
        {"GIF89a", 6, ".gif"},
        {"BM", 2, ".bmp"},
    };
    for (size_t i = 0; i < sizeof(M) / sizeof(M[0]); i++) {
        if (dlen >= M[i].len && !memcmp(data, M[i].sig, M[i].len)) {
            snprintf(ext, en, "%s", M[i].ext); return;
        }
    }
    snprintf(ext, en, ".png");
}

static const char *mime_for(const char *ext) {
    if (!strcmp(ext, ".png")) return "image/png";
    if (!strcmp(ext, ".jpg")) return "image/jpeg";
    if (!strcmp(ext, ".jpeg")) return "image/jpeg";
    if (!strcmp(ext, ".gif")) return "image/gif";
    if (!strcmp(ext, ".webp")) return "image/webp";
    return NULL;
}

json_t *w8_sessionimag_8b2bc8_attach(const json_t *in) {
    const json_t *sid = json_obj_get(in, "session_id");
    const json_t *params = json_obj_get(in, "params");

    if (!sid || sid->type != JSON_STRING || !sid->str_val || !sid->str_val[0])
        return fail("invalid_params");
    if (!params || params->type != JSON_OBJECT)
        return fail("invalid_params");

    static const char *allowed[] = {"session_id", "profile", "content_base64", "data", "filename", "ext"};
    for (size_t i = 0; i < json_object_size(params); i++) {
        const char *k = json_object_get_key_at(params, i);
        int ok = 0;
        for (size_t j = 0; j < sizeof(allowed) / sizeof(allowed[0]); j++)
            if (k && !strcmp(k, allowed[j])) { ok = 1; break; }
        if (!ok) return fail("invalid_params");
        json_t *v = json_object_get_at(params, i);
        if (!v || v->type != JSON_STRING) return fail("invalid_params");
    }

    if (!json_get_bool(in, "subscribed", false)) return fail("permission_denied");
    const char *ae = json_get_str(in, "authorize_error", "");
    if (ae && ae[0]) return fail(ae);

    const char *cb = json_get_str(params, "content_base64", "");
    const char *ds = json_get_str(params, "data", "");
    const char *raw = (cb && cb[0]) ? cb : ((ds && ds[0]) ? ds : "");
    unsigned char *data = NULL;
    size_t dlen = 0;
    int dec = decode_image_b64(raw, &data, &dlen);
    if (dec < 0 || !data || dlen == 0) { free(data); return fail("invalid_params"); }

    char hint[256];
    norm_hint(json_get_str(params, "ext", ""), hint, sizeof(hint));
    const char *fp = json_get_str(params, "filename", "");
    char filename[1024];
    if (fp && fp[0]) snprintf(filename, sizeof(filename), "%s", fp);
    else if (hint[0]) snprintf(filename, sizeof(filename), "x.%s", hint);
    else filename[0] = '\0';

    char ext[64];
    sniff(data, dlen, filename, ext, sizeof(ext));
    free(data);
    const char *mime = mime_for(ext);
    if (!mime) return fail("invalid_params");
    if (!json_get_bool(in, "storage_ok", true)) return fail("storage_unavailable");

    char name[128], path[256], text[512];
    snprintf(name, sizeof(name), "upload_stub%s", ext);
    snprintf(path, sizeof(path), "/stage/%s", name);
    snprintf(text, sizeof(text), "[User attached image: %s]", name);

    json_t *res = json_object();
    json_set(res, "attached", json_bool(true));
    json_set(res, "path", json_string(path));
    json_set(res, "mime", json_string(mime));
    json_set(res, "name", json_string(name));
    json_set(res, "count", json_number(1));
    json_set(res, "remainder", json_string(""));
    json_set(res, "text", json_string(text));
    json_set(res, "bytes", json_number((double)dlen));
    return success(res);
}
