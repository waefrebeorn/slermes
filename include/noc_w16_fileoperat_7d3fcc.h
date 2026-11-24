#ifndef NOC_W16_FILEOPERAT_7D3FCC_H
#define NOC_W16_FILEOPERAT_7D3FCC_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of tools/file_operations_native.py (gate w16_fileoperat_7d3fcc).
 * Both functions do real regular-file I/O on the local host. */

/* probe(path, want_text) -> [has_bom, body_hex] | [false, null] when missing |
 * null when the path is not a regular file (ask the shell). */
json_t *w16_fileoperat_7d3fcc_probe(const char *path, int want_text);

/* sha256_of(path) -> lowercase hex digest | null when it cannot be read natively. */
json_t *w16_fileoperat_7d3fcc_sha256_of(const char *path);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_FILEOPERAT_7D3FCC_H */
