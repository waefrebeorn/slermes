/*
 * test_shell_heredoc.c — behavioral oracle for shell_heredoc.c
 *
 * Cross-checked against the Python reference tools/shell_heredoc.py:
 * each case's expected output below was produced by the real Python
 * implementation and asserted equal to the C output.
 *
 * Build:
 *   cc -D_POSIX_C_SOURCE=200809L -I../include -I../lib/libjson -I../lib/libregex \
 *      ../src/tools/shell_heredoc.c ../lib/libregex/hermes_regex.c \
 *      ../lib/libjson/json.o test_shell_heredoc.c -o t && ./t
 */
#include "shell_heredoc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CK(cond) do { if(!(cond)){ printf("FAIL: %s (line %d)\n", #cond, __LINE__); fails++; } else { printf("ok: %s\n", #cond); } } while(0)
static int fails = 0;

typedef struct { const char *name; const char *input; const char *expected; } case_t;

static const case_t CASES[] = {
    {"cat quoted",
     "cat <<'EOF'\nsecret line\nanother\nEOF",
     "cat <<'EOF'\n\n\n"},
    {"python quoted",
     "python3 <<'PY'\nprint(1 & 2)\nPY",
     "python3 <<'PY'\n\n"},
    {"unquoted delimiter keep",
     "bash <<EOF\necho hi\nEOF",
     "bash <<EOF\necho hi\nEOF"},
    {"mixed unquoted",
     "cat <<EOF\nbody\nEOF",
     "cat <<EOF\nbody\nEOF"},
    {"double quote body",
     "osascript <<\"END\"\ntell app\nEND",
     "osascript <<\"END\"\n\n"},
    {"list operator no mask",
     "cat <<'EOF'\nbody\nEOF; echo x",
     "cat <<'EOF'\nbody\nEOF; echo x"},
    {"nested dollar",
     "cat <<'EOF'\n$(rm -rf)\nEOF",
     "cat <<'EOF'\n\n"},
    {"no heredoc",
     "echo hello world",
     "echo hello world"},
    {"here-string skip",
     "cat <<< 'hi'",
     "cat <<< 'hi'"},
    {"unknown consumer",
     "bash <<'EOF'\nbody\nEOF",
     "bash <<'EOF'\nbody\nEOF"},
    {"two heredocs quoted (unterminated first)",
     "cat <<'A'\nx\nA <<'B'\ny\nB",
     "cat <<'A'\nx\nA <<'B'\ny\nB"},
    {"quote in middle (list op & keeps)",
     "echo 'not heredoc' && cat <<'E'\nbody\nE",
     "echo 'not heredoc' && cat <<'E'\nbody\nE"},
    {"comment then heredoc",
     "# comment\ncat <<'E'\nbody\nE",
     "# comment\ncat <<'E'\n\n"},
};

#define NCASES (sizeof(CASES)/sizeof(CASES[0]))

int main(void) {
    for (size_t i = 0; i < NCASES; i++) {
        char *out = strip_inert_heredoc_bodies(CASES[i].input);
        if (!out) { printf("FAIL: %s returned NULL\n", CASES[i].name); fails++; continue; }
        if (strcmp(out, CASES[i].expected) != 0) {
            printf("FAIL: %s\n  input:    %s\n  expected: %s\n  got:      %s\n",
                   CASES[i].name, CASES[i].input, CASES[i].expected, out);
            fails++;
        } else {
            printf("ok: %s\n", CASES[i].name);
        }
        free(out);
    }
    printf("\n%s (%d failures)\n", fails ? "ORACLE FAILED" : "ORACLE PASSED", fails);
    return fails ? 1 : 0;
}
