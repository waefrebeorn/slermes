/*
 * environment.c — faithful C11 port of upstream Hermes pm/environment.py.
 *
 * PoP: w5_environment_032cbd_is_bytecode_compile_failure @ pm/environment.py:_is_bytecode_compile_failure
 * PoP: w5_environment_032cbd_classify_uv_failure        @ pm/environment.py:classify_uv_failure
 *
 * The uv subprocess engine (PythonEnvironment._run/_run_streaming,
 * managed_environment, sync/lock/check/publish) is process/IO-bound and cannot
 * be captured by a deterministic fixture; only the pure failure classification
 * helpers are ported here.
 */
#include "noc_w5_environment_032cbd.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define BYTECODE_MARKER "failed to bytecode-compile"
#define UNDATED_PIN     "has no publish time"
#define UNDATED_FILE_A  "is missing an upload date"
#define UNDATED_FILE_B  "was filtered by `exclude-newer"
#define BUILD_MARKER    "the build backend returned an error"
#define DEFAULT_REMEDY  "retry, or run `hermes pm doctor`"
static const char *const RESOLVER_MARKERS[] = {
    "no solution found", "conflicting requirements", "conflicting urls",
    "because only the following versions", "and your pyproject depends on",
};
#define UNDATED_HINT "the package index did not report upload times, which locking Hermes's " \
                     "dependencies needs for its 3-day release-age check. Use https://pypi.org/simple " \
                     "or a mirror that serves upload times (pip.conf index-url / UV_INDEX_URL), then retry."

static char *lower_dup(const char *s) {
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 1);
    for (size_t i = 0; i < n; i++) o[i] = (char)tolower((unsigned char)s[i]);
    o[n] = '\0';
    return o;
}

static int py_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *py_strip(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && py_space(s[a])) a++;
    while (b > a && py_space(s[b - 1])) b--;
    char *o = (char *)malloc(b - a + 1);
    memcpy(o, s + a, b - a);
    o[b - a] = '\0';
    return o;
}

int w5_environment_032cbd_is_bytecode_compile_failure(int returncode,
        const char *stderr_text, const char *stdout_text) {
    if (!returncode) return 0;                       /* Python: if not result.returncode */
    size_t ne = stderr_text ? strlen(stderr_text) : 0;
    size_t no = stdout_text ? strlen(stdout_text) : 0;
    char *t = (char *)malloc(ne + 1 + no + 1);
    if (stderr_text) memcpy(t, stderr_text, ne);     /* stderr or '' */
    t[ne] = '\n';                                    /* joined with '\n' */
    if (stdout_text) memcpy(t + ne + 1, stdout_text, no); /* stdout or '' */
    t[ne + 1 + no] = '\0';
    char *low = lower_dup(t);
    int hit = strstr(low, BYTECODE_MARKER) != NULL;
    free(low); free(t);
    return hit;
}

int w5_environment_032cbd_classify_uv_failure(const char *stage, int returncode,
        const char *output, const char *build_remedy,
        w5_environment_032cbd_error_t *out) {
    memset(out, 0, sizeof(*out));
    out->package = strdup("venv");
    out->cause = strdup("");
    out->remedy = strdup(DEFAULT_REMEDY);
    out->kind = "InstallError";

    /* cause = f"uv {stage} exited {returncode}: {output.strip()[-600:]}" */
    char *stripped = py_strip(output ? output : "");
    size_t L = strlen(stripped);
    const char *tail = stripped + (L > 600 ? L - 600 : 0);
    size_t need = 64 + (stage ? strlen(stage) : 0) + strlen(tail) + 1;
    char *cause = (char *)malloc(need);
    snprintf(cause, need, "uv %s exited %d: %s", stage ? stage : "", returncode, tail);
    out->cause = cause;
    free(stripped);

    char *low = lower_dup(output ? output : "");
    if (strstr(low, UNDATED_PIN) ||
        (strstr(low, UNDATED_FILE_A) && strstr(low, UNDATED_FILE_B))) {
        free(out->remedy);
        out->remedy = strdup(UNDATED_HINT);
    } else {
        int resolver = 0;
        for (size_t i = 0; i < sizeof(RESOLVER_MARKERS) / sizeof(RESOLVER_MARKERS[0]); i++)
            if (strstr(low, RESOLVER_MARKERS[i])) { resolver = 1; break; }
        if (resolver) {
            out->kind = "ResolutionConflict";
        } else if (strstr(low, BUILD_MARKER)) {
            out->kind = "BuildFailure";
            free(out->remedy);
            out->remedy = strdup(build_remedy ? build_remedy : "");
        }
        /* else: plain InstallError with the default remedy already set */
    }
    free(low);
    return 1;
}

void w5_environment_032cbd_error_free(w5_environment_032cbd_error_t *e) {
    if (!e) return;
    free(e->package); free(e->cause); free(e->remedy);
    memset(e, 0, sizeof(*e));
}
