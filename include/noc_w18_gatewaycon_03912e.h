#ifndef NOC_W18_GATEWAYCON_03912E_H
#define NOC_W18_GATEWAYCON_03912E_H

#include "hermes_json.h"

/* Faithful C11 port of upstream Hermes hermes_cli/gateway_connections.py
 * (gate w18_gatewaycon_03912e). Each exported symbol is prefixed with the
 * gate key. Arguments mirror the live helper signatures; NULL reproduces
 * Python's None. Strings are copied out of the JSON tree by the callee. */

json_t *w18_gatewaycon_03912e_revision(const json_t *doc);
json_t *w18_gatewaycon_03912e_plain(const json_t *secret);
json_t *w18_gatewaycon_03912e_envelope(const char *value);
json_t *w18_gatewaycon_03912e_token_preview(const char *value);
json_t *w18_gatewaycon_03912e_kept_envelope(const json_t *token, const json_t *existing);
json_t *w18_gatewaycon_03912e_public_connection(const json_t *entry);
json_t *w18_gatewaycon_03912e_public_registry(const json_t *reg);
json_t *w18_gatewaycon_03912e_signed_in_ids_gone(const json_t *before, const json_t *after);
json_t *w18_gatewaycon_03912e_incoming_headers(const json_t *raw, const json_t *existing);

#endif /* NOC_W18_GATEWAYCON_03912E_H */
