/* plugins_install_scan_flow2.c — FLOW port of the UNCLAIMED remainder of
 * hermes_cli/plugins_install_scan_context.py (all 38 defs; strict PoP grep at
 * authoring time found 0 claims for this module). The predicates are pure over
 * (finding, line, rel_path) but regex-dense, so every module-level compiled
 * pattern is a SEAM: the harness feeds the exact match projection recorded
 * from the REAL pattern by gen_plugins_install_scan_flow2_cases.py, and this
 * port implements the surrounding predicate logic (set membership, all/any,
 * span containment, pathlib.Path algebra, base64 media sniffing with the full
 * builtin exception hierarchy). Internal composition is REAL:
 *   prose_cap -> is_agent_facing; catalog_cap -> prose_cap; _executes;
 *   _sudo_in_safety_clause; _decoded_head; logical_line.
 * All functions are static and carry the pis2_ prefix.
 *
 * Representation contract (shared with tests/oracle_plugins_install_scan_flow2.py):
 *   - bool/str/None -> JSON bool/string/null; bytes -> array of ints.
 *   - fixture domain: ASCII operands, integer spans, plain finding objects.
 *   - seam queues live in `in.io`, consumed through pis2_seam in source order.
 * libjson-only; the harness #includes "cli/plugins_install_scan_flow2.c".
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* ── context + tiny helpers ──────────────────────────────────────────────── */
typedef struct pis2_ctx {
    json_t *io;        /* borrowed: seam queues */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} pis2_ctx;

static bool pis2_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool pis2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool pis2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool pis2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool pis2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static const char *pis2_jstr(const json_t *v) {
    return (pis2_is_str(v) && v->str_val) ? v->str_val : "";
}
static double pis2_num(const json_t *v) { return pis2_is_num(v) ? v->num_val : 0.0; }
static json_t *pis2_get(const json_t *obj, const char *key) {
    return pis2_is_obj(obj) ? json_obj_get(obj, key) : NULL;
}
static const char *pis2_fstr(json_t *in, const char *field) {
    return pis2_jstr(pis2_get(pis2_get(in, "finding"), field));
}
static bool pis2_in(const char *s, const char *const *a) {
    for (int i = 0; a[i]; i++) if (pis2_eq(s, a[i])) return true;
    return false;
}
static bool pis2_in_ci(const char *s, const char *const *a) {
    for (int i = 0; a[i]; i++) {
        size_t n = strlen(a[i]);
        if (strlen(s) == n && strncasecmp(s, a[i], n) == 0) return true;
    }
    return false;
}
static void pis2_lower(char *d, size_t n, const char *s) {
    size_t i = 0;
    for (; s[i] && i + 1 < n; i++) d[i] = (char)tolower((unsigned char)s[i]);
    d[i] = '\0';
}
static bool pis2_has(const char *h, const char *needle) {
    return needle && *needle && strstr(h, needle) != NULL;
}

/* ── exception plumbing (FULL builtin hierarchy) ────────────────────────── */
static bool pis2_exc_pending(const pis2_ctx *c) { return c && c->exc != NULL; }
static void pis2_exc_clear(pis2_ctx *c) {
    if (c && c->exc) { json_free(c->exc); c->exc = NULL; }
}
static void pis2_raise(pis2_ctx *c, const char *cls, const char *msg) {
    if (!c) return;
    if (c->exc) json_free(c->exc);
    c->exc = json_object();
    json_set(c->exc, "__raise__", json_string(cls));
    json_set(c->exc, "msg", json_string(msg ? msg : ""));
}
static const char *pis2_exc_class(const pis2_ctx *c) {
    return json_get_str(c->exc, "__raise__", "");
}
static const char *pis2_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"BaseException", NULL}, {"Exception", "BaseException"},
        {"GeneratorExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"SystemExit", "BaseException"},
        {"ArithmeticError", "Exception"}, {"AssertionError", "Exception"},
        {"AttributeError", "Exception"}, {"BufferError", "Exception"},
        {"EOFError", "Exception"}, {"FloatingPointError", "ArithmeticError"},
        {"ImportError", "Exception"}, {"ModuleNotFoundError", "ImportError"},
        {"LookupError", "Exception"}, {"MemoryError", "Exception"},
        {"NameError", "Exception"}, {"UnboundLocalError", "NameError"},
        {"OSError", "Exception"}, {"OverflowError", "ArithmeticError"},
        {"ReferenceError", "Exception"}, {"RuntimeError", "Exception"},
        {"NotImplementedError", "RuntimeError"}, {"RecursionError", "RuntimeError"},
        {"StopAsyncIteration", "Exception"}, {"StopIteration", "Exception"},
        {"SyntaxError", "Exception"}, {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"}, {"SystemError", "Exception"},
        {"TypeError", "Exception"}, {"ValueError", "Exception"},
        {"UnicodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
        {"JSONDecodeError", "ValueError"}, {"binascii.Error", "ValueError"},
        {"ZeroDivisionError", "ArithmeticError"},
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"TimeoutError", "OSError"},
        {"ConnectionError", "OSError"}, {"BrokenPipeError", "ConnectionError"},
        {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"},
        {"ConnectionResetError", "ConnectionError"},
        {"Warning", "Exception"}, {"UserWarning", "Warning"},
        {"DeprecationWarning", "Warning"}, {"RuntimeWarning", "Warning"},
        {"SyntaxWarning", "Warning"}, {"ResourceWarning", "Warning"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (pis2_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool pis2_exc_matches(const pis2_ctx *c, const char *const *classes, size_t n) {
    if (!pis2_exc_pending(c)) return false;
    const char *cls = pis2_exc_class(c);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (pis2_eq(cls, classes[i])) return true;
        const char *p = pis2_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}

/* ── seam queue ───────────────────────────────────────────────────────────── */
/* Pop the next recorded result for seam `name`. Returns an owned json_t* that
 * the caller must json_free (NULL when the queue is absent/exhausted). */
static json_t *pis2_seam(pis2_ctx *ctx, const char *name) {
    json_t *q = pis2_get(ctx->io, name);
    double cur = json_get_num(ctx->counters, name, 0.0);
    if (pis2_is_arr(q) && (size_t)cur < q->c.count) {
        json_set(ctx->counters, name, json_number(cur + 1));
        return json_copy(q->c.items[(size_t)cur]);
    }
    return NULL;
}
static bool pis2_seam_bool(pis2_ctx *ctx, const char *name) {
    json_t *v = pis2_seam(ctx, name);
    bool b = v && v->type == JSON_BOOL && v->bool_val;
    json_free(v);
    return b;
}
static long pis2_seam_int(pis2_ctx *ctx, const char *name) {
    json_t *v = pis2_seam(ctx, name);
    long n = v && pis2_is_num(v) ? (long)v->num_val : 0;
    json_free(v);
    return n;
}
static void pis2_seam_str(pis2_ctx *ctx, const char *name, char *buf, size_t n) {
    json_t *v = pis2_seam(ctx, name);
    const char *s = (v && pis2_is_str(v)) ? v->str_val : "";
    snprintf(buf, n, "%s", s ? s : "");
    json_free(v);
}
/* copy a JSON string field of an object element into buf */
static void pis2_obj_str(char *buf, size_t n, json_t *obj, const char *key) {
    snprintf(buf, n, "%s", pis2_jstr(pis2_get(obj, key)));
}

/* ── pathlib.PurePosixPath algebra (ASCII domain) ───────────────────────── */
#define PIS2_MAXPARTS 64
static int pis2_parts(const char *p, char out[PIS2_MAXPARTS][256]) {
    int n = 0;
    const char *s = p;
    while (*s && n < PIS2_MAXPARTS) {
        while (*s == '/') s++;
        if (!*s) break;
        const char *e = s;
        while (*e && *e != '/') e++;
        size_t len = (size_t)(e - s);
        if (len && !(len == 1 && s[0] == '.')) {
            size_t k = len < 255 ? len : 255;
            memcpy(out[n], s, k); out[n][k] = '\0'; n++;
        }
        s = e;
    }
    return n;
}
static const char *pis2_name(const char *p) {
    const char *slash = NULL;
    for (const char *s = p; *s; s++) if (*s == '/') slash = s;
    return slash ? slash + 1 : p;
}
static void pis2_suffix(const char *p, char *out, size_t n) {
    const char *name = pis2_name(p);
    const char *dot = NULL;
    for (const char *s = name; *s; s++) if (*s == '.') dot = s;
    if (!dot || dot == name || dot[1] == '\0') { out[0] = '\0'; return; }
    size_t i = 0;
    for (const char *s = dot; *s && i + 1 < n; s++) out[i++] = (char)tolower((unsigned char)*s);
    out[i] = '\0';
}

/* ── module constant sets ────────────────────────────────────────────────── */
static const char *DOC_EXT[] = {".md", ".txt", ".rst", ".html", NULL};
static const char *AGENT_FILES[] = {"skill.md", "after-install.md", NULL};
static const char *AGENT_DIRS[] = {"skills", "optional-skills", NULL};
static const char *PROSE_CATS[] = {"injection", "credential_exposure", NULL};
static const char *PROSE_IDS[] = {
    "context_exfil", "send_to_url", "md_image_exfil", "md_link_exfil", "ssh_backdoor",
    "curl_pipe_shell", "wget_pipe_shell", "curl_pipe_python",
    "agent_config_mod", "agent_config_mod_shell", "agent_config_contract", "agent_config_ref",
    "hermes_config_mod", "hermes_config_mod_shell", "hermes_config_ref",
    "other_agent_config_mod", "other_agent_config_mod_shell", "other_agent_config_ref", NULL};
static const char *CI_SUFFIX[] = {".yml", ".yaml", NULL};
static const char *TRUST_HOSTS[] = {
    "hermes-agent.nousresearch.com", "ollama.com", "bun.sh", "get.docker.com",
    "sh.rustup.rs", "deno.land", "astral.sh", "app.factory.ai", NULL};
static const char *PIPE_IDS[] = {"curl_pipe_shell", "wget_pipe_shell", "curl_pipe_python", NULL};
static const char *CATALOG_SUFFIX[] = {".yaml", ".yml", NULL};
static const char *CATALOG_IDS[] = {
    "agent_config_mod", "agent_config_mod_shell", "agent_config_contract", "agent_config_ref",
    "hermes_config_mod", "hermes_config_mod_shell", "hermes_config_ref",
    "other_agent_config_mod", "other_agent_config_mod_shell", "other_agent_config_ref", NULL};
static const char *TEST_DIRS[] = {"tests", "test", "testing", "spec", "specs", "fixtures", NULL};
static const char *TEST_ANY[] = {"__tests__", "__fixtures__", "__mocks__", NULL};
static const char *QUOTE_EXEC_SUFFIX[] = {
    ".py", ".pyi", ".js", ".mjs", ".cjs", ".jsx", ".ts", ".mts", ".cts", ".tsx", NULL};
static const char *BENIGN_OS[] = {
    "environ", "getenv", "fsync", "fspath", "path", "sep", "linesep", "name", "getcwd",
    "getpid", "getppid", "urandom", "cpu_count", "uname", "stat", "listdir", "scandir",
    "walk", "access", "isatty", "get_terminal_size", "devnull", "curdir", "pardir",
    "extsep", "altsep", "pathsep", NULL};
static const char *DISCLOSED_IDS[] = {"hermes_config_mod", NULL};
static const char *AGENT_DOC[] = {"after-install.md", NULL};
static const char *CONT_IDS[] = {"env_exfil_curl", "env_exfil_wget", NULL};
static const char *LOOPBACK_HOSTS[] = {"localhost", "127.0.0.1", "[::1]", NULL};
static const char *JSON_EXEC[] = {"package.json", NULL};
static const char *PUBKEY_PREFIX[] = {"GOCSPX-", "phc_", NULL};
static const char *BENIGN_PROC[] = {
    "cgroup", "mountinfo", "mounts", "stat", "statm", "status", "limits", "io", "comm",
    "cpuset", "sched", "schedstat", "oom_score", NULL};
static const char *INJECTION_IDS[] = {
    "prompt_injection_ignore", "disregard_rules", "forget_rules", NULL};

static int pis2_rank(const char *sev) {
    if (pis2_eq(sev, "low")) return 0;
    if (pis2_eq(sev, "medium")) return 1;
    if (pis2_eq(sev, "high")) return 2;
    if (pis2_eq(sev, "critical")) return 3;
    return 0;
}

/* ═════════════════════ ported defs ═══════════════════════════════════════ */

/* PoP: pis2_is_doc_prose @ hermes_cli/plugins_install_scan_context.py:is_doc_prose */
static json_t *pis2_is_doc_prose(pis2_ctx *ctx, json_t *in) {
    (void)ctx;
    const char *rp = pis2_jstr(pis2_get(in, "rel_path"));
    char suf[64]; pis2_suffix(rp, suf, sizeof suf);
    char nm[256]; snprintf(nm, sizeof nm, "%s", pis2_name(rp));
    if (!pis2_in_ci(suf, DOC_EXT) || pis2_in_ci(nm, AGENT_FILES)) return json_bool(false);
    char parts[PIS2_MAXPARTS][256];
    int n = pis2_parts(rp, parts);
    for (int i = 0; i < n - 1; i++)
        if (pis2_in_ci(parts[i], AGENT_DIRS)) return json_bool(false);
    return json_bool(true);
}

/* PoP: pis2_is_ci_workflow @ hermes_cli/plugins_install_scan_context.py:is_ci_workflow */
static json_t *pis2_is_ci_workflow(pis2_ctx *ctx, json_t *in) {
    (void)ctx;
    const char *rp = pis2_jstr(pis2_get(in, "rel_path"));
    char nm[256]; snprintf(nm, sizeof nm, "%s", pis2_name(rp));
    if (pis2_in_ci(nm, (const char *[]){".gitlab-ci.yml", NULL})) return json_bool(true);
    char parts[PIS2_MAXPARTS][256];
    int n = pis2_parts(rp, parts);
    char suf[64]; pis2_suffix(rp, suf, sizeof suf);
    return json_bool(n == 3 && pis2_in_ci(parts[0], (const char *[]){".github", NULL})
                     && pis2_in_ci(parts[1], (const char *[]){"workflows", NULL})
                     && pis2_in_ci(suf, CI_SUFFIX));
}

/* PoP: pis2_is_agent_facing @ hermes_cli/plugins_install_scan_context.py:is_agent_facing */
static bool pis2_agent_facing(json_t *in) {
    return pis2_in(pis2_fstr(in, "category"), PROSE_CATS)
        || pis2_in(pis2_fstr(in, "pattern_id"), PROSE_IDS);
}
static json_t *pis2_is_agent_facing(pis2_ctx *ctx, json_t *in) {
    (void)ctx;
    return json_bool(pis2_agent_facing(in));
}

static void pis2_stepdown(const char *sev, char *out, size_t n) {
    if (pis2_eq(sev, "critical")) snprintf(out, n, "high");
    else if (pis2_eq(sev, "high")) snprintf(out, n, "medium");
    else out[0] = '\0';
}
/* PoP: pis2_prose_cap @ hermes_cli/plugins_install_scan_context.py:prose_cap */
static json_t *pis2_prose_cap(pis2_ctx *ctx, json_t *in) {
    (void)ctx;
    if (pis2_agent_facing(in)) return json_null();
    char s[32]; pis2_stepdown(pis2_fstr(in, "severity"), s, sizeof s);
    return s[0] ? json_string(s) : json_null();
}

/* PoP: pis2_is_trusted_installer_url @ ...:_is_trusted_installer_url */
static bool pis2_trusted_url(const char *host, const char *path) {
    char h[256]; pis2_lower(h, sizeof h, host);
    if (pis2_in(h, TRUST_HOSTS)) return true;
    char p[1024]; pis2_lower(p, sizeof p, path ? path : "");
    return pis2_eq(h, "raw.githubusercontent.com") && pis2_has(p, "install");
}
static json_t *pis2_is_trusted_installer_url(pis2_ctx *ctx, json_t *in) {
    (void)ctx;
    return json_bool(pis2_trusted_url(pis2_jstr(pis2_get(in, "host")),
                                      pis2_jstr(pis2_get(in, "path"))));
}

/* PoP: pis2_is_trusted_installer_pipe @ ...:is_trusted_installer_pipe */
static json_t *pis2_is_trusted_installer_pipe(pis2_ctx *ctx, json_t *in) {
    if (!pis2_in(pis2_fstr(in, "pattern_id"), PIPE_IDS)) return json_bool(false);
    json_t *urls = pis2_seam(ctx, "url_in_line");
    bool ok = false;
    if (pis2_is_arr(urls) && urls->c.count > 0) {
        ok = true;
        for (size_t i = 0; i < urls->c.count; i++) {
            json_t *u = urls->c.items[i];
            if (!pis2_is_arr(u) || u->c.count < 2 ||
                !pis2_trusted_url(pis2_jstr(u->c.items[0]), pis2_jstr(u->c.items[1]))) {
                ok = false; break;
            }
        }
    }
    json_free(urls);
    return json_bool(ok);
}

/* PoP: pis2_is_locale_catalog @ ...:is_locale_catalog */
static json_t *pis2_is_locale_catalog(pis2_ctx *ctx, json_t *in) {
    (void)ctx;
    const char *rp = pis2_jstr(pis2_get(in, "rel_path"));
    char parts[PIS2_MAXPARTS][256]; int n = pis2_parts(rp, parts);
    char suf[64]; pis2_suffix(rp, suf, sizeof suf);
    return json_bool(n == 2 && pis2_in_ci(parts[0], (const char *[]){"locales", NULL})
                     && pis2_in_ci(suf, CATALOG_SUFFIX));
}

/* PoP: pis2_catalog_cap @ ...:catalog_cap */
static json_t *pis2_catalog_cap(pis2_ctx *ctx, json_t *in) {
    (void)ctx;
    const char *pid = pis2_fstr(in, "pattern_id");
    const char *sev = pis2_fstr(in, "severity");
    if (pis2_in(pid, CATALOG_IDS)) {
        if (pis2_rank(sev) <= 1) return json_string(sev);
        return json_string("medium");
    }
    if (pis2_in(pis2_fstr(in, "category"), PROSE_CATS)
        || pis2_in(pid, PROSE_IDS)) return json_null();
    char s[32]; pis2_stepdown(sev, s, sizeof s);
    return s[0] ? json_string(s) : json_null();
}

/* PoP: pis2_uninstall_operand @ ...:_uninstall_operand */
static json_t *pis2_uninstall_operand(pis2_ctx *ctx, json_t *in) {
    (void)ctx;
    const char *g = pis2_jstr(pis2_get(in, "group"));
    char buf[512];
    snprintf(buf, sizeof buf,
             "(?P<%s>[\"\\']?)(?:\\$HOME|\\$\\{HOME\\}|~)/\\.hermes(?:/[A-Za-z0-9][A-Za-z0-9._-]*)+/?(?P=%s)",
             g, g);
    return json_string(buf);
}

/* PoP: pis2_is_self_uninstall_doc @ ...:is_self_uninstall_doc */
static json_t *pis2_is_self_uninstall_doc(pis2_ctx *ctx, json_t *in) {
    if (!pis2_eq(pis2_fstr(in, "pattern_id"), "destructive_home_rm")) return json_bool(false);
    bool a = pis2_seam_bool(ctx, "self_uninstall_match");
    bool b = pis2_seam_bool(ctx, "self_uninstall_span_search");
    return json_bool(a || b);
}

/* PoP: pis2_exec_first_arg_literal_spans @ ...:_exec_first_arg_literal_spans */
static json_t *pis2_exec_first_arg_literal_spans(pis2_ctx *ctx, json_t *in) {
    json_t *ends = pis2_seam(ctx, "exec_call_ends");
    json_t *spans = pis2_get(in, "spans");
    json_t *out = json_array();
    if (pis2_is_arr(ends) && pis2_is_arr(spans)) {
        for (size_t i = 0; i < ends->c.count; i++) {
            long e = (long)pis2_num(ends->c.items[i]);
            for (size_t j = 0; j < spans->c.count; j++) {
                json_t *sp = spans->c.items[j];
                if (pis2_is_arr(sp) && sp->c.count >= 2 && (long)pis2_num(sp->c.items[0]) == e)
                    json_append(out, json_copy(sp));
            }
        }
    }
    json_free(ends);
    return out;
}

/* PoP: pis2_executes @ ...:_executes */
static bool pis2_executes(pis2_ctx *ctx, const char *rel_path) {
    char suf[64]; pis2_suffix(rel_path, suf, sizeof suf);
    if (pis2_in_ci(suf, QUOTE_EXEC_SUFFIX)) {
        char ignored[16384];
        pis2_seam_str(ctx, "literal_spans", ignored, sizeof ignored);
    }
    return pis2_seam_bool(ctx, "exec_on_line");
}
static json_t *pis2_executes_fn(pis2_ctx *ctx, json_t *in) {
    return json_bool(pis2_executes(ctx, pis2_jstr(pis2_get(in, "rel_path"))));
}

/* PoP: pis2_inert_fixture_line @ ...:is_inert_fixture_line */
static json_t *pis2_inert_fixture_line(pis2_ctx *ctx, json_t *in) {
    json_t *codev = pis2_get(in, "is_code");
    bool is_code = !codev || codev->type != JSON_BOOL || codev->bool_val;
    if (!is_code) return json_bool(true);
    const char *rp = pis2_jstr(pis2_get(in, "rel_path"));
    json_t *hits = pis2_seam(ctx, "pattern_hits");
    if (!pis2_is_arr(hits) || hits->c.count == 0) { json_free(hits); return json_bool(false); }
    json_t *spans = pis2_seam(ctx, "literal_spans");   /* list of {s,e} */
    json_t *exec_arg = pis2_exec_first_arg_literal_spans(ctx, in);
    bool executes = pis2_executes(ctx, rp);
    bool call_open = false;
    if (executes) call_open = pis2_seam_bool(ctx, "exec_call_ends");
    bool non_literal_exec = executes && !call_open;
    bool allq = true;
    if (pis2_is_arr(hits)) {
        for (size_t i = 0; i < hits->c.count; i++) {
            long hs = (long)pis2_num(pis2_get(hits->c.items[i], "s"));
            long he = (long)pis2_num(pis2_get(hits->c.items[i], "e"));
            bool quoted = false;
            if (pis2_is_arr(spans)) {
                for (size_t j = 0; j < spans->c.count; j++) {
                    long a = (long)pis2_num(pis2_get(spans->c.items[j], "s"));
                    long b = (long)pis2_num(pis2_get(spans->c.items[j], "e"));
                    if (a <= hs && he <= b) {
                        bool is_exec_arg = false;
                        if (pis2_is_arr(exec_arg)) {
                            for (size_t k = 0; k < exec_arg->c.count; k++) {
                                long ea = (long)pis2_num(pis2_get(exec_arg->c.items[k], "s"));
                                long eb = (long)pis2_num(pis2_get(exec_arg->c.items[k], "e"));
                                if (ea == a && eb == b) { is_exec_arg = true; break; }
                            }
                        }
                        if (!is_exec_arg) { quoted = true; break; }
                    }
                }
            }
            if (!quoted) { allq = false; break; }
        }
    }
    json_free(hits); json_free(spans); json_free(exec_arg);
    return json_bool(!non_literal_exec && allq);
}

/* PoP: pis2_is_test_tree @ ...:is_test_tree */
static json_t *pis2_is_test_tree(pis2_ctx *ctx, json_t *in) {
    (void)ctx;
    const char *rp = pis2_jstr(pis2_get(in, "rel_path"));
    char parts[PIS2_MAXPARTS][256]; int n = pis2_parts(rp, parts);
    if (n > 1 && pis2_in_ci(parts[0], TEST_DIRS)) return json_bool(true);
    for (int i = 0; i < n - 1; i++)
        if (pis2_in_ci(parts[i], TEST_ANY)) return json_bool(true);
    const char *nm = pis2_name(rp);
    /* ^(?:tests?_[^/]*|[^/]*_tests?\.[^./]+|[^/]*\.(?:test|spec)\.[^./]+)$ */
    char l[256]; pis2_lower(l, sizeof l, nm);
    size_t len = strlen(l);
    if (strncmp(l, "test_", 5) == 0 && len > 5) return json_bool(true);
    if (strncmp(l, "tests_", 6) == 0 && len > 6) return json_bool(true);
    const char *u = strstr(l, "_test.");
    if (u && u > l && strchr(l, '.') == u + 5) return json_bool(true);
    const char *v = strstr(l, "_tests.");
    if (v && v > l && strchr(l, '.') == v + 6) return json_bool(true);
    const char *w = strstr(l, ".test.");
    if (w && w > l && strrchr(l, '.') == w + 5) return json_bool(true);
    const char *z = strstr(l, ".spec.");
    if (z && z > l && strrchr(l, '.') == z + 5) return json_bool(true);
    return json_bool(false);
}

/* ── base64 media sniffing (real decode + exception hierarchy) ───────────── */
static int pis2_b64val(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
/* Strict base64 decode into out; raises binascii.Error on invalid input. */
static bool pis2_b64decode(pis2_ctx *ctx, const char *s, unsigned char *out, size_t *outn) {
    size_t n = strlen(s);
    if (n == 0) { *outn = 0; return true; }
    if (n % 4) { pis2_raise(ctx, "binascii.Error", "Invalid base64-encoded string"); return false; }
    size_t on = 0;
    for (size_t i = 0; i < n; i += 4) {
        int pad = 0;
        for (int j = 0; j < 4; j++) if (s[i + j] == '=') pad++;
        if (i + 4 < n && pad) { pis2_raise(ctx, "binascii.Error", "Invalid base64 data"); return false; }
        if (pad > 2) { pis2_raise(ctx, "binascii.Error", "Invalid base64 data"); return false; }
        int v[4];
        for (int j = 0; j < 4; j++) {
            if (s[i + j] == '=') { v[j] = 0; continue; }
            v[j] = pis2_b64val((unsigned char)s[i + j]);
            if (v[j] < 0) { pis2_raise(ctx, "binascii.Error", "Only base64 data is allowed"); return false; }
        }
        out[on++] = (unsigned char)((v[0] << 2) | (v[1] >> 4));
        if (pad < 2) out[on++] = (unsigned char)((v[1] << 4) | (v[2] >> 2));
        if (pad < 1) out[on++] = (unsigned char)((v[2] << 6) | v[3]);
    }
    *outn = on;
    return true;
}
/* PoP: pis2_decoded_head @ ...:_decoded_head */
static json_t *pis2_decoded_head(pis2_ctx *ctx, json_t *in) {
    const char *blob = pis2_jstr(pis2_get(in, "blob"));
    size_t len = strlen(blob);
    if (len > 16) len = 16;
    len -= len % 4;
    char head[20];
    memcpy(head, blob, len); head[len] = '\0';
    unsigned char out[32]; size_t on = 0;
    if (!pis2_b64decode(ctx, head, out, &on)) {
        static const char *CATCH[] = {"binascii.Error", "ValueError"};
        if (pis2_exc_matches(ctx, CATCH, 2)) { pis2_exc_clear(ctx); on = 0; }
    }
    json_t *arr = json_array();
    for (size_t i = 0; i < on; i++) json_append(arr, json_number((double)out[i]));
    return arr;
}
static const unsigned char MEDIA_MAGIC[][8] = {
    {0x89,'P','N','G'}, {0xff,0xd8,0xff}, {'G','I','F','8'}, {'R','I','F','F'},
    {'w','O','F','F'}, {'w','O','F','2'}, {'O','T','T','O'}, {0,1,0,0},
    {'t','t','c','f'}, {'B','M'}, {'%','P','D','F'}, {0,0,1,0},
    {'<','s','v','g'}, {'<','?','x','m','l'}, {'I','D','3'}, {'O','g','g','S'},
    {'f','L','a','C'}, {0x1a,0x45,0xdf,0xa3},
};
static const size_t MEDIA_MAGIC_LEN[] = {4,3,4,4,4,4,4,4,4,2,4,4,4,5,3,4,4,4};
static bool pis2_media_magic(const unsigned char *b, size_t n) {
    for (size_t i = 0; i < sizeof(MEDIA_MAGIC_LEN)/sizeof(MEDIA_MAGIC_LEN[0]); i++) {
        size_t m = MEDIA_MAGIC_LEN[i];
        if (n >= m && memcmp(b, MEDIA_MAGIC[i], m) == 0) return true;
    }
    return false;
}
/* PoP: pis2_is_base64_media @ ...:is_base64_media */
static json_t *pis2_is_base64_media(pis2_ctx *ctx, json_t *in) {
    json_t *runs = pis2_seam(ctx, "base64_runs");
    bool found = false;
    if (pis2_is_arr(runs)) {
        for (size_t i = 0; i < runs->c.count && !found; i++) {
            const char *blob = pis2_jstr(runs->c.items[i]);
            size_t len = strlen(blob); if (len > 16) len = 16; len -= len % 4;
            char head[20]; memcpy(head, blob, len); head[len] = '\0';
            unsigned char out[32]; size_t on = 0;
            if (pis2_b64decode(ctx, head, out, &on)) {
                if (pis2_media_magic(out, on)) found = true;
            } else {
                static const char *CATCH[] = {"binascii.Error", "ValueError"};
                if (pis2_exc_matches(ctx, CATCH, 2)) pis2_exc_clear(ctx);
            }
        }
    }
    json_free(runs);
    return json_bool(found);
}

/* PoP: pis2_is_base64_archive_roundtrip @ ...:is_base64_archive_roundtrip */
static json_t *pis2_is_base64_archive_roundtrip(pis2_ctx *ctx, json_t *in) {
    if (!pis2_eq(pis2_fstr(in, "pattern_id"), "base64_decode_pipe")) return json_bool(false);
    bool a = pis2_seam_bool(ctx, "b64_roundtrip_search");
    bool b = pis2_seam_bool(ctx, "tar_create_search");
    return json_bool(a && b);
}

/* PoP: pis2_is_alternation_member @ ...:_is_alternation_member */
static bool pis2_alternation_member(const char *line, size_t start, size_t end) {
    size_t len = strlen(line);
    char before = start > 0 ? line[start - 1] : 0;
    char after = end < len ? line[end] : 0;
    if (before == 0 || before == '|' || before == '(') return true;
    if (after == 0 || after == '|' || after == ')') return true;
    return false;
}
static json_t *pis2_is_alternation_member(pis2_ctx *ctx, json_t *in) {
    const char *line = pis2_jstr(pis2_get(in, "line"));
    double sd = pis2_num(pis2_get(in, "start"));
    size_t s = (size_t)sd;
    size_t e = (size_t)pis2_num(pis2_get(in, "end"));
    /* LIVE: before = line[start-1] if start > 0 else "" — a positive index
     * past the end raises IndexError: string index out of range. */
    if (sd > 0 && s - 1 >= strlen(line)) {
        pis2_raise(ctx, "IndexError", "string index out of range");
        return NULL;
    }
    return json_bool(pis2_alternation_member(line, s, e));
}

/* PoP: pis2_is_whole_literal @ ...:_is_whole_literal */
static bool pis2_whole_literal(pis2_ctx *ctx, const char *line, size_t start, size_t end,
                              size_t a, size_t b) {
    if (start != a + 1 || end != b - 1) return false;
    if (a >= strlen(line) || !(line[a] == '"' || line[a] == '\'' || line[a] == '`')) return false;
    return !pis2_seam_bool(ctx, "exec_on_line");
}
static json_t *pis2_is_whole_literal(pis2_ctx *ctx, json_t *in) {
    const char *line = pis2_jstr(pis2_get(in, "line"));
    size_t s = (size_t)pis2_num(pis2_get(in, "start"));
    size_t e = (size_t)pis2_num(pis2_get(in, "end"));
    json_t *sp = pis2_get(in, "span");
    size_t a = pis2_is_arr(sp) && sp->c.count >= 2 ? (size_t)pis2_num(sp->c.items[0]) : 0;
    size_t b = pis2_is_arr(sp) && sp->c.count >= 2 ? (size_t)pis2_num(sp->c.items[1]) : 0;
    return json_bool(pis2_whole_literal(ctx, line, s, e, a, b));
}

/* PoP: pis2_is_regex_alternation_token @ ...:is_regex_alternation_token */
static json_t *pis2_is_regex_alternation_token(pis2_ctx *ctx, json_t *in) {
    const char *pid = pis2_fstr(in, "pattern_id");
    if (!pis2_in(pid, (const char *[]){"sudo_usage", "dump_all_env", "format_filesystem", NULL}))
        return json_bool(false);
    const char *line = pis2_jstr(pis2_get(in, "line"));
    json_t *hits = pis2_seam(ctx, "pattern_token_hits");
    if (!pis2_is_arr(hits) || hits->c.count == 0) { json_free(hits); return json_bool(false); }
    json_t *spans = pis2_seam(ctx, "literal_spans");
    bool allinert = true;
    for (size_t i = 0; i < hits->c.count && allinert; i++) {
        const char *t = pis2_jstr(pis2_get(hits->c.items[i], "t"));
        size_t s = (size_t)pis2_num(pis2_get(hits->c.items[i], "s"));
        size_t e = (size_t)pis2_num(pis2_get(hits->c.items[i], "e"));
        if (pis2_has(t, " ")) { allinert = false; break; }
        size_t a = 0, b = 0; bool have = false;
        if (pis2_is_arr(spans)) {
            for (size_t j = 0; j < spans->c.count; j++) {
                size_t x = (size_t)pis2_num(pis2_get(spans->c.items[j], "s"));
                size_t y = (size_t)pis2_num(pis2_get(spans->c.items[j], "e"));
                if (x <= s && e <= y) { a = x; b = y; have = true; break; }
            }
        }
        if (!have) { allinert = false; break; }
        if (!pis2_alternation_member(line, s, e)
            && !pis2_whole_literal(ctx, line, s, e, a, b)) allinert = false;
    }
    json_free(hits); json_free(spans);
    return json_bool(allinert);
}

/* ── sudo / env / traversals / lazy import ───────────────────────────────── */
/* PoP: pis2_sudo_in_safety_clause @ ...:_sudo_in_safety_clause */
static bool pis2_sudo_in_safety_clause(pis2_ctx *ctx, const char *line, size_t start) {
    /* clause = re.split(r"[.;]", line[:start])[-1] */
    size_t clause = 0;
    for (size_t i = 0; i < start && i < strlen(line); i++)
        if (line[i] == '.' || line[i] == ';') clause = i + 1;
    (void)clause;
    return pis2_seam_bool(ctx, "sudo_negation");
}
static json_t *pis2_sudo_in_safety_clause_fn(pis2_ctx *ctx, json_t *in) {
    return json_bool(pis2_sudo_in_safety_clause(ctx, pis2_jstr(pis2_get(in, "line")),
                      (size_t)pis2_num(pis2_get(in, "start"))));
}

/* PoP: pis2_is_sudo_inert @ ...:is_sudo_inert */
static json_t *pis2_is_sudo_inert(pis2_ctx *ctx, json_t *in) {
    if (!pis2_eq(pis2_fstr(in, "pattern_id"), "sudo_usage")) return json_bool(false);
    const char *line = pis2_jstr(pis2_get(in, "line"));
    const char *rp = pis2_jstr(pis2_get(in, "rel_path"));
    json_t *hits = pis2_seam(ctx, "sudo_tokens");
    if (!pis2_is_arr(hits) || hits->c.count == 0) { json_free(hits); return json_bool(false); }
    bool executes = pis2_executes(ctx, rp);
    json_t *spans = pis2_seam(ctx, "literal_spans");
    json_t *admin = pis2_seam(ctx, "sudo_admin_hints");
    bool in_regex = pis2_seam_bool(ctx, "sudo_in_regex");
    bool allinert = true;
    for (size_t i = 0; i < hits->c.count && allinert; i++) {
        long hs = (long)pis2_num(hits->c.items[i]);
        bool in_lit = false;
        if (pis2_is_arr(spans)) {
            for (size_t j = 0; j < spans->c.count; j++) {
                long a = (long)pis2_num(pis2_get(spans->c.items[j], "s"));
                long b = (long)pis2_num(pis2_get(spans->c.items[j], "e"));
                if (a <= hs && hs <= b) { in_lit = true; break; }
            }
        }
        bool adm = false;
        if (pis2_is_arr(admin))
            for (size_t j = 0; j < admin->c.count; j++)
                if ((long)pis2_num(admin->c.items[j]) == hs) { adm = true; break; }
        if (in_lit && !executes && (adm || in_regex)) continue;
        if (!pis2_sudo_in_safety_clause(ctx, line, (size_t)hs)) allinert = false;
    }
    json_free(hits); json_free(spans); json_free(admin);
    return json_bool(allinert);
}

/* PoP: pis2_is_env_dump_in_safety_clause @ ...:is_env_dump_in_safety_clause */
static json_t *pis2_is_env_dump_in_safety_clause(pis2_ctx *ctx, json_t *in) {
    if (!pis2_eq(pis2_fstr(in, "pattern_id"), "dump_all_env")) return json_bool(false);
    const char *line = pis2_jstr(pis2_get(in, "line"));
    json_t *hits = pis2_seam(ctx, "env_dump_tokens");
    bool all = false;
    if (pis2_is_arr(hits) && hits->c.count > 0) {
        all = true;
        for (size_t i = 0; i < hits->c.count && all; i++)
            if (!pis2_sudo_in_safety_clause(ctx, line, (size_t)pis2_num(hits->c.items[i]))) all = false;
    }
    json_free(hits);
    return json_bool(all);
}

/* PoP: pis2_is_quoted_injection_phrase @ ...:is_quoted_injection_phrase */
static json_t *pis2_is_quoted_injection_phrase(pis2_ctx *ctx, json_t *in) {
    if (!pis2_in(pis2_fstr(in, "pattern_id"), INJECTION_IDS)) return json_bool(false);
    json_t *hits = pis2_seam(ctx, "pattern_hits");
    json_t *spans = pis2_seam(ctx, "quoted_spans");
    bool all = false;
    if (pis2_is_arr(hits) && hits->c.count > 0) {
        all = true;
        for (size_t i = 0; i < hits->c.count && all; i++) {
            long s = (long)pis2_num(pis2_get(hits->c.items[i], "s"));
            long e = (long)pis2_num(pis2_get(hits->c.items[i], "e"));
            bool in = false;
            if (pis2_is_arr(spans))
                for (size_t j = 0; j < spans->c.count; j++) {
                    long a = (long)pis2_num(pis2_get(spans->c.items[j], "s"));
                    long b = (long)pis2_num(pis2_get(spans->c.items[j], "e"));
                    if (a <= s && e <= b) { in = true; break; }
                }
            if (!in) all = false;
        }
    }
    json_free(hits); json_free(spans);
    return json_bool(all);
}

/* PoP: pis2_is_repo_root_traversal @ ...:is_repo_root_traversal */
static json_t *pis2_is_repo_root_traversal(pis2_ctx *ctx, json_t *in) {
    if (!pis2_eq(pis2_fstr(in, "pattern_id"), "path_traversal_deep")) return json_bool(false);
    const char *line = pis2_jstr(pis2_get(in, "line"));
    size_t len = strlen(line);
    json_t *runs = pis2_seam(ctx, "traversal_runs");
    if (!pis2_is_arr(runs) || runs->c.count == 0) { json_free(runs); return json_bool(false); }
    bool ok = true;
    for (size_t i = 0; i < runs->c.count && ok; i++) {
        size_t end = (size_t)pis2_num(runs->c.items[i]);
        const char *rest = line + (end < len ? end : len);
        /* strip the leading run of optional-slash .. segments, then one slash */
        for (;;) {
            if (rest[0] == '.' && rest[1] == '.') { rest += 2; continue; }
            if (rest[0] == '/' && rest[1] == '.' && rest[2] == '.') { rest += 3; continue; }
            break;
        }
        if (rest[0] == '/') rest++;
        if (pis2_seam_bool(ctx, "sensitive_match")) ok = false;
    }
    json_free(runs);
    return json_bool(ok);
}

/* PoP: pis2_is_benign_lazy_os_import @ ...:is_benign_lazy_os_import */
static json_t *pis2_is_benign_lazy_os_import(pis2_ctx *ctx, json_t *in) {
    if (!pis2_eq(pis2_fstr(in, "pattern_id"), "python_import_os")) return json_bool(false);
    json_t *members = pis2_seam(ctx, "import_os_members");
    bool ok = false;
    if (pis2_is_arr(members) && members->c.count > 0) {
        bool dangerous = pis2_seam_bool(ctx, "dangerous_os");
        bool execs = pis2_seam_bool(ctx, "exec_on_line");
        ok = !dangerous && !execs;
        if (ok)
            for (size_t i = 0; i < members->c.count && ok; i++)
                if (!pis2_in(pis2_jstr(members->c.items[i]), BENIGN_OS)) ok = false;
    }
    json_free(members);
    return json_bool(ok);
}

/* PoP: pis2_is_disclosed_config_note @ ...:is_disclosed_config_note */
static json_t *pis2_is_disclosed_config_note(pis2_ctx *ctx, json_t *in) {
    (void)ctx;
    if (!pis2_in(pis2_fstr(in, "pattern_id"), DISCLOSED_IDS)) return json_bool(false);
    const char *rp = pis2_jstr(pis2_get(in, "rel_path"));
    char suf[64]; pis2_suffix(rp, suf, sizeof suf);
    char nm[256]; snprintf(nm, sizeof nm, "%s", pis2_name(rp));
    return json_bool(pis2_eq(suf, ".md") || pis2_in_ci(nm, AGENT_DOC));
}

/* PoP: pis2_is_authorized_keys_prose @ ...:is_authorized_keys_prose */
static json_t *pis2_is_authorized_keys_prose(pis2_ctx *ctx, json_t *in) {
    if (!pis2_eq(pis2_fstr(in, "pattern_id"), "ssh_backdoor")) return json_bool(false);
    const char *line = pis2_jstr(pis2_get(in, "line"));
    if (pis2_seam_bool(ctx, "authorized_keys_write")) return json_bool(false);
    char low[2048]; pis2_lower(low, sizeof low, line);
    const char *pos = strstr(low, "authorized_keys");
    if (!pos) return json_bool(false);
    long st = (long)(pos - low);
    json_t *spans = pis2_seam(ctx, "literal_spans");
    bool inlit = false;
    if (pis2_is_arr(spans))
        for (size_t j = 0; j < spans->c.count; j++) {
            long a = (long)pis2_num(pis2_get(spans->c.items[j], "s"));
            long b = (long)pis2_num(pis2_get(spans->c.items[j], "e"));
            if (a <= st && st + 15 <= b) { inlit = true; break; }
        }
    json_free(spans);
    return json_bool(inlit);
}

/* PoP: pis2_is_loopback_only @ ...:is_loopback_only */
static json_t *pis2_is_loopback_only(pis2_ctx *ctx, json_t *in) {
    if (!pis2_eq(pis2_fstr(in, "pattern_id"), "hardcoded_ip_port")) return json_bool(false);
    json_t *hits = pis2_seam(ctx, "pattern_hits");
    bool all = false;
    if (pis2_is_arr(hits) && hits->c.count > 0) {
        all = true;
        for (size_t i = 0; i < hits->c.count && all; i++) {
            long s = (long)pis2_num(pis2_get(hits->c.items[i], "s"));
            (void)s;
            if (!pis2_seam_bool(ctx, "loopback_match")) all = false;
        }
    }
    json_free(hits);
    return json_bool(all);
}

/* PoP: pis2_is_pip_install_in_prose_literal @ ...:is_pip_install_in_prose_literal */
static json_t *pis2_is_pip_install_in_prose_literal(pis2_ctx *ctx, json_t *in) {
    if (!pis2_eq(pis2_fstr(in, "pattern_id"), "unpinned_pip_install")) return json_bool(false);
    const char *line = pis2_jstr(pis2_get(in, "line"));
    if (pis2_seam_bool(ctx, "exec_on_line")) return json_bool(false);
    json_t *spans = pis2_seam(ctx, "literal_spans");
    json_t *hits = pis2_seam(ctx, "pip_install_tokens");
    bool all = false;
    if (pis2_is_arr(hits) && hits->c.count > 0) {
        all = true;
        for (size_t i = 0; i < hits->c.count && all; i++) {
            long s = (long)pis2_num(hits->c.items[i]);
            bool have = false;
            size_t a = 0, b = 0;
            if (pis2_is_arr(spans))
                for (size_t j = 0; j < spans->c.count; j++) {
                    size_t x = (size_t)pis2_num(pis2_get(spans->c.items[j], "s"));
                    size_t y = (size_t)pis2_num(pis2_get(spans->c.items[j], "e"));
                    if ((long)x <= s && s <= (long)y) { a = x; b = y; have = true; break; }
                }
            if (!have) { all = false; break; }
            size_t cs = a + 1;
            for (size_t k = a; k < b; k++)
                if (line[k] == '"' || line[k] == '\'' || line[k] == '`' || line[k] == '/') { cs = k + 1; break; }
            if (pis2_seam_bool(ctx, "pip_command_position")) all = false;
            (void)cs;
        }
    }
    json_free(spans); json_free(hits);
    return json_bool(all);
}

/* PoP: pis2_is_hex_in_char_class @ ...:is_hex_in_char_class */
static json_t *pis2_is_hex_in_char_class(pis2_ctx *ctx, json_t *in) {
    if (!pis2_eq(pis2_fstr(in, "pattern_id"), "hex_encoded_string")) return json_bool(false);
    json_t *classes = pis2_seam(ctx, "char_classes");
    json_t *hexes = pis2_seam(ctx, "hex_escapes");
    bool all = false;
    if (pis2_is_arr(hexes) && hexes->c.count > 0) {
        all = true;
        for (size_t i = 0; i < hexes->c.count && all; i++) {
            long s = (long)pis2_num(pis2_get(hexes->c.items[i], "s"));
            long e = (long)pis2_num(pis2_get(hexes->c.items[i], "e"));
            bool in = false;
            if (pis2_is_arr(classes))
                for (size_t j = 0; j < classes->c.count; j++) {
                    long a = (long)pis2_num(pis2_get(classes->c.items[j], "s"));
                    long b = (long)pis2_num(pis2_get(classes->c.items[j], "e"));
                    if (a <= s && e <= b) { in = true; break; }
                }
            if (!in) all = false;
        }
    }
    json_free(classes); json_free(hexes);
    return json_bool(all);
}

/* PoP: pis2_logical_line @ ...:logical_line */
static json_t *pis2_logical_line(pis2_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *lines = pis2_get(in, "lines");
    size_t idx = (size_t)pis2_num(pis2_get(in, "index"));
    char buf[16384]; buf[0] = '\0';
    if (!pis2_is_arr(lines) || idx >= lines->c.count) return json_string("");
    size_t outn = 0;
    size_t i = idx;
    size_t parts = 0;
    while (i < lines->c.count && parts <= 8) {
        const char *ln = pis2_jstr(lines->c.items[i]);
        size_t l = strlen(ln);
        while (l > 0 && (ln[l - 1] == ' ' || ln[l - 1] == '\t' || ln[l - 1] == '\r' || ln[l - 1] == '\n')) l--;
        bool cont = l > 0 && ln[l - 1] == '\\';
        if (cont) l--;
        if (parts && outn + 1 < sizeof buf) { buf[outn++] = ' '; }
        for (size_t k = 0; k < l && outn + 1 < sizeof buf; k++) buf[outn++] = ln[k];
        parts++;
        if (!cont || i + 1 >= lines->c.count || parts > 8) break;
        i++;
    }
    buf[outn] = '\0';
    return json_string(buf);
}

/* PoP: pis2_is_loopback_continuation @ ...:is_loopback_continuation */
static json_t *pis2_is_loopback_continuation(pis2_ctx *ctx, json_t *in) {
    if (!pis2_in(pis2_fstr(in, "pattern_id"), CONT_IDS)) return json_bool(false);
    const char *line = pis2_jstr(pis2_get(in, "line"));
    size_t len = strlen(line);
    if (len == 0 || line[len - 1] != '\\') return json_bool(false);
    json_t *rx = pis2_seam(ctx, "pattern_hits");   /* search on joined */
    if (rx && rx->type != JSON_NULL) { json_free(rx); return json_bool(false); }
    json_free(rx);
    json_t *hosts = pis2_seam(ctx, "url_hosts");
    bool all = false;
    if (pis2_is_arr(hosts) && hosts->c.count > 0) {
        all = true;
        for (size_t i = 0; i < hosts->c.count && all; i++) {
            char h[256]; pis2_lower(h, sizeof h, pis2_jstr(hosts->c.items[i]));
            if (!pis2_in(h, LOOPBACK_HOSTS)) all = false;
        }
    }
    json_free(hosts);
    return json_bool(all);
}

/* PoP: pis2_is_json_prose_value @ ...:is_json_prose_value */
static bool pis2_has_ws(const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if (isspace(*p)) return true;
    return false;
}
static json_t *pis2_is_json_prose_value(pis2_ctx *ctx, json_t *in) {
    if (!pis2_eq(pis2_fstr(in, "severity"), "high")) return json_bool(false);
    const char *rp = pis2_jstr(pis2_get(in, "rel_path"));
    char suf[64]; pis2_suffix(rp, suf, sizeof suf);
    char nm[256]; snprintf(nm, sizeof nm, "%s", pis2_name(rp));
    if (!pis2_eq(suf, ".json") || pis2_in_ci(nm, JSON_EXEC)) return json_bool(false);
    const char *line = pis2_jstr(pis2_get(in, "line"));
    json_t *m = pis2_seam(ctx, "json_string_line");
    if (!pis2_is_obj(m)) { json_free(m); return json_bool(false); }
    json_t *kv = pis2_get(m, "k");
    if (kv && pis2_is_arr(kv) && kv->c.count >= 2) {
        char key[256];
        size_t a = (size_t)pis2_num(kv->c.items[0]), b = (size_t)pis2_num(kv->c.items[1]);
        size_t n = b - a; if (n >= sizeof key) n = sizeof key - 1;
        memcpy(key, line + a, n); key[n] = '\0';
        if (!pis2_has_ws(key) && pis2_seam_bool(ctx, "json_command_key"))
            { json_free(m); return json_bool(false); }
    }
    json_t *hits = pis2_seam(ctx, "pattern_hits");
    bool all = false;
    if (pis2_is_arr(hits) && hits->c.count > 0) {
        all = true;
        for (size_t i = 0; i < hits->c.count && all; i++) {
            long s = (long)pis2_num(pis2_get(hits->c.items[i], "s"));
            long e = (long)pis2_num(pis2_get(hits->c.items[i], "e"));
            const char *part = NULL;
            json_t *vv = pis2_get(m, "v");
            if (kv && pis2_is_arr(kv) && kv->c.count >= 2 &&
                (long)pis2_num(kv->c.items[0]) <= s && e <= (long)pis2_num(kv->c.items[1]))
                part = "k";
            else if (vv && pis2_is_arr(vv) && vv->c.count >= 2 &&
                     (long)pis2_num(vv->c.items[0]) <= s && e <= (long)pis2_num(vv->c.items[1]))
                part = "v";
            if (!part) { all = false; break; }
            json_t *sel = pis2_get(m, part);
            size_t ps = (size_t)pis2_num(sel->c.items[0]);
            if (pis2_seam_bool(ctx, "command_position")) all = false;
            (void)ps;
        }
    }
    json_free(m); json_free(hits);
    return json_bool(all);
}

/* PoP: pis2_is_standalone_coin_name @ ...:_is_standalone_coin_name */
static bool pis2_standalone_coin(const char *line, size_t start, size_t end) {
    size_t n = end - start;
    if (n != 6 || strncasecmp(line + start, "monero", 6) != 0) return false;
    if (start > 0 && isalnum((unsigned char)line[start - 1])) return false;
    if (end < strlen(line) && isalnum((unsigned char)line[end])) return false;
    return true;
}
static json_t *pis2_is_standalone_coin_name(pis2_ctx *ctx, json_t *in) {
    (void)ctx;
    const char *line = pis2_jstr(pis2_get(in, "line"));
    return json_bool(pis2_standalone_coin(line,
        (size_t)pis2_num(pis2_get(in, "start")), (size_t)pis2_num(pis2_get(in, "end"))));
}

/* PoP: pis2_is_coin_name_only @ ...:is_coin_name_only */
static json_t *pis2_is_coin_name_only(pis2_ctx *ctx, json_t *in) {
    if (!pis2_eq(pis2_fstr(in, "pattern_id"), "crypto_mining")) return json_bool(false);
    const char *line = pis2_jstr(pis2_get(in, "line"));
    if (pis2_seam_bool(ctx, "mining_machinery")) return json_bool(false);
    json_t *hits = pis2_seam(ctx, "pattern_hits");
    bool all = false;
    if (pis2_is_arr(hits) && hits->c.count > 0) {
        all = true;
        for (size_t i = 0; i < hits->c.count && all; i++) {
            size_t s = (size_t)pis2_num(pis2_get(hits->c.items[i], "s"));
            size_t e = (size_t)pis2_num(pis2_get(hits->c.items[i], "e"));
            if (!pis2_standalone_coin(line, s, e)) all = false;
        }
    }
    json_free(hits);
    return json_bool(all);
}

/* PoP: pis2_is_public_client_key @ ...:is_public_client_key */
static json_t *pis2_is_public_client_key(pis2_ctx *ctx, json_t *in) {
    if (!pis2_eq(pis2_fstr(in, "pattern_id"), "hardcoded_secret")) return json_bool(false);
    json_t *hits = pis2_seam(ctx, "pattern_hits");
    bool all = false;
    if (pis2_is_arr(hits) && hits->c.count > 0) {
        all = true;
        for (size_t i = 0; i < hits->c.count && all; i++) {
            json_t *lit = pis2_seam(ctx, "secret_literals");
            const char *s = (lit && pis2_is_str(lit)) ? lit->str_val : NULL;
            bool good = false;
            for (int k = 0; PUBKEY_PREFIX[k]; k++)
                if (s && strncmp(s, PUBKEY_PREFIX[k], strlen(PUBKEY_PREFIX[k])) == 0) good = true;
            if (!good) all = false;
            json_free(lit);
        }
    }
    json_free(hits);
    return json_bool(all);
}

/* PoP: pis2_is_own_proc_read @ ...:is_own_proc_read */
static json_t *pis2_is_own_proc_read(pis2_ctx *ctx, json_t *in) {
    if (!pis2_eq(pis2_fstr(in, "pattern_id"), "proc_access")) return json_bool(false);
    json_t *leaves = pis2_seam(ctx, "proc_leaves");
    bool all = false;
    if (pis2_is_arr(leaves) && leaves->c.count > 0) {
        all = true;
        for (size_t i = 0; i < leaves->c.count && all; i++) {
            json_t *v = leaves->c.items[i];
            if (v->type == JSON_NULL || !pis2_in(pis2_jstr(v), BENIGN_PROC)) all = false;
        }
        if (all) all = !pis2_seam_bool(ctx, "proc_write");
    }
    json_free(leaves);
    return json_bool(all);
}
