/*
 * strip_ansi.c — Strip ANSI escape sequences (ECMA-48) from text.
 * Port of Python tools/strip_ansi.py.
 *
 * State machine parsing covers:
 *   - CSI sequences: ESC [ params* intermediates* final-byte
 *   - OSC sequences: ESC ] ... (BEL or ESC \ terminator)
 *   - DCS/SOS/PM/APC: ESC P/X/^/_ ... ESC \
 *   - nF (2-byte): ESC 0x20-0x2F + 0x30-0x7E
 *   - Fp/Fe/Fs (1-byte): ESC 0x30-0x7E
 *   - 8-bit CSI (0x9B): params* intermediates* final-byte
 *   - 8-bit OSC (0x9D): ... BEL or 0x9C
 *   - C1 controls (0x80-0x9F): single-byte (except 0x9B, 0x9D handled above)
 *
 * MIT License — WuBu Hermes Project
 */

#include "ansi_strip.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* --- State machine states --- */
typedef enum {
    ST_NORMAL,         /* Copying normal text */
    ST_ESC,            /* Saw ESC (0x1B) — expecting next byte */
    ST_CSI_PARAMS,     /* ESC [ ... — collecting parameter bytes */
    ST_CSI_INTERMED,   /* ESC [ params? intermediates? — before final byte */
    ST_OSC,            /* ESC ] ... — collecting OSC string */
    ST_STRING_ESC,     /* ESC P/X/^/_ ... — collecting DCS/SOS/PM/APC string */
    ST_NF_PARAM,       /* ESC 0x20-0x2F ... — nF intermediate bytes */
    ST_C1_CSI,         /* 0x9B ... — 8-bit CSI params/intermediates/final */
    ST_C1_OSC,         /* 0x9D ... — 8-bit OSC, awaiting terminator */
} strip_state_t;

/* --- Byte classification helpers --- */

static inline bool is_csi_param(uint8_t b) {
    return b >= 0x30 && b <= 0x3F;  /* digits, ; : < = > ? */
}

static inline bool is_intermediate(uint8_t b) {
    return b >= 0x20 && b <= 0x2F;  /* space through / */
}

static inline bool is_final_byte(uint8_t b) {
    return b >= 0x40 && b <= 0x7E;  /* @ through ~ */
}

static inline bool is_escape(const char *s, size_t pos, size_t len) {
    /* Check for ESC within the string */
    if (pos < len && (unsigned char)s[pos] == 0x1B) return true;
    return false;
}

/* --- Fast-path check --- */
bool ansi_has_escape(const char *text) {
    if (!text) return false;
    for (const char *p = text; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == 0x1B || (c >= 0x80 && c <= 0x9F)) return true;
    }
    return false;
}

/* --- Main strip function (into caller buffer) --- */
char *ansi_strip_buf(const char *text, char *buf, size_t buf_size) {
    if (!text || !buf || buf_size == 0) return NULL;

    /* Fast path: no escape bytes, just copy */
    if (!ansi_has_escape(text)) {
        size_t len = strlen(text);
        if (len >= buf_size) {
            if (buf_size > 0) buf[0] = '\0';
            return NULL;
        }
        memcpy(buf, text, len + 1);
        return buf;
    }

    /* State machine strip */
    strip_state_t state = ST_NORMAL;
    size_t src_i = 0;
    size_t dst_i = 0;
    size_t text_len = strlen(text);

    while (src_i < text_len && dst_i < buf_size - 1) {
        unsigned char c = (unsigned char)text[src_i];

        switch (state) {
            case ST_NORMAL:
                if (c == 0x1B) {
                    state = ST_ESC;
                    src_i++;
                } else if (c == 0x9B) {
                    /* 8-bit CSI */
                    state = ST_C1_CSI;
                    src_i++;
                } else if (c == 0x9D) {
                    /* 8-bit OSC */
                    state = ST_C1_OSC;
                    src_i++;
                } else if (c >= 0x80 && c <= 0x9F) {
                    /* Other 8-bit C1 controls — skip single byte */
                    src_i++;
                } else {
                    buf[dst_i++] = (char)c;
                    src_i++;
                }
                break;

            case ST_ESC:
                /* After ESC: dispatch on next byte */
                if (c == '[') {
                    state = ST_CSI_PARAMS;
                } else if (c == ']') {
                    state = ST_OSC;
                } else if (c == 'P' || c == 'X' || c == '^' || c == '_') {
                    /* DCS (P), SOS (X), PM (^), APC (_) */
                    state = ST_STRING_ESC;
                } else if (c >= 0x20 && c <= 0x2F) {
                    /* nF escape — one or more intermediate bytes */
                    state = ST_NF_PARAM;
                } else {
                    /* Fp/Fe/Fs: ESC followed by a single byte 0x30-0x7E,
                     * or any other byte — consume ESC and this byte,
                     * but only count it as consumed if it's in range.
                     * If not, it's data loss, but strip_ansi is lossy by design. */
                    state = ST_NORMAL;
                }
                src_i++;
                break;

            case ST_CSI_PARAMS:
                if (is_csi_param(c)) {
                    src_i++; /* still params */
                } else if (is_intermediate(c)) {
                    state = ST_CSI_INTERMED;
                    src_i++;
                } else if (is_final_byte(c)) {
                    state = ST_NORMAL; /* CSI complete */
                    src_i++;
                } else {
                    /* Invalid byte — abort sequence, start fresh */
                    state = ST_NORMAL;
                }
                break;

            case ST_CSI_INTERMED:
                if (is_intermediate(c)) {
                    src_i++; /* still intermediates */
                } else if (is_final_byte(c)) {
                    state = ST_NORMAL; /* CSI complete */
                    src_i++;
                } else {
                    /* Invalid — abort */
                    state = ST_NORMAL;
                }
                break;

            case ST_OSC:
                /* Collect until BEL (0x07) or ESC \ (ST) */
                if (c == 0x07) {
                    state = ST_NORMAL; /* BEL terminator */
                    src_i++;
                } else if (c == 0x1B) {
                    /* Could be ST (ESC \) or embedded escape */
                    /* Peek forward for \ (0x5C) */
                    if (src_i + 1 < text_len && text[src_i + 1] == '\\') {
                        state = ST_NORMAL; /* ST terminator: ESC \ */
                        src_i += 2; /* skip ESC + \ */
                    } else {
                        /* Embedded escape inside OSC — skip */
                        src_i++;
                    }
                } else {
                    src_i++; /* regular OSC content, skip */
                }
                break;

            case ST_STRING_ESC:
                /* DCS/SOS/PM/APC: skip until ESC \ */
                if (c == 0x1B) {
                    if (src_i + 1 < text_len && text[src_i + 1] == '\\') {
                        state = ST_NORMAL; /* ST terminator */
                        src_i += 2;
                    } else {
                        src_i++;
                    }
                } else {
                    src_i++;
                }
                break;

            case ST_NF_PARAM:
                /* nF: one or more 0x20-0x2F bytes, then 0x30-0x7E */
                if (c >= 0x20 && c <= 0x2F) {
                    src_i++; /* still intermediates */
                } else if (c >= 0x30 && c <= 0x7E) {
                    state = ST_NORMAL; /* complete */
                    src_i++;
                } else {
                    /* Invalid byte — abort */
                    state = ST_NORMAL;
                }
                break;

            case ST_C1_CSI:
                /* 8-bit CSI: params* intermediates* final-byte */
                if (is_csi_param(c)) {
                    src_i++;
                } else if (is_intermediate(c)) {
                    src_i++;
                } else if (is_final_byte(c)) {
                    state = ST_NORMAL;
                    src_i++;
                } else {
                    state = ST_NORMAL;
                }
                break;

            case ST_C1_OSC:
                /* 8-bit OSC: skip until BEL or 0x9C (ST) */
                if (c == 0x07 || c == 0x9C) {
                    state = ST_NORMAL;
                    src_i++;
                } else {
                    src_i++;
                }
                break;

            default:
                /* Should never reach — safety reset */
                state = ST_NORMAL;
                break;
        }
    }

    buf[dst_i] = '\0';
    return buf;
}

/* --- Allocating wrapper --- */
char *strip_ansi(const char *text) {
    if (!text) return NULL;
    size_t len = strlen(text);
    char *buf = (char *)malloc(len + 1);
    if (!buf) return NULL;
    return ansi_strip_buf(text, buf, len + 1);
}

/* --- Unicode TAG stripping (UTF-8 aware) --- */

/* Decode the codepoint at s[0..] (UTF-8). Sets *bytes to the UTF-8 length
 * (1-4) and returns the codepoint, or (uint32_t)-1 on invalid leading byte. */
static uint32_t utf8_decode(const unsigned char *s, size_t avail, size_t *bytes) {
    unsigned char b0 = s[0];
    if (b0 < 0x80) { *bytes = 1; return b0; }
    if ((b0 & 0xE0) == 0xC0) {
        if (avail < 2) { *bytes = 1; return (uint32_t)-1; }
        *bytes = 2;
        return ((uint32_t)(b0 & 0x1F) << 6) | (s[1] & 0x3F);
    }
    if ((b0 & 0xF0) == 0xE0) {
        if (avail < 3) { *bytes = 1; return (uint32_t)-1; }
        *bytes = 3;
        return ((uint32_t)(b0 & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    }
    if ((b0 & 0xF8) == 0xF0) {
        if (avail < 4) { *bytes = 1; return (uint32_t)-1; }
        *bytes = 4;
        return ((uint32_t)(b0 & 0x07) << 18) | ((uint32_t)(s[1] & 0x3F) << 12) |
               ((uint32_t)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    }
    *bytes = 1;
    return (uint32_t)-1;
}

/* Fast-path check: any plane-14 tag character (U+E0000–U+E007F).
 * These are 4-byte UTF-8 sequences with lead 0xF3 and second byte 0xA0. */
static bool has_unicode_tag(const char *text) {
    if (!text) return false;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (*p == 0xF3 && p[1] == 0xA0) return true;
    }
    return false;
}


char *strip_unicode_tags_buf(const char *text, char *buf, size_t buf_size) {
    if (!text || !buf || buf_size == 0) return NULL;
    if (!has_unicode_tag(text)) {
        size_t len = strlen(text);
        if (len >= buf_size) { buf[0] = '\0'; return NULL; }
        memcpy(buf, text, len + 1);
        return buf;
    }
    const unsigned char *src = (const unsigned char *)text;
    size_t srclen = strlen(text);
    size_t si = 0, di = 0;
    while (si < srclen && di < buf_size - 1) {
        size_t blen;
        uint32_t cp = utf8_decode(src + si, srclen - si, &blen);
        if (cp == (uint32_t)-1) { /* invalid byte — copy verbatim */ }
        if (cp == 0x1F3F4) {
            /* Potential emoji tag sequence: U+1F3F4 + U+E0020..U+E007E{1,} + U+E007F */
            size_t scan = si + blen;
            bool valid = false;
            if (scan < srclen) {
                size_t probe = scan;
                /* first spec char must be in range */
                size_t spec_start = probe;
                while (probe < srclen) {
                    size_t pb; uint32_t pcp = utf8_decode(src + probe, srclen - probe, &pb);
                    if (pcp >= 0xE0020 && pcp <= 0xE007E) { probe += pb; continue; }
                    if (pcp == 0xE007F) { valid = (probe > spec_start); break; }
                    break;
                }
                (void)spec_start;
            }
            if (valid) {
                /* emit base + spec + cancel tag verbatim */
                /* recompute end: base + sequence */
                /* find cancel tag end */
                size_t probe = si + blen;
                while (probe < srclen) {
                    size_t pb; uint32_t pcp = utf8_decode(src + probe, srclen - probe, &pb);
                    if (pcp == 0xE007F) { probe += pb; break; }
                    probe += pb;
                }
                size_t seqlen = probe - si;
                if (di + seqlen < buf_size) { memcpy(buf + di, src + si, seqlen); di += seqlen; }
                si = probe;
                continue;
            }
            /* not a valid sequence: emit base char normally */
        }
        if (cp >= 0xE0000 && cp <= 0xE007F) {
            /* plane-14 tag char not part of a valid emoji sequence: strip */
            si += blen;
            continue;
        }
        /* copy the codepoint verbatim */
        if (di + blen < buf_size) { memcpy(buf + di, src + si, blen); di += blen; }
        si += blen;
    }
    buf[di] = '\0';
    return buf;
}

/* PoP: strip_unicode_tags @ tools/ansi_strip.py:strip_unicode_tags */
char *strip_unicode_tags(const char *text) {
    if (!text) return NULL;
    size_t len = strlen(text);
    char *buf = (char *)malloc(len + 1);
    if (!buf) return NULL;
    return strip_unicode_tags_buf(text, buf, len + 1);
}

/* --- sanitize_display_text --- */

/* Fast-path check mirroring Python _HAS_CONTROL: any of
 * 0x00-0x08, 0x0b-0x1f, 0x7f-0x9f. */
static bool has_display_control(const char *text) {
    if (!text) return false;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        unsigned char c = *p;
        if (c <= 0x08) return true;
        if (c >= 0x0b && c <= 0x1f) return true;
        if (c >= 0x7f && c <= 0x9f) return true;
    }
    return false;
}

/* PoP: sanitize_display_text @ tools/ansi_strip.py:sanitize_display_text */
char *sanitize_display_text(const char *text) {
    if (!text) return NULL;
    if (!has_display_control(text)) {
        size_t len = strlen(text);
        char *out = (char *)malloc(len + 1);
        if (!out) return NULL;
        memcpy(out, text, len + 1);
        return out;
    }
    /* strip ANSI sequences first */
    char *stripped = strip_ansi(text);
    if (!stripped) return NULL;

    /* normalize CR: \r\n -> \n, remaining \r -> \n */
    size_t inlen = strlen(stripped);
    char *out = (char *)malloc(inlen + 1);
    if (!out) { free(stripped); return NULL; }
    size_t oi = 0;
    for (size_t i = 0; i < inlen; i++) {
        if (stripped[i] == '\r') {
            if (stripped[i + 1] == '\n') i++;   /* \r\n -> \n */
            out[oi++] = '\n';
        } else {
            out[oi++] = stripped[i];
        }
    }
    out[oi] = '\0';

    /* remove C0 controls: 0x00-0x08, 0x0b, 0x0c, 0x0e-0x1f, 0x7f
     * (preserve tab 0x09 and newline 0x0a). */
    size_t wi = 0;
    for (size_t i = 0; i < oi; i++) {
        unsigned char c = (unsigned char)out[i];
        bool remove = (c <= 0x08) ||
                      (c == 0x0b) || (c == 0x0c) ||
                      (c >= 0x0e && c <= 0x1f) ||
                      (c == 0x7f);
        if (!remove) out[wi++] = out[i];
    }
    out[wi] = '\0';

    free(stripped);
    /* shrink to fit */
    char *final = (char *)realloc(out, strlen(out) + 1);
    return final ? final : out;
}
