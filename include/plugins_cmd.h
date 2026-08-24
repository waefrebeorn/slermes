#ifndef HERMES_PLUGINS_CMD_H
#define HERMES_PLUGINS_CMD_H

/*
 * plugins_cmd.h — canonical C11 port of hermes_cli/plugins_cmd.py command
 * handlers and hermes_cli/plugin_packs.py pack command handlers.
 */

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* enable tri-state shared by install: -1 = prompt/absent, 0 = no, 1 = yes */
#define PCMD_ENABLE_PROMPT (-1)

/* PoP: cmd_install @ hermes_cli/plugins_cmd.py:cmd_install */
void cmd_install(const char *identifier, int force, int enable, const char *ref);

/* PoP: cmd_remove @ hermes_cli/plugins_cmd.py:cmd_remove */
void cmd_remove(const char *name);

/* allow_tool_override tri-state: -1 prompt (deny default), 0 decline, 1 grant */
/* PoP: cmd_enable @ hermes_cli/plugins_cmd.py:cmd_enable */
void cmd_enable(const char *name, int allow_tool_override);

/* PoP: cmd_disable @ hermes_cli/plugins_cmd.py:cmd_disable */
void cmd_disable(const char *name);

/* PoP: cmd_capabilities @ hermes_cli/plugins_cmd.py:cmd_capabilities */
void cmd_capabilities(const char *name);

/* flags mirror argparse: --json / --plain / (--no-bundled|--user) / --enabled */
/* PoP: cmd_list @ hermes_cli/plugins_cmd.py:cmd_list */
void cmd_list(int opt_json, int opt_plain, int opt_no_bundled_or_user,
              int opt_enabled_only);

/* PoP: cmd_toggle @ hermes_cli/plugins_cmd.py:cmd_toggle */
void cmd_toggle(void);

/* PoP: cmd_plugin_doctor @ hermes_cli/plugins_cmd.py:cmd_plugin_doctor */
void cmd_plugin_doctor(const char *target, int ci);

/* PoP: cmd_search @ hermes_cli/plugins_cmd.py:cmd_search */
void cmd_search(const char *term, int json_output, const char *capability,
                int refresh);

/* ---- plugin packs (hermes_cli/plugin_packs.py) ---- */

/* PoP: resolve_pack_plugins @ hermes_cli/plugin_packs.py:resolve_pack_plugins
 * pack: parsed pack document JSON. Returns a fresh JSON array; each element
 * is {display, ref, identifier|null, resolve_error|null, capabilities[]}.
 * Resolution failures are carried per-entry, never raised. */
json_t *resolve_pack_plugins(const json_t *pack);

/* PoP: render_pack_review @ hermes_cli/plugin_packs.py:render_pack_review */
void render_pack_review(const json_t *pack, const json_t *resolved);

/* PoP: install_pack_plugins @ hermes_cli/plugin_packs.py:install_pack_plugins
 * Returns fresh JSON array of {display, ok, installed_name|null, error|null}. */
json_t *install_pack_plugins(const json_t *pack, const json_t *resolved, int force);

/* PoP: cmd_pack_show @ hermes_cli/plugin_packs.py:cmd_pack_show */
void cmd_pack_show(const char *source);

/* PoP: cmd_pack_install @ hermes_cli/plugin_packs.py:cmd_pack_install */
void cmd_pack_install(const char *source, int force);

/* PoP: cmd_pack_export @ hermes_cli/plugin_packs.py:cmd_pack_export */
void cmd_pack_export(int enabled_only, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_PLUGINS_CMD_H */
