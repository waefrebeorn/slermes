#ifndef NOC_W18_MAINPLATFO_97FB7B_H
#define NOC_W18_MAINPLATFO_97FB7B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: mainplatfo_yes_no @ hermes_cli/main_platform_setup.py:_yes_no
 * "[y/N]" decision on a (seam-injected) response; NULL response = EOF/Ctrl-C => no. */
int mainplatfo_yes_no(const char *response);

/* PoP: mainplatfo_whatsapp_mode @ hermes_cli/main_platform_setup.py:_whatsapp_choose_mode
 * Seam: current_mode is get_env_value("WHATSAPP_MODE"), choice is input() (NULL=EOF).
 * Returns a malloc'd mode string, or NULL for cancelled. */
char *mainplatfo_whatsapp_mode(const char *current_mode, const char *choice);

/* PoP: mainplatfo_has_paired_identity @ hermes_cli/main_platform_setup.py:_whatsapp_has_paired_identity
 * Seam: creds is the parsed creds.json object. */
int mainplatfo_has_paired_identity(const struct json_t *creds);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_MAINPLATFO_97FB7B_H */
