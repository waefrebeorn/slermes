/* hermes_home_override.c — context-local Hermes home override.
 * LIVE twin: hermes_constants.get/reset_hermes_home_override.
 */
#include <stdlib.h>
#include <string.h>

static __thread char *t_override;
static __thread unsigned t_token;

/* Returns a nonzero reset token when an override is installed. */
unsigned hc_set_hermes_home_override(const char *home)
{
    free(t_override);
    t_override = NULL;
    if (!home || !*home) return 0;
    t_override = strdup(home);
    return ++t_token;
}

void hc_reset_hermes_home_override(unsigned token)
{
    if (token && token == t_token) {
        free(t_override);
        t_override = NULL;
    }
}

/* PoP: get_hermes_home_override @ hermes_constants.py:get_hermes_home_override */
const char *hc_get_hermes_home_override(void)
{
    return (t_override && *t_override) ? t_override : NULL;
}
