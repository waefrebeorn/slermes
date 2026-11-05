#ifndef PORT_AUTH_ERROR_COPY_H
#define PORT_AUTH_ERROR_COPY_H

#include "hermes_json.h"

/* Port of Python: hermes_cli/auth_error_copy.py
 *
 * Plain-language copy for sign-in / provider-setup failures. In Python the
 * input is an exception; in C it is an afc_error_t describing the same facts
 * (class-name text, message, oauth code, exit code).
 */

typedef enum {
    AFC_OTHER = 0,       /* any other exception                        */
    AFC_CANCELLED,       /* KeyboardInterrupt / EOFError / SystemExit  */
    AFC_NETWORK,         /* httpx / requests / stdlib connection error */
    AFC_SIGNIN_COPY,     /* SignInCopyError: str(exc) is already copy   */
    AFC_DEVICE_EXPIRED   /* DeviceCodeExpired                          */
} afc_kind_t;

typedef struct {
    afc_kind_t  kind;
    const char *message;     /* str(exc)              (may be NULL) */
    const char *type_name;   /* type(exc).__name__    (may be NULL) */
    const char *oauth_code;  /* exc.oauth_error_code  (may be NULL) */
    const char *exc_code;    /* exc.code              (may be NULL) */
    int         exit_code;   /* SystemExit.code; -1 = None          */
} afc_error_t;

/* `_NETWORK_ERROR_TYPES` membership for a class name. */
int afc_is_network_error(const afc_error_t *err);
int afc_is_device_code_expired(const afc_error_t *err);
int afc_is_cancelled(const afc_error_t *err);

/* `  Details: <str(exc).strip() or type(exc).__name__>` — malloc'd. */
char *afc_details_line(const afc_error_t *err);

/* message for an OAuth device-flow error code (returns SignInCopyError text). */
char *afc_device_flow_error(const char *code, const char *description,
                            const char *retry_command);

/* Lines to print; JSON array of strings (caller frees). */
json_t *afc_sign_in_failure_lines(const afc_error_t *err,
                                  const char *service_host,
                                  const char *retry_command);
json_t *afc_provider_setup_failure_lines(const afc_error_t *err,
                                         const char *retry_command);

/* The copy for the removed keyless providers, or NULL when the name is not
 * one of them (the plugin-migration and config-validation branches of the
 * Python original need hermes_cli.left_core_migration / config, which have no
 * C caller here). */
const char *afc_unknown_provider_hint(const char *provider_name);

#endif /* PORT_AUTH_ERROR_COPY_H */
