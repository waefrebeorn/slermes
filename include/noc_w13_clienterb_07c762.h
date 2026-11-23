#ifndef NOC_W13_CLIENTERB_07C762_H
#define NOC_W13_CLIENTERB_07C762_H

#include "hermes_json.h"

/* C port of the fast-input-burst state machine in
 * hermes_cli/cli_enter_burst.py (gate w13_clienterb_07c762).
 *
 * Time and the cli/event/buf object state are injected as explicit args (the
 * live module uses monotonic() and mutable attributes) so the decision logic
 * replays deterministically. All returned json_t* are new refs.
 *
 * note_text_change -> {"ret":<now>,"burst_at":<num>,"held":<bool>}
 * handle_burst_enter -> {"ret":<bool>,"doc":<str>,"held":<bool>,
 *                        "burst_at":<num>,"submit":<int>,"scheduled":<bool>,
 *                        "post_doc":<str>,"post_held":<bool>,"post_submit":<int>}
 */
json_t *w13_clienterb_07c762_note_text_change(double now,
                                              double last_text_change,
                                              double paste_burst_at,
                                              int held_present);

json_t *w13_clienterb_07c762_handle_burst_enter(
    const char *doc,
    double now, double last_text_change, double paste_burst_at, int held_present,
    int has_key_processor, int has_loop, int input_queue_nonempty, int app_running,
    int settle, int clear_held, int edit_doc, int change_last);

#endif /* NOC_W13_CLIENTERB_07C762_H */
