#ifndef NOC_W21_GATEWAYCHA_3D5133_H
#define NOC_W21_GATEWAYCHA_3D5133_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/gateway_chat_listing.py (gate w21_gatewaycha_3d5133).
 *
 * `--list-tools` / `--list-toolsets` on the gateway path: the toolset
 * selection (explicit `--toolsets` wins, else this profile's CLI toolsets minus
 * `agent.disabled_toolsets`) and the exact listing text, including the
 * `:<20` / `:>2` column formats, the first-sentence description cut and the
 * `unknown` toolset bucket. */

/* PoP: w21_gatewaycha_3d5133_render @ hermes_cli/gateway_chat_listing.py:print_tool_listing
 * `print_tool_listing(args)` as a value:
 *   case = {
 *     "toolsets": <str|array|null>,        # args.toolsets
 *     "list_tools": bool,                  # args.list_tools
 *     "config": {"platform_tools": [...],  # _get_platform_tools(config, "cli")
 *                "disabled_toolsets": [...]},   # config["agent"]["disabled_toolsets"]
 *     "tools": [{"name":..., "description":..., "toolset":...|null}],
 *     "all_toolsets": ["name", ...],       # toolsets.get_all_toolsets()
 *     "toolsets_info": {"name": {"tool_count": n, "description": "..."}},
 *     "i18n": {"cli.tools.header": "...", ...}   # message catalog seam
 *   }
 * Answer: {"lines": [...printed lines...], "rc": 0}. */
json_t *w21_gatewaycha_3d5133_render(const json_t *case_);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_GATEWAYCHA_3D5133_H */
