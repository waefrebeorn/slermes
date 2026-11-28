/* gate: quoted_rm_command(dirs) -> "mkdir -p " + shlex-quoted dirs.
 * Each input is one fixture; "|" inside it separates directories, matching the
 * twin, so one fixture can cover a multi-directory batch. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
char* cli_tools_environments_file_sync_quoted_rm_command(const char **dirs, int dir_count,
                                                            char *buf, size_t bufsz);
int main(int argc, char **argv) {
    if (argc < 3) return 2;   /* argv[1] is the c_fn dispatch name */
    char buf[8192];
    for (int i = 2; i < argc; i++) {
        char *work = strdup(argv[i] ? argv[i] : "");
        char *dirs[64]; int n = 0;
        char *tok = work; 
        /* split on '|' keeping empty fields: an empty fixture is one empty dir */
        for (;;) {
            char *bar = strchr(tok, '|');
            if (bar) *bar = '\0';
            if (n < 64) dirs[n++] = tok;
            if (!bar) break;
            tok = bar + 1;
        }
        WCUP_BEGIN("quoted_rm_command");
        char *r = cli_tools_environments_file_sync_quoted_rm_command((const char **)dirs, n, buf, sizeof buf);
        WCUP_VALSTR(r ? r : "");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("quoted_rm_command");
        free(work);
    }
    return 0;
}
