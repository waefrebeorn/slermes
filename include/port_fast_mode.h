/*
 * port_fast_mode.h — pure service-tier helpers from agent/fast_mode.py.
 *
 * parse_service_tier / parse_exact_service_tier map a user/config word to
 * agent.service_tier; service_tier_word maps a stored tier to its user-facing
 * word. These are the single lookup table shared by the CLI, gateway and TUI.
 */
#ifndef PORT_FAST_MODE_H
#define PORT_FAST_MODE_H

/* parse_service_tier(raw): returns a malloc'd canonical tier ("priority",
 * "ultrafast", "auto", "cold") or NULL when the word is a normal/blank/unknown
 * word (Python returns None). NULL == None. Caller frees. */
char *fm_parse_service_tier(const char *raw);

/* parse_exact_service_tier(raw): strict variant for an explicit pick; "" pins
 * normal (returns ""), an unknown word yields -1 (Python raises ValueError).
 * On success *out is a malloc'd string (possibly ""), caller frees. */
int fm_parse_exact_service_tier(const char *raw, char **out);

/* service_tier_word(tier): user-facing word for a stored tier
 * (priority -> fast, None/"" -> normal, else the tier itself). */
char *fm_service_tier_word(const char *tier);

#endif /* PORT_FAST_MODE_H */
