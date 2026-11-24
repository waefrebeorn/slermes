/*
 * noc_w27_adapter_e9cb6b_p2.h - C surface for the p2 slice of upstream
 * plugins/platforms/matrix/adapter.py (gate w27_adapter_e9cb6b_p2).
 *
 * Config/env normalisation: E2EE mode resolution, the truthy/number/csv
 * readers the constructor and module helpers use, thread-root extraction, the
 * YAML bool parsers, server ed25519 key selection, the connected predicate and
 * the two policy knobs the constructor derives (session scope, reactions).
 * Every reader of os.environ / config files / the plugin registry is a seam;
 * what is ported is the decision made from the value already in hand.
 */
#ifndef NOC_W27_ADAPTER_E9CB6B_P2_H
#define NOC_W27_ADAPTER_E9CB6B_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* value -> "required" | "optional" | "off". */
json_t *w27_adapter_normalize_e2ee_mode(const json_t *value);

/* (explicit e2ee_mode, legacy encryption flag) -> mode string. */
json_t *w27_adapter_resolve_e2ee_mode(const json_t *explicit_, const json_t *legacy_enabled);

/* value -> str(value).lower() in {true,1,yes}. */
json_t *w27_adapter_env_truthy(const json_t *value);

/* value/default -> cast(value) or default; want_float selects int vs float parse. */
json_t *w27_adapter_env_number(const json_t *value, const json_t *dflt, int want_float);

/* "a, b" | ["a"," b "] -> sorted array of stripped tokens. */
json_t *w27_adapter_csv_set(const json_t *raw);

/* relates_to -> m.thread root event_id, else null. */
json_t *w27_adapter_thread_root(const json_t *relates_to);

/* configured -> bool passthrough, else true/1/yes semantics. */
json_t *w27_adapter_extra_truthy(const json_t *configured);

/* configured -> bool | null (YAML bool, "off"-style string, other truthiness). */
json_t *w27_adapter_configured_bool(const json_t *configured);

/* configured -> require_mention (absent => true). */
json_t *w27_adapter_parse_require_mention(const json_t *configured);

/* configured -> thread_require_mention (absent => false). */
json_t *w27_adapter_parse_thread_require_mention(const json_t *configured);

/* {kid: key} -> first "ed25519:*" value as string, else null. */
json_t *w27_adapter_extract_server_ed25519(const json_t *keys);

/* homeserver/token -> both non-blank. */
json_t *w27_adapter_is_connected(const json_t *homeserver, const json_t *token);

/* raw -> "auto" | "room" | "thread". */
json_t *w27_adapter_session_scope(const json_t *raw);

/* raw -> str(raw).lower() not in {false,0,no}. */
json_t *w27_adapter_reactions_enabled(const json_t *raw);

#ifdef __cplusplus
}
#endif
#endif
