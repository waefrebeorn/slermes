#ifndef NOC_W5_FILESYSTEM_427A52_H
#define NOC_W5_FILESYSTEM_427A52_H

/* Pure Windows path-spelling primitives from pm/filesystem.py.
 * Returned strings are malloc'd; the caller owns and must free() them.
 * IO/lock/ACL helpers are environment-bound and not gated here. */

/* PoP: w5_filesystem_427a52_native @ pm/filesystem.py:native
 * Strip a leading \\?\ (or \\?\UNC\) verbatim prefix; ordinary text is
 * returned unchanged. */
char *w5_filesystem_427a52_native(const char *path);

/* PoP: w5_filesystem_427a52_long_form @ pm/filesystem.py:_long_form
 /* Add the verbatim prefix to a path. Already-verbatim paths are returned
  * unchanged. CAVEAT: ntpath.abspath()'s separator normalisation for
  * non-verbatim paths is Python-version dependent, so the gate exercises only
  * the deterministic already-verbatim early-return branch. */
char *w5_filesystem_427a52_long_form(const char *text);

#endif /* NOC_W5_FILESYSTEM_427A52_H */
