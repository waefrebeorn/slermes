/*
 * noc_w30_hostedroom_fe22db_p1.h - C surface for upstream
 * persistence/sqlite/gateway/hosted_room_attachments.py (gate
 * w30_hostedroom_fe22db_p1): row shape, ownership and reclamation.
 *
 * The attachment envelope, the blob id/path rules, the durable event that owns
 * an attachment, the reconciliation's naming rule, the orphan sweep's
 * selection and the expiry/disband/reference-count reclamation.  The sqlite
 * connection and the blob directory are seams: every input carries a ``db``
 * snapshot of the tables plus the names pre-created in the blob root.
 */
#ifndef NOC_W30_HOSTEDROOM_FE22DB_P1_H
#define NOC_W30_HOSTEDROOM_FE22DB_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"row": {...}, "idempotent": bool} -> {"ok":true,"value":{attachment_id,
 * kind, name, size, mime, sha256, state, created_at, idempotent[, event_id]}} */
json_t *w30_hostedroom_fe22db_p1_metadata(const json_t *in);

/* {"value": <any>} -> {"ok":true,"value": bool}; a non-string is a TypeError. */
json_t *w30_hostedroom_fe22db_p1_blob_id_ok(const json_t *in);

/* {"blob_id": str} -> {"ok":true,"value": "<blob file name>"} or the
 * AttachmentIntegrityError refusal of an invalid stored id. */
json_t *w30_hostedroom_fe22db_p1_blob_path(const json_t *in);

/* {"db": {...}, "room_id": <any>, "event_id": <any>} ->
 * {"ok":true,"value": null | {"payload_json": <any>}} */
json_t *w30_hostedroom_fe22db_p1_owner_event(const json_t *in);

/* {"db": {...}, "row": {room_id, event_id, attachment_id}} ->
 * {"ok":true,"value": bool} */
json_t *w30_hostedroom_fe22db_p1_named_by_owner_event(const json_t *in);

/* {"db": {"files": [name, ...], "blobs": [...]}} ->
 * {"ok":true,"value":{"removed":[name...],"remaining":[name...]}} */
json_t *w30_hostedroom_fe22db_p1_sweep_select(const json_t *in);

/* {"db": {rooms, attachments, blobs, files}, "now": float} ->
 * {"ok":true,"value":{"removed":n,"deleted_blobs":[...],"ref_counts":{...},
 *                     "attachments":[{attachment_id,state,expires_at}...],
 *                     "files":[...]}} */
json_t *w30_hostedroom_fe22db_p1_prune_plan(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
