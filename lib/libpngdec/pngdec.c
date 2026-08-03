/*
 * pngdec.c — From-scratch PNG decoder.
 *
 * IDAT decompression rides on the project's own wubuzip DEFLATE inflate
 * (lib/libwubuoffice/src/wubuzip/inflate.c, with_zlib_header=1), and chunk
 * integrity uses wubuzip_crc32. Filter reconstruction, palette mapping,
 * sub-byte unpacking and interlacing rejection are implemented here.
 *
 * Spec: PNG (ISO/IEC 15948), zlib (RFC 1950), DEFLATE (RFC 1951).
 */

#include "pngdec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "wubuzip/inflate.h"
#include "wubuzip/crc.h"

/* ── error state ─────────────────────────────────────────────────────── */
static char g_err[128] = "";

static void set_err(const char *fmt, int a) {
    snprintf(g_err, sizeof(g_err), fmt, a);
}
static void set_err_str(const char *msg) {
    snprintf(g_err, sizeof(g_err), "%s", msg);
}

const char *pngdec_last_error(void) { return g_err; }

/* ── chunk walking ───────────────────────────────────────────────────── */
typedef struct {
    const uint8_t *p;
    const uint8_t *end;
} chunk_iter_t;

/* Returns 1 and fills type/len/data if a chunk is available; 0 at IEND or
 * on malformed input. */
static int next_chunk(chunk_iter_t *it, char type[5], uint32_t *len,
                      const uint8_t **data) {
    if (it->end - it->p < 12) return 0;
    uint32_t l = ((uint32_t)it->p[0] << 24) | ((uint32_t)it->p[1] << 16) |
                 ((uint32_t)it->p[2] << 8) | (uint32_t)it->p[3];
    type[0] = (char)it->p[4]; type[1] = (char)it->p[5];
    type[2] = (char)it->p[6]; type[3] = (char)it->p[7]; type[4] = '\0';
    const uint8_t *cdata = it->p + 8;
    /* CRC covers type + data */
    uint32_t crc = ((uint32_t)cdata[l] << 24) | ((uint32_t)cdata[l + 1] << 16) |
                   ((uint32_t)cdata[l + 2] << 8) | (uint32_t)cdata[l + 3];
    uint32_t want = wubuzip_crc32(it->p + 4, 4 + l);
    if (crc != want) {
        set_err_str("PNG chunk CRC mismatch");
        return 0;
    }
    *len = l;
    *data = cdata;
    it->p = cdata + l + 4;
    return 1;
}

/* ── bit-depth helpers ───────────────────────────────────────────────── */
static int bytes_per_pixel(int color_type) {
    switch (color_type) {
        case 0: return 1;              /* grayscale */
        case 2: return 3;              /* RGB */
        case 3: return 1;              /* palette index */
        case 4: return 2;              /* gray + alpha */
        case 6: return 4;              /* RGBA */
        default: return 0;
    }
}

/* Unpack sub-byte samples (bit depth 1/2/4) into 8-bit grayscale/alpha
 * scanline. Only used for color types 0/3 with low bit depths. */
static void unpack_bits(const uint8_t *src, int src_bpp, int depth,
                        int count, uint8_t *dst, int dst_bpp) {
    for (int i = 0; i < count; i++) {
        int bitpos = i * depth;
        int byte = bitpos / 8;
        int shift = 8 - depth - (bitpos % 8);
        int val = (src[byte] >> shift) & ((1 << depth) - 1);
        int v8 = (depth == 1) ? (val ? 255 : 0)
               : (depth == 2) ? (val * 255 / 3)
               : (val * 255 / 15);
        dst[i * dst_bpp] = (uint8_t)v8;
        if (dst_bpp == 2) dst[i * dst_bpp + 1] = 255;
    }
}

/* ── filters (per scanline, bpp = bytes per pixel) ───────────────────── */
static uint8_t paeth(uint8_t a, uint8_t b, uint8_t c) {
    int p = (int)a + (int)b - (int)c;
    int pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

static int unfilter_scanline(uint8_t *line, const uint8_t *prev,
                             size_t len, int bpp, int filter) {
    switch (filter) {
        case 0: /* None */ break;
        case 1: /* Sub */
            for (size_t i = (size_t)bpp; i < len; i++)
                line[i] = (uint8_t)(line[i] + line[i - (size_t)bpp]);
            break;
        case 2: /* Up */
            if (prev)
                for (size_t i = 0; i < len; i++)
                    line[i] = (uint8_t)(line[i] + prev[i]);
            break;
        case 3: /* Average */
            for (size_t i = 0; i < len; i++) {
                int a = (i >= (size_t)bpp) ? line[i - (size_t)bpp] : 0;
                int b = prev ? prev[i] : 0;
                line[i] = (uint8_t)(line[i] + ((a + b) >> 1));
            }
            break;
        case 4: /* Paeth */
            for (size_t i = 0; i < len; i++) {
                int a = (i >= (size_t)bpp) ? line[i - (size_t)bpp] : 0;
                int b = prev ? prev[i] : 0;
                int c = (prev && i >= (size_t)bpp) ? prev[i - (size_t)bpp] : 0;
                line[i] = (uint8_t)(line[i] + paeth((uint8_t)a, (uint8_t)b, (uint8_t)c));
            }
            break;
        default:
            set_err("bad PNG filter %d", filter);
            return -1;
    }
    return 0;
}

/* ── main decoder ────────────────────────────────────────────────────── */
pngdec_image_t *pngdec_decode(const uint8_t *data, size_t len) {
    set_err_str("");
    if (!data || len < 33) { set_err_str("PNG too short"); return NULL; }

    /* Signature: 89 50 4E 47 0D 0A 1A 0A */
    static const uint8_t SIG[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (memcmp(data, SIG, 8) != 0) { set_err_str("bad PNG signature"); return NULL; }

    chunk_iter_t it = { data + 8, data + len };

    int width = 0, height = 0, bit_depth = 0, color_type = 0;
    int interlace = 0;
    uint8_t palette[256 * 3];
    int palette_entries = 0;
    uint8_t *idat = NULL;
    size_t idat_len = 0, idat_cap = 0;

    int seen_ihdr = 0, seen_iend = 0;
    char type[5];
    uint32_t clen;
    const uint8_t *cdata;

    while (next_chunk(&it, type, &clen, &cdata)) {
        if (strcmp(type, "IHDR") == 0) {
            if (clen != 13) { set_err_str("bad IHDR length"); goto fail; }
            width  = ((int)cdata[0] << 24) | ((int)cdata[1] << 16) |
                     ((int)cdata[2] << 8) | (int)cdata[3];
            height = ((int)cdata[4] << 24) | ((int)cdata[5] << 16) |
                     ((int)cdata[6] << 8) | (int)cdata[7];
            bit_depth = cdata[8];
            color_type = cdata[9];
            interlace = cdata[12];
            seen_ihdr = 1;
            if (width <= 0 || height <= 0 || width > 65535 || height > 65535) {
                set_err_str("bad PNG dimensions"); goto fail;
            }
        } else if (strcmp(type, "PLTE") == 0) {
            if (clen > 768 || clen % 3 != 0) { set_err_str("bad PLTE"); goto fail; }
            memcpy(palette, cdata, clen);
            palette_entries = (int)(clen / 3);
        } else if (strcmp(type, "IDAT") == 0) {
            if (idat_len + clen > idat_cap) {
                size_t nc = idat_cap ? idat_cap * 2 : (size_t)clen + 4096;
                uint8_t *nb = realloc(idat, nc);
                if (!nb) { set_err_str("OOM in IDAT"); goto fail; }
                idat = nb; idat_cap = nc;
            }
            memcpy(idat + idat_len, cdata, clen);
            idat_len += clen;
        } else if (strcmp(type, "IEND") == 0) {
            seen_iend = 1;
            break;
        }
        /* other chunks (tEXt, tRNS, gAMA, ...) ignored */
    }

    if (!seen_ihdr) { set_err_str("missing IHDR"); goto fail; }
    if (!seen_iend && idat_len == 0) { set_err_str("missing IEND"); goto fail; }
    if (interlace != 0) { set_err_str("Adam7 interlacing unsupported"); goto fail; }
    if (bit_depth != 8 && bit_depth != 1 && bit_depth != 2 && bit_depth != 4) {
        set_err_str("unsupported bit depth"); goto fail;
    }
    int bpp = bytes_per_pixel(color_type);
    if (bpp == 0) { set_err_str("unsupported color type"); goto fail; }

    /* Inflate the concatenated IDAT (zlib-wrapped). */
    uint8_t *raw = NULL;
    size_t raw_len = 0;
    if (wubuzip_inflate(idat, idat_len, &raw, &raw_len, 1) != 0 || !raw) {
        set_err_str("IDAT inflate failed");
        free(idat);
        return NULL;
    }
    free(idat);

    /* Raw scanline size: bpp bytes per pixel, but sub-byte depths pack. */
    int bpc = bit_depth; /* bits per channel */
    int bits_per_pixel = (color_type == 3) ? bpc : bpc * bpp;
    size_t row_bits = (size_t)width * (size_t)bits_per_pixel;
    size_t row_bytes = (row_bits + 7) / 8;
    size_t stride = row_bytes + 1; /* +1 filter byte */
    if (raw_len < stride * (size_t)height) {
        set_err_str("IDAT too short for image");
        free(raw);
        return NULL;
    }

    pngdec_image_t *img = calloc(1, sizeof(pngdec_image_t));
    if (!img) { free(raw); set_err_str("OOM"); return NULL; }
    img->width = width;
    img->height = height;
    img->channels = (color_type == 0 || color_type == 3) ? 1
                  : (color_type == 2) ? 3
                  : (color_type == 4) ? 2 : 4;
    int out_ch = img->channels;
    size_t px_total = (size_t)width * (size_t)height * (size_t)out_ch;
    img->pixels = malloc(px_total);
    if (!img->pixels) { free(img); free(raw); set_err_str("OOM"); return NULL; }

    uint8_t *prev_line = calloc(row_bytes ? row_bytes : 1, 1);
    uint8_t *work = malloc(stride);
    if (!prev_line || !work) {
        free(prev_line); free(work); pngdec_image_free(img); free(raw);
        set_err_str("OOM"); return NULL;
    }

    for (int y = 0; y < height; y++) {
        const uint8_t *src = raw + (size_t)y * stride;
        int filter = src[0];
        memcpy(work, src + 1, row_bytes);
        if (unfilter_scanline(work, prev_line, row_bytes, bpp, filter) != 0) {
            free(prev_line); free(work); pngdec_image_free(img); free(raw);
            return NULL;
        }

        uint8_t *dst = img->pixels + (size_t)y * (size_t)width * (size_t)out_ch;
        if (bit_depth == 8) {
            for (int x = 0; x < width; x++) {
                const uint8_t *px = work + (size_t)x * (size_t)bpp;
                if (color_type == 6) { /* RGBA */
                    dst[x * 4] = px[0]; dst[x * 4 + 1] = px[1];
                    dst[x * 4 + 2] = px[2]; dst[x * 4 + 3] = px[3];
                } else if (color_type == 2) { /* RGB */
                    dst[x * 3] = px[0]; dst[x * 3 + 1] = px[1]; dst[x * 3 + 2] = px[2];
                } else if (color_type == 4) { /* gray+alpha */
                    dst[x * 2] = px[0]; dst[x * 2 + 1] = px[1];
                } else if (color_type == 0) { /* gray */
                    dst[x] = px[0];
                } else { /* palette */
                    int idx = px[0];
                    if (idx < 0 || idx >= palette_entries) idx = 0;
                    dst[x] = palette[idx * 3];
                }
            }
        } else {
            /* sub-byte depths: grayscale or palette */
            int samples = width;
            uint8_t unpacked[4096];
            if (samples > 4096) samples = 4096;
            unpack_bits(work, bpp, bit_depth, samples, unpacked, out_ch);
            for (int x = 0; x < width && x < 4096; x++) {
                if (color_type == 3) {
                    int idx = unpacked[x * out_ch];
                    if (idx < 0 || idx >= palette_entries) idx = 0;
                    dst[x] = palette[idx * 3];
                } else {
                    dst[x] = unpacked[x * out_ch];
                }
            }
        }
        memcpy(prev_line, work, row_bytes);
    }

    free(prev_line);
    free(work);
    free(raw);
    return img;

fail:
    free(idat);
    return NULL;
}

pngdec_image_t *pngdec_decode_file(const char *path) {
    if (!path) { set_err_str("NULL path"); return NULL; }
    FILE *f = fopen(path, "rb");
    if (!f) { set_err_str("cannot open file"); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 64L * 1024 * 1024) { fclose(f); set_err_str("bad file size"); return NULL; }
    uint8_t *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); set_err_str("OOM"); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    pngdec_image_t *img = pngdec_decode(buf, rd);
    free(buf);
    return img;
}

void pngdec_image_free(pngdec_image_t *img) {
    if (!img) return;
    free(img->pixels);
    free(img);
}
