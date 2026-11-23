#ifndef NOC_W13_REDACTPATT_316808_H
#define NOC_W13_REDACTPATT_316808_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_redactpatt_316808_skip_char_class @ agent/redact_pattern_shape.py:_skip_char_class
 * Given pattern[i] == '[', return the index just past the closing ']'
 * (a leading ']' is literal; backslash escapes the next char). */
int w13_redactpatt_316808_skip_char_class(const char *pattern, int i);

/* PoP: w13_redactpatt_316808_unbounded_quantifier_follows @ agent/redact_pattern_shape.py:_unbounded_quantifier_follows
 * True when an open-ended quantifier (*, +, {m,}) starts at pattern[j]. */
int w13_redactpatt_316808_unbounded_quantifier_follows(const char *pattern, int j);

/* PoP: w13_redactpatt_316808_pattern_structure @ agent/redact_pattern_shape.py:_pattern_structure
 * One scan -> [has_top_level_alternation, has_nested_unbounded_repeat]. */
struct json_t *w13_redactpatt_316808_pattern_structure(const char *pattern);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_REDACTPATT_316808_H */
