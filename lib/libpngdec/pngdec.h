/*
 * pngdec.h — From-scratch PNG decoder (C11, zero external deps).
 *
 * Uses the project's own wubuzip DEFLATE inflate + CRC32 (lib/libwubuoffice)
 * for the IDAT stream — no libpng, no stb_image, no third-party code.
 *
 * Supports the PNG subset used by petdex spritesheets and generated pets:
 *   - color types 0 (grayscale), 2 (RGB), 3 (palette), 4 (gray+alpha),
 *     6 (RGBA)
 *   - bit depths 8 (and 1/2/4 for palette + grayscale)
 *   - non-interlaced (Adam7 interlacing rejected with an error)
 *   - all 5 filter types (None/Sub/Up/Average/Paeth)
 *   - zlib-wrapped IDAT (RFC 1950), CRC32 chunk validation
 */

#ifndef SLERMES_PNGDEC_H
#define SLERMES_PNGDEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decoded RGBA image (8 bits per channel, non-premultiplied). */
typedef struct {
    int      width;
    int      height;
    int      channels;      /* 4 when alpha present, 3 otherwise */
    uint8_t *pixels;        /* width * height * channels, row-major */
} pngdec_image_t;

/* Decode a PNG from memory. Returns NULL on any error (bad signature,
 * unsupported color type / interlace, corrupt IDAT, CRC mismatch, OOM).
 * The caller must free the image with pngdec_image_free(). */
pngdec_image_t *pngdec_decode(const uint8_t *data, size_t len);

/* Convenience: decode a PNG file from disk. Returns NULL on error. */
pngdec_image_t *pngdec_decode_file(const char *path);

/* Free a decoded image (safe to call with NULL). */
void pngdec_image_free(pngdec_image_t *img);

/* Human-readable last error (static buffer; valid until next call). */
const char *pngdec_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_PNGDEC_H */
