#ifndef NOC_W16_CLISESSION_F92C0B_H
#define NOC_W16_CLISESSION_F92C0B_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/cli_session_mixin.py (gate w16_clisession_f92c0b).
 * Prefix every exported symbol with the gate name. */

/* _squash(text, limit=120): collapse whitespace, cap with an ellipsis. */
json_t *w16_clisession_f92c0b_squash(const char *text, int limit);

/* CLISessionMixin._undo_content_to_text(content): flatten str / content-part list. */
json_t *w16_clisession_f92c0b_undo_content_to_text(const json_t *content);

/* CLISessionMixin._title_session_id seam: (session_id, compression_tip, db_present). */
json_t *w16_clisession_f92c0b_title_session_id(const char *session_id,
                                               const json_t *tip, int db_present);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W16_CLISESSION_F92C0B_H */
