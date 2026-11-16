/* port_config_nested.h -- dict-navigation helpers from hermes_cli/config.py.
 *
 * These operate on libjson's json_t DOM. Python dicts map to JSON_OBJECT and
 * Python lists to JSON_ARRAY, which is exactly the shape config.yaml parses to.
 */
#ifndef PORT_CONFIG_NESTED_H
#define PORT_CONFIG_NESTED_H

#include <stddef.h>

#include "libjson/json.h"

/* Result of a _locate_nested walk. `key` is owned by the caller on success. */
typedef struct {
    json_t **parents;        /* containers of the hops above the leaf */
    char **parent_keys;      /* key used at each hop */
    int n_parents;
    json_t *container;       /* object/array holding the leaf */
    char *key;               /* leaf key (object) or index string (array) */
    int is_index;            /* 1 when the leaf is addressed by array index */
} cfg_loc_t;

/* greedy_literal_match: longest dotted literal key present in `container`.
 * Returns the number of consumed segments and copies the literal key into
 * `out` (caller buffer `cap`). 0 when there is no match. */
int cfg_greedy_literal_match(json_t *container, char **parts, int nparts,
                             char *out, size_t cap);

/* phantom_sibling: an existing literal key starting with "<part>.".
 * Copies it into `out`; returns 1 when one exists, else 0. */
int cfg_phantom_sibling(json_t *container, const char *part,
                        char *out, size_t cap);

/* _locate_nested: walk the path. Returns 1 and fills `loc` on success,
 * 0 when any hop is missing, a list index is non-numeric/out of range, or a
 * scalar is hit before the path is consumed. Caller frees loc.key. */
int cfg_locate_nested(json_t *root, char **parts, int nparts, cfg_loc_t *loc);

/* _set_nested. Returns 0 ok, 1 phantom-sibling refusal, 2 un-navigable type,
 * -1 bad args. On 1 or 2 the tree is left as it was found. */
int cfg_set_nested(json_t *root, const char *dotted_key, json_t *value);

/* _get_nested: the addressed node, or NULL when absent. */
json_t *cfg_get_nested(json_t *root, const char *dotted_key);

/* _unset_nested: 1 when a value was removed, 0 when the path was absent.
 * Empty object containers left behind are dropped, as in the Python. */
int cfg_unset_nested(json_t *root, const char *dotted_key);

#endif /* PORT_CONFIG_NESTED_H */
