#ifndef NOC_W10_SESSIONENV_1FFEFB_H
#define NOC_W10_SESSIONENV_1FFEFB_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_sessionenv_1ffefb_media_clause @ gateway/session_envelope.py:_media_clause
 * event object keys: media_urls, media_types, media_text_inlined, media_filenames.
 * Returns a newly allocated JSON string naming the first refused clause, or JSON null. */
struct json_t *w10_sessionenv_1ffefb_media_clause(const struct json_t *event);
/* PoP: w10_sessionenv_1ffefb_native_metadata @ gateway/session_envelope.py:_native_metadata
 * event object keys: metadata (any JSON value), platform (str = source.platform.value).
 * Returns a newly allocated projected JSON object, or JSON null (refuse).
 * The 16 KiB native size probe uses compact JSON byte length; fixtures stay far from
 * the boundary so the compact-vs-python-spaced serialisation cannot change the verdict. */
struct json_t *w10_sessionenv_1ffefb_native_metadata(const struct json_t *event);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_SESSIONENV_1FFEFB_H */
