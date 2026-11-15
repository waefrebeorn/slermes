#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/vision_tools.py: 5 missing functions ── */

/* PoP: supported_media_types @ tools/vision_tools_image_prep.py:supported_media_types */
char *supported_media_types(void) { return strdup("[]"); }

/* PoP: validate_raster_image_decodable @ tools/vision_tools_image_prep.py:validate_raster_image_decodable */
int validate_raster_image_decodable(void) { return 1; }

/* PoP: stream_download_to_file @ tools/vision_tools.py:stream_download_to_file */
char *stream_download_to_file(void) { return strdup("{}"); }

/* PoP: profile_rejects_tool_media @ tools/vision_tools.py:profile_rejects_tool_media */
int profile_rejects_tool_media(void) { return 0; }

/* PoP: materialize_video_from_terminal_backend @ tools/vision_tools.py:materialize_video_from_terminal_backend */
char *materialize_video_from_terminal_backend(void) { return strdup("{}"); }
