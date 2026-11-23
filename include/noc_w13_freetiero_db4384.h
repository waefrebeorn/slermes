#ifndef NOC_W13_FREETIERO_DB4384_H
#define NOC_W13_FREETIERO_DB4384_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_freetiero_db4384_constants @ hermes_cli/free_tier_offer.py (module constants) */
struct json_t *w13_freetiero_db4384_constants(void);

/* PoP: w13_freetiero_db4384_due_at @ hermes_cli/free_tier_offer.py:_due_at */
struct json_t *w13_freetiero_db4384_due_at(const struct json_t *state);

/* PoP: w13_freetiero_db4384_due_in @ hermes_cli/free_tier_offer.py:_due_in
 * eligible and now (_clock()) are injected by the fixture. */
struct json_t *w13_freetiero_db4384_due_in(const struct json_t *state, int eligible, double now);

/* PoP: w13_freetiero_db4384_eligible @ hermes_cli/free_tier_offer.py:_eligible
 * is_guest / connected_flag / guest_allowed / free_active are injected (anon_auth seam). */
struct json_t *w13_freetiero_db4384_eligible(const char *kind, const struct json_t *connected_flag,
                                             int is_guest, int guest_allowed, int free_active);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_FREETIERO_DB4384_H */
