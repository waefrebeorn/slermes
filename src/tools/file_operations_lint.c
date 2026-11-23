/*
 * file_operations_lint.c — C port of upstream Hermes tools/file_operations_lint.py.
 *
 * PoP: w14_fileoperat_46571f_looks_like_linter_unusable @ tools/file_operations_lint.py:_looks_like_linter_unusable
 * PoP: w14_fileoperat_46571f_check_lint_delta @ tools/file_operations_lint.py:LintMixin._check_lint_delta
 *
 * The host-class lint execution (_check_lint) is injected by the oracle as
 * pre/post LintResult values; the pure delta-filtering logic is ported here.
 */
#include "noc_w14_fileoperat_46571f.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int contains_ci(const char *haystack, const char *needle) {
    char lower[16384];
    size_t n = strlen(haystack);
    if (n >= sizeof lower) n = sizeof lower - 1;
    for (size_t i = 0; i < n; i++) lower[i] = (char)tolower((unsigned char)haystack[i]);
    lower[n] = '\0';
    return strstr(lower, needle) != NULL;
}

int w14_fileoperat_46571f_looks_like_linter_unusable(const char *base_cmd,
                                                     const char *output) {
    static const char *const npx[] = {
        "this is not the tsc command you are looking for",
        "could not determine executable to run",
        "not found in npm registry", NULL};
    static const char *const rustfmt[] = {
        "no input filename given", "error: not a workspace", NULL};
    static const char *const go[] = {
        "cannot find package", "go: cannot find main module", NULL};
    const char *const *patterns = NULL;

    if (!output) return 0;
    if (base_cmd && !strcmp(base_cmd, "npx")) patterns = npx;
    else if (base_cmd && !strcmp(base_cmd, "rustfmt")) patterns = rustfmt;
    else if (base_cmd && !strcmp(base_cmd, "go")) patterns = go;
    if (!patterns) return 0;
    for (int i = 0; patterns[i]; i++)
        if (contains_ci(output, patterns[i])) return 1;
    return 0;
}

static int lint_success(const struct json_t *r) {
    return json_is_object(r) && json_get_bool(r, "success", false);
}
static int lint_skipped(const struct json_t *r) {
    return json_is_object(r) && json_get_bool(r, "skipped", false);
}
static const char *lint_output(const struct json_t *r) {
    return json_get_str(r, "output", "");
}

static struct json_t *lint_result(int success, int skipped,
                                  const char *output, const char *message) {
    struct json_t *o = json_object();
    if (!o) return NULL;
    json_set(o, "success", json_bool(success ? true : false));
    json_set(o, "skipped", json_bool(skipped ? true : false));
    json_set(o, "output", json_string(output ? output : ""));
    json_set(o, "message", json_string(message ? message : ""));
    return o;
}

/* Python: {ln.strip() for ln in output.splitlines() if ln.strip()} */
static void collect_lines(const char *text, char lines[256][1024], size_t *count,
                          int unique) {
    const char *p = text ? text : "";
    while (*p) {
        const char *end = strchr(p, '\n');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        const char *a = p, *b = p + len;
        char buf[1024];
        size_t j;
        while (a < b && isspace((unsigned char)*a)) a++;
        while (b > a && isspace((unsigned char)b[-1])) b--;
        if (a < b && b - a < (long)sizeof buf && *count < 256) {
            size_t n = (size_t)(b - a);
            memcpy(buf, a, n);
            buf[n] = '\0';
            for (j = 0; j < *count; j++)
                if (!strcmp(lines[j], buf)) break;
            if (j < *count && unique) { /* duplicate */ }
            else {
                memcpy(lines[*count], buf, n + 1);
                (*count)++;
            }
        }
        if (!end) break;
        p = end + 1;
    }
}

struct json_t *w14_fileoperat_46571f_check_lint_delta(const struct json_t *pre_result,
                                                      const struct json_t *post_result,
                                                      int pre_known) {
    const char *post_out;
    char pre_lines[256][1024], post_lines[256][1024];
    size_t npre = 0, npost = 0;
    char joined[32768], line[2048];
    size_t used = 0;

    if (!pre_known || !json_is_object(post_result) ||
        lint_success(post_result) || lint_skipped(post_result))
        return json_copy(post_result);

    if (!json_is_object(pre_result) || lint_success(pre_result) ||
        lint_skipped(pre_result) || !*lint_output(pre_result))
        return json_copy(post_result);

    post_out = lint_output(post_result);
    collect_lines(lint_output(pre_result), pre_lines, &npre, 1);
    collect_lines(post_out, post_lines, &npost, 0);

    /* Filter post lines already present (stripped) in pre output. */
    used = 0;
    for (size_t i = 0; i < npost; i++) {
        int seen = 0;
        for (size_t j = 0; j < npre; j++)
            if (!strcmp(post_lines[i], pre_lines[j])) { seen = 1; break; }
        if (seen) continue;
        snprintf(line, sizeof line, "%s%s", used ? "\n" : "", post_lines[i]);
        size_t ll = strlen(line);
        if (used + ll >= sizeof joined) break;
        memcpy(joined + used, line, ll);
        used += ll;
    }
    joined[used] = '\0';   /* NUL at the (possibly shortened) length */

    if (!used)
        return lint_result(0, 0, post_out,
            "Pre-existing lint errors \xe2\x80\x94 this edit didn't introduce "
            "new ones but the file is still broken.");

    char out[32768];
    snprintf(out, sizeof out,
             "New lint errors introduced by this edit "
             "(pre-existing errors filtered out):\n%s", joined);
    return lint_result(0, 0, out, "");
}
