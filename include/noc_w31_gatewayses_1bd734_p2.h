/*
 * noc_w31_gatewayses_1bd734_p2.h - C surface for the read-shaping half of
 * upstream hermes_cli/gateway_session_store.py (gate w31_gatewayses_1bd734_p2).
 *
 * How a source filter expands into live/created labels (so ``--source cli``
 * also finds a gateway row labelled ``local``), which argument set a labelled
 * listing call is made with, and the banner inventory a session snapshot yields.
 * The SQL probe for the ``created_source`` column and the banner's consumer stay
 * in the caller.
 */
#ifndef NOC_W31_GATEWAYSES_1BD734_P2_H
#define NOC_W31_GATEWAYSES_1BD734_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _expand(requested, has_column) -> {"live": [...], "created": [...]} or null.
 * ``created`` is sorted and only returned when the column exists. */
json_t *w31_gatewayses_1bd734_p2_expand(const json_t *requested, const json_t *has_column);

/* _labelled(requested, has_column, limit, offset) ->
 * {"sources": <expanded live labels | requested as given>, "limit", "offset",
 *  "surfaces": <expanded created labels | null>}. */
json_t *w31_gatewayses_1bd734_p2_labelled(const json_t *requested, const json_t *has_column,
                                          const json_t *limit, const json_t *offset);

/* banner_inventory(current) -> the snapshot's banner fields, or null until a
 * session whose info carries ``tools`` is open.  ``get_toolset_for_tool`` is
 * emitted as the mapping itself, "toolset_of". */
json_t *w31_gatewayses_1bd734_p2_banner_inventory(const json_t *current);

#ifdef __cplusplus
}
#endif
#endif
