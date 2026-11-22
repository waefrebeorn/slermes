/* Dedicated port for hermes_cli/web_routers/profiles.py:_warn_profile_read_error.
 * Split out of web_server.c (which has 76 unresolved extern symbols) so this
 * self-contained dedup behavior can be linked and oracle-verified in isolation.
 *
 * Python: key = (profile, str(exc)); if key in _profile_read_warned: return;
 * _profile_read_warned.add(key); _log.warning(...).
 * Returns 1 when a warning is emitted (new key), 0 when deduped. The set is
 * module-level in Python; here a static fixed-capacity key table persists for
 * the process lifetime, matching one oracle harness run. */
#include <string.h>
#include <stdlib.h>

#define PROFILE_WARN_CAP 256
typedef struct { char *profile; char *err; } pw_warn_key_t;
static pw_warn_key_t pw_warned[PROFILE_WARN_CAP];
static int pw_warned_count = 0;

/* PoP: profiles_warn_read_error @ hermes_cli/web_routers/profiles.py:_warn_profile_read_error */
int profiles_warn_read_error(const char *profile, const char *error_msg) {
    if (!profile) profile = "";
    if (!error_msg) error_msg = "";
    for (int i = 0; i < pw_warned_count; i++)
        if (strcmp(pw_warned[i].profile, profile) == 0 && strcmp(pw_warned[i].err, error_msg) == 0)
            return 0;                       /* already warned */
    if (pw_warned_count >= PROFILE_WARN_CAP) return 0;  /* capacity guard */
    pw_warned[pw_warned_count].profile = strdup(profile);
    pw_warned[pw_warned_count].err = strdup(error_msg);
    pw_warned_count++;
    return 1;                               /* emitted warning */
}
