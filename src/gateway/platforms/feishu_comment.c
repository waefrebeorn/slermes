/*
 * feishu_comment.c — Name parity wrapper for Python gateway/platforms/feishu_comment.py
 *
 * NOTE: The C implementation for this platform feature is integrated into
 * the main platform adapter file (e.g. feishu.c, yuanbao.c, signal.c),
 * not a standalone C file. This file exists for name parity only.
 *
 * Feishu document comment event handler.
Handles document comment creation/reply/mention events from Feishu webhook.
Integrates with feishu_comment_rules for access-control decisions.

Port of Python gateway/platforms/feishu_comment.py (1382 lines).
N/A: Python async webhook handler — C uses synchronous poll + dispatch.
N/A: Feishu OpenAPI SDK calls — C uses libhttp directly.
N/A: Document comment reply formatting — C uses sprintf patterns.
 */

#include "hermes_core_types.h"
#include "hermes_gateway_feishu.h"
