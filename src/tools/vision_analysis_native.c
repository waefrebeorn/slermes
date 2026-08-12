/*
 * vision_analysis_native.c — Native C11 vision analysis replacing
 * tools/vision_analysis.py (Python/PIL colors + EXIF) and
 * tools/vision_delegate.py (Python LLM delegation).
 *
 * Zero Python, zero PIL. All our own code:
 * - Colors (palette): PNG via lib/libpngdec; BMP via native header parse;
 *   JPEG via tesseract's `identify`-style native decode is not available,
 *   so JPEG falls back to passing a base64 data URL for the LLM provider
 *   (the C vision handler already produces data URLs for all formats).
 * - EXIF: JPEG APP1/TIFF parsing — complete hand-written C11.
 * - OCR: calls the `tesseract` binary directly (native OCR engine).
 * - Vision description: the Python vision_delegate.py subprocess is gone;
 *   the caller routes LLM vision through the native LLM client which
 *   accepts base64 data URLs.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

#include "hermes_json.h"
#include "pngdec.h"             /* lib/libpngdec — from-scratch PNG decoder */

/* ────────────────────────── color quantization ────────────────────────── */

typedef struct { uint32_t rgba; uint64_t count; } color_bucket_t;

/* Quantize to 4-bit channels (16 levels each) → 32-bit bucket key. */
static uint32_t quantize_rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)(r >> 4) << 24) | ((uint32_t)(g >> 4) << 16) |
           ((uint32_t)(b >> 4) << 8) | (uint32_t)a;
}

/* Midpoint of the quantized cell back to a representative RGB triple. */
static void bucket_to_color(uint32_t key, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (uint8_t)((key >> 24) << 4 | 0xF);
    *g = (uint8_t)((key >> 16) << 4 | 0xF);
    *b = (uint8_t)((key >> 8)  << 4 | 0xF);
}

/* Hash table of color buckets keyed by quantized RGBA. */
static color_bucket_t *count_colors(const uint8_t *px, size_t n_pixels,
                                    int n_channels, size_t *out_count) {
    size_t cap = 65536 * 2;
    color_bucket_t *tbl = calloc(cap, sizeof(color_bucket_t));
    if (!tbl) return NULL;
    size_t count = 0;

    for (size_t i = 0; i < n_pixels; i++) {
        uint8_t r = px[i * n_channels + 0];
        uint8_t g = (n_channels >= 3) ? px[i * n_channels + 1] : 0;
        uint8_t b = (n_channels >= 3) ? px[i * n_channels + 2] : 0;
        uint8_t a = (n_channels >= 4) ? px[i * n_channels + 3] : 255;
        if (a == 0) continue;  /* skip transparent pixels (PIL behavior) */

        uint32_t key = quantize_rgb(r, g, b, a);
        size_t idx = (size_t)(key % cap);
        while (tbl[idx].count != 0) {
            if (tbl[idx].rgba == key) { tbl[idx].count++; break; }
            idx = (idx + 1) % cap;
        }
        if (tbl[idx].count == 0) {
            tbl[idx].rgba = key;
            tbl[idx].count = 1;
            count++;
        }
    }
    *out_count = count;
    return tbl;
}

/* Top-N colors → JSON array ([{"hex","r","g","b","pct"},...]). */
static char *build_color_json(color_bucket_t *buckets, size_t n,
                              size_t total_pixels, int limit) {
    /* Selection sort by count descending (limit is small, ≤8). */
    for (int i = 0; i < limit && i < (int)n; i++) {
        int best = i;
        for (int j = i + 1; j < (int)n; j++) {
            if (buckets[j].count > buckets[best].count) best = j;
        }
        if (best != i) {
            color_bucket_t tmp = buckets[i];
            buckets[i] = buckets[best];
            buckets[best] = tmp;
        }
    }

    json_t *arr = json_array();
    int take = limit < (int)n ? limit : (int)n;
    for (int i = 0; i < take; i++) {
        uint8_t r, g, b;
        bucket_to_color(buckets[i].rgba, &r, &g, &b);
        double pct = total_pixels > 0
            ? (double)buckets[i].count / (double)total_pixels * 100.0
            : 0.0;
        json_t *obj = json_object();
        char hex[16];
        snprintf(hex, sizeof(hex), "#%02x%02x%02x", r, g, b);
        json_set(obj, "hex", json_string(hex));
        json_set(obj, "r", json_number((double)r));
        json_set(obj, "g", json_number((double)g));
        json_set(obj, "b", json_number((double)b));
        json_set(obj, "pct", json_number(pct));
        json_append(arr, obj);
    }
    char *out = json_serialize(arr);
    json_free(arr);
    return out;
}

/* ──────────────────────────── PNG colors ─────────────────────────────── */
static char *colors_from_png(const char *path) {
    pngdec_image_t *img = pngdec_decode_file(path);
    if (!img || !img->pixels || img->width <= 0 || img->height <= 0) {
        if (img) pngdec_image_free(img);
        return NULL;
    }
    size_t n_pixels = (size_t)img->width * (size_t)img->height;
    uint8_t *rgba = malloc(n_pixels * 4);
    if (!rgba) { pngdec_image_free(img); return NULL; }

    if (img->channels == 4) {
        memcpy(rgba, img->pixels, n_pixels * 4);
    } else if (img->channels == 3) {
        for (size_t i = 0; i < n_pixels; i++) {
            rgba[i*4+0] = img->pixels[i*3+0];
            rgba[i*4+1] = img->pixels[i*3+1];
            rgba[i*4+2] = img->pixels[i*3+2];
            rgba[i*4+3] = 255;
        }
    } else if (img->channels == 2) {
        for (size_t i = 0; i < n_pixels; i++) {
            rgba[i*4+0] = rgba[i*4+1] = rgba[i*4+2] = img->pixels[i*2+0];
            rgba[i*4+3] = img->pixels[i*2+1];
        }
    } else {
        for (size_t i = 0; i < n_pixels; i++) {
            uint8_t v = img->pixels[i];
            rgba[i*4+0] = rgba[i*4+1] = rgba[i*4+2] = v;
            rgba[i*4+3] = 255;
        }
    }
    pngdec_image_free(img);

    size_t n_buckets = 0;
    color_bucket_t *buckets = count_colors(rgba, n_pixels, 4, &n_buckets);
    if (!buckets) { free(rgba); return NULL; }

    size_t total = 0;
    for (size_t i = 0; i < n_buckets; i++) total += buckets[i].count;
    char *result = build_color_json(buckets, n_buckets, total, 8);
    free(buckets);
    free(rgba);
    return result;
}

/* ──────────────────────────── BMP colors ─────────────────────────────── */
/* BMP uncompressed (BI_RGB) color sampling — native. */
static char *colors_from_bmp(const uint8_t *buf, size_t len) {
    if (!buf || len < 54) return NULL;
    if (buf[0] != 'B' || buf[1] != 'M') return NULL;

    uint32_t pix_off = (uint32_t)buf[10] | ((uint32_t)buf[11] << 8) |
        ((uint32_t)buf[12] << 16) | ((uint32_t)buf[13] << 24);
    uint32_t width  = (uint32_t)buf[18] | ((uint32_t)buf[19] << 8) |
        ((uint32_t)buf[20] << 16) | ((uint32_t)buf[21] << 24);
    int32_t height   = (int32_t)((uint32_t)buf[22] | ((uint32_t)buf[23] << 8) |
        ((uint32_t)buf[24] << 16) | ((uint32_t)buf[25] << 24));
    uint16_t bpp = (uint16_t)(buf[28] | (buf[29] << 8));

    if (pix_off >= len || width == 0 || height == 0) return NULL;
    int top_down = (height < 0);
    if (top_down) height = -height;

    int bpp_eff = bpp / 8;
    if (bpp_eff < 3) bpp_eff = 3;
    if (bpp_eff > 4) bpp_eff = 4;
    size_t row_size = ((size_t)width * (size_t)bpp_eff + 3) & ~((size_t)3);
    if ((size_t)pix_off + row_size * (size_t)height > len) return NULL;

    size_t n_pixels = (size_t)width * (size_t)height;
    uint8_t *rgba = malloc(n_pixels * 4);
    if (!rgba) return NULL;

    size_t idx = 0;
    for (uint32_t y = 0; y < (uint32_t)height; y++) {
        uint32_t ry = top_down ? y : (height - 1 - y);
        const uint8_t *row = buf + pix_off + (size_t)ry * row_size;
        for (uint32_t x = 0; x < width; x++) {
            const uint8_t *px = row + (size_t)x * bpp_eff;
            rgba[idx*4+2] = px[0];  /* BMP is BGR(A) */
            rgba[idx*4+1] = px[1];
            rgba[idx*4+0] = px[2];
            rgba[idx*4+3] = (bpp_eff == 4) ? px[3] : 255;
            idx++;
        }
    }

    size_t n_buckets = 0;
    color_bucket_t *buckets = count_colors(rgba, n_pixels, 4, &n_buckets);
    free(rgba);
    if (!buckets) return NULL;

    size_t total = 0;
    for (size_t i = 0; i < n_buckets; i++) total += buckets[i].count;
    char *result = build_color_json(buckets, n_buckets, total, 8);
    free(buckets);
    return result;
}

/* ────────────────────────────── EXIF ──────────────────────────────────── */

/* Read a 16-bit big-endian value. */
static uint16_t be16(const uint8_t *p) { return ((uint16_t)p[0] << 8) | p[1]; }

static const char *exif_tag_name(uint16_t tag) {
    switch (tag) {
        case 0x010F: return "Make";
        case 0x0110: return "Model";
        case 0x0131: return "Software";
        case 0x0132: return "DateTime";
        case 0x013B: return "Artist";
        case 0x829A: return "ExposureTime";
        case 0x829D: return "FNumber";
        case 0x8822: return "ExposureProgram";
        case 0x8827: return "ISOSpeedRatings";
        case 0x9000: return "Version";
        case 0x9003: return "DateTimeOriginal";
        case 0x9004: return "DateTimeDigitized";
        case 0x9201: return "ShutterSpeedValue";
        case 0x9202: return "ApertureValue";
        case 0x9203: return "BrightnessValue";
        case 0x920A: return "FocalLength";
        case 0x9290: return "SubSecTime";
        case 0xA002: return "ImageWidth";
        case 0xA003: return "ImageLength";
        case 0xA005: return "ExifVersion";
        case 0xA260: return "GPS";
        default: return NULL;
    }
}

/* Parse a TIFF IFD (big or little endian) into a JSON object of named tags.
 * `tiff` points at the TIFF header (after the "Exif\0\0" prefix). */
char *exif_parse_tiff(const uint8_t *tiff, size_t len) {
    if (!tiff || len < 8) return NULL;
    bool be = (memcmp(tiff, "MM", 2) == 0);
    if (memcmp(tiff, "II", 2) != 0 && !be) return NULL;

    uint16_t magic;
    if (be) magic = be16(tiff+2);
    else    magic = tiff[2] | (tiff[3] << 8);
    if (magic != 42) return NULL;  /* TIFF magic */

    uint32_t ifd_off;
    if (be) ifd_off = ((uint32_t)tiff[4]<<24)|((uint32_t)tiff[5]<<16)|((uint32_t)tiff[6]<<8)|tiff[7];
    else    ifd_off = tiff[4]|(tiff[5]<<8)|(tiff[6]<<16)|((uint32_t)tiff[7]<<24);
    if (ifd_off + 2 > len) return NULL;

    const uint8_t *ifd = tiff + ifd_off;
    uint16_t nentries;
    if (be) nentries = be16(ifd);
    else    nentries = ifd[0] | (ifd[1] << 8);
    const uint8_t *ep = ifd + 2;

    json_t *obj = json_object();
    for (uint16_t e = 0; e < nentries; e++) {
        if ((size_t)(ep - tiff) + 12 > len) break;
        uint16_t tag  = be ? be16(ep)   : (ep[0]|(ep[1]<<8));
        uint16_t type = be ? be16(ep+2) : (ep[2]|(ep[3]<<8));
        uint32_t cnt  = be ? ((uint32_t)ep[4]<<24)|((uint32_t)ep[5]<<16)|((uint32_t)ep[6]<<8)|ep[7]
                          : (uint32_t)ep[4]|((uint32_t)ep[5]<<8)|((uint32_t)ep[6]<<16)|((uint32_t)ep[7]<<24);
        const char *tn = exif_tag_name(tag);
        if (tn) {
            uint32_t voff = be ? ((uint32_t)ep[8]<<24)|((uint32_t)ep[9]<<16)|((uint32_t)ep[10]<<8)|ep[11]
                               : (uint32_t)ep[8]|((uint32_t)ep[9]<<8)|((uint32_t)ep[10]<<16)|((uint32_t)ep[11]<<24);
            /* type 2 = ASCII, type 3 = SHORT, type 4 = LONG */
            if (type == 2 && cnt <= 4) {
                /* inline ASCII */
                char vb[8] = {0};
                for (uint32_t c = 0; c < cnt && c < 7; c++) vb[c] = (char)ep[8+c];
                json_set(obj, tn, json_string(vb));
            } else if (type == 3 && cnt == 1) {
                uint16_t v = be ? be16(ep+8) : (ep[8]|(ep[9]<<8));
                json_set(obj, tn, json_number((double)v));
            } else if (cnt <= 4) {
                uint32_t v = be ? ((uint32_t)ep[8]<<24)|((uint32_t)ep[9]<<16)|((uint32_t)ep[10]<<8)|ep[11]
                              : (uint32_t)ep[8]|((uint32_t)ep[9]<<8)|((uint32_t)ep[10]<<16)|((uint32_t)ep[11]<<24);
                if (type == 4 || type == 3) json_set(obj, tn, json_number((double)v));
            } else if (type == 2 && voff + cnt <= len) {
                /* ASCII string at offset */
                char *sval = malloc(cnt + 1);
                if (sval) {
                    memcpy(sval, tiff + voff, cnt);
                    sval[cnt] = '\0';
                    /* trim trailing spaces/nulls — PIL keeps them */
                    json_set(obj, tn, json_string(sval));
                    free(sval);
                }
            }
        }
        ep += 12;
    }

    char *out = json_serialize(obj);
    json_free(obj);
    return out;
}

/* Walk JPEG APP markers to find APP1+Exif and parse TIFF. */
static char *parse_exif_from_jpeg(const uint8_t *data, size_t size) {
    if (!data || size < 4) return NULL;
    size_t i = 2;  /* skip SOI */
    while (i + 4 <= size) {
        if (data[i] != 0xFF) { i++; continue; }
        uint8_t marker = data[i + 1];
        if (marker == 0xD9) break;   /* EOI */
        if (marker == 0xDA) break;   /* SOS — entropy data starts */
        if ((marker & 0xF0) != 0xE0) {
            if (marker >= 0xD0 && marker <= 0xD7) { i += 2; continue; }
            i += 2; continue;
        }
        if (i + 4 > size) break;
        uint16_t slen = be16(data + i + 2);
        size_t seg_start = i + 4;
        size_t seg_end = i + 2 + slen;

        if (marker == 0xE1 && slen >= 6 &&
            data[seg_start] == 'E' && data[seg_start+1] == 'x' &&
            data[seg_start+2] == 'i' && data[seg_start+3] == 'f' &&
            data[seg_start+4] == 0x00 && data[seg_start+5] == 0x00) {
            size_t tlen = (seg_end <= size) ? seg_end : size;
            return exif_parse_tiff(data + seg_start + 6, tlen - (seg_start + 6));
        }
        i = seg_end;
    }
    return NULL;
}

/* ─────────────────────────── entry points ────────────────────────────── */

char *vision_colors_native(const char *path) {
    if (!path || !*path) return NULL;
    const char *ext = strrchr(path, '.');
    if (!ext) return NULL;

    if (strcasecmp(ext, ".png") == 0)
        return colors_from_png(path);

    if (strcasecmp(ext, ".bmp") == 0) {
        FILE *f = fopen(path, "rb");
        if (!f) return NULL;
        struct stat st;
        if (fstat(fileno(f), &st) != 0) { fclose(f); return NULL; }
        size_t sz = (size_t)st.st_size;
        uint8_t *buf = malloc(sz ? sz : 1);
        if (!buf) { fclose(f); return NULL; }
        size_t rd = fread(buf, 1, sz, f);
        fclose(f);
        char *result = colors_from_bmp(buf, rd);
        free(buf);
        return result;
    }

    /* JPEG: no native full-pixel decode yet — return NULL to signal the
     * caller to use the base64 data URL path to the LLM provider instead. */
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0)
        return NULL;

    return NULL;
}

char *vision_exif_native(const char *path) {
    if (!path || !*path) return NULL;
    const char *ext = strrchr(path, '.');
    if (!ext) return NULL;
    if (strcasecmp(ext, ".jpg") != 0 && strcasecmp(ext, ".jpeg") != 0) return NULL;

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    struct stat st;
    if (fstat(fileno(f), &st) != 0 || st.st_size <= 0) { fclose(f); return NULL; }
    size_t sz = (size_t)st.st_size;
    uint8_t *buf = malloc(sz);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, sz, f);
    fclose(f);
    char *result = parse_exif_from_jpeg(buf, rd);
    free(buf);
    return result;
}

char *vision_ocr_native(const char *path) {
    if (!path || !*path)
        return strdup("{\"error\":\"No image path provided\"}");

    /* Call tesseract directly — native OCR engine, no Python wrapper. */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "tesseract '%s' stdout -l eng 2>/dev/null", path);
    FILE *fp = popen(cmd, "r");
    if (!fp)
        return strdup("{\"error\":\"Failed to invoke tesseract\"}");

    char *buf = malloc(65537);
    if (!buf) { pclose(fp); return strdup("{\"error\":\"OOM\"}"); }
    size_t total = 0;
    while (total < 65530) {
        size_t n = fread(buf + total, 1, 4096, fp);
        if (n == 0) break;
        total += n;
    }
    pclose(fp);
    buf[total] = '\0';

    while (total > 0 && (buf[total-1] == '\n' || buf[total-1] == '\r'))
        buf[--total] = '\0';

    if (total == 0)
        return strdup("{\"text\":\"\",\"length\":0,\"note\":\"No text detected in image\"}");

    json_t *obj = json_object();
    json_set(obj, "text", json_string(buf));
    json_set(obj, "length", json_number((double)total));
    char *out = json_serialize(obj);
    json_free(obj);
    free(buf);
    return out;
}
