#ifndef NOC_W18_AUXILIARYC_CBC55F_H
#define NOC_W18_AUXILIARYC_CBC55F_H

#include "hermes_json.h"

/* C home for upstream Hermes agent/auxiliary_codex_response.py.
 * The heavy shared Responses normalizer is a controlled seam: this module's
 * own orchestration (output shaping, finish_reason remap, usage packing) is
 * ported verbatim; the normalizer result is supplied as an input argument. */

/* -> [text_parts, tool_calls, usage, finish_reason] */
json_t *w18_auxiliaryc_cbc55f_parse(const json_t *final, const char *issuer_kind,
                                    const char *issuer_model, const json_t *norm);

#endif /* NOC_W18_AUXILIARYC_CBC55F_H */
