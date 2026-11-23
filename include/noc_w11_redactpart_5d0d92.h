#ifndef NOC_W11_REDPART_5D0D92_H
#define NOC_W11_REDPART_5D0D92_H

#include <hermes_json.h>

/* C11 port of agent/redact_partial_keys.py:_redact_partial_private_keys.
 * Takes a borrowed json_t *args (with a "text" string) and returns a NEW
 * json_t *string with the base64 body beside a lone armor line masked. */
json_t *w11_redactpart_5d0d92_redact(json_t *a);

#endif /* NOC_W11_REDPART_5D0D92_H */
