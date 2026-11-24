/*
 * noc_w26_receipt_cbecab.h - C surface for upstream
 * persistence/sqlite/workers/_receipt.py (gate w26_receipt_cbecab).
 *
 * The one worker receipt transaction and the closed runtime operations it runs.
 * The transaction, the connection, the epoch fence and the handlers themselves
 * are seams; what is ported is the admission gate every operation runs before
 * it touches a row, the row annotations the receipt hands back, and the
 * contract bounds the gate is made of.
 */
#ifndef NOC_W26_RECEIPT_CBECAB_H
#define NOC_W26_RECEIPT_CBECAB_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Largest encoded payload one receipt accepts, and the most rows one batch carries. */
#define W26_RECEIPT_CBECAB_PAYLOAD_MAX_BYTES (4u * 1024u * 1024u)
#define W26_RECEIPT_CBECAB_TRANSCRIPT_MAX 1000
#define W26_RECEIPT_CBECAB_TOKEN_MAX ((double)(1LL << 53))
#define W26_RECEIPT_CBECAB_COST_MAX 1e12
#define W26_RECEIPT_CBECAB_TEXT_MAX 4096
#define W26_RECEIPT_CBECAB_LEASE_TEXT_MAX 1024

/* req: {"op": <operation>, "payload": <any>, "row_exists": bool,
 *       "sequence": <int>, "operation": <str>, "handlers": [...]} ->
 * null when admitted, else the refusal code ("invalid_params" |
 * "permission_denied" | "STALE_EPOCH" is a conn seam and never returned). */
json_t *w26_receipt_cbecab_validate(const json_t *req);

/* req: {"messages": [...]} -> [{annotation key: value, ...}, ...] */
json_t *w26_receipt_cbecab_annotations(const json_t *req);

/* The contract vocabulary: message fields, annotation keys, operation names,
 * bounds. */
json_t *w26_receipt_cbecab_constants(void);

#ifdef __cplusplus
}
#endif
#endif
