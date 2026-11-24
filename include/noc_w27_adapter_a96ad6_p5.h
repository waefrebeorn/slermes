/*
 * noc_w27_adapter_a96ad6_p5.h - C surface for slice 5 of upstream
 * plugins/platforms/telegram/adapter.py (transport classification + bool-extra coercion).
 *
 * Each symbol takes the JSON-lines request object and returns the
 * Python-equivalent JSON value of the same-named upstream function applied
 * to the keys named after its parameters. See adapter.c for the PoP map.
 */
#ifndef NOC_W27_ADAPTER_A96AD6_P5_H
#define NOC_W27_ADAPTER_A96AD6_P5_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_a96ad6_p5_looks_like_network_error(const json_t *req);
json_t *w27_adapter_a96ad6_p5_exception_graph_matches(const json_t *req);
json_t *w27_adapter_a96ad6_p5_looks_like_connect_timeout(const json_t *req);
json_t *w27_adapter_a96ad6_p5_looks_like_pool_timeout(const json_t *req);
json_t *w27_adapter_a96ad6_p5_coerce_bool_extra(const json_t *req);

/* Dispatch by request "func"; returns "UNKNOWN_FUNC" when absent. */
json_t *w27_adapter_a96ad6_p5_dispatch(const char *func, const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_A96AD6_P5_H */
