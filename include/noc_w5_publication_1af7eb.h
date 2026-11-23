#ifndef NOC_W5_PUBLICATION_1AF7EB_H
#define NOC_W5_PUBLICATION_1AF7EB_H
#include "hermes_json.h"

/* Pure metadata decoding from pm/publication.py.
 * The PluginSelection/StagedPlugin classes are all filesystem/IO/locking bound
 * and out of scope; _metadata_records is the one pure, testable primitive. */

/* PoP: w5_publication_1af7eb_metadata_records @ pm/publication.py:_metadata_records
 * data == NULL models Python None and yields an empty object (success).
 * Otherwise data is a UTF-8 JSON document: a JSON object yields a deep copy of
 * that object (success); anything else (invalid JSON, array, string, null,
 * number) returns 0 (Python raises ValueError). On success *out is a json_t
 * the caller must json_free(). */
int w5_publication_1af7eb_metadata_records(const char *data, json_t **out);

#endif /* NOC_W5_PUBLICATION_1AF7EB_H */
