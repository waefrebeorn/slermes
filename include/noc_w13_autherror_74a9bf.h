#ifndef NOC_W13_AUTHERROR_74A9BF_H
#define NOC_W13_AUTHERROR_74A9BF_H

#include "hermes_json.h"

/* C port of hermes_cli/auth_error_copy.py (gate w13_autherror_74a9bf).
 * char/json are new refs; int predicates return 1/0 for Python True/False. */

/* device_flow_error(code, description, retry_command) -> {"str":..,"oauth_error_code":..} */
json_t *w13_autherror_74a9bf_device_flow_error(const char *code,
                                               const char *description,
                                               const char *retry_command);

/* is_network_error: type_name + oauth_error_code + exc.code attributes. */
int w13_autherror_74a9bf_is_network_error(const char *type_name,
                                          const char *oauth_error_code,
                                          const char *code);

/* is_device_code_expired: exc.code in {device_code_timeout,timeout} or
 * exc.oauth_error_code == "expired_token". */
int w13_autherror_74a9bf_is_device_code_expired(const char *oauth_error_code,
                                                const char *code);

/* is_cancelled: KeyboardInterrupt/EOFError, or SystemExit with code
 * 130 / None(sentinel -1) / 0. */
int w13_autherror_74a9bf_is_cancelled(const char *type_name, long long exit_code);

#endif /* NOC_W13_AUTHERROR_74A9BF_H */
