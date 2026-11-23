#ifndef NOC_W6_USAGEGAUGES_1DD126_H
#define NOC_W6_USAGEGAUGES_1DD126_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C port of agent/usage_gauges.py (gate w6_usagegauges_1dd126).
 * A gauge object in JSON has: id, scope, label, used_bp, resets_at_ms,
 * full, used_fraction. JSON null stands in for Python None. */

/* PoP: w6_usagegauges_1dd126_paused_for_payment @ agent/usage_gauges.py:paused_for_payment */
int w6_usagegauges_1dd126_paused_for_payment(const char *reason);   /* NULL == None */

/* PoP: w6_usagegauges_1dd126_parse_gauges @ agent/usage_gauges.py:parse_gauges
 * Accepts a JSON array or its JSON-string form; returns a JSON array of
 * gauge objects in wire order, or NULL (None) when absent/unreadable/empty. */
struct json_t *w6_usagegauges_1dd126_parse_gauges(const struct json_t *value);

/* PoP: w6_usagegauges_1dd126_gauge_title @ agent/usage_gauges.py:gauge_title */
char *w6_usagegauges_1dd126_gauge_title(const char *id, const char *label);
/* PoP: w6_usagegauges_1dd126_gauge_limit_name @ agent/usage_gauges.py:gauge_limit_name */
char *w6_usagegauges_1dd126_gauge_limit_name(const char *id, const char *label);
/* PoP: w6_usagegauges_1dd126_gauge_short @ agent/usage_gauges.py:gauge_short */
char *w6_usagegauges_1dd126_gauge_short(const char *id, const char *label);

/* Selectors take the PARSED gauge array (as returned by parse_gauges). */
/* PoP: w6_usagegauges_1dd126_all_scope @ agent/usage_gauges.py:all_scope */
struct json_t *w6_usagegauges_1dd126_all_scope(const struct json_t *gauges);
/* PoP: w6_usagegauges_1dd126_top_tier_gauge @ agent/usage_gauges.py:top_tier_gauge */
struct json_t *w6_usagegauges_1dd126_top_tier_gauge(const struct json_t *gauges);
/* PoP: w6_usagegauges_1dd126_binding_gauge @ agent/usage_gauges.py:binding_gauge */
struct json_t *w6_usagegauges_1dd126_binding_gauge(const struct json_t *gauges);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W6_USAGEGAUGES_1DD126_H */
