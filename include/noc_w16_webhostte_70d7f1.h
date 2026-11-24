#ifndef NOC_W16_WEBHOSTTE_70D7F1_H
#define NOC_W16_WEBHOSTTE_70D7F1_H

#include <stdbool.h>
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_webhostte_70d7f1_terminal_lc_ctype @ hermes_cli/web_host_terminal.py:terminal_lc_ctype
 * Returns a JSON string or null. */
struct json_t *w16_webhostte_70d7f1_terminal_lc_ctype(const struct json_t *env,
                                                      const char *platform);

/* PoP: w16_webhostte_70d7f1_query_dimension @ hermes_cli/web_host_terminal.py:query_dimension
 * raw is a JSON scalar; default/maximum are numbers. */
double w16_webhostte_70d7f1_query_dimension(const struct json_t *raw,
                                            double def, double maximum);

/* PoP: w16_webhostte_70d7f1_request_allowed @ hermes_cli/web_host_terminal.py:request_allowed
 * Seam: app.state fields and _LOOPBACK_HOSTS are injected from the fixture. */
bool w16_webhostte_70d7f1_request_allowed(bool host_terminal, bool auth_required,
                                          const char *bound_host,
                                          const struct json_t *loopback_hosts);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_WEBHOSTTE_70D7F1_H */
