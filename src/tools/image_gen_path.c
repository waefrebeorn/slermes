/*
 * image_gen_path.c — focused extraction from tools/image_generation_tool.py
 *
 * The ONE genuinely pure, oracle-verifiable helper from the image-gen
 * monolith: classify a value as an absolute file path (POSIX or Windows
 * drive) vs a URL / data-URI / relative path. Faithful C port of
 * tools/image_generation_tool.py:_looks_like_absolute_file_path.
 *
 * The sibling path-translation helpers (_agent_cache_base_for_env,
 * _agent_visible_cache_path, _postprocess_image_generate_result) are
 * CONFIG/MOUNT-COUPLED in live Python (they consult
 * tools.credential_files.map_cache_path_to_container + env.__class__.__name__,
 * which is non-deterministic without a real mount table). Those stay in
 * port_image_generation_tool.c as documented PoP ports; this module owns
 * only the pure classifier so it can be oracle-verified 1:1.
 */

#ifndef SRC_TOOLS_IMAGE_GEN_PATH_C
#define SRC_TOOLS_IMAGE_GEN_PATH_C

#include "image_gen_path.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PoP: image_gen_path_looks_like_absolute_file_path @ tools/image_generation_tool.py:_looks_like_absolute_file_path */
bool image_gen_path_looks_like_absolute_file_path(const char *value)
{
    if (!value || !*value) return false;

    char *lower_buf = strdup(value);
    if (!lower_buf) return false;
    for (char *p = lower_buf; *p; p++) *p = (char)tolower((unsigned char)*p);

    bool result = false;
    if (strncmp(lower_buf, "http://", 7) == 0 ||
        strncmp(lower_buf, "https://", 8) == 0 ||
        strncmp(lower_buf, "data:", 5) == 0) {
        result = false;
    } else {
        /* ntpath.isabs (CPython 3.14): absolute iff, after viewing the first 3
         * chars with '/' folded to '\', the window is "X:\" (drive + root) at
         * index 1, OR the path starts with "\\" (UNC / device). A bare leading
         * '/' or '\' with no drive is *not* absolute on Windows. */
        char win[4] = {0, 0, 0, 0};
        size_t n = strlen(value);
        for (size_t k = 0; k < 3 && k < n; k++)
            win[k] = (value[k] == '/') ? '\\' : value[k];
        result = (win[1] == ':' && (win[2] == '\\')) ||
                 (win[0] == '\\' && win[1] == '\\');
    }

    free(lower_buf);
    return result;
}

#endif /* SRC_TOOLS_IMAGE_GEN_PATH_C */
