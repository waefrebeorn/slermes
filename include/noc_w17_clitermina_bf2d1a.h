#ifndef NOC_W17_CLITERMINA_BF2D1A_H
#define NOC_W17_CLITERMINA_BF2D1A_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/cli_terminal_mixin.py (gate w17_clitermina_bf2d1a).
 * Pure helpers extracted from the terminal repaint/resize mixin. */
json_t *w17_clitermina_bf2d1a_is_eio(int err);
json_t *w17_clitermina_bf2d1a_tmux_passthrough(const char *seq);
json_t *w17_clitermina_bf2d1a_rebuilds_scrollback(const json_t *cli_config);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_CLITERMINA_BF2D1A_H */
