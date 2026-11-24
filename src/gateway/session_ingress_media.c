/*
 * session_ingress_media.c — C port of upstream Hermes gateway/session_ingress_media.py.
 *
 * PoP: w17_sessioning_a01966_sniff_image_mime @ gateway/session_ingress_media.py:sniff_image_mime
 * PoP: w17_sessioning_a01966_admission_media_references @ gateway/session_ingress_media.py:admission_media_references
 */
#include "noc_w17_sessioning_a01966.h"
#include "hermes_json.h"
#include <string.h>

static int s_hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decode a hex string into out (up to cap bytes); return the byte count. */
static size_t s_hexdecode(const char *hex, unsigned char *out, size_t cap) {
    size_t n = 0;
    if (!hex) return 0;
    while (hex[0] && hex[1] && n < cap) {
        int hi = s_hexval(hex[0]), lo = s_hexval(hex[1]);
        if (hi < 0 || lo < 0) break;
        out[n++] = (unsigned char)((hi << 4) | lo);
        hex += 2;
    }
    return n;
}

const char *w17_sessioning_a01966_sniff_image_mime(const char *data_hex) {
    unsigned char b[16] = {0};
    size_t n = s_hexdecode(data_hex ? data_hex : "", b, sizeof(b));
    /* PNG: b'\x89PNG\r\n\x1a\n' */
    if (n >= 8 && b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G' &&
        b[4] == 0x0d && b[5] == 0x0a && b[6] == 0x1a && b[7] == 0x0a)
        return "image/png";
    /* JPEG: b'\xff\xd8\xff' */
    if (n >= 3 && b[0] == 0xff && b[1] == 0xd8 && b[2] == 0xff)
        return "image/jpeg";
    /* GIF: b'GIF87a' or b'GIF89a' */
    if (n >= 6 && b[0] == 'G' && b[1] == 'I' && b[2] == 'F' && b[3] == '8' &&
        b[4] == '7' && b[5] == 'a' ||
        n >= 6 && b[0] == 'G' && b[1] == 'I' && b[2] == 'F' && b[3] == '8' &&
        b[4] == '9' && b[5] == 'a')
        return "image/gif";
    /* WEBP: data[:4] == b'RIFF' and data[8:12] == b'WEBP' */
    if (n >= 12 && b[0] == 'R' && b[1] == 'I' && b[2] == 'F' && b[3] == 'F' &&
        b[8] == 'W' && b[9] == 'E' && b[10] == 'B' && b[11] == 'P')
        return "image/webp";
    return NULL;
}

json_t *w17_sessioning_a01966_admission_media_references(const json_t *payload) {
    json_t *out = json_array();
    static const char *SECTIONS[2] = {"attachments_v1", "native_text_v1"};
    for (size_t s = 0; s < 2; s++) {
        const json_t *sec = payload ? json_obj_get(payload, SECTIONS[s]) : NULL;
        if (!sec || sec->type != JSON_OBJECT) continue;
        const json_t *media = json_obj_get(sec, "media");
        if (!media || media->type != JSON_ARRAY) continue;
        for (size_t i = 0; i < media->c.count; i++)
            json_append(out, json_copy(media->c.items[i]));
    }
    return out;
}
