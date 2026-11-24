/*
 * noc_w23_imagegener_6a7ba9.h - C surface for upstream
 * tools/image_generation_managed.py (gate w23_imagegener_6a7ba9).
 *
 * One managed ("Nous Subscription") image-generation selection, three gateways
 * behind it: the stored ``image_gen.model`` decides which one serves a request.
 *
 * Seams (injected, never decided here): the FAL and Krea catalogs are static
 * in-tree id lists (transcribed from tools/image_generation_catalog.py and
 * plugins/image_gen/krea), and the raw stored config values arrive as parsed
 * JSON. Nothing here reads a config file or a plugin registry.
 */
#ifndef NOC_W23_IMAGEGENER_6A7BA9_H
#define NOC_W23_IMAGEGENER_6A7BA9_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Gateway that serves model under the managed selection: "fal" / "krea" / "portal". */
const char *w23_imagegener_6a7ba9_backend_for_model(const json_t *model_id);

/* Gateway a request is dispatched to for the raw stored provider/model.
 * Returns NULL when a direct/BYO provider owns the request (Python None). */
const char *w23_imagegener_6a7ba9_route(const json_t *provider, const json_t *model_id);

/* The FAL/Krea id a Portal-shaped duplicate collapses onto, or NULL. */
const char *w23_imagegener_6a7ba9_portal_duplicate_of(const json_t *model_id);

/* True for a FAL catalog id that is really Krea-on-FAL (excluded from the row). */
int w23_imagegener_6a7ba9_is_fal_krea_entry(const json_t *model_id);

#ifdef __cplusplus
}
#endif
#endif
