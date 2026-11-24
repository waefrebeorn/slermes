#ifndef NOC_W16_TERMINALTO_F0D0D6_H
#define NOC_W16_TERMINALTO_F0D0D6_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of tools/terminal_tool_backends.py (gate w16_terminalto_f0d0d6).
 * Config-to-kwargs shapers and the two pure requirement checkers. */

json_t *w16_terminalto_f0d0d6_ssh_config(const json_t *config);
json_t *w16_terminalto_f0d0d6_local_config(const json_t *config);
json_t *w16_terminalto_f0d0d6_container_config(const json_t *config);
json_t *w16_terminalto_f0d0d6_resources(const json_t *cc);
json_t *w16_terminalto_f0d0d6_is_supported_vercel_runtime(const char *runtime);
json_t *w16_terminalto_f0d0d6_check_vercel(const json_t *config, int vercel_installed,
                                          const json_t *secrets);
json_t *w16_terminalto_f0d0d6_ssh_pre(const json_t *config);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_TERMINALTO_F0D0D6_H */
