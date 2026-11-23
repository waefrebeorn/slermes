#ifndef NOC_W11_STALEMODUL_BF7D5C_H
#define NOC_W11_STALEMODUL_BF7D5C_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w11_stalemodul_bf7d5c_drop_stale_root_modules @ hermes_cli/stale_modules.py:drop_stale_root_modules
 * required: null -> module default {"utils": (file_signature, file_cache_version)};
 *           object {name: [attr,...]} -> explicit checks mapping.
 * modules:  object {name: [present_attr,...]}; a missing key or JSON null means
 *           sys.modules.get(name) is None (skip). An empty array means the
 *           module exists but has no attributes.
 * Returns a JSON array of dropped names, in checks iteration order. */
struct json_t *w11_stalemodul_bf7d5c_drop_stale_root_modules(const struct json_t *required,
                                                             const struct json_t *modules);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_STALEMODUL_BF7D5C_H */
