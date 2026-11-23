#ifndef NOC_W5_TERMUXLIBS_3B3B3B_H
#define NOC_W5_TERMUXLIBS_3B3B3B_H

#include <stddef.h>
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_termuxlibs_3b3b3b_package_of @ pm/termux_libs.py:package_of
 * The pool package a pinned URL names, or NULL for any other source. Caller frees. */
char *w5_termuxlibs_3b3b3b_package_of(const char *url);

/* PoP: w5_termuxlibs_3b3b3b_filename @ pm/termux_libs.py:_filename
 * Final path segment of a URL (rsplit('/', 1)[-1]). Caller frees. */
char *w5_termuxlibs_3b3b3b_filename(const char *url);

/* PoP: w5_termuxlibs_3b3b3b_parse_index @ pm/termux_libs.py:parse_index
 * RFC822-ish index -> JSON object {name: {name,version,url,sha256}}. */
struct json_t *w5_termuxlibs_3b3b3b_parse_index(const char *text);

/* PoP: w5_termuxlibs_3b3b3b_constants @ pm/termux_libs.py:<module>
 * The module-level pool/table literals. */
struct json_t *w5_termuxlibs_3b3b3b_constants(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_TERMUXLIBS_3B3B3B_H */
