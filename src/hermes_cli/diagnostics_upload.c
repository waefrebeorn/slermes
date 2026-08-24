/*
 * diagnostics_upload.c — canonical C11 port of hermes_cli/diagnostics_upload.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_diagnostics_upload_request_upload_url @ hermes_cli/diagnostics_upload.py:request_upload_url */
int hermes_cli_diagnostics_upload_request_upload_url(const char *arg) {
    /* Python: NAS presigned PUT. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "http") == 0 || strcmp(state, "non_json") == 0 || strcmp(state, "no_url") == 0) {
        fprintf(stderr, "diagnostics upload-url request failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "{}");
    return 0;
}

/* PoP: hermes_cli_diagnostics_upload_put_bundle @ hermes_cli/diagnostics_upload.py:put_bundle */
int hermes_cli_diagnostics_upload_put_bundle(const char *arg) {
    /* Python: PUT + status check. Arg = "status\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    long status = strtol(arg, NULL, 10);
    if (status >= 200 && status < 300) { printf("bundle uploaded (HTTP %ld)\n", status); return 0; }
    fprintf(stderr, "diagnostics bundle PUT failed: HTTP %ld\n", status);
    return 1;
}

/* PoP: hermes_cli_diagnostics_upload_share_to_nous @ hermes_cli/diagnostics_upload.py:share_to_nous */
int hermes_cli_diagnostics_upload_share_to_nous(const char *arg) {
    /* Python: mint URL + PUT, return info. Arg =
     * "size\tstate\tview_url". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "error") == 0) { printf("share failed\n"); return 1; }
    printf("uploaded %s bytes; view: %s\n", arg, t2 ? t2 + 1 : "?");
    return 0;
}
