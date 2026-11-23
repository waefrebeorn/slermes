/*
 * profiles_config.c — C port of upstream Hermes hermes_cli/profiles_config.py.
 * PoP: profcfg_should_migrate @ hermes_cli/profiles_config.py:_migrate_profile_config_if_outdated
 *
 * Faithful guard ordering of the live function's decision path:
 *   if not supports_file_tooling(): return
 *   if not config_exists(profile_dir / "config.yaml"): return
 *   with suppress(Exception): ... if current_ver < latest_ver: migrate_config(...)
 * The filesystem/HOME/plugin side effects are outside a pure C port; see the
 * header comment for the documented limitation.
 */
#include "noc_profcfg.h"

int profcfg_should_migrate(int supports_file_tooling, int config_exists,
                           int current_ver, int latest_ver)
{
    if (!supports_file_tooling) return 0;
    if (!config_exists) return 0;
    return (current_ver < latest_ver) ? 1 : 0;
}
