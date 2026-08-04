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

/* ── Cluster 3: sync state + materialize + snapshot + refs ─────────────
 * Port of tools/skills_sync_client.py (L946-L1252). */

/* PoP: read_sync_state @ tools/skills_sync_client.py:read_sync_state */
json_t *ssc_read_sync_state(void);
/* PoP: write_sync_state @ tools/skills_sync_client.py:write_sync_state */
int ssc_write_sync_state(json_t *data);
/* PoP: materialize_tree @ tools/skills_sync_client.py:materialize_tree */
int ssc_materialize_tree(ssc_sync_client_t *client, const char *tree_hash,
                         const char *dest, bool org_scope);
/* PoP: _skill_rel_path @ tools/skills_sync_client.py:_skill_rel_path */
const char *ssc_skill_rel_path(const char *skill_name, char *out, size_t out_sz);
/* PoP: snapshot_profile @ tools/skills_sync_client.py:snapshot_profile */
json_t *ssc_snapshot_profile(const char *const *skill_names, size_t n_names,
                             long max_object_bytes, ssc_object_set_t *objects);
/* PoP: _build_root_tree @ tools/skills_sync_client.py:_build_root_tree */
char *ssc_build_root_tree(json_t *node, ssc_object_set_t *objects,
                          const char *manifest_hash);
/* PoP: user_head_ref @ tools/skills_sync_client.py:user_head_ref */
void ssc_user_head_ref(const char *owner, char *out, size_t out_sz);
/* PoP: user_conflict_ref @ tools/skills_sync_client.py:user_conflict_ref */
void ssc_user_conflict_ref(const char *owner, int n, char *out, size_t out_sz);
/* PoP: _root_tree_of_commit @ tools/skills_sync_client.py:_root_tree_of_commit */
const char *ssc_root_tree_of_commit(ssc_sync_client_t *client,
                                    const char *commit_hash, bool org_scope);
/* PoP: _skill_trees_of_root @ tools/skills_sync_client.py:_skill_trees_of_root */
json_t *ssc_skill_trees_of_root(ssc_sync_client_t *client,
                                const char *root_tree_hash, bool org_scope);
/* PoP: read_manifest_of_root @ tools/skills_sync_client.py:read_manifest_of_root */
json_t *ssc_read_manifest_of_root(ssc_sync_client_t *client,
                                  const char *root_tree_hash);
/* PoP: _check_version @ tools/skills_sync_client.py:_check_version */
int ssc_check_version(json_t *caps);


/* ── Cluster 4: push/pull + three-way conflict + org sync ─────────────
 * Port of tools/skills_sync_client.py (L1258-L2188). */

/* PoP: _merge_skill @ tools/skills_sync_client.py:_merge_skill */
const char *ssc_merge_skill(const char *base, const char *ours,
                            const char *theirs);
/* PoP: _next_conflict_index @ tools/skills_sync_client.py:_next_conflict_index */
int ssc_next_conflict_index(ssc_sync_client_t *client, const char *owner);
/* PoP: _assemble_root_from_skill_trees @ tools/skills_sync_client.py:_assemble_root_from_skill_trees */
char *ssc_assemble_root_from_skill_trees(ssc_sync_client_t *client,
                                         json_t *skill_trees,
                                         ssc_object_set_t *objects);
/* PoP: _resolve_push_conflict @ tools/skills_sync_client.py:_resolve_push_conflict */
json_t *ssc_resolve_push_conflict(ssc_sync_client_t *client,
                                  json_t *identity, const char *actual_head,
                                  const char *our_root, const char *our_commit,
                                  ssc_object_set_t *objects,
                                  const char *const *skill_names,
                                  size_t n_skill_names, const char *message,
                                  const char *base_head);
/* PoP: push_skills @ tools/skills_sync_client.py:push_skills */
json_t *ssc_push_skills(ssc_sync_client_t *client, json_t *identity,
                        const char *const *skill_names, size_t n_skill_names,
                        const char *message);
/* PoP: pull_skills @ tools/skills_sync_client.py:pull_skills */
json_t *ssc_pull_skills(ssc_sync_client_t *client, json_t *identity);
/* PoP: _opted_in_rel_paths @ tools/skills_sync_client.py:_opted_in_rel_paths */
json_t *ssc_opted_in_rel_paths(void);
/* PoP: list_org_skill_names @ tools/skills_sync_client.py:list_org_skill_names */
json_t *ssc_list_org_skill_names(void);
/* PoP: org_head_ref @ tools/skills_sync_client.py:org_head_ref */
void ssc_org_head_ref(const char *org_id, char *out, size_t out_sz);
/* PoP: org_skill_is_locally_modified @ tools/skills_sync_client.py:org_skill_is_locally_modified */
bool ssc_org_skill_is_locally_modified(const char *skill_rel_path,
                                       const char *org_id);
/* PoP: list_locally_modified_org_skills @ tools/skills_sync_client.py:list_locally_modified_org_skills */
json_t *ssc_list_locally_modified_org_skills(const char *org_id);
/* PoP: pull_org_skills @ tools/skills_sync_client.py:pull_org_skills */
json_t *ssc_pull_org_skills(ssc_sync_client_t *client, json_t *identity);
/* PoP: propose_skill @ tools/skills_sync_client.py:propose_skill */
json_t *ssc_propose_skill(const char *skill_name, ssc_sync_client_t *client,
                          json_t *identity, const char *message);
/* PoP: resolve_org_identity @ tools/skills_sync_client.py:resolve_org_identity */
json_t *ssc_resolve_org_identity(void);
/* PoP: org_sync_available @ tools/skills_sync_client.py:org_sync_available */
bool ssc_org_sync_available(void);
/* PoP: maybe_push_skills @ tools/skills_sync_client.py:maybe_push_skills */
json_t *ssc_maybe_push_skills(const char *message);
/* PoP: maybe_pull_skills @ tools/skills_sync_client.py:maybe_pull_skills */
json_t *ssc_maybe_pull_skills(void);
/* PoP: sync_status @ tools/skills_sync_client.py:sync_status */
json_t *ssc_sync_status(void);
/* PoP: _clear_active_org_marker @ tools/skills_sync_client.py:_clear_active_org_marker */
void ssc_clear_active_org_marker(void);
/* PoP: maybe_pull_org_skills @ tools/skills_sync_client.py:maybe_pull_org_skills */
json_t *ssc_maybe_pull_org_skills(void);


#endif /* PORT_SKILLS_SYNC_CLIENT_H */
