#ifndef NOC_W6_FEISHULARK_1501FF_H
#define NOC_W6_FEISHULARK_1501FF_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_feishulark_1501ff_raw_body @ tools/feishu_lark.py:raw_body
 * has_raw indicates response.raw is present and has a `content` attribute;
 * content is that raw HTTP body string. Returns the parsed dict as a new
 * json_t (caller frees), or JSON null when absent/unparseable/not a dict. */
struct json_t *w6_feishulark_1501ff_raw_body(const char *content, int has_raw);

/* PoP: w6_feishulark_1501ff_response_data @ tools/feishu_lark.py:response_data
 * typed_data stands in for response.data; data_vars stands in for
 * vars(response.data) when that value is a non-dict object with __dict__.
 * Returns a new json_t (caller frees). */
struct json_t *w6_feishulark_1501ff_response_data(const char *content, int has_raw,
                                                  const struct json_t *typed_data,
                                                  const struct json_t *data_vars);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_FEISHULARK_1501FF_H */
