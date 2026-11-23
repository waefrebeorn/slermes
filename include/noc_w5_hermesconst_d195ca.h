#ifndef NOC_W5_HERMESCONST_D195CA_H
#define NOC_W5_HERMESCONST_D195CA_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes hermes_constants_scratch.py (gate w5_hermesconst_d195ca).
 * The pure path/ancestry predicates and the tri-state idle scan are ported;
 * filesystem/process side effects are injected from the fixture. */

/* PoP: w5_hermesconst_d195ca_under @ hermes_constants_scratch.py:_under
 * path == root or path.startswith(root.rstrip(sep) + sep). */
struct json_t *w5_hermesconst_d195ca_under(const char *path, const char *root, const char *sep);

/* PoP: w5_hermesconst_d195ca_same_birth @ hermes_constants_scratch.py:_same_birth
 * now/started are injected birth stamps; has_proc selects exact-vs-2s tolerance. */
struct json_t *w5_hermesconst_d195ca_same_birth(double now, double started, int has_proc);

/* PoP: w5_hermesconst_d195ca_scan_subtree @ hermes_constants_scratch.py:_scan_subtree
 * in carries root_stat_ok/root_mtime/root_is_dir/root_is_symlink/cutoff/path and
 * an `entries` array of child records (dir, path, mtime, nlink, is_dir, is_file,
 * stat_ok, perm_err, scandir_fail). Returns {"state":str,"denied_dir":str|null}. */
struct json_t *w5_hermesconst_d195ca_scan_subtree(const struct json_t *in);

/* PoP: w5_hermesconst_d195ca_subtree_touch_state @ hermes_constants_scratch.py:subtree_touch_state
 * Same injected walk; returns just the state string. */
struct json_t *w5_hermesconst_d195ca_subtree_touch_state(const struct json_t *in);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_HERMESCONST_D195CA_H */
