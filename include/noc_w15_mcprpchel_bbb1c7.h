#ifndef NOC_W15_MCPRPCHEL_BBB1C7_H
#define NOC_W15_MCPRPCHEL_BBB1C7_H
/*
 * Public C API for the port of tui_gateway/mcp_rpc_helpers.py
 * (gate w15_mcprpchel_bbb1c7). All exported symbols are gate-prefixed.
 *
 * PoP: w15_mcprpchel_bbb1c7_summarize_server             @ tui_gateway/mcp_rpc_helpers.py:summarize_server
 * PoP: w15_mcprpchel_bbb1c7_server_configs_with_sources  @ tui_gateway/mcp_rpc_helpers.py:server_configs_with_sources
 * PoP: w15_mcprpchel_bbb1c7_record_mcp_add               @ tui_gateway/mcp_rpc_helpers.py:record_mcp_add
 *
 * Plugin discovery / OAuth probing / catalog telemetry are environment reads
 * that a pure C port cannot perform; the caller injects the already-resolved
 * facts (portable servers, owners, oauth token presence, enabled flag). All
 * projection, sorting and string decisions are reproduced exactly.
 */
#include "hermes_json.h"

/* summarize_server(name, cfg, plugin, oauth_tokens_present, server_enabled) */
json_t *w15_mcprpchel_bbb1c7_summarize_server(const char *name, const json_t *cfg,
        const char *plugin, int oauth_tokens_present, int server_enabled);

/* server_configs_with_sources(config_servers, portable, owners)
 * -> {"servers": {...}, "plugins": {...}} */
json_t *w15_mcprpchel_bbb1c7_server_configs_with_sources(const json_t *config_servers,
        const json_t *portable, const json_t *owners);

/* record_mcp_add(entry_present, entry_name, server_config, saved)
 * -> the (source, name, status, failure_class) reported to the catalog. */
json_t *w15_mcprpchel_bbb1c7_record_mcp_add(int entry_present, const char *entry_name,
        const json_t *server_config, int saved);

#endif /* NOC_W15_MCPRPCHEL_BBB1C7_H */
