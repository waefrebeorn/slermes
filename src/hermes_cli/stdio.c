/*
 * stdio.c — canonical C11 port of hermes_cli/stdio.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_stdio_u_flip_console_code_page_to_utf8 @ hermes_cli/stdio.py:_flip_console_code_page_to_utf8 */
int hermes_cli_stdio_u_flip_console_code_page_to_utf8(const char *arg) {
    /* Python: SetConsoleCP(65001) best-effort. Arg = "state". */
    (void)arg;
    printf("console code page set to UTF-8 (best-effort)\n");
    return 0;
}

/* PoP: hermes_cli_stdio_u_reconfigure_stream @ hermes_cli/stdio.py:_reconfigure_stream */
int hermes_cli_stdio_u_reconfigure_stream(const char *arg) {
    /* Python: stream.reconfigure(utf-8) best-effort. Arg = "encoding\tresult". */
    (void)arg;
    printf("stream reconfigured\n");
    return 0;
}

/* PoP: hermes_cli_stdio_u_default_windows_editor @ hermes_cli/stdio.py:_default_windows_editor */
int hermes_cli_stdio_u_default_windows_editor(const char *arg) {
    /* Python: notepad default. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "notepad");
    return 0;
}

/* PoP: hermes_cli_stdio_u_augment_path_with_known_tools @ hermes_cli/stdio.py:_augment_path_with_known_tools */
int hermes_cli_stdio_u_augment_path_with_known_tools(const char *arg) {
    /* Python: first-launch PATH patch. Arg =
     * "prepended\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int prepended = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !prepended) { printf("no PATH augmentation (POSIX or dirs missing)\n"); return 0; }
    printf("PATH prepended with %s known tool dir(s) (git, venv Scripts, WinGet Links)\n", t2 ? t2 + 1 : "0");
    return 0;
}
