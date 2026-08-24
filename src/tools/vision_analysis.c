/*
 * vision_analysis.c — canonical C11 port of src/tools/vision_analysis.py and
 * src/tools/vision_delegate.py.
 *
 * The native engine lives in vision_analysis_native.c (color + EXIF analysis
 * replacing the Python PIL path; the Python subprocess delegation in
 * vision_delegate.py was removed by design — see the header). This module
 * re-exports the native entry points under their module names so callers
 * read the same as the quarry (extend-don't-duplicate).
 */

#include "vision_analysis_native.h"
#include "hermes_json.h"

/* PoP: va_colors @ tools/vision_analysis.py:colors */
char *va_colors(const char *path) {
    extern char *vision_colors_native(const char *path);
    return vision_colors_native(path);
}

/* PoP: va_exif @ tools/vision_delegate.py:exif */
char *va_exif(const char *path) {
    extern char *vision_exif_native(const char *path);
    return vision_exif_native(path);
}
