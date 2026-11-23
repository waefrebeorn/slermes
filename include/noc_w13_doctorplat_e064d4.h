#ifndef NOC_W13_DOCTORPLAT_E064D4_H
#define NOC_W13_DOCTORPLAT_E064D4_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_doctorplat_e064d4_system_package_install_cmd @ hermes_cli/doctor_platform.py:_system_package_install_cmd
 * platform/sys.platform and termux/_is_termux() are injected by the fixture. */
struct json_t *w13_doctorplat_e064d4_system_package_install_cmd(const char *pkg,
                                                                const char *platform, int termux);

/* PoP: w13_doctorplat_e064d4_python_repair_hint @ hermes_cli/doctor_platform.py:_python_repair_hint
 * method = detect_install_method(PROJECT_ROOT), cmd = recommended_update_command_for_method(method),
 * nix = is_nix_install_method(method); all injected via the fixture. */
struct json_t *w13_doctorplat_e064d4_python_repair_hint(const char *method, const char *cmd, int nix);

/* PoP: w13_doctorplat_e064d4_sqlite_upgrade_hint @ hermes_cli/doctor_platform.py:_sqlite_upgrade_hint
 * install_method/cmd/nix injected (install_method is the explicit argument). */
struct json_t *w13_doctorplat_e064d4_sqlite_upgrade_hint(const char *method, const char *cmd, int nix);

/* PoP: w13_doctorplat_e064d4_tcc_grant_check @ hermes_cli/doctor_platform.py:check_macos_tcc_grants
 * dr = designated requirement string returned by _macos_desktop_dr; returns the ordered
 * [kind, message, detail-or-null] rows the check emits. */
struct json_t *w13_doctorplat_e064d4_tcc_grant_check(const char *dr);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_DOCTORPLAT_E064D4_H */
