/*
 * noc_w21_adapterrea_e0609e.h -- compact emoji-reaction markers for Slack
 * thread context (port of plugins/platforms/slack/adapter_reactions_context.py).
 *
 * PoP: w21_adapterrea_e0609e_slack_reactions_marker @ plugins/platforms/slack/adapter_reactions_context.py:slack_reactions_marker
 * PoP: w21_adapterrea_e0609e_sanitize_name @ plugins/platforms/slack/adapter_reactions_context.py:_UNSAFE_NAME_CHARS
 * PoP: w21_adapterrea_e0609e_marker_max @ plugins/platforms/slack/adapter_reactions_context.py:REACTION_MARKER_MAX
 */
#ifndef NOC_W21_ADAPTERREA_E0609E_H
#define NOC_W21_ADAPTERREA_E0609E_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* REACTION_MARKER_MAX: the tail of a heavily-reacted message collapses to "+N more". */
int w21_adapterrea_e0609e_marker_max(void);

/* str(value or "") with Python's coercion rules, then the _UNSAFE_NAME_CHARS
 * substitution (drop every char outside [\w+'\-:] and non-ASCII bytes) and
 * .strip(":") -- the exact name normalisation slack_reactions_marker applies. */
json_t *w21_adapterrea_e0609e_sanitize_name(const json_t *value);

/* "[reactions: :name:xcount ...]" (the separator is U+00D7, emitted as two
 * UTF-8 bytes by the port) for a Slack message dict, or "". A non-dict
 * message is Python's AttributeError and comes back as
 * {"status":"error","type":"AttributeError","message":...}. */
json_t *w21_adapterrea_e0609e_slack_reactions_marker(const json_t *msg);

/* Footer bytes appended to the marker for a heavily reacted message ("+N more")
 * or "" when the part list fits; exposed so the truncation branch is testable. */
json_t *w21_adapterrea_e0609e_more_label(int total_parts);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_ADAPTERREA_E0609E_H */
