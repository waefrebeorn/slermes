#ifndef NOC_W5_FILES_19AE74_H
#define NOC_W5_FILES_19AE74_H

/* C home for upstream Hermes utils/files.py — gate w5_files_19ae74. */
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_files_19ae74_yaml11_ambiguous @ utils/files.py:_rt_value
 * True when a plain string is a YAML-1.1-ambiguous word (_YAML11_AMBIGUOUS_WORDS). */
int w5_files_19ae74_yaml11_ambiguous(const char *word);

/* PoP: w5_files_19ae74_contended_replace_error @ utils/files.py:_is_contended_windows_replace_error
 * True when is_windows and winerror is in _WINDOWS_CONTENDED_REPLACE_ERRORS.
 * winerror == -1 represents a missing winerror (None). */
int w5_files_19ae74_contended_replace_error(int is_windows, long long winerror);

/* PoP: w5_files_19ae74_mtime_is_racy @ utils/files.py:mtime_is_racy
 * now_ns - mtime_ns < RACY_MTIME_WINDOW_NS. Arguments are int64 DECIMAL STRINGS. */
int w5_files_19ae74_mtime_is_racy(const char *mtime_ns, const char *now_ns);

/* PoP: w5_files_19ae74_file_signature @ utils/files.py:file_signature
 * (mtime_ns, size, ino, ctime_ns) as an array of four int64 decimal strings;
 * all four arguments are int64 DECIMAL STRINGS. Caller frees. */
json_t *w5_files_19ae74_file_signature(const char *mtime_ns, const char *size,
                                       const char *ino, const char *ctime_ns);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_FILES_19AE74_H */
