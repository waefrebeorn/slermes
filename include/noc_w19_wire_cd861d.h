#ifndef NOC_W19_WIRE_CD861D_H
#define NOC_W19_WIRE_CD861D_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_wire_cd861d_execute_error_family @ tools/connectors/gateway/wire.py:execute_error_family
 * The gateway's family when it sent one ("product"/"backend"), else the
 * EXECUTE_ERROR_CODES table's, else FILE_ERROR_FAMILIES', else NULL for an unknown code. */
const char *w19_wire_cd861d_execute_error_family(const char *code, const char *family);

/* PoP: w19_wire_cd861d_execute_error_status @ tools/connectors/gateway/wire.py:EXECUTE_ERROR_CODES
 * HTTP status of a single-call execute failure code from the module table, -1 when unlisted. */
int w19_wire_cd861d_execute_error_status(const char *code);

/* PoP: w19_wire_cd861d_file_max_bytes @ tools/connectors/gateway/wire.py:CONNECTOR_FILE_MAX_BYTES
 * The gateway's cap on a staged file's declared size (20 MiB). */
long long w19_wire_cd861d_file_max_bytes(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_WIRE_CD861D_H */
