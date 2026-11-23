#ifndef NOC_W8_CPUPROFILE_DFA160_H
#define NOC_W8_CPUPROFILE_DFA160_H
#include "hermes_json.h"

/* C home for upstream Hermes hermes_cli/cpu_profile.py (gate w8_cpuprofile_dfa160).
 *
 * PoP: w8_cpuprofile_resolve_profile_path_ex @ hermes_cli/cpu_profile.py:resolve_profile_path
 *
 * Faithful port of resolve_profile_path(path):
 *   target = Path(path).expanduser()
 *   if not target.is_absolute():
 *       target = get_hermes_home() / "logs" / "profiles" / target
 *   return target.resolve()
 * `home` is injected for both get_hermes_home() and ~ expansion (the Python
 * oracle sets HOME/USERPROFILE to the same value).  The returned string is the
 * resolved path in pathlib's POSIX spelling (forward slashes), matching
 * Path.as_posix() on Windows.
 */
json_t *w8_cpuprofile_resolve_profile_path_ex(const char *path, const char *home);

#endif /* NOC_W8_CPUPROFILE_DFA160_H */
