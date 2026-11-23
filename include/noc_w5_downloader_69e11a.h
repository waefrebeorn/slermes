#ifndef NOC_W5_DOWNLOADER_69E11A_H
#define NOC_W5_DOWNLOADER_69E11A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_downloader_69e11a_pinned @ pm/downloader.py:_pinned */
struct json_t *w5_downloader_69e11a_pinned(const char *sha256);

/* PoP: w5_downloader_69e11a_host @ pm/downloader.py:_host */
struct json_t *w5_downloader_69e11a_host(const char *url);

/* PoP: w5_downloader_69e11a_loopback @ pm/downloader.py:_loopback
 * Malformed bracketed hosts return false, matching the ValueError catch. */
struct json_t *w5_downloader_69e11a_loopback(const char *url);

/* PoP: w5_downloader_69e11a_coalesce @ pm/downloader.py:_coalesce
 * ranges: array of [start, end] pairs (numbers or decimal strings). */
struct json_t *w5_downloader_69e11a_coalesce(const struct json_t *ranges);

/* PoP: w5_downloader_69e11a_missing @ pm/downloader.py:_missing
 * Returns the uncovered gaps of [0, total). */
struct json_t *w5_downloader_69e11a_missing(const struct json_t *total,
                                            const struct json_t *covered);

/* PoP: w5_downloader_69e11a_source_plain_http @ pm/downloader.py:Source.plain_http */
struct json_t *w5_downloader_69e11a_source_plain_http(const char *url, const char *sha256,
                                                      const char *http_mirror);

/* PoP: w5_downloader_69e11a_source_byte_limit @ pm/downloader.py:Source.byte_limit */
struct json_t *w5_downloader_69e11a_source_byte_limit(const char *url, const char *sha256,
                                                      const char *http_mirror);

/* PoP: w5_downloader_69e11a_validate_range @ pm/downloader.py:_validate_range
 * Response headers are fixture fields; errors are returned as
 * {"__error__": "<live message>"} because C has no exceptions. */
struct json_t *w5_downloader_69e11a_validate_range(const struct json_t *status,
                        const char *content_range, const struct json_t *content_length,
                        const char *content_encoding, const struct json_t *start,
                        const struct json_t *end, const struct json_t *total);

/* PoP: w5_downloader_69e11a_strong_etag @ pm/downloader.py:_strong_etag */
struct json_t *w5_downloader_69e11a_strong_etag(const char *etag);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_DOWNLOADER_69E11A_H */
