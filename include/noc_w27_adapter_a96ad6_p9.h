/*
 * noc_w27_adapter_a96ad6_p9.h - C surface for slice 9 of upstream
 * plugins/platforms/telegram/adapter.py (keyboard rows + media notes/size policy).
 *
 * Each symbol takes the JSON-lines request object and returns the
 * Python-equivalent JSON value of the same-named upstream function applied
 * to the keys named after its parameters. See adapter.c for the PoP map.
 */
#ifndef NOC_W27_ADAPTER_A96AD6_P9_H
#define NOC_W27_ADAPTER_A96AD6_P9_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_a96ad6_p9_rows_of_two(const json_t *req);
json_t *w27_adapter_a96ad6_p9_ea_escape(const json_t *req);
json_t *w27_adapter_a96ad6_p9_missing_media_path_error(const json_t *req);
json_t *w27_adapter_a96ad6_p9_media_too_large_note(const json_t *req);
json_t *w27_adapter_a96ad6_p9_int_or_zero(const json_t *req);
json_t *w27_adapter_a96ad6_p9_media_size_allowed(const json_t *req);
json_t *w27_adapter_a96ad6_p9_caption_1024(const json_t *req);

/* Dispatch by request "func"; returns "UNKNOWN_FUNC" when absent. */
json_t *w27_adapter_a96ad6_p9_dispatch(const char *func, const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_A96AD6_P9_H */
