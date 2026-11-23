#ifndef NOC_W5_TESTENV_CA5DA8_H
#define NOC_W5_TESTENV_CA5DA8_H
#include <stddef.h>

/* Module-level constants mirrored from pm/testenv.py. */
#define W5TESTENV_NAME "test-environment"
const char *w5testenv_name(void);
const char *const *w5testenv_default_extras(size_t *outn); /* ("all",) */
const char *const *w5testenv_groups(size_t *outn);         /* ("dev","test") */
const char *const *w5testenv_developer_const(size_t *outn); /* ("just",) */

/* PoP: w5testenv_developer_tools @ pm/testenv.py:developer_tools
 * Returns the DEVELOPER_TOOLS entries present in pinned (upstream order).
 * The returned list is malloc'd; free with w5testenv_free_list. */
int w5testenv_developer_tools(const char **pinned, size_t npinned,
                              char ***out, size_t *outn);
/* PoP: w5testenv_parse_extras @ pm/testenv.py:parse_extras
 * value.replace(",", " ").split() -- None when the result is empty (outn=0,
 * out set to NULL). Returned list is malloc'd; free with w5testenv_free_list. */
int w5testenv_parse_extras(const char *value, char ***out, size_t *outn);
void w5testenv_free_list(char **arr, size_t n);

#endif /* NOC_W5_TESTENV_CA5DA8_H */
