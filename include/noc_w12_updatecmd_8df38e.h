#ifndef NOC_W12_UPDATECMD_8DF38E_H
#define NOC_W12_UPDATECMD_8DF38E_H

#include "hermes_json.h"

/* PoP port of hermes_cli/update_cmd_zip.py (gate w12_updatecmd_8df38e). */

/* _status_top_level(path) -> string. */
json_t *w12_updatecmd_8df38e_status_top_level(const char *path);
/* _is_zip_preserved_entry_status_line(line, shipped|null) -> bool. */
json_t *w12_updatecmd_8df38e_is_zip_preserved_entry_status_line(const char *line, const json_t *shipped);
/* _is_zip_staging_artifact_status_line(line, staged[]) -> bool. */
json_t *w12_updatecmd_8df38e_is_zip_staging_artifact_status_line(const char *line, const json_t *staged);

#endif /* NOC_W12_UPDATECMD_8DF38E_H */
