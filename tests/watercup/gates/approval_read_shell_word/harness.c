/* gate: approval_read_shell_word
 * Calls the REAL C port src/tools/approval.c::read_shell_word (linked from
 * the repo's own object file) against LIVE Python tools/approval.py::
 * _read_shell_word. Input argv format: "<pos><TAB><command>".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"

size_t read_shell_word(const char *s, size_t start, size_t len, char *out, size_t out_cap);

/* out-of-path externals from approval.o (not on read_shell_word's path) */
void audit_log_security(const char *fmt, ...){(void)fmt;}
int apr_has_allowlist_shell_operator(const char *a){(void)a;return 0;}
int tirith_inline_scan(const char *a){(void)a;return 0;}
int url_is_safe(const char *a){(void)a;return 1;}
void gw_approval_set_context(void){}
void hermes_config_load(void){}
void registry_register(void){}
void hermes_log(int lvl,const char *fmt,...){(void)lvl;(void)fmt;}

static void run_one(const char *input) {
    WCUP_BEGIN("read_shell_word");
    char *tmp = strdup(input);
    char *tab = strchr(tmp, '\t');
    size_t pos = 0;
    const char *command = tmp;
    if (tab) { *tab = '\0'; pos = (size_t)strtoul(tmp, NULL, 10); command = tab + 1; }
    size_t len = strlen(command);

    /* LIVE: start = _skip_shell_whitespace(command, pos) */
    size_t start = pos;
    while (start < len && isspace((unsigned char)command[start])) start++;

    char out[512];
    size_t n = read_shell_word(command, start, len, out, sizeof out);
    WCUP_BRANCH(n == 0);
    char sbuf[32], ebuf[32];
    snprintf(sbuf, sizeof sbuf, "%zu", start);
    snprintf(ebuf, sizeof ebuf, "%zu", start + n);
    WCUP_VALSTR(sbuf);
    WCUP_VALSTR(ebuf);
    WCUP_VALSTR(out);
    free(tmp);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("read_shell_word");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s read_shell_word <pos\\tcmd>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
