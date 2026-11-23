#ifndef NOC_W5_RECOVERY_15F063_H
#define NOC_W5_RECOVERY_15F063_H

/* C home for upstream Hermes pm/recovery.py — gate w5_recovery_15f063. */
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_recovery_15f063_startup_script @ pm/recovery.py:validate_environment
 * Returns the exact startup-validation script string the candidate interpreter
 * is asked to run (malloc'd; caller frees). Reproduced byte-for-byte from the
 * live f-string template with the live STARTUP_IMPORTS tuple. */
char *w5_recovery_15f063_startup_script(void);

/* PoP: w5_recovery_15f063_normalize_dependency @ pm/recovery.py:validate_environment
 * Mirrors re.split(r'[\[<>=!~; @]', dep, 1)[0].lower().replace('_','-').
 * Returns malloc'd string; caller frees. */
char *w5_recovery_15f063_normalize_dependency(const char *dep);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_RECOVERY_15F063_H */
