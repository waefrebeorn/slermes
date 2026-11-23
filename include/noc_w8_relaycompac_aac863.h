#ifndef NOC_W8_RELAYCOMPAC_AAC863_H
#define NOC_W8_RELAYCOMPAC_AAC863_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_relaycompac_aac863_turn_target @ agent/relay_compaction.py:_turn_target */
struct json_t *w8_relaycompac_aac863_turn_target(const struct json_t *args);

/* PoP: w8_relaycompac_aac863_session_target @ agent/relay_compaction.py:_session_target */
struct json_t *w8_relaycompac_aac863_session_target(const struct json_t *args);

/* PoP: w8_relaycompac_aac863_resolve_target @ agent/relay_compaction.py:_resolve_target */
struct json_t *w8_relaycompac_aac863_resolve_target(const struct json_t *args);

/* PoP: w8_relaycompac_aac863_target_available @ agent/relay_compaction.py:compaction_target_available */
int w8_relaycompac_aac863_target_available(const struct json_t *args);

/* PoP: w8_relaycompac_aac863_emit_mark @ agent/relay_compaction.py:emit_compaction_mark
 * Returns true/false, or {"__raise__":"TimeoutError"} when run_in_session times out. */
struct json_t *w8_relaycompac_aac863_emit_mark(const struct json_t *args);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_RELAYCOMPAC_AAC863_H */
