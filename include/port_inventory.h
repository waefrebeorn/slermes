/*
 * port_inventory.h -- pure helpers ported from hermes_cli/inventory.py.
 *
 * See port_inventory.c: only the dependency-free core is ported. The module's
 * config loading, HTTP probing and credential-pool reads stay in Python.
 */
#ifndef PORT_INVENTORY_H
#define PORT_INVENTORY_H

#include <stddef.h>
#include "hermes_json.h"

/* inventory.py:_slug -- stripped, lowercased slug of a row's "slug" field.
 * Missing / null / object / array -> "". Caller frees. */
char *inv_slug(const json_t *row);

/* inventory.py:_without_slug -- new array of the rows whose slug differs.
 * Rows are borrowed, not copied. Caller frees the array. */
json_t *inv_without_slug(const json_t *rows, const char *slug);

/* inventory.py:_row -- the fixed row shape every picker surface reads. */
json_t *inv_row(const char *slug, const char *name, int is_current);

/* inventory.py:_iso_from_epoch -- UTC isoformat with Python's exact shape
 * (microseconds omitted when zero, "+00:00" always). Caller frees. */
char *inv_iso_from_epoch(double epoch);

/* inventory.py:ConfigContext.with_overrides -- apply TRUTHY overrides only.
 * Returns NULL when every argument is empty (Python returns self). */
json_t *inv_with_overrides(const json_t *ctx,
                           const char *current_provider,
                           const char *current_model,
                           const char *current_base_url);

#endif /* PORT_INVENTORY_H */
