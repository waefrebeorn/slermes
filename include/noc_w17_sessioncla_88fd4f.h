/*
 * noc_w17_sessioncla_88fd4f.h — public C API for the faithful C11 port of
 * upstream Hermes gateway/session_clarify_batch.py (gate w17_sessioncla_88fd4f).
 */
#ifndef NOC_W17_SESSIONCLA_88FD4F_H
#define NOC_W17_SESSIONCLA_88FD4F_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_sessioncla_88fd4f_ask_clarify_batch @
 *      gateway/session_clarify_batch.py:ask_clarify_batch
 * Returns {"answers": {...}, "outcome": "..."}.  Observable ask_viewer_result
 * arguments (including all wire questions, qids, timeout and cancelled value)
 * are recorded into `state`. */
json_t *w17_sessioncla_88fd4f_ask_clarify_batch(json_t *turn, json_t *authority,
        json_t *questions, json_t *io, json_t *state);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W17_SESSIONCLA_88FD4F_H */
