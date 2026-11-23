#ifndef NOC_W5_CLI_D7CB5A_H
#define NOC_W5_CLI_D7CB5A_H
#include <stddef.h>

/* Public C API for pm/cli.py (gate w5_cli_d7cb5a). The flag/format/split seams
 * are deterministic; the command bodies (install/doctor/update/...) are IO and
 * process orchestration and are not part of this pure gate. */

/* PoP: pmcli_fmt_bytes @ pm/cli.py:_fmt_bytes — malloc'd "N.N MiB" string. */
char *pmcli_fmt_bytes(long long n);

/* PoP: pmcli_install_flag_error @ pm/cli.py:_install_flag_error
 * Returns a malloc'd user-facing error string, or NULL when the flags are
 * compatible. JSON array arguments are passed as their serialized strings. */
char *pmcli_install_flag_error(const char *cross_target, int n_names,
                               const char *extras_json, int tools_only,
                               int trust_recorded, int test_env_given,
                               const char *without_json,
                               const char *default_names_json,
                               const char *droppable_json);

/* PoP: pmcli_split_without @ pm/cli.py:_split_without
 * Returns a malloc'd JSON string {"defaults":[...],"extras":[...],"unknown":[...]}. */
char *pmcli_split_without(const char *names_json, const char *default_names_json,
                          const char *droppable_json);

#endif /* NOC_W5_CLI_D7CB5A_H */
