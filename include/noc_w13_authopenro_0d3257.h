#ifndef NOC_W13_AUTHOPENRO_0D3257_H
#define NOC_W13_AUTHOPENRO_0D3257_H

#include "hermes_json.h"

/* C port of _openrouter_exchange_code in hermes_cli/auth_openrouter.py
 * (gate w13_authopenro_0d3257). The HTTP response is injected as explicit
 * args (the live httpx.post seam) so the decision logic is pure/testable.
 *
 * Returns a new json_t object:
 *   success -> {"ok":true,  "key":<str>, "code":null, "message":null,
 *               "provider":null, "relogin":false}
 *   error   -> {"ok":false, "key":null, "code":<str>, "message":<str>,
 *               "provider":"openrouter", "relogin":<bool>}
 */
json_t *w13_authopenro_0d3257_exchange_code(const char *code,
                                            const char *verifier,
                                            long long status,
                                            const char *text,
                                            const json_t *payload,
                                            int json_error,
                                            const char *post_error);

#endif /* NOC_W13_AUTHOPENRO_0D3257_H */
