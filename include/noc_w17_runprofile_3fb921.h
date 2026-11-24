#ifndef NOC_W17_RUNPROFILE_3FB921_H
#define NOC_W17_RUNPROFILE_3FB921_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_runprofile_3fb921_served_profile_names @ gateway/run_profile_reconcile.py:GatewayProfileReconcileMixin.served_profile_names
 * fixture: homes (JSON array of names), active (string or null). Returns the
 * projected list: active first when present (or when homes is empty), then the
 * remaining names sorted lexicographically. */
struct json_t *w17_runprofile_3fb921_served_profile_names(const struct json_t *fixture);

/* PoP: w17_runprofile_3fb921_added_profile_blocker @ gateway/run_profile_reconcile.py:_added_profile_blocker
 * fixture: opted_out (bool), implicit (string or null). Returns a JSON string
 * or null. */
struct json_t *w17_runprofile_3fb921_added_profile_blocker(const struct json_t *fixture);

/* PoP: w17_runprofile_3fb921_identity_verb_error @ gateway/run_profile_reconcile.py:_identity_verb_error
 * fixture: collision (bool), type_name (string), message (string). Returns
 * "<name>: <message>" where a collision is reported as ValueError. */
struct json_t *w17_runprofile_3fb921_identity_verb_error(const struct json_t *fixture);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_RUNPROFILE_3FB921_H */
