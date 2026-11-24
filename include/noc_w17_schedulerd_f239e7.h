#ifndef NOC_W17_SCHEDULERD_F239E7_H
#define NOC_W17_SCHEDULERD_F239E7_H

#include "hermes_json.h"

/* Port of _send_media_via_adapter with all collaborators injected via `media`
 * entries. Each entry is an object:
 *   path (str), voice (bool), audio (bool: should_send_media_as_audio result),
 *   kept (bool: filter_media_delivery_paths decision),
 *   validate (str|null: validate_media_delivery_path result),
 *   result (str: "ok"|"ambiguous"|"failed"|"no_future"|"timeout").
 * Returns {"errors":[str,...], "calls":[{"method","path_kw","path"}],
 *          "ack_lost":bool}. */
json_t *w17_schedulerd_f239e7_send_media(json_t *media, int force_document,
                                         const char *platform, const char *chat_id);

#endif
