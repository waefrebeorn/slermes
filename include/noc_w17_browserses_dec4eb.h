#ifndef NOC_W17_BROWSERSES_DEC4EB_H
#define NOC_W17_BROWSERSES_DEC4EB_H

#include "hermes_json.h"

/* C home for upstream Hermes tools/browser_session_sandbox.py.
 * Pure devtools-endpoint parsing + launcher-key formatting and constants. */

/* _shared_key with the env object's id() injected explicitly: "sandbox:<hex>:<udd>". */
json_t *w17_browserses_dec4eb_shared_key(const char *env_id, const char *udd);

/* _read_devtools' parsing of a sandbox exec result: [port, path] or null. */
json_t *w17_browserses_dec4eb_read_devtools(int returncode, const char *stdout_text);

/* _live_devtools' decision tree: [port, path], "gone", or null.
 * raise_exec=1 models a run_in that raised (=> null). */
json_t *w17_browserses_dec4eb_live_devtools(int returncode, const char *stdout_text, bool raise_exec);

/* Module-level constants, including the exact sandbox scripts. */
json_t *w17_browserses_dec4eb_constants(void);

#endif /* NOC_W17_BROWSERSES_DEC4EB_H */
