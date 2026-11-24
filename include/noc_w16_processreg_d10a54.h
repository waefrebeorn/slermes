#ifndef NOC_W16_PROCESSREG_D10A54_H
#define NOC_W16_PROCESSREG_D10A54_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of tools/process_registry_stdin.py (gate w16_processreg_d10a54).
 * Session attributes and PTY availability are injected from the fixture. */

json_t *w16_processreg_d10a54_no_pty_reason(const char *pty_fallback,
                                            const char *output_log, int pty_available);
json_t *w16_processreg_d10a54_already_exited_error(int process_exists, int stdin_open,
                                                  const char *pty_fallback,
                                                  const char *output_log, int pty_available);
json_t *w16_processreg_d10a54_stdin_unavailable_error(int process_exists,
                                                     const char *pty_fallback,
                                                     const char *output_log, int pty_available);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_PROCESSREG_D10A54_H */
