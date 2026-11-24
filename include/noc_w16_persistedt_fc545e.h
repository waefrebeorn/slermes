#ifndef NOC_W16_PERSISTEDT_FC545E_H
#define NOC_W16_PERSISTEDT_FC545E_H

#include "hermes_json.h"

/* Faithful C11 port of upstream agent/persisted_turn_receipt.py.
 * The caller-supplied prefix_row_matches predicate is pinned to exact JSON
 * object equality by the oracle seam. */
json_t *w16_persistedt_fc545e_committed_row_id(const json_t *message);
json_t *w16_persistedt_fc545e_receipt(const json_t *messages, const char *start,
                                      const json_t *history, const char *raw,
                                      const char *status, int compression_unchanged);

#endif /* NOC_W16_PERSISTEDT_FC545E_H */
