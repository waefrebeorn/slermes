#ifndef NOC_SESSOP_H
#define NOC_SESSOP_H

#include <stddef.h>

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Python str(v) coercion used by is_viewer_input's
 * `str(row['request_id']).startswith('hosted:')`.  Returns a malloc'd string;
 * caller frees.  None->"None", True->"True", False->"False", ints as decimal,
 * floats via %g, strings copied verbatim. */
char *sessop_py_str(const struct json_t *v);

/* PoP: sessop_is_viewer_input @ gateway/session_operator.py:is_viewer_input
 *
 * request_id_str is the Python str() of row['request_id'] (see sessop_py_str).
 * keys / n_keys are the keys of row['payload'].
 * True iff: request_id does not start with "hosted:" and
 *           {"text"} <= set(payload) <= LOCAL_INPUT_FIELDS|{"local_operator_v1"}.
 */
int sessop_is_viewer_input(const char *request_id_str,
                           const char *const *keys, size_t n_keys);

#ifdef __cplusplus
}
#endif

#endif /* NOC_SESSOP_H */
