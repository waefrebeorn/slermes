/*
 * update_cmd_check.c — C home for upstream Hermes hermes_cli/update_cmd_check.py.
 *
 * PoP: w15_updatecmd_78c8c3_tracking_refspec @ hermes_cli/update_cmd_check.py:tracking_refspec
 * PoP: w15_updatecmd_78c8c3_is_shallow_repository @ hermes_cli/update_cmd_check.py:is_shallow_repository
 * PoP: w15_updatecmd_78c8c3_compare_ref_exists @ hermes_cli/update_cmd_check.py:compare_ref_exists
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w15_updatecmd_78c8c3.h"

#define CAP 8192

static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

static void strip_copy(const char *s, char *buf, size_t cap) {
    size_t n = strlen(s ? s : "");
    if (n >= cap) n = cap - 1;
    memcpy(buf, s ? s : "", n);
    buf[n] = '\0';
    size_t b = 0, e = n;
    while (b < e && isspace((unsigned char)buf[b])) b++;
    while (e > b && isspace((unsigned char)buf[e - 1])) e--;
    if (b > 0) memmove(buf, buf + b, e - b);
    buf[e - b] = '\0';
}

json_t *w15_updatecmd_78c8c3_tracking_refspec(const char *remote, const char *branch) {
    char out[CAP];
    snprintf(out, sizeof(out), "+refs/heads/%s:refs/remotes/%s/%s",
             branch ? branch : "", remote ? remote : "", branch ? branch : "");
    return json_string(out);
}

json_t *w15_updatecmd_78c8c3_is_shallow_repository(const char *git_stdout) {
    char s[CAP];
    strip_copy(git_stdout, s, sizeof(s));
    return json_bool(!strcmp(s, "true"));
}

json_t *w15_updatecmd_78c8c3_compare_ref_exists(int returncode) {
    return json_bool(returncode == 0);
}

void hermes_cli_update_cmd_check_port_pending(void) { (void)0; }
