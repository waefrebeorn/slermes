/*
 * noc_w24_names_184724.h - C surface for upstream
 * tools/connectors/gateway/names.py (gate w24_names_184724).
 *
 * The model-facing connector tool-name codec: the "connectors__" prefix, the
 * planner batch sentinel, the bounded split that keeps internal underscores in
 * a tool slug, the conventional-prefix strip on compose and the vendor-slug
 * candidates a lookup tries in order.
 *
 * Every argument is taken as a json_t* because upstream takes ``object`` and
 * branches on ``isinstance(..., str)``: an absent key and an explicit null are
 * Python ``None`` (NULL here), a non-string is rejected exactly where Python
 * rejects it.  ``str.upper()`` is Unicode-aware upstream and ASCII here -- the
 * names this codec composes are ASCII vendor slugs by contract.
 */
#ifndef W24_NAMES_184724_H
#define W24_NAMES_184724_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* is_connector_name(name) -> bool.  Prefix match only. */
json_t *w24_names_184724_is_connector_name(const json_t *name);

/* parse_connector_name(name) -> {"raw","connector","tool"} or null. */
json_t *w24_names_184724_parse_connector_name(const json_t *name);

/* format_connector_name(connector, tool) -> "connectors__<connector>__<tool>"
 * with the "<CONNECTOR>_" conventional prefix stripped from <tool>.  A
 * non-string argument is upstream's AttributeError: {"error":"AttributeError"}. */
json_t *w24_names_184724_format_connector_name(const json_t *connector, const json_t *tool);

/* vendor_slug_candidates(connector, tool) -> ["<CONNECTOR>_<tool>", "<tool>"];
 * {"error":"TypeError"} for a non-string argument. */
json_t *w24_names_184724_vendor_slug_candidates(const json_t *connector, const json_t *tool);

/* CONNECTOR_NAME_PREFIX, CONNECTOR_BATCH_SENTINEL and __all__, as an object. */
json_t *w24_names_184724_constants(void);

#ifdef __cplusplus
}
#endif
#endif
