/*
 * noc_w17_runturnfo_18196f.h — public C API for the faithful C11 port of
 * upstream Hermes gateway/run_turn_followup_ack.py (gate w17_runturnfo_18196f).
 *
 * All exported symbols are prefixed with the gate key verbatim.
 */
#ifndef NOC_W17_RUNTRUNFO_18196F_H
#define NOC_W17_RUNTRUNFO_18196F_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_runturnfo_18196f_followup_processing_hooks_apply @
 *      gateway/run_turn_followup_ack.py:_followup_processing_hooks_apply */
bool w17_runturnfo_18196f_followup_processing_hooks_apply(json_t *adapter, json_t *event);

/* PoP: w17_runturnfo_18196f_followup_cancel_outcome @
 *      gateway/run_turn_followup_ack.py:_followup_cancel_outcome
 * Returns "CANCELLED" or "FAILURE". `io` supplies the scripted
 * asyncio.current_task() result ({"$raise":"RuntimeError"} -> failure). */
const char *w17_runturnfo_18196f_followup_cancel_outcome(json_t *adapter, json_t *io);

/* PoP: w17_runturnfo_18196f_run_followup_processing_hook @
 *      gateway/run_turn_followup_ack.py:_run_followup_processing_hook
 * Returns a JSON array of recorded hook calls (0 or 1 element). */
json_t *w17_runturnfo_18196f_run_followup_processing_hook(json_t *adapter, json_t *event,
        const char *hook_name, json_t *extra_args, json_t *io);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W17_RUNTRUNFO_18196F_H */
