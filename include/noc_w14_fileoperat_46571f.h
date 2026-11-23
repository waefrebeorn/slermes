#ifndef NOC_W14_FILEOPERAT_46571F_H
#define NOC_W14_FILEOPERAT_46571F_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_fileoperat_46571f_looks_like_linter_unusable @ tools/file_operations_lint.py:_looks_like_linter_unusable */
int w14_fileoperat_46571f_looks_like_linter_unusable(const char *base_cmd,
                                                     const char *output);

/* PoP: w14_fileoperat_46571f_check_lint_delta @ tools/file_operations_lint.py:LintMixin._check_lint_delta */
/* pre_result / post_result are LintResult-shaped objects:
 *   {"success": bool, "skipped": bool, "output": str, "message": str}
 * pre_known models Python's `pre_content is not None`. The returned object has
 * the same LintResult shape. */
struct json_t *w14_fileoperat_46571f_check_lint_delta(const struct json_t *pre_result,
                                                      const struct json_t *post_result,
                                                      int pre_known);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_FILEOPERAT_46571F_H */
