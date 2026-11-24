/*
 * noc_w26_sessioncom_fd19d0.h - C surface for upstream
 * gateway/session_completion_notes.py (gate w26_sessioncom_fd19d0).
 *
 * The pure decision core of "a completion nobody is watching becomes a note":
 * which producer an admission identity names, whether a local completion row is
 * note-eligible at all, and whether a resume/failed-delegation wake still has
 * budget with no viewer attached.  Every I/O seam (the admission queue, the
 * persistence store, the close registry, the event stream) stays in the caller.
 */
#ifndef NOC_W26_SESSIONCOM_FD19D0_H
#define NOC_W26_SESSIONCOM_FD19D0_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* identity: a JSON-encoded descriptor identity string -> its first element, or
 * null when it does not parse / is not indexable (json.loads(identity)[0]). */
json_t *w26_sessioncom_fd19d0_producer(const json_t *identity);

/* row: a local completion admission -> true when its descriptor names a noted
 * producer.  A non-object row / non-object payload is the empty mapping. */
json_t *w26_sessioncom_fd19d0_noted_completion(const json_t *row);

/* req: {descriptor:{...}, closed:bool, unattended_wakes:int} ->
 * {wakes: bool, unattended_wakes: int}.  ``closed`` is the fixed input standing
 * in for _closed_by_user(); the wake counter is threaded through as upstream does. */
json_t *w26_sessioncom_fd19d0_wakes(const json_t *req);

/* Module-level literal tables. */
json_t *w26_sessioncom_fd19d0_constants(void);

#ifdef __cplusplus
}
#endif
#endif
