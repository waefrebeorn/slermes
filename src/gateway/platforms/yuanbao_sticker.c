/*
 * yuanbao_sticker.c — Name parity wrapper for Python gateway/platforms/yuanbao_sticker.py
 *
 * NOTE: The C implementation for this platform feature is integrated into
 * the main platform adapter file (e.g. feishu.c, yuanbao.c, signal.c),
 * not a standalone C file. This file exists for name parity only.
 *
 * Yuanbao sticker/emoji handling.
Maps sticker IDs to emoji, handles sticker pack downloads.

Port of Python gateway/platforms/yuanbao_sticker.py (558 lines).
N/A: Python sticker pack download logic — C uses libhttp.
The actual Yuanbao platform adapter is in src/gateway/platforms/yuanbao.c.
 */

#include "hermes.h"
