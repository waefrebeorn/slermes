/*
 * noc_w31_shared_97a29d_p1.h - C surface for upstream
 * gateway/platforms/_shared.py (gate w31_shared_97a29d_p1).
 *
 * The ingress primitives every gateway adapter shares: the timing-safe secret
 * / signature comparison, the webhook replay-window check, the JSON-literal
 * allowlist decoder, the port coercion and the "did this request ever leave"
 * predicate over a transport exception chain.  Every I/O seam (the env, the
 * clock, the exception objects themselves) stays in the caller; these take
 * values and return values.
 */
#ifndef NOC_W31_SHARED_97A29D_P1_H
#define NOC_W31_SHARED_97A29D_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* WEBHOOK_REPLAY_WINDOW_SECONDS: the default drift allowance, either way. */
#define W31_SHARED_97A29D_P1_REPLAY_WINDOW_SECONDS 300.0

/* (provided, expected) -> bool.  secrets_match: false unless both are strings
 * and expected is non-empty; otherwise byte equality of the UTF-8 form. */
json_t *w31_shared_97a29d_p1_secrets_match(const json_t *provided, const json_t *expected);

/* (raw, window, now) -> bool.  timestamp_fresh with the clock handed in:
 * `abs(now - int(raw)) <= window`; unparseable raw (or a non-numeric window)
 * is false.  `int()` is CPython's: str/float/bool accepted, trunc toward zero. */
json_t *w31_shared_97a29d_p1_timestamp_fresh(const json_t *raw, const json_t *window,
                                            const json_t *now);

/* raw -> value.  decode_json_list_literal: a string whose first non-space char
 * is '[' is json.loads'd and returned only when it is a list; malformed JSON
 * (and every non-string, or a string not starting with '[') passes through
 * unchanged, keeping the legacy comma-split path. */
json_t *w31_shared_97a29d_p1_decode_json_list_literal(const json_t *raw);

/* (value, default) -> number.  coerce_port: int(value), or `default` when
 * int() raises (TypeError / ValueError / OverflowError). */
json_t *w31_shared_97a29d_p1_coerce_port(const json_t *value, const json_t *default_value);

/* name -> str.  bridged_env_marker: the ownership marker this bridge writes
 * beside an env value it made from YAML ("HERMES_BRIDGED_" + name). */
json_t *w31_shared_97a29d_p1_bridged_env_marker(const json_t *name);

/* chain -> bool.  request_never_left over the MRO names of every exception in
 * the cause/context walk: [[name, ...], ...].  True when any class name is one
 * of the pre-request transport errors (connect / pool, never a read timeout). */
json_t *w31_shared_97a29d_p1_request_never_left(const json_t *chain);

#ifdef __cplusplus
}
#endif
#endif
