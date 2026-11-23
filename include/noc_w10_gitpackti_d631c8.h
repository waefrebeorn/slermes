#ifndef NOC_W10_GITPACKTI_D631C8_H
#define NOC_W10_GITPACKTI_D631C8_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_gitpackti_d631c8_is_redundant @ hermes_cli/git_pack_tidy.py:_is_redundant
 * oids/others are JSON arrays of hex-free oid strings; deadline never fires (fixtures are bounded).
 * Returns whether every oid is present in at least one of the other packs. */
int w10_gitpackti_d631c8_is_redundant(const struct json_t *oids, const struct json_t *others);
/* PoP: w10_gitpackti_d631c8_index_contains @ hermes_cli/git_pack_tidy.py:_Index.__contains__
 * Binary search a v2 SHA-1 pack index supplied as hex bytes for a 20-byte oid (hex). */
int w10_gitpackti_d631c8_index_contains(const char *idx_hex, const char *oid_hex);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_GITPACKTI_D631C8_H */
