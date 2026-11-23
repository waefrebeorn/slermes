#ifndef NOC_W15_CONFIGEFFE_5314B2_H
#define NOC_W15_CONFIGEFFE_5314B2_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes hermes_cli/config_effective.py (gate w15_configeffe_5314b2).
 * All results are freshly allocated json_t values freed by the caller. */

/* PoP: w15_configeffe_5314b2_effective @ hermes_cli/config_effective.py:_effective
 * Replays the exact _effective pipeline: ${VAR} expansion of the user layer,
 * managed overlay (deep-merge on top), then root model-key canonicalization.
 * `env` injects the process environment; `managed` injects the managed layer. */
struct json_t *w15_configeffe_5314b2_effective(const struct json_t *raw,
                                              const struct json_t *env,
                                              const struct json_t *managed);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_CONFIGEFFE_5314B2_H */
