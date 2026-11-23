#ifndef NOC_BRIDGEDENV_H
#define NOC_BRIDGEDENV_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: bridgedenv_digest @ gateway/bridged_env.py:bridged_env_digest
 * None -> NULL; else malloc'd "sha256:" + sha256(utf8(value)).hexdigest(). */
char *bridgedenv_digest(const char *value);

/* PoP: bridgedenv_drop_markers @ gateway/bridged_env.py:drop_bridge_markers
 * Returns a NEW object: every marker removed, and each still-owned value popped. */
struct json_t *bridgedenv_drop_markers(const struct json_t *env);

/* PoP: bridgedenv_follow @ gateway/bridged_env.py:follow_bridged_env
 * Explicit seam (Windows CRT cannot keep os.environ[name] == ""): `live`
 * stands in for os.environ and `grant` for bridged_allow_all_users() (JSON
 * null == None). Returns a NEW scope object; inputs are not mutated. */
struct json_t *bridgedenv_follow(const struct json_t *scope,
                                 const struct json_t *live,
                                 const struct json_t *grant);

#ifdef __cplusplus
}
#endif

#endif /* NOC_BRIDGEDENV_H */
