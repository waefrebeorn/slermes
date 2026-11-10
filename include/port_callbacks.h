#ifndef SLERMES_PORT_CALLBACKS_H
#define SLERMES_PORT_CALLBACKS_H

#include "json.h"

/* _skipped(var_name, reason, message): {success:True, reason, stored_as,
 * validated:False, skipped:True, message}. Returns new json_t, caller frees. */
json_t *cb_skipped(const char *var_name, const char *reason, const char *message);

#endif /* SLERMES_PORT_CALLBACKS_H */
