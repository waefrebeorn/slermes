/* t_port_file_lint.c — faithful verification harness for the file_lint module
 * (src/tools/file_lint.c), the v549 extraction of tools/file_operations.py's
 * in-process linters. Emits one JSON line per (fn, input) so
 * sta_oracle_file_lint.py can compare against the LIVE Python linters.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file_lint.h"

static char *json_escape(const char *s)
{
    size_t need = 2; /* quotes */
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '"': case '\\': case '\n': case '\r': case '\t':
                need += 2; break;
            default: need += 1; break;
        }
    }
    char *out = malloc(need + 1);
    if (!out) return NULL;
    char *q = out;
    *q++ = '"';
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '"':  *q++ = '\\'; *q++ = '"';  break;
            case '\\': *q++ = '\\'; *q++ = '\\'; break;
            case '\n': *q++ = '\\'; *q++ = 'n';  break;
            case '\r': *q++ = '\\'; *q++ = 'r';  break;
            case '\t': *q++ = '\\'; *q++ = 't';  break;
            default:   *q++ = *p; break;
        }
    }
    *q++ = '"';
    *q = '\0';
    return out;
}

static void emit(file_lint_t *ctx, const char *fn, const char *in)
{
    char *in_json  = json_escape(in);
    char *out_raw  = NULL;
    if      (strcmp(fn, "yaml") == 0)   out_raw = file_lint_yaml(in);
    else if (strcmp(fn, "toml") == 0)   out_raw = file_lint_toml(in);
    else if (strcmp(fn, "python") == 0) out_raw = file_lint_python(ctx, in);
    else if (strcmp(fn, "json") == 0)   out_raw = file_lint_json(in);

    char *out_json = json_escape(out_raw ? out_raw : "null");
    printf("{\"fn\":\"%s\",\"in\":%s,\"out\":%s}\n", fn, in_json, out_json);
    free(out_raw);
    free(in_json);
    free(out_json);
}

int main(void)
{
    file_lint_t *ctx = file_lint_init(NULL);

    /* YAML fixtures */
    emit(ctx, "yaml", "name: hermes\nversion: 1\nnested:\n  key: value\n");
    emit(ctx, "yaml", "a: 1\nb:\n  - x\n  - y\n");
    emit(ctx, "yaml", "foo: [1, 2, 3]\nbar: \"str\"\n");
    emit(ctx, "yaml", "a: 1\n  b: 2\n");          /* bad indent -> error */
    emit(ctx, "yaml", ": missing key\n");            /* error */
    emit(ctx, "yaml", "!!invalid tag: here\n");      /* error */

    /* TOML fixtures */
    emit(ctx, "toml", "name = \"hermes\"\nversion = 1\n[table]\nkey = \"v\"\n");
    emit(ctx, "toml", "arr = [1, 2, 3]\nflag = true\n");
    emit(ctx, "toml", "[server]\nhost = \"localhost\"\nport = 8080\n");
    emit(ctx, "toml", "a = 1\nb = 2 = 3\n");         /* error */
    emit(ctx, "toml", "= \"no key\"\n");             /* error */
    emit(ctx, "toml", "[unterminated\n");            /* error */

    /* Python fixtures */
    emit(ctx, "python", "def f(x):\n    return x + 1\n");
    emit(ctx, "python", "x = [1, 2, 3]\ny = {'a': 1}\n");
    emit(ctx, "python", "class A:\n    def m(self):\n        pass\n");
    emit(ctx, "python", "def f(:\n    return 1\n");  /* SyntaxError */
    emit(ctx, "python", "x = 1\n y = 2\n");          /* IndentationError */
    emit(ctx, "python", "return 5\n");               /* SyntaxError */

    /* JSON fixtures (the original lint_json_inproc) */
    emit(ctx, "json", "{\"a\": 1, \"b\": [2, 3]}");
    emit(ctx, "json", "{not valid json");

    file_lint_free(ctx);
    return 0;
}
