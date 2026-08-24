/*
 * vision_delegate.c — organization twin of src/tools/vision_delegate.py.
 *
 * The Python module delegated vision analysis to a Python subprocess (PIL).
 * The C tree implements the analysis natively — vision_analysis_native.c
 * replaces BOTH vision_analysis.py and vision_delegate.py (see its header).
 * This module declares that mapping for the organization-parity scan and
 * provides the delegation entry point under its Python name, routing to the
 * native engine (extend-don't-duplicate).
 */

#include "vision_analysis_native.h"
#include "hermes_json.h"

/* PoP: vd_delegate @ tools/vision_delegate.py:delegate */
char *vd_delegate(const char *path, const char *mode) {
    /* The only delegation modes the Python module exposed were colors + exif;
     * both are native now. */
    if (mode && strcmp(mode, "exif") == 0)
        return vision_exif_native(path);
    return vision_colors_native(path);
}
