#ifndef NOC_W5_REGISTRY_8FD451_H
#define NOC_W5_REGISTRY_8FD451_H

/* C home for upstream Hermes pm/registry.py — gate w5_registry_8fd451. */
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_registry_8fd451_walk @ pm/registry.py:walk
 * Deps-first topological order over `names`, using the name->[deps] map in
 * `deps`. Returns a JSON array of names, or
 * {"__raise__":"ValueError","msg":"dependency cycle: a -> b -> a"} on a cycle.
 * Caller frees. */
json_t *w5_registry_8fd451_walk(const json_t *names, const json_t *deps);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_REGISTRY_8FD451_H */
