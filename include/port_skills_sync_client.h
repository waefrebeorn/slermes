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

/* ── Cluster 2: ObjectSet + tree/commit + device ID + wire client ──────
 * Port of tools/skills_sync_client.py (L558-L928). */

typedef struct ssc_object ssc_object_t;
typedef struct {
    ssc_object_t *head;
    size_t count;
} ssc_object_set_t;

typedef struct ssc_sync_client ssc_sync_client_t;

/* PoP: add @ tools/skills_sync_client.py:add */
char *ssc_object_set_add(ssc_object_set_t *set, const char *kind,
                         const unsigned char *data, size_t len);
void ssc_object_set_free(ssc_object_set_t *set);
/* PoP: __len__ @ tools/skills_sync_client.py:__len__ */
size_t ssc_object_set_len(const ssc_object_set_t *set);

/* PoP: build_tree @ tools/skills_sync_client.py:build_tree */
char *ssc_build_tree(const char *dir_path, ssc_object_set_t *objects,
                     long max_object_bytes, int *too_large);
/* PoP: build_commit @ tools/skills_sync_client.py:build_commit */
char *ssc_build_commit(const char *tree_hash,
                       const char *const *parents, size_t nparents,
                       const char *owner, const char *device,
                       const char *message, ssc_object_set_t *objects,
                       const char *ts);
/* PoP: stable_device_id @ tools/skills_sync_client.py:stable_device_id */
char *ssc_stable_device_id(void);
/* PoP: set_device_name @ tools/skills_sync_client.py:set_device_name */
int ssc_set_device_name(const char *name, char *out, size_t out_sz);

/* PoP: __init__ @ tools/skills_sync_client.py:__init__ */
ssc_sync_client_t *ssc_client_new(const char *base_url, const char *api_key,
                                  int timeout_sec);
void ssc_client_free(ssc_sync_client_t *c);
/* PoP: capabilities @ tools/skills_sync_client.py:capabilities */
json_t *ssc_client_capabilities(ssc_sync_client_t *c, int *out_status);
/* PoP: get_refs @ tools/skills_sync_client.py:get_refs */
json_t *ssc_client_get_refs(ssc_sync_client_t *c, const char *prefix,
                            bool org_scope, int *out_status);
/* PoP: get_object @ tools/skills_sync_client.py:get_object */
int ssc_client_get_object(ssc_sync_client_t *c, const char *obj_hash,
                          bool org_scope, char **out_kind,
                          unsigned char **out_data, size_t *out_len,
                          int *out_status);
/* PoP: get_commit_json @ tools/skills_sync_client.py:get_commit_json */
json_t *ssc_client_get_commit_json(ssc_sync_client_t *c, const char *hash,
                                   bool org_scope, int *out_status);
/* PoP: get_tree_json @ tools/skills_sync_client.py:get_tree_json */
json_t *ssc_client_get_tree_json(ssc_sync_client_t *c, const char *hash,
                                 bool org_scope, int *out_status);
/* PoP: put_objects @ tools/skills_sync_client.py:put_objects */
int ssc_client_put_objects(ssc_sync_client_t *c, const ssc_object_set_t *set,
                           bool org_scope, int *out_status);
/* PoP: cas_ref @ tools/skills_sync_client.py:cas_ref */
int ssc_client_cas_ref(ssc_sync_client_t *c, const char *name,
                       const char *from_hash, const char *to_hash,
                       char *out_actual, size_t actual_sz,
                       int *out_status, json_t **out_body);


/* Oracle/test accessors for the object set. */
ssc_object_t *ssc_object_set_head(const ssc_object_set_t *set);
const char *ssc_object_addr(const ssc_object_t *o);
const char *ssc_object_kind(const ssc_object_t *o);
const unsigned char *ssc_object_data(const ssc_object_t *o);
size_t ssc_object_len(const ssc_object_t *o);
const ssc_object_t *ssc_object_next(const ssc_object_t *o);

#endif /* PORT_SKILLS_SYNC_CLIENT_H */
