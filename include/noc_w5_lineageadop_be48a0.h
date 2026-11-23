#ifndef NOC_W5_LINEAGEADOP_BE48A0_H
#define NOC_W5_LINEAGEADOP_BE48A0_H

#include "hermes_json.h"

/* C port of the pure dict-shaping leaves of lineage_adoption.py.
 * All exports are lineageadop_-prefixed. Returned json_t* are caller-owned. */

/* _result_dict(ImportResult): the import_sessions dict; id lists appear only
 * when ok is true, errors always. */
json_t *lineageadop_result_dict(json_t *result);

/* _import_entry(SessionExport): session.as_dict() with a "messages" key
 * replaced by the list of message wire rows. */
json_t *lineageadop_import_entry(json_t *segment);

#endif /* NOC_W5_LINEAGEADOP_BE48A0_H */
