#ifndef NOC_W15_DELEGATETO_C3B7D4_H
#define NOC_W15_DELEGATETO_C3B7D4_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_delegateto_c3b7d4_stringify @ tools/delegate_tool_results.py:_stringify_tool_content */
struct json_t *w15_delegateto_c3b7d4_stringify(const struct json_t *content);
/* PoP: w15_delegateto_c3b7d4_looks_error @ tools/delegate_tool_results.py:_looks_like_error_output */
struct json_t *w15_delegateto_c3b7d4_looks_error(const struct json_t *content);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_DELEGATETO_C3B7D4_H */
