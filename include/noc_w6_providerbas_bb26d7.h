#ifndef NOC_W6_PROVIDERBAS_BB26D7_H
#define NOC_W6_PROVIDERBAS_BB26D7_H

#include "hermes_json.h"

/* C port of the pure identity/picker helpers of agent/provider_base.py.
 * Returned char* values are malloc'd; caller frees them. */

/* PoP: w6_providerbas_bb26d7_display_name @ agent/provider_base.py:ProviderBase.display_name */
char *w6_providerbas_bb26d7_display_name(const char *name);

/* PoP: w6_providerbas_bb26d7_setup_schema @ agent/provider_base.py:ProviderBase.get_setup_schema */
json_t *w6_providerbas_bb26d7_setup_schema(const char *name);

/* PoP: w6_providerbas_bb26d7_catalog_display_name @ agent/provider_base.py:CatalogProviderBase.display_name */
char *w6_providerbas_bb26d7_catalog_display_name(const char *name);

/* PoP: w6_providerbas_bb26d7_catalog_is_available @ agent/provider_base.py:CatalogProviderBase.is_available */
int w6_providerbas_bb26d7_catalog_is_available(void);

/* PoP: w6_providerbas_bb26d7_catalog_list_models @ agent/provider_base.py:CatalogProviderBase.list_models */
json_t *w6_providerbas_bb26d7_catalog_list_models(void);

/* PoP: w6_providerbas_bb26d7_catalog_default_model @ agent/provider_base.py:CatalogProviderBase.default_model */
json_t *w6_providerbas_bb26d7_catalog_default_model(const json_t *models);

#endif /* NOC_W6_PROVIDERBAS_BB26D7_H */
