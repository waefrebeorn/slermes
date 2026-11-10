/*
 * models_dev_info.h - model capability / format / cache helpers.
 *
 * Module-internal header (models_dev.c <-> models_dev_info.c).
 */
#ifndef MODELS_DEV_INFO_H
#define MODELS_DEV_INFO_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "hermes_json.h"   /* json_t */

#define MODELS_DEV_CACHE_FILE "models_dev_cache.json"
#define MODELS_DEV_CACHE_TTL 3600  /* 1 hour */


char * get_models_dev_cache_path(void);
char * models_dev_format_capabilities(json_t *entry);
char * models_dev_format_cost(json_t *entry);
json_t * models_dev_load_disk_cache(void);


/* used earlier in the module than defined */
bool models_dev_has_cost_data(json_t *m);
/* used earlier in the module than defined */
bool models_dev_supports_vision(json_t *m);
/* used earlier in the module than defined */
bool models_dev_supports_pdf(json_t *m);
/* used earlier in the module than defined */
bool models_dev_supports_audio_input(json_t *m);

#endif /* MODELS_DEV_INFO_H */
