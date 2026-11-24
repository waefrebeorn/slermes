#ifndef NOC_W20_STORE_EF85F3_H
#define NOC_W20_STORE_EF85F3_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/teams_pipeline/store.py (gate w20_store_ef85f3).
 *
 * The store is a JSON file behind a lock: every read is a deepcopy and every
 * write an atomic replace. The parts that decide *what* lands on disk are pure
 * -- the load normalisation, the upsert merge, the notification receipt key and
 * the receipt de-duplication -- and are ported here. The locking, the temp-file
 * replace and the path resolution stay in the Python store. */

/* The five persisted top-level buckets, in source order. */
#define W20_STORE_EF85F3_BUCKET_COUNT 5

/* PoP: w20_store_ef85f3_bucket_name @ plugins/teams_pipeline/store.py:_BUCKETS
 * _BUCKETS[i]; NULL past the end (subscriptions, notification_receipts,
 * event_timestamps, jobs, sink_records). */
const char *w20_store_ef85f3_bucket_name(int i);

/* PoP: w20_store_ef85f3_load_state @ plugins/teams_pipeline/store.py:TeamsPipelineStore._load
 * `_load` for a store file's *text* (the file read is the caller's business):
 * utf-8-sig decode (a BOM never reaches the parser), `json.loads(text or
 * "{}")`, a non-dict document keeps the
 * fresh empty buckets, and each bucket becomes dict(value or {}). A falsy
 * bucket value is {}; a truthy non-object one is reported as "parse_error"
 * (Python's dict() also accepts a sequence of pairs -- out of scope here).
 * Answer is
 * {"buckets": {bucket: {...}}, "stats": {bucket: count}}; a parse failure
 * yields {"buckets": {...empty...}, "stats": {...0...}, "parse_error": true}
 * because the live json.loads raises there and nothing is assigned. */
json_t *w20_store_ef85f3_load_state(const char *text);

/* PoP: w20_store_ef85f3_receipt_key @ plugins/teams_pipeline/store.py:TeamsPipelineStore.build_notification_receipt_key
 * `"id:<id>"` when the notification carries a truthy "id", else
 * `"sha256:" + sha256(json.dumps(n, sort_keys=True, separators=(",", ":")))`.
 * Malloc'd; caller frees; NULL only on allocation failure. */
char *w20_store_ef85f3_receipt_key(const json_t *notification);

/* PoP: w20_store_ef85f3_id_field @ plugins/teams_pipeline/store.py:upsert_subscription
 * The partialmethod bindings: subscriptions -> subscription_id, jobs -> job_id,
 * sink_records -> sink_key. NULL for a bucket with no upsert binding (the
 * module binds only those three). */
const char *w20_store_ef85f3_id_field(const char *bucket);

/* PoP: w20_store_ef85f3_upsert @ plugins/teams_pipeline/store.py:TeamsPipelineStore._upsert
 * The merged record `_upsert` stores: {**existing, **payload}, the bucket's id
 * field forced to ``key``, created_at kept from the existing record (or stamped
 * ``now``), updated_at stamped ``now``. ``existing`` may be NULL/{} for a first
 * write. ``now`` is the _utc_now_iso seam. */
json_t *w20_store_ef85f3_upsert(const char *bucket, const json_t *existing,
                                const json_t *payload, const char *key, const char *now);

/* PoP: w20_store_ef85f3_record_receipt @ plugins/teams_pipeline/store.py:TeamsPipelineStore.record_notification_receipt
 * Records a notification once. Answer is the store's receipts bucket after the
 * call plus whether this delivery was new:
 *   {"added": false, "receipts": {...}}   already seen, nothing written
 *   {"added": true,  "receipts": {..., key: {"received_at": ..., "payload": ...}}}
 * ``received_at`` beats ``now``; a non-object payload is stored as it came. */
json_t *w20_store_ef85f3_record_receipt(const json_t *receipts, const char *key,
                                        const json_t *payload, const char *received_at,
                                        const char *now);

/* PoP: w20_store_ef85f3_history_bump @ plugins/teams_pipeline/store.py:_BUCKETS
 * (helper, not a translation) -- NULL unless the name is a persisted bucket. */
int w20_store_ef85f3_is_bucket(const char *name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_STORE_EF85F3_H */
