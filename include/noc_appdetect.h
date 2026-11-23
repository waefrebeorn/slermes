#ifndef NOC_APPDETECT_H
#define NOC_APPDETECT_H

#include "hermes_json.h"

/* C port of selected pure helpers in tools/approval_detection.py.
 * All exports appdetect_-prefixed. Returned char* values are malloc'd and
 * caller-owned; NULL represents Python None. */

/* _lower_preserving_flags: lowercase every re.split(r"(\s+)") token except
 * dash-prefixed tokens (kept byte-for-byte). */
char *appdetect_lower_preserving_flags(const char *command);

/* _splice: apply sorted, non-overlapping (start, end, text) edits to command. */
char *appdetect_splice(const char *command, json_t *edits);

/* _eval_payload: drop a leading "--", join args with spaces; "" -> None. */
char *appdetect_eval_payload(json_t *args);

/* _interpreter_family: family name for an interpreter basename, or NULL. */
const char *appdetect_interpreter_family(const char *executable);

#endif
