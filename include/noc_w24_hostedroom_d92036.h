/*
 * noc_w24_hostedroom_d92036.h - C surface for upstream
 * gateway/hosted_room_work_lineage.py (gate w24_hostedroom_d92036).
 *
 * The passive lineage checks: which authority span a sequence lands in, whether
 * a committed event prefix verifies against those spans, whether a work
 * record's receipts name their original span, and the origin span of each task.
 * Reading the history and the events is a seam -- the caller hands in the rows
 * it already read, and every function here decides over them.
 */
#ifndef W24_HOSTEDROOM_D92036_H
#define W24_HOSTEDROOM_D92036_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"spans": [{"gateway_id", "epoch", "from_seq"}, ...], "seq": <int>} ->
 * the last span whose from_seq <= seq, or {"error": "precedes"}. */
json_t *w24_hostedroom_d92036_at_sequence(const json_t *req);

/* {"spans": <list|null>, "seq": <int>, "events": [{"seq", "authority_epoch",
 *  "actor_json", "kind"}, ...]} ->
 * {"ok": true, "spans": [...]} or {"ok": false, "error":
 *  "no_lineage"|"precedes"|"unverified"|"unavailable"|"decode"}. */
json_t *w24_hostedroom_d92036_prefix(const json_t *req);

/* {"record": {"tasks": [...], "receipts": [...]}, "spans": [...]} ->
 * {"ok": true} or {"ok": false, "error": "precedes"|"no_authority_span"|"key"}. */
json_t *w24_hostedroom_d92036_provenance(const json_t *req);

/* {"record": ..., "spans": [...]} -> {task_id: {"gateway_id", "epoch"}} or
 * {"error": "precedes"|"key"}. */
json_t *w24_hostedroom_d92036_task_origins(const json_t *req);

/* {"transfer_kinds": [...], "max_authority_spans": 1024}. */
json_t *w24_hostedroom_d92036_constants(void);

#ifdef __cplusplus
}
#endif
#endif
