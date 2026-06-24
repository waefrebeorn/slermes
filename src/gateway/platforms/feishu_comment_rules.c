/*
 * feishu_comment_rules.c — Name parity wrapper for Python gateway/platforms/feishu_comment_rules.py
 *
 * NOTE: The C implementation for this platform feature is integrated into
 * the main platform adapter file (e.g. feishu.c, yuanbao.c, signal.c),
 * not a standalone C file. This file exists for name parity only.
 *
 * Feishu document comment access-control rules.
3-tier rule resolution: exact doc > wildcard > top-level > defaults.
Fields: enabled/policy/allow_from with independent fallback per tier.
Config: ~/.hermes/feishu_comment_rules.json (mtime-cached, hot-reload).
Pairing: ~/.hermes/feishu_comment_pairing.json.

Port of Python gateway/platforms/feishu_comment_rules.py (429 lines).
N/A: Python dataclasses (CommentDocumentRule, CommentPairing) map to C structs.
N/A: JSON file hot-reload with mtime caching — C uses config.c pattern.
N/A: frozenset allow_from — C uses comma-separated string or array.
 */

#include "hermes.h"
