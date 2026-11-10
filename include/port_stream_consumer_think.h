/*
 * port_stream_consumer_think.h — pure think-block primitives from
 * gateway/stream_consumer_think.py (StreamThinkFilterMixin): the block-boundary
 * predicate and the earliest-opening-tag scan. The self facts the live methods
 * read (_accumulated, _OPEN_THINK_TAGS) are parameters/constants here.
 */
#ifndef PORT_STREAM_CONSUMER_THINK_H
#define PORT_STREAM_CONSUMER_THINK_H

/* _at_block_boundary(buf, idx): True when the tag at `idx` in `buf` starts a
 * block — start of text, or preceded by a newline/whitespace. `accumulated` is
 * the running stripped-output prefix (self._accumulated). */
int sct_at_block_boundary(const char *accumulated, const char *buf, int idx);

/* _earliest_open_tag(buf, lower_buf): (index, length) of the earliest
 * block-boundary opening think tag, or (-1, 0). Returns found(1)/not(0);
 * outputs the chosen (idx, len). lower_buf must be buf lowercased. */
int sct_earliest_open_tag(const char *accumulated, const char *buf,
                          const char *lower_buf, int *out_idx, int *out_len);

#endif /* PORT_STREAM_CONSUMER_THINK_H */
