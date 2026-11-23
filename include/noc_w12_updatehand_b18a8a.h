#ifndef NOC_W12_UPDATEHAND_B18A8A_H
#define NOC_W12_UPDATEHAND_B18A8A_H

#include "hermes_json.h"

/* PoP port of hermes_cli/update_handoff.py (gate w12_updatehand_b18a8a). */

/* is_post_swap_child with the env value injected (Windows CRT-safe seam). */
json_t *w12_updatehand_b18a8a_is_post_swap_child_ex(const char *env_value);
/* _takeover_request(payload, argv_tail[]) -> dict. */
json_t *w12_updatehand_b18a8a_takeover_request(const json_t *payload, const json_t *argv_tail);

#endif /* NOC_W12_UPDATEHAND_B18A8A_H */
