/*
 * file_ops_lint.c — lint-unusability guard, ported from tools/file_operations.py.
 *
 * Faithful port of tools/file_operations.py:_looks_like_linter_unusable
 * (tools/file_operations.py:585). Python keys the unusable-detection patterns
 * by the linter's base command (npx / rustfmt / go) via the module-level
 * _LINTER_UNUSABLE_PATTERNS dict. Returns true only when `base_cmd` has a
 * pattern set AND one of its patterns appears (case-insensitive) in `output`.
 *
 * The previous C form ignored base_cmd entirely and hard-coded two substrings
 * ("command not found" / "not installed") — wrong. This restores the
 * base_cmd-keyed, pattern-set semantics 1:1 with the Python source.
 *
 * Oracle-verified against LIVE tools/file_operations.py:_looks_like_linter_unusable
 * (see tests/sta_oracle_file_ops_lint.py).
 */

#include "file_ops_lint.h"
#include <stdlib.h>
#include <string.h>

bool file_ops_looks_like_linter_unusable(const char *base_cmd, const char *output)
{
    if (!base_cmd || !*base_cmd || !output) return false;

    /* Mirrors tools/file_operations.py:_LINTER_UNUSABLE_PATTERNS (line 562). */
    static const struct { const char *cmd; const char *pat[3]; } TABLE[] = {
        {"npx", {
            "this is not the tsc command you are looking for",
            "could not determine executable to run",
            "not found in npm registry" }},
        {"rustfmt", {
            "no input filename given",
            "error: not a workspace",
            NULL }},
        {"go", {
            "cannot find package",
            "go: cannot find main module",
            NULL }},
        {NULL, {NULL}},
    };

    const char *patterns[3] = {NULL, NULL, NULL};
    for (int i = 0; TABLE[i].cmd; i++) {
        if (strcmp(base_cmd, TABLE[i].cmd) == 0) {
            patterns[0] = TABLE[i].pat[0];
            patterns[1] = TABLE[i].pat[1];
            patterns[2] = TABLE[i].pat[2];
            break;
        }
    }
    if (!patterns[0]) return false;  /* no pattern set for this base_cmd */

    /* case-insensitive substring scan */
    char *low = malloc(strlen(output) + 1);
    if (!low) return false;
    for (size_t i = 0; output[i]; i++)
        low[i] = (char)(output[i] >= 'A' && output[i] <= 'Z' ? output[i] + 32 : output[i]);
    low[strlen(output)] = '\0';

    bool hit = false;
    for (int i = 0; i < 3 && patterns[i]; i++) {
        if (strstr(low, patterns[i])) { hit = true; break; }
    }
    free(low);
    return hit;
}
