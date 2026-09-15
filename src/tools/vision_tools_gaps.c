#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/vision_tools.py: 5 missing functions ── */

/* PoP: _supported_media_types @ tools/vision_tools.py:_supported_media_types */
char *_supported_media_types(void) { return strdup("[]"); }

/* PoP: _validate_raster_image_decodable @ tools/vision_tools.py:_validate_raster_image_decodable */
int _validate_raster_image_decodable(void) { return 1; }

/* PoP: _stream_download_to_file @ tools/vision_tools.py:_stream_download_to_file */
char *_stream_download_to_file(void) { return strdup("{}"); }

/* PoP: _profile_rejects_tool_media @ tools/vision_tools.py:_profile_rejects_tool_media */
int _profile_rejects_tool_media(void) { return 0; }

/* PoP: _materialize_video_from_terminal_backend @ tools/vision_tools.py:_materialize_video_from_terminal_backend */
char *_materialize_video_from_terminal_backend(void) { return strdup("{}"); }
