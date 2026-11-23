/*
 * main_query_file.c — C port of upstream Hermes hermes_cli/main_query_file.py.
 *
 * _read_query_file(args) mutates args.query (or prints an error and exits 2):
 *   if not args.query_file: return
 *   if args.query: error "mutually exclusive"; exit(2)
 *   content = sys.stdin.read() if query_file == "-" else open(query_file,
 *             encoding="utf-8-sig", errors="replace").read()
 *   on OSError: error "cannot read --query-file <qf>: <e>"; exit(2)
 *   args.query = content
 *   if not (args.query or "").strip(): error "... is empty"; exit(2)
 *
 * File/stdin content is injected at the gate so no real IO is needed.
 *
 * PoP: qfile_read @ hermes_cli/main_query_file.py:_read_query_file
 * PoP: qfile_is_empty @ hermes_cli/main_query_file.py:_read_query_file
 * PoP: qfile_errors @ hermes_cli/main_query_file.py:_read_query_file
 */
#include "noc_qfile.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *qfile_mutual_error(void) {
    return strdup("Error: -q/--query and --query-file are mutually exclusive\n");
}
char *qfile_read_error_msg(const char *qf, const char *detail) {
    size_t n = strlen("Error: cannot read --query-file : \n") + strlen(qf ? qf : "") +
               strlen(detail ? detail : "");
    char *s = (char *)malloc(n + 1);
    if (!s) return NULL;
    snprintf(s, n + 1, "Error: cannot read --query-file %s: %s\n",
             qf ? qf : "", detail ? detail : "");
    return s;
}
char *qfile_empty_error_msg(const char *qf) {
    size_t n = strlen("Error: --query-file  is empty\n") + strlen(qf ? qf : "");
    char *s = (char *)malloc(n + 1);
    if (!s) return NULL;
    snprintf(s, n + 1, "Error: --query-file %s is empty\n", qf ? qf : "");
    return s;
}

/* Python str.strip() over the ASCII whitespace the gate exercises. */
int qfile_is_empty(const char *s) {
    if (!s) return 1;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if (!isspace(*p)) return 0;
    return 1;
}

/* utf-8-sig strips one leading UTF-8 BOM. */
static char *strip_bom(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    if (n >= 3 && (unsigned char)s[0] == 0xEF &&
        (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
        return strdup(s + 3);
    return strdup(s);
}

/* PoP: qfile_read @ hermes_cli/main_query_file.py:_read_query_file */
void qfile_read(const char *query_file, int qf_is_null,
                const char *query, int query_is_null,
                const char *stdin_content,
                const char *file_content, int read_ok,
                const char *err_detail,
                qfile_result_t *out) {
    memset(out, 0, sizeof(*out));
    const char *qf = qf_is_null ? NULL : query_file;

    /* if not _qfile: return */
    if (!qf || !qf[0]) {
        out->action = QFILE_NOOP;
        out->query = (query_is_null || !query) ? NULL : strdup(query);
        return;
    }
    /* if args.query: mutually exclusive */
    if (!query_is_null && query && query[0]) {
        out->action = QFILE_EXIT;
        out->code = 2;
        out->stderr_msg = qfile_mutual_error();
        out->query = strdup(query);
        return;
    }

    char *content = NULL;
    if (!strcmp(qf, "-")) {
        content = strdup(stdin_content ? stdin_content : "");
    } else {
        if (!read_ok) {
            out->action = QFILE_EXIT;
            out->code = 2;
            out->stderr_msg = qfile_read_error_msg(qf, err_detail);
            out->query = (query_is_null || !query) ? NULL : strdup(query);
            return;
        }
        content = strip_bom(file_content);
    }

    out->query = strdup(content);
    if (qfile_is_empty(content)) {
        out->action = QFILE_EXIT;
        out->code = 2;
        out->stderr_msg = qfile_empty_error_msg(qf);
    } else {
        out->action = QFILE_SET;
    }
    free(content);
}

void qfile_result_free(qfile_result_t *r) {
    if (!r) return;
    free(r->query);
    free(r->stderr_msg);
    memset(r, 0, sizeof(*r));
}
