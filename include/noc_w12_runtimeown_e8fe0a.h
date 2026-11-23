#ifndef NOC_W12_RUNTIMEOWN_E8FE0A_H
#define NOC_W12_RUNTIMEOWN_E8FE0A_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w12_runtimeown_e8fe0a_foreign_lock_recovery @ gateway/runtime_ownership.py:foreign_lock_recovery
 * The recovery text for a lock file this user cannot even open read-only. */
struct json_t *w12_runtimeown_e8fe0a_foreign_lock_recovery(const struct json_t *path);
/* PoP: w12_runtimeown_e8fe0a_same_inode @ gateway/runtime_ownership.py:_same_inode
 * (st_dev, st_ino) equality of two {\"dev\":..,\"ino\":..} descriptors. */
struct json_t *w12_runtimeown_e8fe0a_same_inode(const struct json_t *left, const struct json_t *right);
/* PoP: w12_runtimeown_e8fe0a_canonical_home @ gateway/runtime_ownership.py:canonical_home
 * Path(os.path.normcase(str(Path(home).expanduser().resolve()))) on Windows: slash->backslash,
 * collapse . / .., lowercase. Absolute inputs only (no cwd dependence). */
struct json_t *w12_runtimeown_e8fe0a_canonical_home(const struct json_t *home);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_RUNTIMEOWN_E8FE0A_H */
