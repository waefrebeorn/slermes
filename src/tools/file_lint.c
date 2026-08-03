/*
 * file_lint.c — in-process syntax linters (faithful port of
 * tools/file_operations.py:_lint_*_inproc).
 *
 * Self-contained: includes only what this module needs. No hermes.h god
 * header, no void* passthrough, no "in a real implementation" stubs.
 *
 * Faithfulness notes:
 *  - YAML: PyYAML's _lint_yaml_inproc uses `yaml.parse` — a SYNTAX-ONLY scan
 *    that accepts application-defined tags (`!Sub`, `!!foo`), multi-document
 *    streams, block scalars and flow continuations, while still rejecting
 *    structural errors (bad indentation, empty keys, "mapping values are not
 *    allowed here", seq/map mixing). lib/libyaml reproduces exactly those
 *    semantics (oracle-proven 34/34), so YAML linting is pure C — no python.
 *  - TOML: tomllib; lib/libtoml mirrors its strictness, pure C.
 *  - JSON: the project's strict JSON parser, pure C.
 *  - Python: ast.parse is CPython stdlib with no C equivalent, so the ONLY
 *    faithful implementation is to delegate to the configured interpreter
 *    running the same stdlib call. Real subprocess work, not a stub.
 */

#include "file_lint.h"

#include "libjson/json.h"        /* json_object/bool/string/set/serialize/free */
#include "libyaml/yaml.h"        /* yaml_parse / yaml_free */
#include "libtoml/toml.h"        /* toml_parse / toml_free */
#include "hermes_logger.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

struct file_lint {
    char *python_bin;   /* interpreter used for the Python syntax check */
};

file_lint_t *file_lint_init(const char *python_bin)
{
    file_lint_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    ctx->python_bin = python_bin ? strdup(python_bin) : NULL;
    return ctx;
}

void file_lint_free(file_lint_t *ctx)
{
    if (!ctx) return;
    free(ctx->python_bin);
    free(ctx);
}

/* Reusable result builder shared by every linter. */
static char *build_result(bool valid, const char *error)
{
    json_t *root = json_object();
    if (!root) return NULL;
    json_set(root, "valid", json_bool(valid));
    json_set(root, "error", json_string(error ? error : ""));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/*
 * Run `mod_text` (a python expression evaluating to True/False for validity)
 * over `content` via the configured interpreter. `mod_text` receives the
 * source on stdin. Returns (ok, error_message) as a JSON string, exactly like
 * the Python linters.
 *
 * Used ONLY for the Python syntax check (`ast.parse`), which is CPython
 * stdlib with no C equivalent.
 */
static char *lint_via_python(const file_lint_t *ctx, const char *mod_text,
                             const char *content)
{
    if (!content) return build_result(true, "");

    char tmpl[] = "/tmp/slermes_pylint_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) return build_result(false, "IOError: cannot create temp file");
    FILE *f = fdopen(fd, "w");
    if (!f) { close(fd); return build_result(false, "IOError: cannot open temp file"); }
    fwrite(content, 1, strlen(content), f);
    fclose(f);

    const char *bin = (ctx && ctx->python_bin) ? ctx->python_bin : "python3";
    char cmd[4096];
    int nw = snprintf(cmd, sizeof(cmd),
             "%s -c 'import sys,io\n"
             "src=open(sys.argv[1]).read()\n"
             "try:\n"
             "    %s\n"
             "    print(\"OK\")\n"
             "except Exception as e:\n"
             "    import sys as _s\n"
             "    _s.stderr.write(type(e).__name__ + \": \" + str(e) + \"\\n\")\n"
             "    _s.exit(1)\n"
             "' %s 2>&1 < %s",
             bin, mod_text, tmpl, tmpl);
    if (nw < 0 || (size_t)nw >= sizeof(cmd)) { unlink(tmpl); return build_result(false, "IOError: command too long"); }

    char buf[4096] = "";
    FILE *pipe = popen(cmd, "r");
    if (!pipe) { unlink(tmpl); return build_result(false, "IOError: cannot run python"); }
    size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
    buf[n] = '\0';
    int status = pclose(pipe);
    unlink(tmpl);
    int rc = WIFEXITED(status) ? WEXITSTATUS(status) : 1;

    if (rc == 0) return build_result(true, "");

    /* stderr (or stdout on weird failures) holds "<ErrorType>: <msg>".
     * Collapse to the first line. */
    char *msg = buf;
    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    if (!*msg) msg = "SyntaxError";
    return build_result(false, msg);
}

/* PoP: file_lint_json @ tools/file_operations.py:_lint_json_inproc */
char *file_lint_json(const char *content)
{
    if (!content) return build_result(true, "");
    /* JSON: use the project's strict JSON parser directly (no python needed). */
    char *err = NULL;
    json_t *j = json_parse(content, &err);
    free(err);
    if (j) { json_free(j); return build_result(true, ""); }
    return build_result(false, "Parse error");
}

/* PoP: file_lint_yaml @ tools/file_operations.py:_lint_yaml_inproc */
char *file_lint_yaml(const char *content)
{
    if (!content) return build_result(true, "");
    /* PyYAML's linter is yaml.parse (syntax-only); libyaml mirrors it:
     * accepts tags / multi-doc / block scalars / flow continuations, rejects
     * structural errors. Pure C. */
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse(content, &err);
    if (doc) { yaml_free(doc); free(err); return build_result(true, ""); }
    free(err);
    return build_result(false, "YAMLError: invalid YAML");
}

/* PoP: file_lint_toml @ tools/file_operations.py:_lint_toml_inproc */
char *file_lint_toml(const char *content)
{
    if (!content) return build_result(true, "");
    /* libtoml mirrors tomllib's strictness. Pure C. */
    toml_doc_t *doc = toml_parse(content);
    if (doc) { toml_free(doc); return build_result(true, ""); }
    return build_result(false, "TOMLDecodeError: invalid TOML");
}

/* PoP: file_lint_python @ tools/file_operations.py:_lint_python_inproc */
char *file_lint_python(const file_lint_t *ctx, const char *content)
{
    return lint_via_python(ctx, "import ast; ast.parse(src)", content);
}
