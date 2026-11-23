/*
 * clipboard_cmd.c — C home for upstream Hermes hermes_cli/clipboard_cmd.py.
 *
 * PoP: w10_clipboardc_9f518c_mime @ hermes_cli/clipboard_cmd.py:_mime
 * PoP: w10_clipboardc_9f518c_exit_codes @ hermes_cli/clipboard_cmd.py:EXIT_OK
 *
 * Faithful C11 port of the pure magic-byte MIME detector and the module's
 * exit-code constants. The effectful clipboard extraction (filesystem /
 * platform clipboard tool) is deliberately out of scope for this pure gate.
 */
#include "noc_w10_clipboardc_9f518c.h"

#include "hermes_json.h"

#include <stddef.h>
#include <string.h>

#define EXIT_OK_VALUE           0
#define EXIT_NO_IMAGE_VALUE     1
#define EXIT_NO_TOOL_VALUE      3
#define EXIT_WRITE_FAILED_VALUE 4

struct w10_sig { const unsigned char *bytes; size_t len; const char *mime; };

static const struct w10_sig W10_SIGNATURES[] = {
    { (const unsigned char *)"\x89PNG\r\n\x1a\n", 8, "image/png" },
    { (const unsigned char *)"\xff\xd8\xff",      3, "image/jpeg" },
    { (const unsigned char *)"GIF8",             4, "image/gif" },
    { (const unsigned char *)"BM",               2, "image/bmp" },
    { (const unsigned char *)"II*\x00",          4, "image/tiff" },
    { (const unsigned char *)"MM\x00*",          4, "image/tiff" },
};
#define W10_SIG_COUNT (sizeof(W10_SIGNATURES) / sizeof(W10_SIGNATURES[0]))

static unsigned char w10_byte_at(const json_t *head, size_t i) {
    const json_t *v = json_get(head, i);
    if (!v || v->type != JSON_NUMBER) return 0;
    return (unsigned char)((long long)v->num_val & 0xFF);
}

struct json_t *w10_clipboardc_9f518c_mime(const struct json_t *head) {
    unsigned char buf[16] = {0};
    size_t n = (head && head->type == JSON_ARRAY) ? json_len(head) : 0;
    if (n > 16) n = 16;
    for (size_t i = 0; i < n; ++i) buf[i] = w10_byte_at(head, i);

    /* Python: head[:4] == b"RIFF" and head[8:12] == b"WEBP" */
    if (n >= 12 && memcmp(buf, "RIFF", 4) == 0 && memcmp(buf + 8, "WEBP", 4) == 0)
        return json_string("image/webp");

    for (size_t s = 0; s < W10_SIG_COUNT; ++s) {
        const struct w10_sig *sig = &W10_SIGNATURES[s];
        if (n >= sig->len && memcmp(buf, sig->bytes, sig->len) == 0)
            return json_string(sig->mime);
    }
    return json_string("application/octet-stream");
}

struct json_t *w10_clipboardc_9f518c_exit_codes(void) {
    json_t *out = json_object();
    json_set(out, "EXIT_OK", json_number(EXIT_OK_VALUE));
    json_set(out, "EXIT_NO_IMAGE", json_number(EXIT_NO_IMAGE_VALUE));
    json_set(out, "EXIT_NO_TOOL", json_number(EXIT_NO_TOOL_VALUE));
    json_set(out, "EXIT_WRITE_FAILED", json_number(EXIT_WRITE_FAILED_VALUE));
    return out;
}
