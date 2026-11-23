#ifndef NOC_W15_RELEASECHA_5F3AF5_H
#define NOC_W15_RELEASECHA_5F3AF5_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_releasecha_5f3af5_validate_name @ hermes_cli/release_channels.py:validate_name */
struct json_t *w15_releasecha_5f3af5_validate_name(const struct json_t *value);
/* PoP: w15_releasecha_5f3af5_validate_repository @ hermes_cli/release_channels.py:validate_repository */
struct json_t *w15_releasecha_5f3af5_validate_repository(const struct json_t *value);
/* PoP: w15_releasecha_5f3af5_require_sha256 @ hermes_cli/release_channels.py:require_sha256 */
struct json_t *w15_releasecha_5f3af5_require_sha256(const struct json_t *value);
/* PoP: w15_releasecha_5f3af5_require_commit @ hermes_cli/release_channels.py:require_commit */
struct json_t *w15_releasecha_5f3af5_require_commit(const struct json_t *value);
/* PoP: w15_releasecha_5f3af5_build_prefix @ hermes_cli/release_channels.py:build_prefix */
struct json_t *w15_releasecha_5f3af5_build_prefix(const struct json_t *value);
/* PoP: w15_releasecha_5f3af5_channel_key @ hermes_cli/release_channels.py:channel_key */
struct json_t *w15_releasecha_5f3af5_channel_key(const struct json_t *value);
/* PoP: w15_releasecha_5f3af5_package_versions @ hermes_cli/release_channels.py:package_versions */
struct json_t *w15_releasecha_5f3af5_package_versions(const char *sequence_decimal);
/* PoP: w15_releasecha_5f3af5_artifact_key @ hermes_cli/release_channels.py:artifact_key */
struct json_t *w15_releasecha_5f3af5_artifact_key(const struct json_t *value);
/* PoP: w15_releasecha_5f3af5_head @ hermes_cli/release_channels.py:_head */
struct json_t *w15_releasecha_5f3af5_head(const struct json_t *value);
/* PoP: w15_releasecha_5f3af5_validate_identity @ hermes_cli/release_channels.py:validate_identity */
struct json_t *w15_releasecha_5f3af5_validate_identity(const struct json_t *value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_RELEASECHA_5F3AF5_H */
