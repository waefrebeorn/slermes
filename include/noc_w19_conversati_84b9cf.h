/* noc_w19_conversati_84b9cf.h - public C API for agent/conversation_compression_manual.py port. */
#ifndef NOC_W19_CONVERSATI_84B9CF_H
#define NOC_W19_CONVERSATI_84B9CF_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_conversati_84b9cf_summary_committed @ agent/conversation_compression_manual.py:summary_committed */
json_t *w19_conversati_84b9cf_summary_committed(json_t *summary);
/* PoP: w19_conversati_84b9cf_parse_compress_args @ agent/conversation_compression_manual.py:parse_compress_args */
json_t *w19_conversati_84b9cf_parse_compress_args(const char *raw_args);
/* PoP: w19_conversati_84b9cf_estimate_request_tokens @ agent/conversation_compression_manual.py:estimate_request_tokens */
json_t *w19_conversati_84b9cf_estimate_request_tokens(json_t *messages, json_t *rough_map,
                                                      double rough_default);
/* PoP: w19_conversati_84b9cf_compress_now @ agent/conversation_compression_manual.py:compress_now */
json_t *w19_conversati_84b9cf_compress_now(json_t *spec);
/* PoP: w19_conversati_84b9cf_render_compress_result @ agent/conversation_compression_manual.py:render_compress_result */
json_t *w19_conversati_84b9cf_render_compress_result(json_t *result, const char *prefix);
/* PoP: w19_conversati_84b9cf_result_props @ agent/conversation_compression_manual.py:CompressResult */
json_t *w19_conversati_84b9cf_result_props(json_t *before_messages, json_t *after_messages,
                                           const char *status, json_t *summary);
/* PoP: w19_conversati_84b9cf_constants @ agent/conversation_compression_manual.py:MIN_MESSAGES */
json_t *w19_conversati_84b9cf_constants(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_CONVERSATI_84B9CF_H */
