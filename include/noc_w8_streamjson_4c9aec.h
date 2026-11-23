#ifndef NOC_W8_STREAMJSON_4C9AEC_H
#define NOC_W8_STREAMJSON_4C9AEC_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_streamjson_4c9aec_requested @ hermes_cli/stream_json.py:stream_json_requested
 * Returns {"requested":bool,"error":str|null,"quiet":bool}. The quiet flag
 * reflects the forced args.quiet=True on the accepted path. */
struct json_t *w8_streamjson_4c9aec_requested(const struct json_t *args);

/* PoP: w8_streamjson_4c9aec_text_delta @ hermes_cli/stream_json.py:StreamJsonEmitter.on_text_delta
 * Drop only falsy sentinels; otherwise {"type":"text","text":str(text)}. */
struct json_t *w8_streamjson_4c9aec_text_delta(const struct json_t *text);

/* PoP: w8_streamjson_4c9aec_result_payload @ hermes_cli/stream_json.py:StreamJsonEmitter.emit_result
 * Terminal result envelope. duration_ms is injected (fixture seam); the
 * timestamp added by _emit is not part of this payload. */
struct json_t *w8_streamjson_4c9aec_result_payload(const struct json_t *result,
                                                  const char *session_id,
                                                  const struct json_t *exit_code,
                                                  const struct json_t *duration_ms,
                                                  const char *bound_session_id);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_STREAMJSON_4C9AEC_H */
