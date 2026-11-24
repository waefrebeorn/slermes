/*
 * patterns.c -- real C11 port of upstream Hermes
 *   plugins/security-guidance/patterns.py: the regex-based security pattern table.
 *
 * The Python module is pure data: three ``_JS_EXTS``/``_PY_EXTS``/``_DOC_EXTS``
 * membership helpers, the ``_rule`` dict builder and ``SECURITY_PATTERNS``.  All three
 * are ported here.  The rule table below is generated from the live AST, so the
 * reminder text is byte-identical (UTF-8, octal-escaped to keep this source ASCII).
 *
 * PoP: w19_patterns_a4e066_rule @ plugins/security-guidance/patterns.py:_rule
 * PoP: w19_patterns_a4e066_js_only @ plugins/security-guidance/patterns.py:_JS_ONLY
 * PoP: w19_patterns_a4e066_py_only @ plugins/security-guidance/patterns.py:_PY_ONLY
 * PoP: w19_patterns_a4e066_not_docs @ plugins/security-guidance/patterns.py:_NOT_DOCS
 * PoP: w19_patterns_a4e066_pattern @ plugins/security-guidance/patterns.py:SECURITY_PATTERNS
 * PoP: w19_patterns_a4e066_names @ plugins/security-guidance/patterns.py:SECURITY_PATTERNS
 * PoP: w19_patterns_a4e066_count @ plugins/security-guidance/patterns.py:SECURITY_PATTERNS
 */
#include "hermes_json.h"
#include "noc_w19_patterns_a4e066.h"
#include <stddef.h>
#include <string.h>

/* path_filter / path_check classification: the Python table holds lambdas, which have
 * no wire form, so each predicate is identified by its behaviour. */
enum { PF_NONE = 0, PF_JS_ONLY = 1, PF_PY_ONLY = 2, PF_NOT_DOCS = 3 };
enum { SEC_PC_NONE = 0, SEC_PC_GITHUB_ACTIONS = 1 };

struct rule_t {
    const char *name;
    const char *reminder;
    const char *regex;             /* NULL when the rule has no regex trigger */
    const char *const *subs;       /* NULL when the rule has no substring triggers */
    int pf;
    int pc;
    const char *keys[7];           /* trigger keys present, in the Python kwargs order */
};

/* generated from the live patterns.py AST: exact reminder bytes (UTF-8, octal-escaped) */
static const char REM_UNSAFE_DESERIALIZATION_REMINDER[] =
    "\342\232\240\357\270\217 Security Warning: Loading pickle data (or equivalents: cPickle, cloudpickle, dill, marshal, shelve, joblib, pandas.read_pickle, numpy with allow_pickle=True) from untrusted sources allows arbitrary code execution.\n\nFor simple data, prefer JSON or msgspec. For typed objects, prefer a schema-validated deserializer (msgspec.Struct, pydantic, marshmallow) that constructs only declared types.\n\nIf this is safe or is explicitly needed, briefly document that in a comment before continuing.";
static const char REM_UNSAFE_YAML_LOAD_REMINDER[] =
    "\342\232\240\357\270\217 Security Warning: yaml.load() / yaml.unsafe_load() execute arbitrary Python via !!python/object tags.\n\nUse yaml.safe_load() if the file only contains simple data structures (dicts, lists, strings, numbers). If you need typed objects, parse with safe_load and validate the result against a schema (pydantic, msgspec, marshmallow) \342\200\224 never use a custom Loader that constructs arbitrary types.";
static const char REM_UNSAFE_TORCH_LOAD_REMINDER[] =
    "\342\232\240\357\270\217 Security Warning: torch.load() defaults to weights_only=False, which unpickles arbitrary Python objects and allows arbitrary code execution.\n\nIf the file only contains tensors and simple data structures, pass weights_only=True (or set TORCH_FORCE_WEIGHTS_ONLY_LOAD=1).";
static const char REM_GITHUB_ACTIONS_REMINDER[] =
    "\342\232\240\357\270\217 Security Warning: You are editing a GitHub Actions workflow file. Be aware of these security risks:\n\n1. **Command Injection**: Never use untrusted input (like issue titles, PR descriptions, commit messages) directly in run: commands without proper escaping\n2. **Use environment variables**: Instead of ${{ github.event.issue.title }}, use env: with proper quoting\n3. **Review the guide**: https://github.blog/security/vulnerability-research/how-to-catch-github-actions-workflow-injections-before-attackers-do/\n\nExample of UNSAFE pattern to avoid:\nrun: echo \"${{ github.event.issue.title }}\"\n\nExample of SAFE pattern:\nenv:\n  TITLE: ${{ github.event.issue.title }}\nrun: echo \"$TITLE\"\n\nOther risky inputs to be careful with:\n- github.event.issue.body\n- github.event.pull_request.title\n- github.event.pull_request.body\n- github.event.comment.body\n- github.event.review.body\n- github.event.review_comment.body\n- github.event.pages.*.page_name\n- github.ev" "ent.commits.*.message\n- github.event.head_commit.message\n- github.event.head_commit.author.email\n- github.event.head_commit.author.name\n- github.event.commits.*.author.email\n- github.event.commits.*.author.name\n- github.event.pull_request.head.ref\n- github.event.pull_request.head.label\n- github.event.pull_request.head.repo.default_branch\n- github.event.client_payload.* (repository_dispatch events \342\200\224 attacker can set any field)\n\n4. **Ref injection**: Never use untrusted input in `ref:` parameters of `actions/checkout`. For `client_payload.pr_number`, validate it matches `^[0-9]+$` before using in `ref: refs/pull/${{ ... }}/head`\n- github.head_ref";
static const char REM_CHILD_PROCESS_EXEC_REMINDER[] =
    "\342\232\240\357\270\217 Security Warning: Using child_process.exec() can lead to command injection vulnerabilities.\n\nexec() runs the command string through a shell, so any user input interpolated into it can inject arbitrary commands. Prefer child_process.execFile() (or spawn()) with an argument array instead of building a shell string.\n\nInstead of:\n  exec(`command ${userInput}`)\n\nUse:\n  import { execFile } from 'node:child_process'\n  execFile('command', [userInput], callback)\n\nWhy execFile/spawn with an argument array is safer:\n- No shell is involved, so shell metacharacters in arguments are not interpreted\n- Arguments are passed directly to the program rather than interpolated into a command string\n\nOnly use exec() if you absolutely need shell features and the input is guaranteed to be safe.";
static const char REM_SUBPROCESS_SHELL_REMINDER[] =
    "\342\232\240\357\270\217 Security Warning: Using subprocess with shell=True enables command injection.\n\nUNSAFE:\n  subprocess.run(f\"ls {user_input}\", shell=True)\n  subprocess.call(\"grep \" + pattern, shell=True)\n\nSAFE - pass arguments as a list without shell:\n  subprocess.run([\"ls\", user_input])\n  subprocess.call([\"grep\", pattern])\n\nWhen arguments are passed as a list without shell=True, special characters cannot be interpreted as shell metacharacters.";
static const char REM_GO_EXEC_SHELL_REMINDER[] =
    "\342\232\240\357\270\217 Security Warning: Using exec.Command with a shell interpreter (sh/bash) enables command injection.\n\nUNSAFE:\n  exec.Command(\"sh\", \"-c\", \"ping -c 1 \" + host)\n  exec.Command(\"bash\", \"-c\", fmt.Sprintf(\"df -h %s\", path))\n\nSAFE - pass arguments directly without a shell:\n  exec.Command(\"ping\", \"-c\", \"1\", host)\n  exec.Command(\"df\", \"-h\", path)\n\nWhen arguments are passed directly (not through a shell), special characters in user input cannot be interpreted as shell metacharacters. This prevents command injection entirely.\n\nAdditionally, validate user inputs:\n- For hostnames/IPs: use net.ParseIP() or a hostname regex\n- For file paths: use filepath.Clean() and verify the result is within an allowed directory\n- For numeric values: parse to int/float first";
static const char REM_INLINE_new_function_injection[] =
    "\342\232\240\357\270\217 Security Warning: Using new Function() with string interpolation is a CODE INJECTION vulnerability. If any variable is concatenated or interpolated into the function body string, an attacker controlling that variable can execute arbitrary code. Use safe alternatives: for property access use obj[key] or array.reduce((o, k) => o[k], root); for computation use a safe expression parser. NEVER interpolate untrusted strings into new Function() bodies.";
static const char REM_INLINE_eval_injection[] =
    "\342\232\240\357\270\217 Security Warning: eval() executes arbitrary code and is a major security risk. Use JSON.parse() for data, ast.literal_eval() for Python literals, or a safe expression parser. If this is safe or is explicitly needed, briefly document that in a comment before continuing.";
static const char REM_INLINE_react_dangerously_set_html[] =
    "\342\232\240\357\270\217 Security Warning: dangerouslySetInnerHTML can lead to XSS vulnerabilities if used with untrusted content. Ensure all content is properly sanitized using an HTML sanitizer library like DOMPurify, or use safe alternatives.";
static const char REM_INLINE_document_write_xss[] =
    "\342\232\240\357\270\217 Security Warning: document.write() can be exploited for XSS attacks and has performance issues. Use DOM manipulation methods like createElement() and appendChild() instead.";
static const char REM_INLINE_innerHTML_xss[] =
    "\342\232\240\357\270\217 Security Warning: Setting innerHTML with untrusted content can lead to XSS vulnerabilities. Use textContent for plain text or safe DOM methods for HTML content. If you need HTML support, consider using an HTML sanitizer library such as DOMPurify.";
static const char REM_INLINE_os_system_injection[] =
    "\342\232\240\357\270\217 Security Warning: os.system() runs a shell and is a command-injection sink. Use subprocess.run([...]) with a list of arguments instead. If this is safe or is explicitly needed, briefly document that in a comment before continuing.";
static const char REM_INLINE_node_createcipher_no_iv[] =
    "\342\232\240\357\270\217 Security Warning: Use crypto.createCipheriv() / createDecipheriv(). createCipher was removed in Node 22 and derives the key insecurely (no IV, MD5-based KDF).";
static const char REM_INLINE_aes_ecb_mode[] =
    "\342\232\240\357\270\217 Security Warning: Use AES-GCM or AES-CBC with HMAC. ECB mode leaks plaintext structure (identical blocks encrypt to identical ciphertext).";
static const char REM_INLINE_tls_verification_disabled[] =
    "\342\232\240\357\270\217 Security Warning: Don't disable TLS verification. This allows MITM attacks. For self-signed dev certs, add the CA to your trust store or use a properly-issued cert.";
static const char REM_INLINE_xml_unsafe_parse[] =
    "\342\232\240\357\270\217 Security Warning: Use defusedxml.ElementTree. Python's stdlib XML parsers are vulnerable to XXE (external entity) and billion-laughs attacks by default.";
static const char REM_INLINE_outerHTML_xss[] =
    "\342\232\240\357\270\217 Security Warning: Use textContent or sanitize with DOMPurify. outerHTML assignment is an XSS sink equivalent to innerHTML.";
static const char REM_INLINE_insertAdjacentHTML_xss[] =
    "\342\232\240\357\270\217 Security Warning: Use insertAdjacentText() or sanitize with DOMPurify. insertAdjacentHTML is an XSS sink.";
static const char REM_INLINE_script_src_without_sri[] =
    "\342\232\240\357\270\217 Security Warning: Add integrity=\"sha384-...\" crossorigin=\"anonymous\" to external script tags. Loading scripts without Subresource Integrity exposes you to CDN compromise.";
static const char *const SUBS_child_process_exec[] = {"child_process.exec", "execSync(",  NULL};
static const char *const SUBS_new_function_injection[] = {"new Function",  NULL};
static const char *const SUBS_react_dangerously_set_html[] = {"dangerouslySetInnerHTML",  NULL};
static const char *const SUBS_document_write_xss[] = {"document.write",  NULL};
static const char *const SUBS_innerHTML_xss[] = {".innerHTML =", ".innerHTML=",  NULL};
static const char *const SUBS_os_system_injection[] = {"from os import system",  NULL};
static const char *const SUBS_outerHTML_xss[] = {".outerHTML =", ".outerHTML=",  NULL};
static const char *const SUBS_insertAdjacentHTML_xss[] = {".insertAdjacentHTML(",  NULL};

static const struct rule_t RULES[] = {
    {"github_actions_workflow", REM_GITHUB_ACTIONS_REMINDER, NULL, NULL, 0, 1, {"path_check", NULL}},
    {"child_process_exec", REM_CHILD_PROCESS_EXEC_REMINDER, "(?<![a-zA-Z0-9_\\.])exec\\(", SUBS_child_process_exec, 1, 0, {"path_filter", "substrings", "regex", NULL}},
    {"new_function_injection", REM_INLINE_new_function_injection, NULL, SUBS_new_function_injection, 0, 0, {"substrings", NULL}},
    {"eval_injection", REM_INLINE_eval_injection, "(?<![a-zA-Z0-9_\\.])eval\\(", NULL, 3, 0, {"path_filter", "regex", NULL}},
    {"react_dangerously_set_html", REM_INLINE_react_dangerously_set_html, NULL, SUBS_react_dangerously_set_html, 0, 0, {"substrings", NULL}},
    {"document_write_xss", REM_INLINE_document_write_xss, NULL, SUBS_document_write_xss, 0, 0, {"substrings", NULL}},
    {"innerHTML_xss", REM_INLINE_innerHTML_xss, NULL, SUBS_innerHTML_xss, 0, 0, {"substrings", NULL}},
    {"pickle_deserialization", REM_UNSAFE_DESERIALIZATION_REMINDER, "(?<![a-zA-Z0-9_])pickle\\.(loads?|Unpickler)\\b|(?<![a-zA-Z0-9_])pkl_load\\(", NULL, 2, 0, {"path_filter", "regex", NULL}},
    {"os_system_injection", REM_INLINE_os_system_injection, "\\bos\\.system\\s*\\(", SUBS_os_system_injection, 2, 0, {"path_filter", "regex", "substrings", NULL}},
    {"python_subprocess_shell", REM_SUBPROCESS_SHELL_REMINDER, "subprocess\\.(?:run|call|Popen|check_output|check_call)\\(.*shell\\s*=\\s*True", NULL, 0, 0, {"regex", NULL}},
    {"go_exec_shell_injection", REM_GO_EXEC_SHELL_REMINDER, "exec\\.Command\\(\\s*\"(?:sh|bash|/bin/sh|/bin/bash)\"", NULL, 0, 0, {"regex", NULL}},
    {"unsafe_yaml_load", REM_UNSAFE_YAML_LOAD_REMINDER, "\\byaml\\.load\\s*\\((?![^)\\n]{0,80}\\bSafe)", NULL, 0, 0, {"regex", NULL}},
    {"node_createcipher_no_iv", REM_INLINE_node_createcipher_no_iv, "\\bcrypto\\.(createCipher|createDecipher)\\b", NULL, 0, 0, {"regex", NULL}},
    {"aes_ecb_mode", REM_INLINE_aes_ecb_mode, "\\bAES\\.MODE_ECB\\b|\\bmodes\\.ECB\\s*\\(|[\\x22\\x27]aes-\\d+-ecb[\\x22\\x27]", NULL, 0, 0, {"regex", NULL}},
    {"tls_verification_disabled", REM_INLINE_tls_verification_disabled, "\\bverify\\s*=\\s*False\\b|rejectUnauthorized\\s*:\\s*false|InsecureSkipVerify\\s*:\\s*true|NODE_TLS_REJECT_UNAUTHORIZED\\s*=\\s*[\\x22\\x27]?0|ssl\\._create_unverified_context|check_hostname\\s*=\\s*False", NULL, 0, 0, {"regex", NULL}},
    {"marshal_loads", REM_UNSAFE_DESERIALIZATION_REMINDER, "\\bmarshal\\.loads?\\s*\\(", NULL, 0, 0, {"regex", NULL}},
    {"shelve_open", REM_UNSAFE_DESERIALIZATION_REMINDER, "\\bshelve\\.open\\s*\\(", NULL, 0, 0, {"regex", NULL}},
    {"xml_unsafe_parse", REM_INLINE_xml_unsafe_parse, "\\b(xml\\.etree\\.ElementTree|ElementTree|ET)\\.(parse|fromstring|XML)\\s*\\(|\\bminidom\\.(parse|parseString)\\s*\\(|\\bxml\\.sax\\.(parse|make_parser)\\b", NULL, 0, 0, {"regex", NULL}},
    {"pickle_variants_load", REM_UNSAFE_DESERIALIZATION_REMINDER, "\\b(cPickle|cloudpickle|dill)\\.(load|loads)\\s*\\(", NULL, 0, 0, {"regex", NULL}},
    {"outerHTML_xss", REM_INLINE_outerHTML_xss, NULL, SUBS_outerHTML_xss, 0, 0, {"substrings", NULL}},
    {"insertAdjacentHTML_xss", REM_INLINE_insertAdjacentHTML_xss, NULL, SUBS_insertAdjacentHTML_xss, 0, 0, {"substrings", NULL}},
    {"script_src_without_sri", REM_INLINE_script_src_without_sri, "<script\\s+(?![^>]{0,400}integrity\\s*=)[^>]{0,200}src\\s*=\\s*[\\x22\\x27](?:https?:)?//[^\\x22\\x27]{1,300}[\\x22\\x27][^>]{0,100}>", NULL, 0, 0, {"regex", NULL}},
    {"torch_unsafe_load", REM_UNSAFE_TORCH_LOAD_REMINDER, "(?:\\btorch\\.load|\\.torch_load)\\s*\\((?![^)\\n]{0,200}weights_only\\s*=\\s*True)", NULL, 0, 0, {"regex", NULL}},
    {"yaml_unsafe_load_variants", REM_UNSAFE_YAML_LOAD_REMINDER, "(?:\\byaml\\.unsafe_load|\\.yaml_unsafe_load)\\s*\\(", NULL, 0, 0, {"regex", NULL}},
    {"pickle_wrapper_load", REM_UNSAFE_DESERIALIZATION_REMINDER, "\\bjoblib\\.load\\s*\\(|\\b(?:pd|pandas)\\.read_pickle\\s*\\(|\\.cloudpickle_load\\s*\\(|\\b(?:np|numpy)\\.load\\s*\\([^)\\n]{0,200}allow_pickle\\s*=\\s*True", NULL, 0, 0, {"regex", NULL}},
};

#define RULE_COUNT (sizeof(RULES) / sizeof(RULES[0]))

static const char *const JS_EXTS[] = {
    ".js", ".jsx", ".ts", ".tsx", ".mjs", ".cjs", ".mts", ".cts", ".vue", ".svelte", NULL
};
static const char *const PY_EXTS[] = { ".py", ".pyi", ".ipynb", NULL };
static const char *const DOC_EXTS[] = { ".md", ".mdx", ".txt", ".rst", ".json", ".yaml", ".yml", NULL };

/* p.endswith(EXTS) */
static int ends_with_any(const char *p, const char *const *exts) {
    if (!p) return 0;
    size_t n = strlen(p);
    for (size_t i = 0; exts[i]; i++) {
        size_t m = strlen(exts[i]);
        if (n >= m && !memcmp(p + n - m, exts[i], m)) return 1;
    }
    return 0;
}

int w19_patterns_a4e066_js_only(const char *path) { return ends_with_any(path, JS_EXTS); }
int w19_patterns_a4e066_py_only(const char *path) { return ends_with_any(path, PY_EXTS); }
int w19_patterns_a4e066_not_docs(const char *path) { return path ? !ends_with_any(path, DOC_EXTS) : 1; }

/* ".github/workflows/" in path and (path.endswith(".yml") or path.endswith(".yaml")) */
static int github_actions_check(const char *path) {
    if (!path) return 0;
    size_t n = strlen(path);
    int suffix_ok = (n >= 4 && !strcmp(path + n - 4, ".yml")) ||
                    (n >= 5 && !strcmp(path + n - 5, ".yaml"));
    return strstr(path, ".github/workflows/") != NULL && suffix_ok;
}

static int rule_matches_filter(const struct rule_t *r, const char *path) {
    switch (r->pf) {
        case PF_JS_ONLY:  return w19_patterns_a4e066_js_only(path);
        case PF_PY_ONLY:  return w19_patterns_a4e066_py_only(path);
        case PF_NOT_DOCS: return w19_patterns_a4e066_not_docs(path);
        default:          return 1;   /* no path_filter: the rule applies everywhere */
    }
}

static int rule_matches_check(const struct rule_t *r, const char *path) {
    switch (r->pc) {
        case SEC_PC_GITHUB_ACTIONS: return github_actions_check(path);
        default:                return 1;   /* no path_check: the rule applies everywhere */
    }
}

static const struct rule_t *find_rule(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < RULE_COUNT; i++)
        if (!strcmp(RULES[i].name, name)) return &RULES[i];
    return NULL;
}

int w19_patterns_a4e066_count(void) { return (int)RULE_COUNT; }

json_t *w19_patterns_a4e066_names(void) {
    json_t *arr = json_array();
    for (size_t i = 0; i < RULE_COUNT; i++) json_append(arr, json_string(RULES[i].name));
    return arr;
}

/* _rule(name, reminder, **triggers) -> {"ruleName": name, "reminder": reminder, **triggers} */
json_t *w19_patterns_a4e066_rule(const char *name, const char *reminder, const json_t *triggers) {
    json_t *out = json_object();
    json_set(out, "ruleName", json_string(name ? name : ""));
    json_set(out, "reminder", reminder ? json_string(reminder) : json_null());
    if (triggers && triggers->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(triggers); i++) {
            const char *k = json_object_get_key_at(triggers, i);
            if (!k) continue;
            json_set(out, k, json_copy(json_object_get_at(triggers, i)));
        }
    }
    return out;
}

/* One SECURITY_PATTERNS entry as wire data.  A callable predicate is rendered as the
 * vector of its answers over `probe_paths`, which is what makes it comparable. */
json_t *w19_patterns_a4e066_pattern(const char *name, const json_t *probe_paths) {
    const struct rule_t *r = find_rule(name);
    if (!r) return json_null();
    json_t *out = json_object();
    json_set(out, "ruleName", json_string(r->name));
    json_set(out, "reminder", json_string(r->reminder));
    for (int i = 0; r->keys[i]; i++) {
        const char *k = r->keys[i];
        if (!strcmp(k, "path_filter") || !strcmp(k, "path_check")) {
            json_t *vec = json_array();
            size_t n = (probe_paths && probe_paths->type == JSON_ARRAY) ? json_len(probe_paths) : 0;
            for (size_t j = 0; j < n; j++) {
                json_t *e = json_get(probe_paths, j);
                const char *p = (e && e->type == JSON_STRING) ? e->str_val : NULL;
                int hit = !strcmp(k, "path_filter") ? rule_matches_filter(r, p) : rule_matches_check(r, p);
                json_append(vec, json_bool(hit ? true : false));
            }
            json_set(out, k, vec);
        } else if (!strcmp(k, "substrings")) {
            json_t *arr = json_array();
            for (size_t j = 0; r->subs && r->subs[j]; j++) json_append(arr, json_string(r->subs[j]));
            json_set(out, k, arr);
        } else if (!strcmp(k, "regex")) {
            json_set(out, k, json_string(r->regex ? r->regex : ""));
        }
    }
    return out;
}
