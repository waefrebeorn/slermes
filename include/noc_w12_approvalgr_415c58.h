#ifndef NOC_W12_APPROVALGR_415C58_H
#define NOC_W12_APPROVALGR_415C58_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_approvalgr_415c58_quoted_grep_pattern_spans @ tools/approval_grep_spans.py:_quoted_grep_pattern_spans
 * Returns {"spans": [[start,end],...], "malformed": bool}; caller frees. */
struct json_t *w12_approvalgr_415c58_quoted_grep_pattern_spans(const char *command);

/* PoP: w12_approvalgr_415c58_grep_safe_detection_variant @ tools/approval_grep_spans.py:_grep_safe_detection_variant
 * Returns {"command": <str>, "malformed": bool}; caller frees. */
struct json_t *w12_approvalgr_415c58_grep_safe_detection_variant(const char *command);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_APPROVALGR_415C58_H */
