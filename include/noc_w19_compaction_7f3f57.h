/* noc_w19_compaction_7f3f57.h - public C API for persistence/workers/compaction.py port. */
#ifndef NOC_W19_COMPACTION_7F3F57_H
#define NOC_W19_COMPACTION_7F3F57_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_compaction_7f3f57_rows @ persistence/workers/compaction.py:_rows
 * None -> null; a sequence -> [dict(row) for row in rows]. */
json_t *w19_compaction_7f3f57_rows(json_t *rows);

/* PoP: w19_compaction_7f3f57_compaction_payload @ persistence/workers/compaction.py:WorkerCompaction.payload
 * The nine keys of a compression.archive payload. */
json_t *w19_compaction_7f3f57_compaction_payload(
    json_t *messages, json_t *model_config_patch, int has_watermark, long long watermark,
    const char *lock_holder, long long tail_count, json_t *carried_messages,
    int has_carried_messages, json_t *covered_ids, json_t *unresolved_held, json_t *held_for_cas);

/* PoP: w19_compaction_7f3f57_publication_payload @ persistence/workers/compaction.py:WorkerCompressionPublication.payload
 * The fourteen keys of a compression.publish payload. */
json_t *w19_compaction_7f3f57_publication_payload(
    const char *child_session_id, const char *source, json_t *messages, const char *model,
    json_t *model_config, const char *system_prompt, const char *cwd, const char *profile_name,
    const char *compression_lock_holder, int require_compression_lease, int require_lease_refresh,
    double lease_ttl_seconds, int has_watermark, long long watermark,
    int has_watermark_ceiling, long long watermark_ceiling);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_COMPACTION_7F3F57_H */
