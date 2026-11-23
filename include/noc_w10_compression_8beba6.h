#ifndef NOC_W10_COMPRESSION_8BEBA6_H
#define NOC_W10_COMPRESSION_8BEBA6_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_compression_8beba6_elision_marker @ agent/compression_marker.py:_elision_marker
 * Renders the non-imitable elision marker with comma-grouped counts; caller frees. */
char *w10_compression_8beba6_elision_marker(long long omitted, long long total);

/* PoP: w10_compression_8beba6_elision_marker_max_len @ agent/compression_marker.py:ELISION_MARKER_MAX_LEN
 * Widest marker rendering (10^12-1 counts), measured in code points. */
long long w10_compression_8beba6_elision_marker_max_len(void);

/* PoP: w10_compression_8beba6_elide @ agent/compression_marker.py:elide
 * Cap text at `limit` code points appending an elision marker; caller frees. */
char *w10_compression_8beba6_elide(const char *text, long long limit);

/* PoP: w10_compression_8beba6_elide_middle @ agent/compression_marker.py:elide_middle
 * Keep `head` code points from the start and `tail` from the end, elide the middle; caller frees. */
char *w10_compression_8beba6_elide_middle(const char *text, long long head, long long tail);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_COMPRESSION_8BEBA6_H */
