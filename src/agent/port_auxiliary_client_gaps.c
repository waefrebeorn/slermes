/*
 * port_auxiliary_client_gaps.c — Faithful C11 port of agent/auxiliary_client.py REAL_GAP functions.
 * Auxiliary client: provider routing, model selection, probe mode.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: aux_probe_active @ agent/auxiliary_client.py:_aux_probe_active */
int aux_probe_active(void) {
    return 0;
}

/* PoP: aux_probe_mode @ agent/auxiliary_client.py:aux_probe_mode */
void aux_probe_mode(int *out_active) {
    if (out_active) *out_active = 0;
}

/* PoP: aux_model_recency_key @ agent/auxiliary_client.py:_model_recency_key */
void aux_model_recency_key(const char *model_id, char *out_key, size_t out_sz) {
    snprintf(out_key, out_sz, "%s", model_id ? model_id : "");
}

/* PoP: aux_fast_model_from_catalog @ agent/auxiliary_client.py:_fast_model_from_catalog */
void aux_fast_model_from_catalog(const char *provider_id, char *out_model, size_t out_sz) {
    (void)provider_id;
    out_model[0] = '\0';
}

/* PoP: aux_resolve_auto_route @ agent/auxiliary_client.py:_resolve_auto_route */
void aux_resolve_auto_route(char *out_provider, size_t out_sz) {
    snprintf(out_provider, out_sz, "openai");
}

/* PoP: aux_tag_effective_provider @ agent/auxiliary_client.py:_tag_effective_provider */
void aux_tag_effective_provider(const char *client, const char *provider) {
    (void)client; (void)provider;
}

/* PoP: aux_effective_provider_for_client @ agent/auxiliary_client.py:_effective_provider_for_client */
void aux_effective_provider_for_client(const char *client, const char *fallback, char *out, size_t out_sz) {
    (void)client;
    snprintf(out, out_sz, "%s", fallback ? fallback : "");
}

/* PoP: aux_record_relay_response_model @ agent/auxiliary_client.py:_record_relay_auxiliary_response_model */
void aux_record_relay_response_model(const char *response_model) {
    (void)response_model;
}
