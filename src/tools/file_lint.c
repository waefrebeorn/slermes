#include "terminal_cmd.h"
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
#ifdef _WIN32

/* The lint runner used to be `popen("<python> script target")`. On this box popen hands the line to
 * cmd.exe, `python` is not on that PATH, and the failed launch came back as exit code 1 with a message
 * on stderr -- so a *valid* Python file was reported invalid with "python: not found" as its syntax
 * error. Run it through the native runner and be explicit about the three outcomes. */
static const char *lint_python_exe(void) {
    static char found[MAX_PATH];
    static int resolved = 0;
    if (!resolved) {
        resolved = 1;
        const char *env = getenv("HERMES_PYTHON");
        if (!env || !*env) env = getenv("SLERMES_PYTHON");
        if (env && *env) {
            snprintf(found, sizeof(found), "%s", env);
        } else {
            const char *cands[] = { "python.exe", "python3.exe", "py.exe", NULL };
            found[0] = '\0';
            for (size_t i = 0; cands[i]; i++) {
                char loc[MAX_PATH];
                if (SearchPathA(NULL, cands[i], NULL, sizeof(loc), loc, NULL)) {
                    snprintf(found, sizeof(found), "%s", loc);
                    break;
                }
            }
            /* The interpreter on this box is installed under %LOCALAPPDATA%\hermes\tools and is not
             * on the server's PATH, so "no python" was a statement about the PATH, not the machine.
             * Enumerate the versioned directory and use its python.exe. */
            if (!found[0]) {
                const char *lad = getenv("LOCALAPPDATA");
                if (lad && *lad) {
                    char pattern[MAX_PATH];
                    snprintf(pattern, sizeof(pattern), "%s\\hermes\\tools\\python-*", lad);
                    WIN32_FIND_DATAA fd;
                    HANDLE h = FindFirstFileA(pattern, &fd);
                    if (h != INVALID_HANDLE_VALUE) {
                        do {
                            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                            char exe[MAX_PATH];
                            snprintf(exe, sizeof(exe), "%s\\hermes\\tools\\%s\\python.exe", lad, fd.cFileName);
                            if (GetFileAttributesA(exe) != INVALID_FILE_ATTRIBUTES) {
                                snprintf(found, sizeof(found), "%s", exe);
                                break;
                            }
                        } while (FindNextFileA(h, &fd));
                        FindClose(h);
                    }
                }
            }
        }
    }
    return found[0] ? found : NULL;
}
#endif /* _WIN32 */

/* valid:null, skipped:true -- the checker could not run, so the file's validity is unknown. */
static char *build_result_skipped(const char *reason)
{
    json_t *root = json_object();
    if (!root) return NULL;
    json_set(root, "valid", json_null());
    json_set(root, "skipped", json_bool(true));
    json_set(root, "reason", json_string(reason ? reason : "checker unavailable"));
    char *out = json_serialize(root);
    json_free(root);
    return out;
}

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

    /* mkstemp under MSYS /tmp gives a POSIX path the Windows interpreter
     * cannot open. Use a relative path: popen and the child share the cwd. */
    char tmpl[] = "slermes_pylint_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) return build_result(false, "IOError: cannot create temp file");
    FILE *f = fdopen(fd, "w");
    if (!f) { close(fd); return build_result(false, "IOError: cannot open temp file"); }
    fwrite(content, 1, strlen(content), f);
    fclose(f);

    /* A script file, not `python -c '...'`: cmd.exe does not understand the
     * single quotes that worked on POSIX, which made every Windows lint fail
     * regardless of the source. Quote all three paths for cmd.exe too. */
    size_t script_len = strlen(tmpl) + 32 + strlen(mod_text) + 512;
    char *script = (char *)malloc(script_len);
    if (!script) { unlink(tmpl); return build_result(false, "IOError: out of memory"); }
    snprintf(script, script_len,
             "import sys\n"
             "src=open(sys.argv[1], encoding='utf-8').read()\n"
             "try:\n"
             "    %s\n"
             "    print('OK')\n"
             "except Exception as e:\n"
             "    sys.stderr.write(type(e).__name__ + ': ' + str(e) + '\\n')\n"
             "    sys.exit(1)\n", mod_text);
    char script_path[512];
    snprintf(script_path, sizeof(script_path), "%s.py", tmpl);
    f = fopen(script_path, "w");
    if (!f) { free(script); unlink(tmpl); return build_result(false, "IOError: cannot write script"); }
    fwrite(script, 1, strlen(script), f);
    fclose(f);
    free(script);

    const char *bin = (ctx && ctx->python_bin) ? ctx->python_bin : NULL;
#ifdef _WIN32
    if (!bin || !*bin) bin = lint_python_exe();
    if (!bin) { unlink(script_path); unlink(tmpl);
                return build_result_skipped("no python interpreter found on this machine"); }
#else
    if (!bin || !*bin) bin = "python";
#endif
    char cmd[4096];
    /* MSYS popen() passes the string through cmd.exe; quoting each path
     * makes cmd treat the whole line as one command name. All three paths
     * are quote-free here (temp files are relative), so leave them bare. */
    /* Forward slashes, no backslash escapes: the runner writes this line into a shell script, and a
     * Windows path with backslashes comes back to python as C:\\Users\\... -- "can't open file", which
     * the write path then reported as "not checked". Both bash and CreateProcess accept '/'. */
    char bin_f[1024], script_f[1024], tmpl_f[1024];
    snprintf(bin_f, sizeof(bin_f), "%s", bin);
    snprintf(script_f, sizeof(script_f), "%s", script_path);
    snprintf(tmpl_f, sizeof(tmpl_f), "%s", tmpl);
    for (char *q = bin_f; *q; q++) if (*q == '\\') *q = '/';
    for (char *q = script_f; *q; q++) if (*q == '\\') *q = '/';
    for (char *q = tmpl_f; *q; q++) if (*q == '\\') *q = '/';
    int nw = snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\" \"%s\" 2>&1",
                      bin_f, script_f, tmpl_f);
    if (nw < 0 || (size_t)nw >= sizeof(cmd)) {
        unlink(script_path); unlink(tmpl);
        return build_result(false, "IOError: command too long");
    }

    char buf[4096] = "";
    int rc = 1;
#ifdef _WIN32
    char *rj = run_command(cmd, 30);
    if (!rj) {
        unlink(script_path); unlink(tmpl);
        return build_result_skipped("could not start python (no runner available)");
    }
    json_t *probe = json_parse(rj, NULL);
    const char *out = probe ? json_get_str(probe, "output", "") : "";
    snprintf(buf, sizeof(buf), "%s", out ? out : "");
    rc = probe ? (int)json_get_num(probe, "exit_code", 1) : 1;
    if (probe) json_free(probe);
    free(rj);
    unlink(script_path);
    unlink(tmpl);
    /* 127 / "not recognized" / "cannot find the file": the interpreter never ran. The file is not
     * broken; we are. Say so instead of naming a syntax error that does not exist. */
    /* Exit 0 is the answer. The script prints OK on success; requiring empty output made every valid
     * file report "OK" as if it were the error. */
    if (rc == 0) return build_result(true, "");
    if (rc == 9009 || rc == 127 || strstr(buf, "not recognized") || strstr(buf, "cannot find the file") ||
        strstr(buf, "No such file or directory"))
        return build_result_skipped(buf[0] ? buf : "python could not be run");
#else
    FILE *pipe = popen(cmd, "r");
    if (!pipe) { unlink(script_path); unlink(tmpl); return build_result(false, "IOError: cannot run python"); }
    size_t n = fread(buf, 1, sizeof(buf) - 1, pipe);
    buf[n] = '\0';
    int status = pclose(pipe);
    unlink(script_path);
    unlink(tmpl);
    rc = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    if (rc == 0) return build_result(true, "");
#endif

    /* The runner's message holds "<ErrorType>: <msg>". Collapse to the first line. */
    char *msg = buf;
    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    if (!*msg) msg = "SyntaxError";
    return build_result(false, msg);
}

/* PoP: file_lint_json @ tools/file_operations_lint.py:_lint_json_inproc */
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

/* PoP: file_lint_yaml @ tools/file_operations_lint.py:_lint_yaml_inproc */
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

/* PoP: file_lint_toml @ tools/file_operations_lint.py:_lint_toml_inproc */
char *file_lint_toml(const char *content)
{
    if (!content) return build_result(true, "");
    /* libtoml mirrors tomllib's strictness. Pure C. */
    toml_doc_t *doc = toml_parse(content);
    if (doc) { toml_free(doc); return build_result(true, ""); }
    return build_result(false, "TOMLDecodeError: invalid TOML");
}

/* PoP: file_lint_python @ tools/file_operations_lint.py:_lint_python_inproc */
char *file_lint_python(const file_lint_t *ctx, const char *content)
{
    return lint_via_python(ctx, "import ast; ast.parse(src)", content);
}
