/*
 * noc_w27_adapter_1f12c3_p14.h - C surface for upstream
 * plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p14).
 *
 * forum titles, REST response seams, YAML seeding (adapter.py:6811-7188).
 * PORTED (pure): the forum post title derivation (first line, heading markers
 * stripped, 100-char cap), the standalone HTTP response encoding reader, the
 * missing-media warning text and the config.yaml discord: -> extra seeder.
 * SEAM: the t() forum default title (supplied by the fixture), aiohttp response
 * objects and the YAML->env bridge (env writes are dropped; only the returned
 * extra is compared).
 */
#ifndef NOC_W27_ADAPTER_1F12C3_P14_H
#define NOC_W27_ADAPTER_1F12C3_P14_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_1f12c3_p14_derive_forum_thread_name(const json_t *message, const json_t *default_title);
json_t *w27_adapter_1f12c3_p14_standalone_response_encoding(const json_t *encoding);
json_t *w27_adapter_1f12c3_p14_standalone_warn_missing_media(const json_t *path);
json_t *w27_adapter_1f12c3_p14_apply_yaml_config(const json_t *yaml_cfg, const json_t *discord_cfg);

#ifdef __cplusplus
}
#endif
#endif

