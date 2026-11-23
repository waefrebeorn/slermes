#ifndef NOC_W14_LOCALVIEWE_D6B45A_H
#define NOC_W14_LOCALVIEWE_D6B45A_H

#include "hermes_json.h"

/* C port of the local-viewer progress payload projection in
 * gateway/local_viewer_events.py (gate w14_localviewe_d6b45a).
 *
 * `publish_progress`'s reasoning.available / moa.* branch is pure once the two
 * live predicates (show_reasoning / classic_cli_turn) are injected as ints:
 * the C function builds exactly the payload the live function hands to
 * turn._publish_execution. Returns a NEW json_t*:
 *   {"emit": bool, "event_type": <str>|null, "payload": <obj>|null}
 *
 * Scope: subagent.text is a no-op and is mirrored; tool.progress and the
 * subagent.* dispatch to _publish_subagent need the live registry and are not
 * projected here (no fixture exercises them).
 */
json_t *w14_localviewe_d6b45a_publish_progress(const char *event_type,
                                               const char *name,
                                               const char *preview,
                                               json_t *kwargs,
                                               int show_reasoning,
                                               int classic_cli);

#endif /* NOC_W14_LOCALVIEWE_D6B45A_H */
