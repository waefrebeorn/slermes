#ifndef NOC_W12_BOOTBOOTST_5D9F10_H
#define NOC_W12_BOOTBOOTST_5D9F10_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_bootbootst_5d9f10_owed_config_migration @ hermes_cli/boot_bootstrap.py:owed_config_migration */
struct json_t *w12_bootbootst_5d9f10_owed_config_migration(const struct json_t *known);

/* PoP: w12_bootbootst_5d9f10_owed_steps @ hermes_cli/boot_bootstrap.py:_owed_steps */
struct json_t *w12_bootbootst_5d9f10_owed_steps(const struct json_t *known);

/* PoP: w12_bootbootst_5d9f10_read_last_known @ hermes_cli/boot_bootstrap.py:read_last_known
 * The file's decoded text is injected; BOM stripping and dict validation are ported. */
struct json_t *w12_bootbootst_5d9f10_read_last_known(const char *text);

/* PoP: w12_bootbootst_5d9f10_current_install_identity @ hermes_cli/boot_bootstrap.py:current_install_identity
 * git HEAD, .git presence, and install stamp are injected as fixture data. */
struct json_t *w12_bootbootst_5d9f10_current_install_identity(int has_git, const struct json_t *git_head,
                                                              const struct json_t *stamp);

/* PoP: w12_bootbootst_5d9f10_needs_bootstrap @ hermes_cli/boot_bootstrap.py:needs_bootstrap
 * The resolved identity and the record contents are injected. */
struct json_t *w12_bootbootst_5d9f10_needs_bootstrap(const struct json_t *identity,
                                                     const struct json_t *known);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_BOOTBOOTST_5D9F10_H */
