/*
 * noc_w30_hostedroom_fe22db_p2.h - C surface for upstream
 * persistence/sqlite/gateway/hosted_room_attachments.py (gate
 * w30_hostedroom_fe22db_p2): the commit, retention and read fences.
 *
 * The canonical manifest, the retention fences inside the event append, the
 * viewer authority fence, the committed/expired/owned/published/recipient
 * fences, the blob slice-length fence and the quotas the verbs apply.  The
 * connection, the blob directory and the store handle are seams, fed as a
 * ``db`` snapshot and a declared file size.
 */
#ifndef NOC_W30_HOSTEDROOM_FE22DB_P2_H
#define NOC_W30_HOSTEDROOM_FE22DB_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {} -> every quota/TTL the attachment verbs apply, plus the current columns. */
json_t *w30_hostedroom_fe22db_p2_constants(const json_t *in);

/* {"manifest": <any>} -> {"ok":true,"value":[{attachment_id,kind,name,size,mime}...]}
 * or the AttachmentError refusal of a non-canonical manifest. */
json_t *w30_hostedroom_fe22db_p2_manifest_validate(const json_t *in);

/* {"db": {...}, "room_id": str, "event_id": str, "manifest": [...], "now": float}
 * -> {"ok":true,"value":{"rows":[{attachment_id,state,expires_at,updated_at}...]}}
 * or the conflict the event append raises. */
json_t *w30_hostedroom_fe22db_p2_retain(const json_t *in);

/* {"db": {...}, "room_id": str, "authority_gateway_id": str,
 *  "authority_epoch": int} -> {"ok":true,"value":null} or the refusal. */
json_t *w30_hostedroom_fe22db_p2_require_viewer_room(const json_t *in);

/* {"db": {...}, "room_id": str, "attachment_id": str,
 *  "recipient_member_id": str|null, "event_id": str|null, "viewer": bool,
 *  "reconciled": bool, "now": float} -> {"ok":true,"value":<envelope>} or the
 * refusal of a row this caller may not read. */
json_t *w30_hostedroom_fe22db_p2_read_gate(const json_t *in);

/* {"size": int, "offset": int, "length": int, "file_size": int|null,
 *  "blob_id": str} -> {"ok":true,"value":{"bytes":n}} or the integrity refusal. */
json_t *w30_hostedroom_fe22db_p2_range_slice(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
