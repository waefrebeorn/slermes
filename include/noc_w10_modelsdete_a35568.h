#ifndef NOC_W10_MODELDETE_A35568_H
#define NOC_W10_MODELDETE_A35568_H
#include "hermes_json.h"

/* PoP port of hermes_cli/models_detect.py — live-catalog / credential guards.
 * External catalog + normalizer results are injected explicitly (the Python
 * module reads disk-cached catalogs and imports helpers lazily). */
json_t *w10_modelsdete_a35568_current_provider_catalog_match(
        const char *model_name, const char *current_provider,
        const char *normalized_provider, int catalog_error, json_t *catalog);
int w10_modelsdete_a35568_current_provider_owns_vendor(
        const char *model_name, const char *current_provider,
        const char *normalized_provider, int is_aggregator,
        const char *vendor_of_model, json_t *native_vendors);
#endif /* NOC_W10_MODELDETE_A35568_H */
