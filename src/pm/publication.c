/*
 * publication.c — faithful C11 port of upstream Hermes pm/publication.py.
 *
 * PoP: w5_publication_1af7eb_metadata_records @ pm/publication.py:_metadata_records
 *
 * PluginSelection/StagedPlugin, candidate_members, validate_manifest and the
 * publish paths are all filesystem/locking/YAML bound and cannot be driven by
 * a deterministic JSON fixture; the one pure primitive is _metadata_records.
 */
#include "noc_w5_publication_1af7eb.h"
#include <stdlib.h>

int w5_publication_1af7eb_metadata_records(const char *data, json_t **out) {
    /* Python: if data is None: return {} */
    if (!data) {
        *out = json_object();
        return 1;
    }
    char *err = NULL;
    json_t *parsed = json_parse(data, &err);
    if (err) free(err);
    /* Python: records = json.loads(data); if not isinstance(records, dict):
     *   raise ValueError(...). json_parse failure or a non-object both fail. */
    if (!parsed || parsed->type != JSON_OBJECT) {
        if (parsed) json_free(parsed);
        return 0;
    }
    *out = parsed;
    return 1;
}
