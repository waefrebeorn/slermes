#ifndef NOC_W5_MARKEREVAL_A9D132_H
#define NOC_W5_MARKEREVAL_A9D132_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_markereval_a9d132_eval @ pm/_marker_eval.py:Marker.evaluate
 * Faithful PEP 508 marker evaluation. `marker` is the marker expression,
 * `env` is the environment mapping (JSON object of string -> string).
 * Returns 1 when the marker evaluates true, 0 otherwise. */
int w5_markereval_a9d132_eval(const char *marker, const struct json_t *env);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W5_MARKEREVAL_A9D132_H */
