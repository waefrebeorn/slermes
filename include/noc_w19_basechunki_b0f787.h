/* noc_w19_basechunki_b0f787.h - public C API for gateway/platforms/base_chunking.py port. */
#ifndef NOC_W19_BASECHUNKI_B0F787_H
#define NOC_W19_BASECHUNKI_B0F787_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_basechunki_b0f787_chunk_markdown_message @ gateway/platforms/base_chunking.py:chunk_markdown_message */
json_t *w19_basechunki_b0f787_chunk_markdown_message(const char *content, long long max_length,
                                                    int len_fn_utf16);
/* PoP: w19_basechunki_b0f787_open_link_bracket @ gateway/platforms/base_chunking.py:_open_link_bracket */
json_t *w19_basechunki_b0f787_open_link_bracket(const char *text);
/* PoP: w19_basechunki_b0f787_fence_state_after @ gateway/platforms/helpers.py:fence_state_after */
json_t *w19_basechunki_b0f787_fence_state_after(const char *text, int in_code, const char *lang);
/* PoP: w19_basechunki_b0f787_table_row_start @ gateway/platforms/helpers.py:table_row_start */
json_t *w19_basechunki_b0f787_table_row_start(const char *text, long long split_at);
/* PoP: w19_basechunki_b0f787_table_rollback_point @ gateway/platforms/helpers.py:table_rollback_point */
json_t *w19_basechunki_b0f787_table_rollback_point(const char *text, long long split_at,
                                                   long long max_units, int len_fn_utf16);
/* PoP: w19_basechunki_b0f787_table_header_to_repeat @ gateway/platforms/helpers.py:table_header_to_repeat */
json_t *w19_basechunki_b0f787_table_header_to_repeat(const char *chunk, const char *rest,
                                                     long long max_units, int len_fn_utf16);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_BASECHUNKI_B0F787_H */
