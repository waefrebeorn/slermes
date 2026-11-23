#ifndef NOC_W13_MODELMETAD_9E75AB_H
#define NOC_W13_MODELMETAD_9E75AB_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_modelmetad_9e75ab_drop_context_cache_rows @ agent/model_metadata_cache.py:drop_context_cache_rows
 * Projects the cache-file write of one locked drop cycle. Takes the loaded
 * context-cache document and the stale keys; returns the document that
 * _write_context_cache would persist (context_lengths + optional
 * bedrock_confirmed_v1), or JSON null when no key was present and no write
 * happens. The cross-process lock and atomic YAML write are seams: the oracle
 * stubs _file_lock to succeed and captures atomic_yaml_write's document. */
struct json_t *w13_modelmetad_9e75ab_drop_context_cache_rows(const struct json_t *document,
                                                             const struct json_t *stale_keys);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_MODELMETAD_9E75AB_H */
