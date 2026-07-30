/**
 * @file hermes_gateway_rate_limiter.h
 * @brief Gateway rate limiter API (P101).
 */
#ifndef HERMES_GATEWAY_RATE_LIMITER_H
#define HERMES_GATEWAY_RATE_LIMITER_H

#include "hermes_gateway_types.h"

/* ================================================================
 *  P101: Rate limiter API
 * ================================================================ */

/* Initialize rate limiter for platform index.
 * tokens_per_sec: messages allowed per second (defaults)
 * max_burst: maximum burst size (default ~3x rate) */
void gw_rate_limit_init(int idx, double tokens_per_sec, double max_burst);

/* Check and consume a token. Returns true if allowed, false if rate-limited. */
bool gw_rate_limit_check(int idx);

#endif /* HERMES_GATEWAY_RATE_LIMITER_H */