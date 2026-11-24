#ifndef NOC_W18_KANBANDW_DE7A45_H
#define NOC_W18_KANBANDW_DE7A45_H

#include "hermes_json.h"

/* Faithful C11 port of hermes_cli/kanban_db_workspace.py
 * (gate w18_kanbandbw_de7a45). `path_key` implements unicodedata.normalize("NFC")
 * with canonical composition of the Latin combining marks used on Windows /
 * macOS paths; `constants` mirrors the module-level SQL and message literals. */

json_t *w18_kanbandbw_de7a45_path_key(const char *path);
json_t *w18_kanbandbw_de7a45_constants(void);

#endif /* NOC_W18_KANBANDW_DE7A45_H */
