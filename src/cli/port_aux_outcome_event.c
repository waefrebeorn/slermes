/*
 * port_aux_outcome_event.c — pure helpers (see header).
 */

#include "port_aux_outcome_event.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ---- clean_attachment_filename ---- */

/* Drop a leading Unicode control (Cc) or format (Cf) codepoint from *p,
 * advancing the cursor. Returns the number of bytes consumed (0 = keep). */
static size_t eve_drop_cccf(const unsigned char *p)
{
    unsigned char c = p[0];
    unsigned long cp;
    if (c < 0x80) {
        if (c < 0x20 || c == 0x7f) return 1; /* ASCII Cc */
        return 0;
    }
    /* decode up-to-3-byte UTF-8 (surrogate/4-byte not needed for Cc/Cf here) */
    if (c >= 0xC2 && c <= 0xDF && (p[1] & 0xC0) == 0x80) {
        cp = ((unsigned long)(c & 0x1F) << 6) | (p[1] & 0x3F);
    } else if (c >= 0xE0 && c <= 0xEF && (p[1] & 0xC0) == 0x80 &&
               (p[2] & 0xC0) == 0x80) {
        cp = ((unsigned long)(c & 0x0F) << 12) |
             ((unsigned long)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    } else {
        return 0;
    }
    /* Cf (format) set commonly found in filenames: soft hyphen, zero-widths,
     * bidi controls, word joins, FEFF; Cc controls here are ASCII (<0x80). */
    if (cp == 0x00AD || cp == 0xFEFF) return (c >= 0xE0) ? 3 : 2;
    if (cp >= 0x200B && cp <= 0x200F) return 3;
    if (cp >= 0x202A && cp <= 0x202E) return 3;
    if (cp >= 0x2060 && cp <= 0x2069) return 3;
    return 0;
}

char *eve_clean_attachment_filename(const char *name)
{
    const char *s = name ? name : "";
    size_t len = strlen(s), last = len, i = 0;
    char *buf;
    size_t blen = 0, bcap;

    (void)i;
    /* base = name.replace("\\","/").rsplit("/",1)[-1] — the text after the last '/' */
    for (size_t k = 0; k < len; k++)
        if (s[k] == '\\' || s[k] == '/') last = k;
    s = s + (last == len ? 0 : last + 1);
    len = strlen(s);

    /* drop Cc/Cf chars into buf */
    bcap = len + 1;
    buf = malloc(bcap);
    if (!buf) return NULL;
    {
        size_t k = 0;
        while (k < len) {
            const unsigned char *p = (const unsigned char *)(s + k);
            size_t drop = eve_drop_cccf(p);
            if (drop && k + drop <= len) { k += drop; continue; }
            buf[blen++] = s[k++];
        }
    }
    buf[blen] = '\0';
    /* .strip() */
    while (blen > 0 && isspace((unsigned char)buf[blen - 1])) blen--;
    {
        size_t b0 = 0;
        while (b0 < blen && isspace((unsigned char)buf[b0])) b0++;
        memmove(buf, buf + b0, blen - b0);
        blen -= b0;
        buf[blen] = '\0';
    }
    if (blen == 0 || (blen == 1 && buf[0] == '.') ||
        (blen == 2 && buf[0] == '.' && buf[1] == '.')) {
        buf[0] = '\0';
        return buf;
    }

    /* cap to _MAX_FILENAME_CHARS = 255, keeping the extension */
    if (blen > 255) {
        const char *dot = NULL;
        size_t stem_len, ext_len, keep = 0, dlen;
        for (size_t k = 0; k < blen; k++)
            if (buf[k] == '.') dot = buf + k;
        if (dot) {
            dlen = 1; /* the "." */
            ext_len = blen - (size_t)(dot - buf) - 1;
            stem_len = (size_t)(dot - buf);
            if (stem_len > 0 && ext_len <= 16)
                keep = dlen + ext_len; /* f"{dot}{ext}" */
        }
        if (keep > 0) {
            memmove(buf + (255 - keep), buf + blen - keep, keep);
            blen = 255;
        } else {
            blen = 255;
        }
        buf[blen] = '\0';
    }
    return buf;
}

/* ---- _command_text ---- */

static int eve_ref_at(const char *s)
{
    /* returns content-start offset (index after ":") of a leading
     * @image:/@file:/@url: ref with >=1 non-newline char, else -1 */
    static const char *const tags[] = {"@image:", "@file:", "@url:"};
    for (int i = 0; i < 3; i++) {
        size_t t = strlen(tags[i]);
        if (strncasecmp(s, tags[i], t) == 0) {
            if (s[t] == '\0' || s[t] == '\n') return -1; /* no content */
            return (int)t;
        }
    }
    return -1;
}

char *eve_command_text(const char *text)
{
    const char *s = text ? text : "";
    size_t len = strlen(s), i = 0, j = 0;
    char *out;
    /* (text or "").lstrip() */
    while (i < len && isspace((unsigned char)s[i])) i++;
    /* strip leading @image:/@file:/@url: refs, one-or-more (each tag:content\n?) */
    while (i < len) {
        int ci = eve_ref_at(s + i);
        if (ci < 0) break;
        i += (size_t)ci;                    /* past the tag: */
        while (i < len && s[i] != '\n') i++; /* consume content */
        if (i < len && s[i] == '\n') i++;    /* optional trailing newline */
    }
    /* final .lstrip() */
    while (i < len && isspace((unsigned char)s[i])) i++;
    out = malloc(len - i + 1);
    if (!out) return NULL;
    for (j = 0; i < len; i++) out[j++] = s[i];
    out[j] = '\0';
    return out;
}

/* ---- embedded_provider_error ---- */

char *aco_embedded_provider_error(const char *response_json)
{
    json_t *resp = response_json ? json_parse(response_json, NULL) : NULL;
    json_t *err = NULL;
    char *out = NULL;
    if (!resp) return NULL;
    if (resp->type == JSON_OBJECT)
        err = json_obj_get(resp, "error");
    /* dict error -> it (non-empty), else None */
    if (err && err->type == JSON_OBJECT && err->c.count > 0)
        out = json_serialize(err);
    json_free(resp);
    return out;
}

/* ---- is_cancellation ---- */

int aco_is_cancellation(int is_exception, int is_interrupted)
{
    return (!is_exception) || is_interrupted;
}
