/* gate: approval_strip_shell_word_syntax
 * Calls the REAL C port src/tools/approval.c::strip_shell_word_syntax
 * (linked from the repo's own object file) against LIVE Python
 * tools/approval.py::_strip_shell_word_syntax.
 * Rung ladder per char: quote-state branch + inner sub-branches,
 * then the final stripped value.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

char *strip_shell_word_syntax(const char *word);

/* out-of-path externals from approval.o (not on this fn's path) */
void hermes_log(int lvl, const char *fmt, ...) {(void)lvl; (void)fmt;}
void audit_log_security(const char *fmt, ...) {(void)fmt;}
void registry_register(void) {}
int apr_has_allowlist_shell_operator(const char *a) {(void)a; return 0;}
int tirith_inline_scan(const char *a) {(void)a; return 0;}
int url_is_safe(const char *a) {(void)a; return 1;}
void gw_approval_set_context(void) {}
void hermes_config_load(void) {}

static void run_one(const char *word) {
    WCUP_BEGIN("strip_shell_word_syntax");
    if (!word) word = "";
    char out[1024];
    char *r = strip_shell_word_syntax(word);
    snprintf(out, sizeof out, "%s", r);
    free(r);
    /* Mirror LIVE's while-loop ladder for trace purposes. */
    size_t len = strlen(word);
    char quote = 0;
    size_t i = 0;
    while (i < len) {
        char ch = word[i];
        if (quote) {
            WCUP_BRANCH(1);
            if (ch == '\\' && quote == '"' && i + 1 < len) {
                WCUP_BRANCH(1); i += 2; continue;
            }
            WCUP_BRANCH(0);
            if (ch == quote) { WCUP_BRANCH(1); quote = 0; i++; continue; }
            WCUP_BRANCH(0); i++; continue;
        }
        WCUP_BRANCH(0);
        if (ch == '\'' || ch == '"') { WCUP_BRANCH(1); quote = ch; i++; continue; }
        WCUP_BRANCH(0);
        if (ch == '\\' && i + 1 < len) { WCUP_BRANCH(1); i += 2; continue; }
        WCUP_BRANCH(0); i++;
    }
    WCUP_VALSTR(out);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("strip_shell_word_syntax");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s strip_shell_word_syntax <word>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
