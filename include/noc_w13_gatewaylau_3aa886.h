#ifndef NOC_W13_GATEWAYLAU_3AA886_H
#define NOC_W13_GATEWAYLAU_3AA886_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_gatewaylau_3aa886_constants @ hermes_cli/gateway_launchd.py (module constants) */
struct json_t *w13_gatewaylau_3aa886_constants(void);

/* PoP: w13_gatewaylau_3aa886_get_label @ hermes_cli/gateway_launchd.py:get_launchd_label
 * suffix = _gw()._profile_suffix() injected by the fixture. */
struct json_t *w13_gatewaylau_3aa886_get_label(const char *suffix);

/* PoP: w13_gatewaylau_3aa886_error_indicates_unloaded @ hermes_cli/gateway_launchd.py:_launchd_error_indicates_unloaded */
struct json_t *w13_gatewaylau_3aa886_error_indicates_unloaded(int returncode);

/* PoP: w13_gatewaylau_3aa886_domain_unsupported @ hermes_cli/gateway_launchd.py:_launchctl_domain_unsupported */
struct json_t *w13_gatewaylau_3aa886_domain_unsupported(int returncode);

/* PoP: w13_gatewaylau_3aa886_program_arguments @ hermes_cli/gateway_launchd.py:launchd_program_arguments
 * command is a JSON array of strings; stdout_log/stderr_log are strings. Fully pure. */
struct json_t *w13_gatewaylau_3aa886_program_arguments(const struct json_t *command,
                                                       const char *stdout_log, const char *stderr_log);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_GATEWAYLAU_3AA886_H */
