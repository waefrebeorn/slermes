/* port_inline_source.c -- pure helpers ported from gateway/status_inline_source.py.
 *
 * PoP: st_inline_source_flag_index     @ gateway/status_inline_source.py:inline_source_flag_index
 * PoP: st_command_line_runs_inline    @ gateway/status_inline_source.py:command_line_runs_inline_source
 *
 * The regex half of that module (_bootstrap_entry / inline_bootstrap_argv) is a
 * separate unit: it needs seven fullmatch patterns plus base64, and a faithful
 * C transcription of those is higher-risk than it is valuable without a real
 * regex engine behind the oracle.
 */
#include "port_inline_source.h"

#include <stddef.h>
#include <string.h>

/* Canonical operand-taking interpreter options, verbatim from
 * hermes_state_holders.py:39-42. */
static const char *const LONG_OPS_WITH_OPERANDS[] = {
    "--check-hash-based-pycs", "--jit",
};

/* Index of the ``-c`` token when *tokens* is an interpreter running inline
 * source, or -1.
 *
 * Only interpreter options may precede ``-c``; the first non-option token ends
 * the option block (so ``python -m hermes_cli.main ...`` never matches). The
 * walk is VALUE-AWARE: -X/-W/-Q and the two long options take a SEPARATE
 * operand, and an operand-taking letter in a short cluster consumes the rest of
 * the cluster as its attached value -- so ``-Xc`` is ``-X c``, not an
 * inline-source ``-c``.
 *
 * *tokens* must be CASE-PRESERVING: the operand-taking ``-Q``/``-W``/``-X``
 * differ from the operand-less ``-q``/``-b``.
 */
int
st_inline_source_flag_index(const char **tokens, size_t n)
{
    size_t index = 1;
    while (index < n) {
        const char *token = tokens[index];

        if (strcmp(token, "--") == 0)
            return -1;

        int takes_operand = 0;
        for (size_t k = 0; k < sizeof(LONG_OPS_WITH_OPERANDS)
                                / sizeof(LONG_OPS_WITH_OPERANDS[0]); k++) {
            if (strcmp(token, LONG_OPS_WITH_OPERANDS[k]) == 0) {
                takes_operand = 1;
                break;
            }
        }
        if (takes_operand) {
            index += 2; /* the next token is this option's operand */
            continue;
        }
        if (token[0] == '-' && token[1] == '-') {
            index += 1; /* --opt=value and operand-less long options */
            continue;
        }
        if (token[0] != '-' || token[1] == '\0')
            return -1; /* first non-option token ends the option block */

        const char *cluster = token + 1;
        int advanced = 0;
        for (size_t pos = 0; cluster[pos] != '\0'; pos++) {
            char letter = cluster[pos];
            if (letter == 'c')
                return (int)index;
            if (letter == 'Q' || letter == 'W' || letter == 'X') {
                /* attached value in the cluster, else the following token */
                index += (cluster[pos + 1] != '\0') ? 1 : 2;
                advanced = 1;
                break;
            }
        }
        if (!advanced)
            index += 1;
    }
    return -1;
}

/* command_line_runs_inline_source: True when *tokens* is ``python -c <src>
 * [args]``. */
int
st_command_line_runs_inline(const char **tokens, size_t n)
{
    return st_inline_source_flag_index(tokens, n) >= 0;
}
