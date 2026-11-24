#ifndef NOC_W17_STREAMCONS_BD185C_H
#define NOC_W17_STREAMCONS_BD185C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_streamcons_bd185c_at_block_boundary @ gateway/stream_consumer_think.py:StreamThinkFilterMixin._at_block_boundary
 * idx is a Python character index (not a byte offset). */
int w17_streamcons_bd185c_at_block_boundary(const char *accumulated, const char *buf, int idx);
/* PoP: w17_streamcons_bd185c_earliest_open_tag @ gateway/stream_consumer_think.py:StreamThinkFilterMixin._earliest_open_tag
 * Writes the (character index, character length) of the earliest block-boundary open tag,
 * or (-1, 0). */
void w17_streamcons_bd185c_earliest_open_tag(const char *accumulated, const char *buf,
                                            int *out_idx, int *out_len);
/* PoP: w17_streamcons_bd185c_strip_orphan_close_tags @ gateway/stream_consumer_think.py:StreamThinkFilterMixin._strip_orphan_close_tags
 * Heap copy of the scrubbed text. Caller frees. */
char *w17_streamcons_bd185c_strip_orphan_close_tags(const char *text);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_STREAMCONS_BD185C_H */
