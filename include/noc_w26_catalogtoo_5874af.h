/*
 * noc_w26_catalogtoo_5874af.h - C surface for upstream
 * tools/connectors/catalog_tool.py (gate w26_catalogtoo_5874af).
 *
 * ``manage_catalog`` is the desktop chat's catalog search / install tool.  The
 * catalog resolver, the skills hub, the approval card and the installer are the
 * seams (connections, a human at the card, a subprocess); what the model sends
 * is a small value -- an action, an optional kind, a query, item rows -- and the
 * whole first half of the tool is a pure verdict on that value.
 */
#ifndef NOC_W26_CATALOGTOO_5874AF_H
#define NOC_W26_CATALOGTOO_5874AF_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The one surface that draws catalog rows. */
#define CT_NOT_HERE \
    "manage_catalog is available only in the Hermes desktop app, where the approval card can be drawn. " \
    "Tell the user to install from a terminal instead: `hermes plugins install <id>` for a plugin, " \
    "`hermes skills install <id>` for a skill."

/* _check_args(args) -> the error string, or null when the arguments are valid. */
json_t *w26_catalogtoo_5874af_check_args(const json_t *args);

/* manage_catalog(args, card_surface=..., connection_callback=...) without its
 * seams -> {"route": "error"|"not_here"|"search"|"install", "error"?: "..."}. */
json_t *w26_catalogtoo_5874af_dispatch(const json_t *args, int card_surface, int has_callback);

/* ACTIONS / ITEM_KINDS / _TOP_KEYS / _ITEM_KEYS / SEARCH_LIMIT, as an object. */
json_t *w26_catalogtoo_5874af_constants(void);

#ifdef __cplusplus
}
#endif
#endif
