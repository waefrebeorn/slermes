#ifndef NOC_W17_PLUGINCOMM_D95180_H
#define NOC_W17_PLUGINCOMM_D95180_H

#include "hermes_json.h"

/* ask_viewer_result / get_clarify_timeout are injected: timeout is the raw
 * configured int (<=0 becomes the None the live code passes), result is the
 * injected result dict (null -> None).
 * Returns {"answer":str|null, "request":{...}}. */
json_t *w17_plugincomm_d95180_ask_viewers(const char *session_id, const char *question,
                                          json_t *labels, int timeout, json_t *result);

/* "bound" when the live chat_choices returns bind_asker(...), "none" when it
 * returns nullcontext() (asker already bound or a LOCAL source). */
const char *w17_plugincomm_d95180_chat_choices(int asker_bound, int is_local);

#endif
