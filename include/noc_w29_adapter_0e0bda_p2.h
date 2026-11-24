/*
 * noc_w29_adapter_0e0bda_p2.h - C surface for upstream
 * plugins/platforms/a2a/adapter.py, gate p2 (served-agent routing + Agent Card).
 *
 * The served-agent registry is supplied as a fixture (what
 * A2AAdapter._load_served_agents() would have produced); what this gate owns is
 * everything decided afterwards: longest-prefix path routing, v1.0 tenant
 * routing, the base/public URL, the served-agent summary and the Agent Card the
 * adapter serves.  The tool registry is a seam: the card's skills come from the
 * static fallback (protocol.skills_from_toolsets), as they do in a process with
 * no registered toolsets.
 */
#ifndef NOC_W29_ADAPTER_0E0BDA_P2_H
#define NOC_W29_ADAPTER_0E0BDA_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Host:port the adapter advertises; an IPv6 literal is bracketed. */
json_t *w29_adapter_0e0bda_base_url(const json_t *host, const json_t *port, const json_t *public_url);

/* agents: [{slug,path,tenant,profile,local,name,description,...}, ...] ->
 * {slug, subpath} for the longest matching path prefix, root as fallback. */
json_t *w29_adapter_0e0bda_route_for_path(const json_t *agents, const json_t *raw_path);

/* Same plus v1.0 tenant routing: {slug,subpath} or {"error": <message>} when
 * params.tenant contradicts the routed agent's tenant. */
json_t *w29_adapter_0e0bda_route_for_request(const json_t *agents, const json_t *raw_path, const json_t *params);

/* [{slug,name,url,tenant,profile,local}, ...] for the served-agent list. */
json_t *w29_adapter_0e0bda_served_agent_summary(const json_t *agents, const json_t *host,
                                                const json_t *port, const json_t *public_url);

/* [slug, tenant] of the routed agent (empty strings for the root agent). */
json_t *w29_adapter_0e0bda_scope_for_agent(const json_t *agents, const json_t *agent_slug);

/* The A2A v1.0 Agent Card. provider_org/provider_url are the captured
 * A2A_PROVIDER_ORG / A2A_PROVIDER_URL values (null => unset). */
json_t *w29_adapter_0e0bda_build_card(const json_t *agents, const json_t *agent_slug,
                                      const json_t *host, const json_t *port,
                                      const json_t *public_url, const json_t *agent_name,
                                      const json_t *localhost_only,
                                      const json_t *provider_org, const json_t *provider_url);

#ifdef __cplusplus
}
#endif
#endif
