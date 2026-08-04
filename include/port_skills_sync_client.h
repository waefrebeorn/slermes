#ifndef PORT_SKILLS_SYNC_CLIENT_H
#define PORT_SKILLS_SYNC_CLIENT_H

#include <stddef.h>
#include <stdbool.h>

#include "hermes_json.h"

/* ── Cluster 1: manifest + identity + config + eligibility ──────────────
 * Port of tools/skills_sync_client.py (L118-L547). */

/* PoP: build_sync_manifest_bytes @ tools/skills_sync_client.py:build_sync_manifest_bytes */
char *skills_sync_build_manifest_bytes(const char *const *names,
                                       const bool *enabled, size_t count,
                                       size_t *out_len);

/* PoP: parse_sync_manifest @ tools/skills_sync_client.py:parse_sync_manifest */
json_t *skills_sync_parse_manifest(const char *data, size_t len);

/* PoP: wire_address @ tools/skills_sync_client.py:wire_address */
char *skills_sync_wire_address(const unsigned char *data, size_t len);

/* PoP: canonical_json_bytes @ tools/skills_sync_client.py:canonical_json_bytes */
char *skills_sync_canonical_json_bytes(json_t *obj, size_t *out_len);

/* PoP: _decode_jwt_payload_unverified @ tools/skills_sync_client.py:_decode_jwt_payload_unverified */
json_t *skills_sync_decode_jwt_payload(const char *token);

/* PoP: resolve_identity @ tools/skills_sync_client.py:resolve_identity */
json_t *skills_sync_resolve_identity(void);

/* PoP: dev_gate_open @ tools/skills_sync_client.py:dev_gate_open */
bool skills_sync_dev_gate_open(void);

/* PoP: resolve_sync_base_url @ tools/skills_sync_client.py:resolve_sync_base_url */
char *skills_sync_resolve_base_url(void);

/* PoP: sync_feature_enabled @ tools/skills_sync_client.py:sync_feature_enabled */
bool skills_sync_feature_enabled(void);

/* PoP: sync_org_auto_propose @ tools/skills_sync_client.py:sync_org_auto_propose */
bool skills_sync_org_auto_propose(void);

/* PoP: sync_default_opt_in @ tools/skills_sync_client.py:sync_default_opt_in */
bool skills_sync_default_opt_in(void);

/* PoP: is_sync_eligible @ tools/skills_sync_client.py:is_sync_eligible */
bool skills_sync_is_eligible(const char *skill_name);

/* PoP: list_synced_skill_names @ tools/skills_sync_client.py:list_synced_skill_names */
char **skills_sync_list_synced_skill_names(size_t *out_count);

/* PoP: _all_local_skill_names @ tools/skills_sync_client.py:_all_local_skill_names */
char **skills_sync_all_local_skill_names(size_t *out_count);

#endif /* PORT_SKILLS_SYNC_CLIENT_H */
