#ifndef NOC_QFILE_H
#define NOC_QFILE_H
#include <stddef.h>

typedef enum { QFILE_NOOP = 0, QFILE_SET = 1, QFILE_EXIT = 2 } qfile_action_t;

typedef struct {
    int  action;       /* qfile_action_t */
    int  code;         /* SystemExit code (2) when action == QFILE_EXIT */
    char *query;       /* resulting args.query; NULL = Python None */
    char *stderr_msg;  /* full text incl. trailing '\n'; NULL = nothing printed */
} qfile_result_t;

/* PoP: qfile_read @ hermes_cli/main_query_file.py:_read_query_file
 * query_file/query NULL model Python None. stdin_content is sys.stdin.read()
 * for the '-' case; file_content/read_ok/err_detail model open()+read(). */
void qfile_read(const char *query_file, int qf_is_null,
                const char *query, int query_is_null,
                const char *stdin_content,
                const char *file_content, int read_ok,
                const char *err_detail,
                qfile_result_t *out);
void qfile_result_free(qfile_result_t *r);

/* PoP: qfile_is_empty @ hermes_cli/main_query_file.py:_read_query_file
 * Python `not (args.query or "").strip()`. */
int qfile_is_empty(const char *s);

/* PoP: qfile_errors @ hermes_cli/main_query_file.py:_read_query_file
 * The two fixed error lines (with trailing '\n', matching print()). */
char *qfile_mutual_error(void);
char *qfile_read_error_msg(const char *qf, const char *detail);
char *qfile_empty_error_msg(const char *qf);

#endif /* NOC_QFILE_H */
