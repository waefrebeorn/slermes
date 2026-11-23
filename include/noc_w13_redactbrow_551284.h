#ifndef NOC_W13_REDACTBROW_551284_H
#define NOC_W13_REDACTBROW_551284_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_redactbrow_551284_literal_end @ agent/redact_browser_input.py:_literal_end
 * (prefix, quote, end) of the string literal at `start`, or JSON null. */
struct json_t *w13_redactbrow_551284_literal_end(const char *code, int start);

/* PoP: w13_redactbrow_551284_skip_comment @ agent/redact_browser_input.py:_skip_comment
 * Index past the '#' comment at `i` (line break excluded). */
int w13_redactbrow_551284_skip_comment(const char *code, int i);

/* PoP: w13_redactbrow_551284_literal_mask @ agent/redact_browser_input.py:_literal_mask
 * [start, end, replacement] or JSON null. */
struct json_t *w13_redactbrow_551284_literal_mask(const char *code, int start);

/* PoP: w13_redactbrow_551284_call_end @ agent/redact_browser_input.py:_call_end
 * Index of the bracket closing the "()" that `i` sits inside, skipping strings
 * and comments. */
int w13_redactbrow_551284_call_end(const char *code, int i);

/* PoP: w13_redactbrow_551284_literal_regions @ agent/redact_browser_input.py:_literal_regions
 * Array of [start, end, replacement] masks for every string literal in
 * code[start:end] (raw-text scan, fail closed). */
struct json_t *w13_redactbrow_551284_literal_regions(const char *code, int start, int end);

/* PoP: w13_redactbrow_551284_unparsed_masks @ agent/redact_browser_input.py:_unparsed_masks
 * Every literal inside each typing-call region of an unparsed snippet. */
struct json_t *w13_redactbrow_551284_unparsed_masks(const char *code);

/* PoP: w13_redactbrow_551284_mask_unparsed @ agent/redact_browser_input.py:mask_typed_browser_text
 * The fail-closed masked code for a snippet that does not parse: unparsed
 * typing-call masks + .value-assignment masks, sorted and applied. The oracle
 * forces ast.parse to raise so the live module takes this exact branch. */
struct json_t *w13_redactbrow_551284_mask_unparsed(const char *code);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_REDACTBROW_551284_H */
