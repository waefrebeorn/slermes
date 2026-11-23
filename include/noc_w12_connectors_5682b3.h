#ifndef NOC_W12_CONNECTORS_5682B3_H
#define NOC_W12_CONNECTORS_5682B3_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/connectors_cmd.py (gate w12_connectors_5682b3). */

/* str(row.get("alias") or row.get("label") or row.get("connectionId") or "") */
json_t *w12_connectors_5682b3_account_name(const json_t *row);

/* row["alias"] if set, else "<label|connectionId> (unnamed)". */
json_t *w12_connectors_5682b3_display_name(const json_t *row);

/* "Its accounts are: <sorted names>." / "You have no <app> account." */
json_t *w12_connectors_5682b3_names_hint(const char *app, const json_t *rows);

/* _valid_alias against ALIAS_PATTERN ^[a-z0-9][a-z0-9-]{0,31}$ :
 * {"ok":true,"alias":..} or {"ok":false,"error":<CommandError text>}. */
json_t *w12_connectors_5682b3_valid_alias(const char *alias);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_CONNECTORS_5682B3_H */
