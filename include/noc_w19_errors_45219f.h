/* noc_w19_errors_45219f.h - public C API for persistence/sqlite/_state/errors.py. */
#ifndef NOC_W19_ERRORS_45219F_H
#define NOC_W19_ERRORS_45219F_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_errors_45219f_is_no_more_rows @ persistence/sqlite/_state/errors.py:_is_no_more_rows */
int w19_errors_45219f_is_no_more_rows(const char *msg);

/* PoP: w19_errors_45219f_is_transient @ persistence/sqlite/_state/errors.py:is_transient_sqlite_error
 * kind: "operational"|"database"|"integrity"|"programming"|"sqlite_error"|"runtime"|"str"|"none".
 * has_code/code model `exc.sqlite_errorcode` (absent when has_code == 0). */
int w19_errors_45219f_is_transient(const char *kind, int has_code, long long code,
                                   const char *msg);

/* PoP: w19_errors_45219f_is_malformed_schema @ persistence/sqlite/_state/errors.py:is_malformed_schema_error */
int w19_errors_45219f_is_malformed_schema(const char *kind, const char *msg);

/* PoP: w19_errors_45219f_classify @ persistence/sqlite/_state/errors.py:classify_persistence_error
 * exc is either a JSON string (RPC-wrapped text) or {"kind","code","msg"} with code optional.
 * Returns one of PERSISTENCE_ERROR_CAUSES. */
json_t *w19_errors_45219f_classify(json_t *exc);

/* PoP: w19_errors_45219f_compression_closed_msg @ persistence/sqlite/_state/errors.py:CompressionSessionClosedError.__init__ */
json_t *w19_errors_45219f_compression_closed_msg(const char *session_id);

/* PoP: w19_errors_45219f_is_fts_scoped @ storage_errors.py:is_fts_scoped_corruption_error */
int w19_errors_45219f_is_fts_scoped(int has_code, long long code, const char *msg);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_ERRORS_45219F_H */
