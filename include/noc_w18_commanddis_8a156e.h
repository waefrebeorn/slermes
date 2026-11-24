#ifndef NOC_W18_COMMANDDIS_8A156E_H
#define NOC_W18_COMMANDDIS_8A156E_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of tui_gateway/command_discovery.py (gate w18_commanddis_8a156e).
 * Pure slash-discovery builders: catalog item shape, /details root metadata
 * and the /details argument-completion table. */

/* PoP: w18_commanddis_8a156e_item @ tui_gateway/command_discovery.py:_item
 * display == NULL means "not supplied" -> Python's display=None -> text. */
json_t *w18_commanddis_8a156e_item(const char *text, const char *meta, const char *display);
/* PoP: w18_commanddis_8a156e_details_root_meta @ tui_gateway/command_discovery.py:_details_root_meta */
json_t *w18_commanddis_8a156e_details_root_meta(const char *candidate);
/* PoP: w18_commanddis_8a156e_details_completions @ tui_gateway/command_discovery.py:_details_completions
 * Returns a JSON array, or JSON null when text is not the /details command. */
json_t *w18_commanddis_8a156e_details_completions(const char *text);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_COMMANDDIS_8A156E_H */
