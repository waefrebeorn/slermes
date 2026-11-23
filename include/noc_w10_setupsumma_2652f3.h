#ifndef NOC_W10_SETUPSUMMA_2652F3_H
#define NOC_W10_SETUPSUMMA_2652F3_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_setupsumma_2652f3_managed_or_provider_row @ hermes_cli/setup_summary.py:_managed_or_provider_row
 * Returns [label, available, hint] for a Nous-manageable feature.
 * `feature` is {"managed_by_nous":bool,"available":bool,"current_provider":str|null}. Caller frees. */
json_t *w10_setupsumma_2652f3_managed_or_provider_row(const char *name, const char *managed_label,
                                                     const char *missing_hint, const json_t *feature);

/* PoP: w10_setupsumma_2652f3_web_row @ hermes_cli/setup_summary.py:_web_row */
json_t *w10_setupsumma_2652f3_web_row(const json_t *feature);
/* PoP: w10_setupsumma_2652f3_browser_row @ hermes_cli/setup_summary.py:_browser_row */
json_t *w10_setupsumma_2652f3_browser_row(const json_t *feature);
/* PoP: w10_setupsumma_2652f3_image_gen_row @ hermes_cli/setup_summary.py:_image_gen_row
 * `backend` is the injected _first_available_plugin_provider("image_gen_registry", skip="fal") seam. */
json_t *w10_setupsumma_2652f3_image_gen_row(const json_t *feature, const char *backend);
/* PoP: w10_setupsumma_2652f3_video_gen_row @ hermes_cli/setup_summary.py:_video_gen_row
 * `backend` is the injected _first_available_plugin_provider("video_gen_registry") seam; null means none. */
json_t *w10_setupsumma_2652f3_video_gen_row(const json_t *feature, const char *backend);
/* PoP: w10_setupsumma_2652f3_always_on_rows @ hermes_cli/setup_summary.py:_always_on_rows */
json_t *w10_setupsumma_2652f3_always_on_rows(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_SETUPSUMMA_2652F3_H */
