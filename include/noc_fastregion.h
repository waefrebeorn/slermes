#ifndef NOC_FASTREGION_H
#define NOC_FASTREGION_H
/*
 * Public C API for the port of hermes_cli/models_fast_regions.py (gate fastregion).
 * All exported symbols are module-prefixed to avoid collisions.
 */

/* True when a regional OpenAI host documents a fast tier for this model.
 * tier may be NULL (Python default None); only the exact string "ultrafast"
 * has special meaning. */
int fastregion_openai_fast_region_allows(const char *base_url,
                                         const char *model_id,
                                         const char *tier);

#endif /* NOC_FASTREGION_H */
