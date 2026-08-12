/*
 * vision_analysis_native.h — Native C11 vision analysis replacing
 * tools/vision_analysis.py (Python PIL-based color + EXIF) and
 * tools/vision_delegate.py (Python subprocess delegation).
 *
 * All of this is our own C11 code — zero Python, zero PIL.
 * PNG decoding reuses lib/libpngdec (from-scratch, wubuzip inflate/CRC).
 * EXIF TIFF parsing is hand-written C11.
 * OCR shells out to the `tesseract` binary directly (tesseract is the
 * native OCR engine; no Python wraps it).
 */
#ifndef VISION_ANALYSIS_NATIVE_H
#define VISION_ANALYSIS_NATIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Color palette analysis. Returns malloc'd JSON array string
 * (matching vision_analysis.py colors mode):
 *   [{"hex":"#rrggbb","r":r,"g":g,"b":b,"pct":float}, ...]
 * Caller must free(). NULL on failure. */
char *vision_colors_native(const char *path);

/* EXIF metadata extraction (JPEG APP1). Returns malloc'd JSON object
 * string matching vision_analysis.py exif mode. Caller must free(). */
char *vision_exif_native(const char *path);

/* OCR via native tesseract binary (no Python wrapper).
 * Returns malloc'd JSON string {"text":..., "length":N} or
 * {"error":...}. Caller must free(). */
char *vision_ocr_native(const char *path);

#ifdef __cplusplus
}
#endif
#endif /* VISION_ANALYSIS_NATIVE_H */
