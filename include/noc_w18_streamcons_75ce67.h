#ifndef NOC_W18_STREAMCONS_75CE67_H
#define NOC_W18_STREAMCONS_75CE67_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_streamcons_75ce67_escape_code_fences_for_display @ gateway/stream_consumer_fences.py:escape_code_fences_for_display
 * Replace each ``` with \`\`\` . NULL (non-str) passes through. Caller frees result. */
char *w18_streamcons_75ce67_escape_code_fences_for_display(const char *text);

/* PoP: w18_streamcons_75ce67_open_code_fence_span @ gateway/stream_consumer_fences.py:open_code_fence_span
 * [start,end] of the opening ``` line left open by text[:upto], or JSON null. */
struct json_t *w18_streamcons_75ce67_open_code_fence_span(const char *text, long long upto);

/* PoP: w18_streamcons_75ce67_ensure_closed_code_fences @ gateway/stream_consumer_fences.py:ensure_closed_code_fences
 * Append a closing ``` and/or ` for orphaned code markers. Caller frees result. */
char *w18_streamcons_75ce67_ensure_closed_code_fences(const char *text);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_STREAMCONS_75CE67_H */
