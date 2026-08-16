/* port_utils_truthy.c — faithful C11 port of the truthy helpers in utils.py.
 * TRUTHY_STRINGS = {"1", "true", "yes", "on"} — the project's ONE shared
 * truthy set. Every subsystem should call this instead of re-implementing
 * (port_agent_verify_hooks.c keeps a private static copy for historical
 * reasons; new code links here).
 */

#include "truthy.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* PoP: is_truthy_value @ utils.py:is_truthy_value */
bool is_truthy_value(const char *value, bool default_value) {
    if (!value) return default_value;   /* Python: value is None -> default */
    /* Python str branch: value.strip().lower() in TRUTHY_STRINGS */
    const char *b = value;
    while (*b && isspace((unsigned char)*b)) b++;
    const char *e = b + strlen(b);
    while (e > b && isspace((unsigned char)e[-1])) e--;
    char buf[16];
    size_t len = (size_t)(e - b);
    if (len >= sizeof(buf)) return false;   /* longer than any truthy word */
    for (size_t i = 0; i < len; i++) buf[i] = (char)tolower((unsigned char)b[i]);
    buf[len] = '\0';
    return strcmp(buf, "1") == 0 || strcmp(buf, "true") == 0 ||
           strcmp(buf, "yes") == 0 || strcmp(buf, "on") == 0;
}

/* PoP: env_var_enabled @ utils.py:env_var_enabled */
bool env_var_enabled(const char *name, const char *default_value) {
    const char *v = name ? getenv(name) : NULL;
    if (!v) v = default_value ? default_value : "";
    return is_truthy_value(v, false);
}
