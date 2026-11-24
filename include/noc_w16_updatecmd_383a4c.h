#ifndef NOC_W16_UPDATECMD_383A4C_H
#define NOC_W16_UPDATECMD_383A4C_H

#include "hermes_json.h"

/* C port of selected pure helpers in hermes_cli/update_cmd_commit.py.
 * Returned json_t* / char* values are caller-owned. */

/* debt_sha_for_move(pre, target): the commit the obligations are owed for.
 * run_start_head is _run_start[1][0] (NULL/"" means no run start). */
char *w16_updatecmd_383a4c_debt_sha_for_move(const char *pre, const char *target,
                                             const char *run_start_head);

/* _table(tables, *keys): nested dict walk; {} when any step is not a dict. */
json_t *w16_updatecmd_383a4c_table(json_t *tables, json_t *keys);

/* startup_files(pyproject, undeclared) -> {"files": [..], "required": bool}
 * or JSON null when the pyproject is not valid TOML / declares an invalid
 * update-startup-files list.  A small TOML subset (section headers, scalar
 * assignments, single-line quoted string arrays) is parsed faithfully. */
json_t *w16_updatecmd_383a4c_startup_files(const char *pyproject, json_t *undeclared);

#endif
