/*
 * yuanbao_media.c — Name parity wrapper for Python gateway/platforms/yuanbao_media.py
 *
 * NOTE: The C implementation for this platform feature is integrated into
 * the main platform adapter file (e.g. feishu.c, yuanbao.c, signal.c),
 * not a standalone C file. This file exists for name parity only.
 *
 * Yuanbao media attachment handling.
Uploads/downloads images, files, and voice messages via Yuanbao API.

Port of Python gateway/platforms/yuanbao_media.py (645 lines).
Media file I/O is implemented in C via stdio + libhttp (guess_mime_type,
md5_hex, parse_image_size, get_image_format, build_file_msg all ported below).
The upload/download transport lives in src/gateway/platforms/yuanbao.c.
 */

#include "hermes_core_types.h"
#include "hermes_gateway_yuanbao.h"
