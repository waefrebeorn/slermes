/*
 * runtime.c — C port of upstream Hermes pm/runtime.py.
 *
 * PoP: w5runtime_validate_result @ pm/runtime.py:_validate
 * PoP: w5runtime_command_args    @ pm/runtime.py:runtime_command
 *
 * The filesystem/subprocess/lease-bound stages are IO-only upstream; the gate
 * covers the two deterministic transformations: _validate's result expression
 * and runtime_command's argv construction for both launch contracts.
 */
#include "noc_w5_runtime_3f1397.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *LAUNCHER =
    "import runpy,sys; sys.path.insert(0,sys.argv.pop(1)); "
    "script=sys.argv.pop(1); sys.argv[0]=script; "
    "runpy.run_path(script,run_name='__main__')";

const char *w5runtime_launcher(void) { return LAUNCHER; }

char *w5runtime_validate_result(int returncode, const char *stderr_text) {
    if (!returncode) return strdup("");
    const char *s = stderr_text ? stderr_text : "";
    while (*s && isspace((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    if (e > s) {
        size_t n = (size_t)(e - s);
        char *out = (char *)malloc(n + 1);
        if (out) { memcpy(out, s, n); out[n] = '\0'; }
        return out;
    }
    char *out = (char *)malloc(32);
    if (out) snprintf(out, 32, "exit %d", returncode);
    return out;
}

int w5runtime_command_args(int resident, const char *python, const char *site,
                           const char *script, const char **args, size_t nargs,
                           char ***out, size_t *outn) {
    size_t extra = resident ? 7 : 4; /* fixed tokens before user args */
    const char **v = (const char **)calloc(extra + nargs, sizeof(const char *));
    size_t k = 0;
    if (!v) { if (out) *out = NULL; if (outn) *outn = 0; return 0; }
    v[k++] = python ? python : "";
    v[k++] = "-I";
    if (resident) {
        v[k++] = "-S";
        v[k++] = "-B";
        v[k++] = "-c";
        v[k++] = LAUNCHER;
        v[k++] = site ? site : "";
        v[k++] = script ? script : "";
    } else {
        v[k++] = "-B";
        v[k++] = script ? script : "";
    }
    for (size_t i = 0; i < nargs; i++) v[k++] = args[i] ? args[i] : "";

    char **res = (char **)calloc(k, sizeof(char *));
    if (res) for (size_t i = 0; i < k; i++) res[i] = strdup(v[i]);
    free(v);
    if (out) *out = res;
    if (outn) *outn = k;
    return 1;
}

void w5runtime_free_list(char **arr, size_t n) {
    if (!arr) return;
    for (size_t i = 0; i < n; i++) free(arr[i]);
    free(arr);
}
