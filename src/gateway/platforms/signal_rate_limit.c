/*
 * signal_rate_limit.c — Name parity wrapper for Python gateway/platforms/signal_rate_limit.py
 *
 * NOTE: The C implementation for this platform feature is integrated into
 * the main platform adapter file (e.g. feishu.c, yuanbao.c, signal.c),
 * not a standalone C file. This file exists for name parity only.
 *
 * Signal rate-limit tracking for message sending.
Implements exponential backoff when Signal API returns rate-limit errors.

Port of Python gateway/platforms/signal_rate_limit.py (369 lines).
N/A: Python datetime arithmetic for backoff window calculation.
N/A: RateLimitState dataclass — C uses struct with timestamps.
 */

#include "hermes_core_types.h"
