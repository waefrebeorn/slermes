/*
 * status_inline_source.c — C port of upstream Hermes gateway/status_inline_source.py.
 *
 * PoP: w16_statusinli_06dc54_inline_source_flag_index @ gateway/status_inline_source.py:inline_source_flag_index
 * PoP: w16_statusinli_06dc54_command_line_runs_inline_source @ gateway/status_inline_source.py:command_line_runs_inline_source
 *
 * Value-aware walk over case-preserving argv tokens (from process_identity's
 * canonical operand sets). Returns -1 for "no -c flag" (Python None).
 */
#include "noc_w16_statusinli_06dc54.h"
#include <string.h>

/* process_identity._PYTHON_LONG_OPTIONS_WITH_OPERANDS / _SHORT_..._WITH_OPERANDS */
static const char *const LONG_OPTS[] = { "--check-hash-based-pycs", "--jit", NULL };

static bool is_long_opt(const char *t) {
    for (size_t i = 0; LONG_OPTS[i]; i++)
        if (!strcmp(t, LONG_OPTS[i])) return true;
    return false;
}
static bool is_short_opt(char c) {
    return c == 'W' || c == 'X' || c == 'Q';
}
static const char *tok(const json_t *tokens, size_t i) {
    json_t *n = json_get(tokens, i);
    return (n && n->type == JSON_STRING) ? n->str_val : "";
}

static int flag_index(const json_t *tokens) {
    if (!tokens || tokens->type != JSON_ARRAY) return -1;
    size_t n = json_len(tokens);
    size_t index = 1;
    while (index < n) {
        const char *token = tok(tokens, index);
        if (!strcmp(token, "--")) return -1;
        if (is_long_opt(token)) { index += 2; continue; }
        if (token[0] == '-' && token[1] == '-') { index += 1; continue; }
        if (token[0] != '-' || token[0] == '\0') return -1;  /* not startswith("-") */
        if (token[0] == '-' && token[1] == '\0') return -1;   /* token == "-" */
        /* Clustered short options; an operand-taking letter consumes the rest
         * of the cluster as its attached value, or the following token. */
        const char *cluster = token + 1;
        size_t clen = strlen(cluster);
        bool consumed = false;
        for (size_t pos = 0; pos < clen; pos++) {
            char letter = cluster[pos];
            if (letter == 'c') return (int)index;
            if (is_short_opt(letter)) {
                index += (pos + 1 < clen) ? 1 : 2;
                consumed = true;
                break;
            }
        }
        if (!consumed) index += 1;
    }
    return -1;
}

json_t *w16_statusinli_06dc54_inline_source_flag_index(const json_t *tokens) {
    int idx = flag_index(tokens);
    return idx < 0 ? json_null() : json_number((double)idx);
}

bool w16_statusinli_06dc54_command_line_runs_inline_source(const json_t *tokens) {
    return flag_index(tokens) >= 0;
}
