#ifndef NOC_W5_HERMESSTATE_732FEC_H
#define NOC_W5_HERMESSTATE_732FEC_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes hermes_state_registry.py (gate w5_hermesstate_732fec).
 * The compatibility shim's two observable decisions are ported: which SessionDB
 * class acquire() lends (and whether it warns), and whether release() closes. */

/* PoP: w5_hermesstate_732fec_release @ hermes_state_registry.py:release
 * is_none mirrors a None db. Returns {"result":bool, "closed":bool}: release()
 * answers False for None, else calls db.close() and answers True. */
struct json_t *w5_hermesstate_732fec_release(int is_none);

/* PoP: w5_hermesstate_732fec_acquire_variant @ hermes_state_registry.py:acquire
 * Exposed seams: first_warning_present (is _first_warning defined?),
 * first_warning_result (its boolean return), and the two message fragments.
 * Returns {"db_class":"SessionDB"|"_SessionDB", "warn":bool, "msg":str|null}. */
struct json_t *w5_hermesstate_732fec_acquire_variant(int first_warning_present,
                                                     int first_warning_result,
                                                     const char *removal_release,
                                                     const char *migration_guide);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_HERMESSTATE_732FEC_H */
