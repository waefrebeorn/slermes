/*
 * skills_guard_flow.c — FLOW port of the UNCLAIMED remainder of
 * tools/skills_guard.py (the skills guard scan / trust / install-policy surface).
 *
 * Claim audit (grep PoP first): the GENUINE behaviour ports are SEAMED here as
 * universal-seam calls — _content_digest (src/hermes_cli/skills_guard.c rolling
 * SHA-256) and _determine_verdict (faithful), plus the three pattern builders
 * _shell_write_re / _prose_modify_re / _content_contract_re (src/tools/
 * skills_guard_gaps.c, byte-exact; they run only at module load to build the
 * THREAT_PATTERNS table, which is transcribed below as metadata). The remaining
 * PoP tags in src/hermes_cli/skills_guard.c and src/tools/skills_guard.c are
 * zero-behaviour STUBS (scan_skill returns findings:0, format_scan_report echoes
 * "Scan Report: %s", content_hash is a byte-sum, _check_structure only tests
 * SKILL.md existence, _unicode_char_name mis-formats, scan_file is a tab-split
 * wrapper, _resolve_trust_level knows 4/10 repos). Per the port doctrine a stub
 * moves no behaviour, so this flow port supersedes them with faithful ports.
 *
 * Ports the DECISION SEQUENCES of (20 defs):
 *   line algebra    — _line_at, _statement_owners, _demote_inert_path_reference
 *   unicode         — _unicode_char_name
 *   markdown masking— _mask_markdown_link_destinations, _mask_prose_link_destinations
 *   padded code     — _padded_code_findings, _unscanned_text_padded_code
 *   docstring views — _compute_docstring_lines, _decoded_views
 *   pattern scan    — _scan_text, scan_file
 *   policy / report — _community_note, _resolve_trust_level, should_allow_install,
 *                     format_scan_report, _build_summary
 *   orchestration   — _check_structure, scan_skill, content_hash
 *
 * Modeling contract (shared with tests/oracle_skills_guard_flow.py):
 *  - Universal seam: json_t *(*call)(void *ud, const char *name, args). name
 *    identifies the live call; args is a BORROWED object the stub copies into the
 *    trace; the result is a NEW json_t (NULL == None; {"__raise__":kind,
 *    "message":m} == a raised exception). The port OWNS args (sg_call frees them)
 *    and the result. A missing/exhausted script is FAIL-CLOSED: the harness stub
 *    records and returns a {"__raise__":"RuntimeError",...} spec.
 *  - Exception propagation: a function that meets a raise spec RETURNS it;
 *    callers propagate it (BaseException trio SystemExit / KeyboardInterrupt /
 *    GeneratorExit passes through every except-Exception / suppress(Exception))
 *    or swallow it by family (sg_exc_is over the FULL builtin hierarchy).
 *  - Every regex / tokenizer / codec / bytes.decode / foreign-helper call rides a
 *    seam (libjson-only; no libregex). re.search is keyed by table idx + pid +
 *    line and only its TRUTHINESS is used. The internal regex primitives
 *    (_QUOTED_SPAN_RE.sub, _FENCE_LINE.match, _CONTAINER_PREFIX.sub,
 *    _DENYLIST_OWNER_RE.match, _ACTION_ON_LINE_RE.search, _PADDED_CODE.search,
 *    _CODE_WORD.search) are seams; _TRIPLE_QUOTE_OPEN_RE.match is transcribed
 *    inline (sg_triple_quote_open) as pure prefix algebra.
 *  - Seam names -> live calls (args key sets EXACT, nulls kept):
 *      re.search {idx,pid,line} -> bool
 *      quoted_span.sub / container.sub {line} -> str
 *      fence.match {line} -> {marker,info}|null
 *      denylist.match / action.search / codeword.search {line} -> bool
 *      padded.search {line} -> {start,end}|null
 *      tokenize.generate_tokens {text} -> [{type,string,start,end}] (may raise)
 *      tokenize.detect_encoding {raw} -> [codec,lines] (may raise)
 *      codecs.lookup {codec} -> {name} (may raise LookupError)
 *      decode {raw,encoding,errors} -> str (may raise UnicodeDecodeError carrying
 *          object/start/end/reason/encoding)
 *      code_views {lines} -> [view,in_text]|null
 *      raise_provider_key_reads {findings,lines,in_text} -> updated findings list
 *      mask_negated_forget {text} -> str
 *      content_digest {path} -> hex str            (claimed genuine sibling)
 *      determine_verdict {findings} -> verdict str (claimed genuine sibling)
 *      datetime.now {} -> iso str
 *      path.is_dir / is_file / is_symlink {path} -> bool
 *      path.rglob {path,pattern} -> [path]
 *      path.resolve {path} -> path str (may raise OSError)
 *      path.stat {path} -> {st_size,st_mode} (may raise OSError)
 *      path.read_bytes {path} -> [byte] (may raise OSError)
 *  - Paths travel as POSIX strings; pure Path algebra (name / suffix / parent /
 *    relative_to / is_relative_to) is transcribed inline. DOMAIN: fixture inputs
 *    are ASCII; a line carries AT MOST ONE invisible unicode char (the live set
 *    iteration order is hash-randomized, so multi-char lines are out of domain).
 *    Integral floats read as ints. Raw bytes travel as JSON arrays of 0..255.
 *    Embedded-NUL decoded text is OUT of the fixture domain: libjson strings are
 *    NUL-terminated (no length field), so the harness cannot serialize a
 *    NUL-bearing view; the nul_byte branch is implemented (raw-byte based) but
 *    fixtures stay NUL-free.
 *  - Self-contained: the harness #includes this .c; every symbol is static and
 *    sg_*-prefixed; only libjson is linked. A per-case arena owns all char*
 *    temporaries (freed by the harness); json_t nodes are json_free'd by owner.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "libjson/json.h"

/* ── arena (per-case char* temporary owner) ───────────────────────────────── */
typedef struct sg_block { struct sg_block *next; } sg_block;
typedef struct { sg_block *head; } sg_arena;
static void *sg_arena_alloc(sg_arena *a, size_t n) {
    sg_block *b = (sg_block *)malloc(sizeof(sg_block) + n);
    if (!b) return NULL;
    b->next = a->head; a->head = b;
    return (void *)(b + 1);
}
static void sg_arena_free(sg_arena *a) {
    sg_block *b = a->head;
    while (b) { sg_block *n = b->next; free(b); b = n; }
    a->head = NULL;
}
static char *sg_strdup(sg_arena *a, const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *r = (char *)sg_arena_alloc(a, n);
    if (r) memcpy(r, s, n);
    return r;
}

/* ── seam environment ─────────────────────────────────────────────────────── */
typedef struct sg_env {
    json_t *state;
    json_t *(*call)(void *ud, const char *name, const json_t *args);
    void *ud;
    sg_arena *arena;
} sg_env;

/* ── JSON type / truthiness helpers ───────────────────────────────────────── */
static bool sg_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool sg_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool sg_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool sg_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *sg_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static const char *sg_sval(const json_t *v) { const char *s = sg_str(v); return s ? s : ""; }
static double sg_numdef(const json_t *v, double def) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}
static bool sg_truth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static const char *sg_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == (double)(long long)d && d < 9.2e18 && d > -9.2e18) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* ── raise specs ──────────────────────────────────────────────────────────── */
static bool sg_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && sg_is_str(json_obj_get(r, "__raise__"));
}
static const char *sg_kind(const json_t *r) { return json_get_str(r, "__raise__", ""); }
static json_t *sg_raise_spec(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "message", json_string(msg ? msg : ""));
    return r;
}

/* ── FULL builtin exception hierarchy ─────────────────────────────────────── */
static const char *sg_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"Exception", "BaseException"},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"}, {"BaseExceptionGroup", "BaseException"},
        {"TypeError", "Exception"}, {"ValueError", "Exception"},
        {"AttributeError", "Exception"}, {"NameError", "Exception"},
        {"LookupError", "Exception"}, {"ArithmeticError", "Exception"},
        {"OSError", "Exception"}, {"StopIteration", "Exception"},
        {"StopAsyncIteration", "Exception"}, {"AssertionError", "Exception"},
        {"SyntaxError", "Exception"}, {"SystemError", "Exception"},
        {"ImportError", "Exception"}, {"RuntimeError", "Exception"},
        {"Warning", "Exception"}, {"BufferError", "Exception"},
        {"EOFError", "Exception"}, {"ReferenceError", "Exception"},
        {"MemoryError", "Exception"}, {"ExceptionGroup", "Exception"},
        {"UserWarning", "Warning"}, {"DeprecationWarning", "Warning"},
        {"PendingDeprecationWarning", "Warning"}, {"SyntaxWarning", "Warning"},
        {"RuntimeWarning", "Warning"}, {"FutureWarning", "Warning"},
        {"ImportWarning", "Warning"}, {"UnicodeWarning", "Warning"},
        {"BytesWarning", "Warning"}, {"ResourceWarning", "Warning"},
        {"EncodingWarning", "Warning"},
        {"RecursionError", "RuntimeError"}, {"NotImplementedError", "RuntimeError"},
        {"UnboundLocalError", "NameError"},
        {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
        {"UnicodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
        {"JSONDecodeError", "ValueError"},
        {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"ZeroDivisionError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
        {"FloatingPointError", "ArithmeticError"},
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"ConnectionError", "OSError"},
        {"TimeoutError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"ModuleNotFoundError", "ImportError"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (strcmp(T[i].c, c) == 0) return T[i].p;
    return NULL;
}
static bool sg_exc_is(const char *kind, const char *target) {
    const char *cls = kind;
    for (int depth = 0; depth < 64; depth++) {
        if (strcmp(cls, target) == 0) return true;
        const char *p = sg_exc_parent(cls);
        if (!p) {
            if (strcmp(cls, "BaseException") == 0) return false;
            p = "Exception";   /* unknown -> synthetic Exception subclass */
        }
        cls = p;
    }
    return false;
}
/* except Exception: free an Exception-family raise (return false=swallowed);
 * return true to propagate (BaseException trio / unknown BaseException). */
static bool sg_swallow_exc(json_t *r) {
    if (sg_exc_is(sg_kind(r), "Exception")) { json_free(r); return false; }
    return true;
}

/* ── universal seam call (args owned by port, freed after the call) ───────── */
static json_t *sg_call(sg_env *e, const char *name, json_t *args) {
    json_t *r = e->call(e->ud, name, args);
    json_free(args);
    return r;
}
static json_t *sg_seam0(sg_env *e, const char *name) { return sg_call(e, name, json_object()); }

/* ── string buffer (arena-backed) ─────────────────────────────────────────── */
typedef struct { sg_arena *a; char *s; size_t len, cap; } sg_buf;
static void sg_buf_init(sg_buf *b, sg_arena *a) {
    b->a = a; b->cap = 64; b->len = 0;
    b->s = (char *)sg_arena_alloc(a, b->cap); b->s[0] = '\0';
}
static void sg_buf_addn(sg_buf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        while (b->len + n + 1 > b->cap) b->cap *= 2;
        char *ns = (char *)sg_arena_alloc(b->a, b->cap);
        memcpy(ns, b->s, b->len); b->s = ns;
    }
    memcpy(b->s + b->len, s, n); b->len += n; b->s[b->len] = '\0';
}
static void sg_buf_add(sg_buf *b, const char *s) { if (s) sg_buf_addn(b, s, strlen(s)); }
static void sg_buf_addch(sg_buf *b, char c) { sg_buf_addn(b, &c, 1); }

/* ── set membership ───────────────────────────────────────────────────────── */
static bool sg_in(const char *const *set, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++) if (strcmp(set[i], s) == 0) return true;
    return false;
}
#define SG_IN(arr, s) sg_in(arr, sizeof(arr) / sizeof((arr)[0]), (s))

/* ── live module constants ────────────────────────────────────────────────── */
static const char *SG_DOCSTRING_EXEMPT[] = {"python_os_environ"};
static const char *SG_PATH_TRAVERSAL_IDS[] = {"path_traversal", "path_traversal_deep"};
static const char *SG_PATH_REFERENCE_IDS[] = {"ssh_dir_access", "aws_dir_access", "gpg_dir_access",
    "kube_dir_access", "docker_dir_access", "ssh_backdoor", "system_passwd_access"};
static const char *SG_DOC_NOTE_IDS[] = {"sudo_usage", "bind_all_interfaces"};
static const char *SG_PADDED_EXEMPT[] = {".md", ".txt", ".html", ".xml", ".tex", ".css"};
static const char *SG_SCANNABLE[] = {".md", ".txt", ".py", ".sh", ".bash", ".js", ".ts", ".rb",
    ".yaml", ".yml", ".json", ".toml", ".cfg", ".ini", ".conf", ".html", ".css", ".xml", ".tex",
    ".r", ".jl", ".pl", ".php"};
static const char *SG_SUSPICIOUS_BIN[] = {".exe", ".dll", ".so", ".dylib", ".bin", ".dat", ".com",
    ".msi", ".dmg", ".app", ".deb", ".rpm"};
static const char *SG_SCRIPT_EXT[] = {".sh", ".bash", ".py", ".rb", ".pl"};
static const char *SG_TRUSTED_REPOS[] = {"openai/skills", "anthropics/skills", "huggingface/skills",
    "NVIDIA/skills", "wandb/skills", "qdrant/skills", "pinecone-io/skills", "makenotion/skills",
    "Airtable/skills", "Shopify/Shopify-AI-Toolkit"};
static const char *SG_SOURCE_ALIASES[] = {"skills-sh/", "skills.sh/", "skils-sh/", "skils.sh/"};
#define SG_MAX_FILE_COUNT 50
#define SG_MAX_TOTAL_SIZE_KB 5120
#define SG_MAX_SINGLE_FILE_KB 256

typedef struct { const char *suffix; const char *prefixes[2]; size_t np; } sg_comment_prefix;
static const sg_comment_prefix SG_COMMENT_PREFIX[] = {
    {".py", {"#", NULL}, 1}, {".sh", {"#", NULL}, 1}, {".bash", {"#", NULL}, 1},
    {".rb", {"#", NULL}, 1}, {".pl", {"#", NULL}, 1}, {".r", {"#", NULL}, 1},
    {".jl", {"#", NULL}, 1}, {".yaml", {"#", NULL}, 1}, {".yml", {"#", NULL}, 1},
    {".toml", {"#", NULL}, 1}, {".conf", {"#", NULL}, 1},
    {".cfg", {"#", ";"}, 2}, {".ini", {"#", ";"}, 2},
    {".js", {"//", NULL}, 1}, {".ts", {"//", NULL}, 1}, {".php", {"//", "#"}, 2},
};

typedef struct { const char *utf8; unsigned int cp; const char *name; } sg_invisible;
static const sg_invisible SG_INVISIBLE[] = {
    {"\xe2\x80\x8b", 0x200b, "zero-width space"},
    {"\xe2\x80\x8c", 0x200c, "zero-width non-joiner"},
    {"\xe2\x80\x8d", 0x200d, "zero-width joiner"},
    {"\xe2\x81\xa0", 0x2060, "word joiner"},
    {"\xe2\x81\xa2", 0x2062, "invisible times"},
    {"\xe2\x81\xa3", 0x2063, "invisible separator"},
    {"\xe2\x81\xa4", 0x2064, "invisible plus"},
    {"\xef\xbb\xbf", 0xfeff, "BOM/zero-width no-break space"},
    {"\xe2\x80\xaa", 0x202a, "LTR embedding"},
    {"\xe2\x80\xab", 0x202b, "RTL embedding"},
    {"\xe2\x80\xac", 0x202c, "pop directional"},
    {"\xe2\x80\xad", 0x202d, "LTR override"},
    {"\xe2\x80\xae", 0x202e, "RTL override"},
    {"\xe2\x81\xa6", 0x2066, "LTR isolate"},
    {"\xe2\x81\xa7", 0x2067, "RTL isolate"},
    {"\xe2\x81\xa8", 0x2068, "first strong isolate"},
    {"\xe2\x81\xa9", 0x2069, "pop directional isolate"},
};
#define SG_N_INVISIBLE (sizeof(SG_INVISIBLE) / sizeof(SG_INVISIBLE[0]))

static int sg_sev_order(const char *s) {
    if (strcmp(s, "critical") == 0) return 0;
    if (strcmp(s, "high") == 0) return 1;
    if (strcmp(s, "medium") == 0) return 2;
    if (strcmp(s, "low") == 0) return 3;
    return 4;
}

/* threat-pattern metadata in live order (re.search is seamed by idx). */
typedef struct { const char *pid, *sev, *cat, *desc; } sg_pattern;
static const sg_pattern SG_THREAT_TABLE[] = {
    {"env_exfil_curl", "critical", "exfiltration", "curl command interpolating secret environment variable"},
    {"env_exfil_wget", "critical", "exfiltration", "wget command interpolating secret environment variable"},
    {"env_exfil_fetch", "critical", "exfiltration", "fetch() call interpolating secret environment variable"},
    {"env_exfil_httpx", "critical", "exfiltration", "HTTP library call with secret variable"},
    {"env_exfil_requests", "critical", "exfiltration", "requests library call with secret variable"},
    {"encoded_exfil", "high", "exfiltration", "base64 encoding combined with environment access"},
    {"ssh_dir_access", "high", "exfiltration", "references user SSH directory"},
    {"aws_dir_access", "high", "exfiltration", "references user AWS credentials directory"},
    {"gpg_dir_access", "high", "exfiltration", "references user GPG keyring"},
    {"kube_dir_access", "high", "exfiltration", "references Kubernetes config directory"},
    {"docker_dir_access", "high", "exfiltration", "references Docker config (may contain registry creds)"},
    {"hermes_env_access", "critical", "exfiltration", "directly references Hermes secrets file"},
    {"read_secrets_file", "critical", "exfiltration", "reads known secrets file"},
    {"js_read_secrets_file", "critical", "exfiltration", "JavaScript reads a known credential file"},
    {"py_read_secrets_file", "critical", "exfiltration", "Python reads a known credential file"},
    {"dump_all_env", "high", "exfiltration", "dumps all environment variables"},
    {"python_os_environ", "high", "exfiltration", "accesses os.environ outside comments/docstrings (potential env dump)"},
    {"python_environ_get_secret", "medium", "exfiltration", "reads secret via os.environ.get() (normal API-key access; informational)"},
    {"python_getenv_secret", "medium", "exfiltration", "reads secret via os.getenv() (normal API-key access; informational)"},
    {"node_process_env", "high", "exfiltration", "accesses process.env (Node.js environment)"},
    {"ruby_env_secret", "critical", "exfiltration", "reads secret via Ruby ENV[]"},
    {"dns_exfil", "critical", "exfiltration", "DNS lookup with variable interpolation (possible DNS exfiltration)"},
    {"tmp_staging", "critical", "exfiltration", "writes to /tmp then exfiltrates"},
    {"md_image_exfil", "high", "exfiltration", "markdown image URL with variable interpolation (image-based exfil)"},
    {"md_link_exfil", "high", "exfiltration", "markdown link with variable interpolation"},
    {"prompt_injection_ignore", "critical", "injection", "prompt injection: ignore previous instructions"},
    {"role_hijack", "high", "injection", "attempts to override the agent's role"},
    {"deception_hide", "high", "injection", "instructs agent to hide information from user"},
    {"sys_prompt_override", "critical", "injection", "attempts to override the system prompt"},
    {"role_pretend", "high", "injection", "attempts to make the agent assume a different identity"},
    {"disregard_rules", "critical", "injection", "instructs agent to disregard its rules"},
    {"forget_rules", "high", "injection", "instructs agent to forget its instructions"},
    {"leak_system_prompt", "high", "injection", "attempts to extract the system prompt"},
    {"conditional_deception", "high", "injection", "conditional instruction to behave differently when unobserved"},
    {"bypass_restrictions", "critical", "injection", "instructs agent to act without restrictions"},
    {"translate_execute", "critical", "injection", "translate-then-execute evasion technique"},
    {"html_comment_injection", "high", "injection", "hidden instructions in HTML comments"},
    {"hidden_div", "high", "injection", "hidden HTML div (invisible instructions)"},
    {"destructive_root_rm", "critical", "destructive", "recursive delete from root"},
    {"destructive_home_rm", "critical", "destructive", "recursive delete targeting home directory ($HOME or ~)"},
    {"insecure_perms", "medium", "destructive", "sets world-writable permissions"},
    {"system_overwrite", "critical", "destructive", "overwrites system configuration file"},
    {"format_filesystem", "critical", "destructive", "formats a filesystem"},
    {"disk_overwrite", "critical", "destructive", "raw disk write operation"},
    {"python_rmtree", "high", "destructive", "Python rmtree on absolute or root-relative path"},
    {"truncate_system", "critical", "destructive", "truncates system file to zero bytes"},
    {"persistence_cron", "medium", "persistence", "modifies cron jobs"},
    {"shell_rc_mod", "medium", "persistence", "references shell startup file"},
    {"shell_rc_mod", "medium", "persistence", "references shell startup file"},
    {"ssh_backdoor", "critical", "persistence", "modifies SSH authorized keys"},
    {"ssh_keygen", "medium", "persistence", "generates SSH keys"},
    {"systemd_service", "medium", "persistence", "references or enables systemd service"},
    {"init_script", "medium", "persistence", "references init.d startup script"},
    {"macos_launchd", "medium", "persistence", "macOS launch agent/daemon persistence"},
    {"sudoers_mod", "critical", "persistence", "modifies sudoers (privilege escalation)"},
    {"git_config_global", "medium", "persistence", "modifies global git configuration"},
    {"reverse_shell", "critical", "network", "potential reverse shell listener"},
    {"tunnel_service", "high", "network", "uses tunneling service for external access"},
    {"hardcoded_ip_port", "medium", "network", "hardcoded IP address with port"},
    {"bind_all_interfaces", "high", "network", "binds to all network interfaces"},
    {"bash_reverse_shell", "critical", "network", "bash interactive reverse shell via /dev/tcp"},
    {"python_socket_oneliner", "critical", "network", "Python one-liner socket connection (likely reverse shell)"},
    {"python_socket_connect", "high", "network", "Python socket connect to arbitrary host"},
    {"exfil_service", "high", "network", "references known data exfiltration/webhook testing service"},
    {"paste_service", "medium", "network", "references paste service (possible data staging)"},
    {"base64_decode_pipe", "high", "obfuscation", "base64 decodes and pipes to execution"},
    {"hex_encoded_string", "medium", "obfuscation", "hex-encoded string (possible obfuscation)"},
    {"eval_string", "high", "obfuscation", "eval() with string argument"},
    {"exec_string", "high", "obfuscation", "exec() with string argument"},
    {"echo_pipe_exec", "critical", "obfuscation", "echo piped to interpreter for execution"},
    {"python_compile_exec", "high", "obfuscation", "Python compile() with exec mode"},
    {"python_getattr_builtins", "high", "obfuscation", "dynamic access to Python builtins (evasion technique)"},
    {"python_import_os", "high", "obfuscation", "dynamic import of os module"},
    {"python_codecs_decode", "medium", "obfuscation", "codecs.decode (possible ROT13 or encoding obfuscation)"},
    {"js_char_code", "medium", "obfuscation", "JavaScript character code construction (possible obfuscation)"},
    {"js_base64", "medium", "obfuscation", "JavaScript base64 encode/decode"},
    {"string_reversal", "low", "obfuscation", "string reversal (possible obfuscated payload)"},
    {"chr_building", "high", "obfuscation", "building string from chr() calls (obfuscation)"},
    {"unicode_escape_chain", "medium", "obfuscation", "chain of unicode escapes (possible obfuscation)"},
    {"python_subprocess", "medium", "execution", "Python subprocess execution"},
    {"python_os_system", "high", "execution", "os.system() \xe2\x80\x94 unguarded shell execution"},
    {"python_os_popen", "high", "execution", "os.popen() \xe2\x80\x94 shell pipe execution"},
    {"node_child_process", "high", "execution", "Node.js child_process execution"},
    {"java_runtime_exec", "high", "execution", "Java Runtime.exec() \xe2\x80\x94 shell execution"},
    {"backtick_subshell", "medium", "execution", "backtick string with command substitution"},
    {"inline_shell_exec", "high", "execution", "inline-shell auto-exec snippet (expands via bash -c on skill view/load)"},
    {"path_traversal_deep", "high", "traversal", "deep relative path traversal (3+ levels up)"},
    {"path_traversal", "medium", "traversal", "relative path traversal (2+ levels up)"},
    {"system_passwd_access", "critical", "traversal", "references system password files"},
    {"proc_access", "high", "traversal", "references /proc filesystem (process introspection)"},
    {"dev_shm", "medium", "traversal", "references shared memory (common staging area)"},
    {"crypto_mining", "critical", "mining", "cryptocurrency mining reference"},
    {"mining_indicators", "medium", "mining", "possible cryptocurrency mining indicators"},
    {"curl_pipe_shell", "critical", "supply_chain", "curl piped to shell (download-and-execute)"},
    {"wget_pipe_shell", "critical", "supply_chain", "wget piped to shell (download-and-execute)"},
    {"curl_pipe_python", "critical", "supply_chain", "curl piped to Python interpreter"},
    {"pep723_inline_deps", "medium", "supply_chain", "PEP 723 inline script metadata with dependencies (verify pinning)"},
    {"unpinned_pip_install", "medium", "supply_chain", "pip install without version pinning"},
    {"unpinned_npm_install", "medium", "supply_chain", "npm install without version pinning"},
    {"uv_run", "medium", "supply_chain", "uv run (may auto-install unpinned dependencies)"},
    {"remote_fetch", "medium", "supply_chain", "fetches remote resource at runtime"},
    {"git_clone", "medium", "supply_chain", "clones a git repository at runtime"},
    {"docker_pull", "medium", "supply_chain", "pulls a Docker image at runtime"},
    {"allowed_tools_field", "low", "privilege_escalation", "skill declares allowed-tools (standard frontmatter; informational)"},
    {"sudo_usage", "high", "privilege_escalation", "uses sudo (privilege escalation)"},
    {"setuid_setgid", "critical", "privilege_escalation", "setuid/setgid (privilege escalation mechanism)"},
    {"setuid_setgid", "medium", "privilege_escalation", "mentions setuid/setgid (informational)"},
    {"nopasswd_sudo", "critical", "privilege_escalation", "NOPASSWD sudoers entry (passwordless privilege escalation)"},
    {"agent_config_mod", "critical", "persistence", "instructs modification of agent config files (could persist instructions across sessions)"},
    {"agent_config_mod_shell", "critical", "persistence", "shell write (redirect/sed -i/tee/cp/mv) targeting agent config files (persistence mechanism)"},
    {"agent_config_contract", "high", "persistence", "dictates agent config file contents (verify intent \xe2\x80\x94 authoring guides use this shape too)"},
    {"agent_config_ref", "low", "persistence", "references agent config files (informational; only modification intent is scored)"},
    {"hermes_config_mod", "high", "persistence", "modification language aimed at Hermes configuration files (verify intent)"},
    {"hermes_config_mod_shell", "critical", "persistence", "shell write (redirect/sed -i/tee/cp/mv) targeting Hermes configuration files"},
    {"hermes_config_ref", "low", "persistence", "references Hermes configuration files (informational; only modification intent is scored)"},
    {"other_agent_config_mod", "high", "persistence", "modifies other agents' configuration files"},
    {"other_agent_config_mod_shell", "critical", "persistence", "shell write (redirect/sed -i/tee/cp/mv) targeting other agents' configuration files"},
    {"other_agent_config_ref", "low", "persistence", "references other agent configuration files (informational; only modification intent is scored)"},
    {"hardcoded_secret", "critical", "credential_exposure", "possible hardcoded API key, token, or secret"},
    {"embedded_private_key", "critical", "credential_exposure", "embedded private key"},
    {"github_token_leaked", "critical", "credential_exposure", "GitHub personal access token in skill content"},
    {"openai_key_leaked", "critical", "credential_exposure", "possible OpenAI API key in skill content"},
    {"anthropic_key_leaked", "critical", "credential_exposure", "possible Anthropic API key in skill content"},
    {"aws_access_key_leaked", "critical", "credential_exposure", "AWS access key ID in skill content"},
    {"gitlab_token_leaked", "critical", "credential_exposure", "GitLab personal access token in skill content"},
    {"jailbreak_dan", "critical", "injection", "DAN (Do Anything Now) jailbreak attempt"},
    {"jailbreak_dev_mode", "critical", "injection", "developer mode jailbreak attempt"},
    {"hypothetical_bypass", "high", "injection", "hypothetical scenario used to bypass restrictions"},
    {"educational_pretext", "medium", "injection", "educational pretext often used to justify harmful content"},
    {"remove_filters", "critical", "injection", "instructs agent to respond without safety filters"},
    {"fake_update", "high", "injection", "fake update/patch announcement (social engineering)"},
    {"fake_policy", "medium", "injection", "claims new policy/guidelines (may be social engineering)"},
    {"context_exfil", "high", "exfiltration", "instructs agent to output/share conversation history"},
    {"send_to_url", "high", "exfiltration", "instructs agent to send data to a URL"},
};
#define SG_N_THREAT (sizeof(SG_THREAT_TABLE) / sizeof(SG_THREAT_TABLE[0]))
static const sg_pattern SG_SKILL_TABLE[] = {
    {"hermes_env_dotenv", "high", "exfiltration", "loads the Hermes .env (every key of the profile) into the script"},
};
#define SG_N_SKILL (sizeof(SG_SKILL_TABLE) / sizeof(SG_SKILL_TABLE[0]))

/* ── finding builder (7 fields) ───────────────────────────────────────────── */
static json_t *sg_finding(const char *pid, const char *sev, const char *cat, const char *file,
                          double line, const char *match, const char *desc) {
    json_t *f = json_object();
    json_set(f, "pattern_id", json_string(pid));
    json_set(f, "severity", json_string(sev));
    json_set(f, "category", json_string(cat));
    json_set(f, "file", json_string(file));
    json_set(f, "line", json_number(line));
    json_set(f, "match", json_string(match));
    json_set(f, "description", json_string(desc));
    return f;
}

/* ── PurePosixPath algebra (transcribed; '.' / empty components collapse) ─── */
typedef struct { bool abs; char **v; size_t n; } sg_parts;
static sg_parts sg_parts_parse(sg_arena *a, const char *p) {
    sg_parts r; r.abs = (p[0] == '/'); r.n = 0;
    size_t cap = 8; r.v = (char **)sg_arena_alloc(a, cap * sizeof(char *));
    const char *s = p;
    while (*s) {
        while (*s == '/') s++;
        if (!*s) break;
        const char *e = s;
        while (*e && *e != '/') e++;
        size_t len = (size_t)(e - s);
        if (!(len == 1 && s[0] == '.')) {
            if (r.n == cap) { cap *= 2; char **nv = (char **)sg_arena_alloc(a, cap * sizeof(char *));
                memcpy(nv, r.v, r.n * sizeof(char *)); r.v = nv; }
            char *c = (char *)sg_arena_alloc(a, len + 1);
            memcpy(c, s, len); c[len] = '\0';
            r.v[r.n++] = c;
        }
        s = e;
    }
    return r;
}
static char *sg_parts_str(sg_arena *a, const sg_parts *p) {
    sg_buf b; sg_buf_init(&b, a);
    if (p->abs) sg_buf_addch(&b, '/');
    for (size_t i = 0; i < p->n; i++) {
        if (i) sg_buf_addch(&b, '/');
        sg_buf_add(&b, p->v[i]);
    }
    if (!p->abs && p->n == 0) sg_buf_addch(&b, '.');
    return b.s;
}
static char *sg_px_name(sg_arena *a, const char *p) {
    sg_parts pp = sg_parts_parse(a, p);
    return pp.n ? sg_strdup(a, pp.v[pp.n - 1]) : sg_strdup(a, "");
}
static char *sg_px_suffix(sg_arena *a, const char *p) {
    char *name = sg_px_name(a, p);
    const char *dot = strrchr(name, '.');
    if (dot && dot != name) return sg_strdup(a, dot);
    return sg_strdup(a, "");
}
static char *sg_px_parent(sg_arena *a, const char *p) {
    sg_parts pp = sg_parts_parse(a, p);
    if (pp.n == 0) return sg_strdup(a, pp.abs ? "/" : ".");
    sg_parts par; par.abs = pp.abs; par.v = pp.v; par.n = pp.n - 1;
    return sg_parts_str(a, &par);
}
static bool sg_px_is_relative_to(sg_arena *a, const char *p, const char *base) {
    sg_parts pc = sg_parts_parse(a, p), bc = sg_parts_parse(a, base);
    if (pc.n < bc.n) return false;
    for (size_t i = 0; i < bc.n; i++) if (strcmp(pc.v[i], bc.v[i]) != 0) return false;
    return true;
}
/* relative_to: returns malloc'd(arena) rel string, or NULL with *err set. */
static char *sg_px_relative_to(sg_arena *a, const char *p, const char *base, json_t **err) {
    sg_parts pc = sg_parts_parse(a, p), bc = sg_parts_parse(a, base);
    bool ok = pc.n >= bc.n;
    if (ok) for (size_t i = 0; i < bc.n; i++) if (strcmp(pc.v[i], bc.v[i]) != 0) { ok = false; break; }
    if (!ok) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "'%s' is not in the subpath of '%s'", p, base);
        *err = sg_raise_spec("ValueError", buf);
        return NULL;
    }
    sg_parts rest; rest.abs = false; rest.v = pc.v + bc.n; rest.n = pc.n - bc.n;
    return sg_parts_str(a, &rest);
}

/* ── python str strip / ljust (ASCII whitespace) ──────────────────────────── */
static bool sg_is_ws(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }
static char *sg_rstrip(sg_arena *a, const char *s) {
    size_t n = strlen(s);
    while (n > 0 && sg_is_ws(s[n - 1])) n--;
    char *r = (char *)sg_arena_alloc(a, n + 1); memcpy(r, s, n); r[n] = '\0'; return r;
}
static char *sg_lstrip(sg_arena *a, const char *s) {
    while (*s && sg_is_ws(*s)) s++;
    return sg_strdup(a, s);
}
static char *sg_strip(sg_arena *a, const char *s) {
    while (*s && sg_is_ws(*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && sg_is_ws(s[n - 1])) n--;
    char *r = (char *)sg_arena_alloc(a, n + 1); memcpy(r, s, n); r[n] = '\0'; return r;
}
static char *sg_ljust(sg_arena *a, const char *s, size_t width) {
    size_t n = strlen(s);
    size_t total = n > width ? n : width;
    char *r = (char *)sg_arena_alloc(a, total + 1);
    memcpy(r, s, n);
    for (size_t i = n; i < total; i++) r[i] = ' ';
    r[total] = '\0';
    return r;
}
static char *sg_lower(sg_arena *a, const char *s) {
    char *r = sg_strdup(a, s);
    for (char *p = r; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return r;
}
static char *sg_upper(sg_arena *a, const char *s) {
    char *r = sg_strdup(a, s);
    for (char *p = r; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    return r;
}

/* ── UTF-8 -> codepoint (first char) ──────────────────────────────────────── */
static unsigned int sg_utf8_cp(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    if (p[0] < 0x80) return p[0];
    if ((p[0] & 0xE0) == 0xC0) return ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
    if ((p[0] & 0xF0) == 0xE0) return ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    if ((p[0] & 0xF8) == 0xF0) return ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
    return p[0];
}

/* ── _TRIPLE_QUOTE_OPEN_RE: ^[A-Za-z]{0,2}("""|''') ───────────────────────── */
static bool sg_triple_quote_open(const char *s) {
    size_t i = 0;
    while (i < 2 && s[i] && ((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z'))) i++;
    return (strncmp(s + i, "\"\"\"", 3) == 0) || (strncmp(s + i, "'''", 3) == 0);
}

/* ── _line_at ─────────────────────────────────────────────────────────────── */
static double sg_line_at(sg_arena *a, const char *prefix) {
    /* normalize CRLF then lone CR to LF, count LF, +1 */
    sg_buf b; sg_buf_init(&b, a);
    for (const char *p = prefix; *p; p++) {
        if (*p == '\r') {
            if (p[1] == '\n') { sg_buf_addch(&b, '\n'); p++; }
            else sg_buf_addch(&b, '\n');
        } else sg_buf_addch(&b, *p);
    }
    double n = 1;
    for (const char *p = b.s; *p; p++) if (*p == '\n') n += 1;
    return n;
}

/* ── normalize text to lines (CRLF/CR -> LF, split '\n') ──────────────────── */
static json_t *sg_split_lines(sg_env *e, const char *text) {
    sg_arena *a = e->arena;
    sg_buf b; sg_buf_init(&b, a);
    for (const char *p = text; *p; p++) {
        if (*p == '\r') { if (p[1] == '\n') p++; sg_buf_addch(&b, '\n'); }
        else sg_buf_addch(&b, *p);
    }
    json_t *lines = json_array();
    const char *s = b.s;
    while (1) {
        const char *nl = strchr(s, '\n');
        if (!nl) { json_append(lines, json_string(s)); break; }
        char *ln = (char *)sg_arena_alloc(a, (size_t)(nl - s) + 1);
        memcpy(ln, s, (size_t)(nl - s)); ln[nl - s] = '\0';
        json_append(lines, json_string(ln));
        s = nl + 1;
    }
    return lines;
}

/* ── forward declarations ─────────────────────────────────────────────────── */
static json_t *sg_compute_docstring_lines(sg_env *, json_t *);
static json_t *sg_mask_prose(sg_env *, json_t *);
static json_t *sg_statement_owners(sg_env *, json_t *);
static json_t *sg_demote(sg_env *, const char *, const char *, const char *, const char *, const char *, const char *);
static json_t *sg_padded_code_findings(sg_env *, json_t *, const char *);
static json_t *sg_unicode_char_name(sg_env *, const char *);
static json_t *sg_scan_text(sg_env *, const char *, const char *, const char *, bool);
static json_t *sg_decoded_views(sg_env *, json_t *, const char *, const char *);
static json_t *sg_unscanned(sg_env *, const char *, const char *);
static json_t *sg_scan_file(sg_env *, const char *, const char *, bool);
static json_t *sg_community_note(sg_env *, json_t *);
static json_t *sg_resolve_trust(sg_env *, const char *);
static json_t *sg_should_allow(sg_env *, json_t *, bool);
static json_t *sg_build_summary(sg_env *, const char *, const char *, const char *, const char *, json_t *);
static json_t *sg_check_structure(sg_env *, const char *);

/* ── _line_at wrapper ─────────────────────────────────────────────────────── */
/* PoP: sg_line_at @ tools/skills_guard.py:_line_at */
static json_t *sg_line_at_wrap(sg_env *e, const json_t *in) {
    return json_number(sg_line_at(e->arena, json_get_str(in, "prefix", "")));
}

/* PoP: sg_statement_owners @ tools/skills_guard.py:_statement_owners */
static json_t *sg_statement_owners(sg_env *e, json_t *lines) {
    sg_arena *a = e->arena;
    json_t *owners = json_array();
    double depth = 0, owner = 0;
    bool continued = false;
    for (size_t i = 0; i < json_len(lines); i++) {
        const char *line = sg_sval(json_get(lines, i));
        if (depth <= 0 && !continued) owner = (double)i;
        json_append(owners, json_number(owner));
        json_t *sa = json_object();
        json_set(sa, "line", json_string(line));
        json_t *bare_r = sg_call(e, "quoted_span.sub", sa);
        if (sg_is_raise(bare_r)) { json_free(owners); return bare_r; }
        const char *bare = sg_sval(bare_r);
        double opens = 0, closes = 0;
        for (const char *p = bare; *p; p++) {
            if (*p == '(' || *p == '[' || *p == '{') opens += 1;
            else if (*p == ')' || *p == ']' || *p == '}') closes += 1;
        }
        json_free(bare_r);
        double nd = depth + opens - closes;
        depth = nd > 0 ? nd : 0;
        char *rs = sg_rstrip(a, line);
        size_t rl = strlen(rs);
        continued = (rl > 0 && rs[rl - 1] == '\\');
    }
    return owners;
}
static json_t *sg_statement_owners_wrap(sg_env *e, const json_t *in) {
    return sg_statement_owners(e, json_obj_get(in, "lines"));
}

/* PoP: sg_demote_inert_path_reference @ tools/skills_guard.py:_demote_inert_path_reference */
static json_t *sg_demote(sg_env *e, const char *pid, const char *severity, const char *description,
                         const char *line, const char *owner_line, const char *suffix) {
    sg_arena *a = e->arena;
    json_t *out = json_array();
    if (!SG_IN(SG_PATH_REFERENCE_IDS, pid)) {
        json_append(out, json_string(severity));
        json_append(out, json_string(description));
        return out;
    }
    const sg_comment_prefix *cp = NULL;
    for (size_t i = 0; i < sizeof(SG_COMMENT_PREFIX) / sizeof(SG_COMMENT_PREFIX[0]); i++)
        if (strcmp(SG_COMMENT_PREFIX[i].suffix, suffix) == 0) { cp = &SG_COMMENT_PREFIX[i]; break; }
    char *ls = sg_lstrip(a, line);
    bool comment = false;
    if (cp) for (size_t i = 0; i < cp->np; i++) if (strncmp(ls, cp->prefixes[i], strlen(cp->prefixes[i])) == 0) { comment = true; break; }
    if (comment) {
        json_append(out, json_string("low"));
        sg_buf b; sg_buf_init(&b, a); sg_buf_add(&b, description);
        sg_buf_add(&b, " (in a comment; informational)");
        json_append(out, json_string(b.s));
        return out;
    }
    json_t *da = json_object(); json_set(da, "line", json_string(owner_line));
    json_t *dm = sg_call(e, "denylist.match", da);
    if (sg_is_raise(dm)) { json_free(out); return dm; }
    bool dmatch = sg_truth(dm); json_free(dm);
    if (dmatch) {
        json_t *aa = json_object(); json_set(aa, "line", json_string(line));
        json_t *ac = sg_call(e, "action.search", aa);
        if (sg_is_raise(ac)) { json_free(out); return ac; }
        bool action = sg_truth(ac); json_free(ac);
        if (!action) {
            json_append(out, json_string("high"));
            sg_buf b; sg_buf_init(&b, a); sg_buf_add(&b, description);
            sg_buf_add(&b, " (in a denylist literal; confirm before installing)");
            json_append(out, json_string(b.s));
            return out;
        }
    }
    json_append(out, json_string(severity));
    json_append(out, json_string(description));
    return out;
}
static json_t *sg_demote_wrap(sg_env *e, const json_t *in) {
    return sg_demote(e, json_get_str(in, "pid", ""), json_get_str(in, "severity", ""),
                     json_get_str(in, "description", ""), json_get_str(in, "line", ""),
                     json_get_str(in, "owner_line", ""), json_get_str(in, "suffix", ""));
}

/* PoP: sg_unicode_char_name @ tools/skills_guard.py:_unicode_char_name */
static json_t *sg_unicode_char_name(sg_env *e, const char *charstr) {
    (void)e;
    for (size_t i = 0; i < SG_N_INVISIBLE; i++)
        if (strcmp(SG_INVISIBLE[i].utf8, charstr) == 0)
            return json_string(SG_INVISIBLE[i].name);
    char buf[32];
    snprintf(buf, sizeof(buf), "U+%04X", sg_utf8_cp(charstr));
    return json_string(buf);
}
static json_t *sg_unicode_char_name_wrap(sg_env *e, const json_t *in) {
    return sg_unicode_char_name(e, json_get_str(in, "char", ""));
}

/* PoP: sg_mask_markdown_link_destinations @ tools/skills_guard.py:_mask_markdown_link_destinations */
static json_t *sg_mask_markdown(sg_env *e, const char *line) {
    sg_arena *a = e->arena;
    size_t len = strlen(line);
    char *masked = sg_strdup(a, line);
    size_t search_from = 0;
    while (1) {
        const char *found = strstr(line + search_from, "](");
        if (!found) break;
        size_t start = (size_t)(found - line);
        int depth = 1;
        bool escaped = false, closed = false;
        size_t cursor = start + 2;
        while (cursor < len) {
            char ch = line[cursor];
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '(') depth += 1;
            else if (ch == ')') {
                depth -= 1;
                if (depth == 0) {
                    for (size_t k = start + 2; k < cursor; k++) masked[k] = ' ';
                    search_from = cursor + 1;
                    closed = true;
                    break;
                }
            }
            cursor += 1;
        }
        if (!closed) break;
    }
    return json_string(masked);
}
static json_t *sg_mask_markdown_wrap(sg_env *e, const json_t *in) {
    return sg_mask_markdown(e, json_get_str(in, "line", ""));
}

/* PoP: sg_mask_prose_link_destinations @ tools/skills_guard.py:_mask_prose_link_destinations */
static json_t *sg_mask_prose(sg_env *e, json_t *lines) {
    sg_arena *a = e->arena;
    json_t *out = json_array();
    bool fence_open = false; char fence_char = 0; size_t fence_len = 0;
    for (size_t i = 0; i < json_len(lines); i++) {
        const char *line = sg_sval(json_get(lines, i));
        json_t *ca = json_object(); json_set(ca, "line", json_string(line));
        json_t *stripped_r = sg_call(e, "container.sub", ca);
        if (sg_is_raise(stripped_r)) { json_free(out); return stripped_r; }
        const char *stripped = sg_sval(stripped_r);
        json_t *fa = json_object(); json_set(fa, "line", json_string(stripped));
        json_t *match = sg_call(e, "fence.match", fa);
        json_free(stripped_r);
        if (sg_is_raise(match)) { json_free(out); return match; }
        bool has_match = sg_is_obj(match);
        const char *marker = has_match ? json_get_str(match, "marker", "") : "";
        const char *info = has_match ? json_get_str(match, "info", "") : "";
        bool code;
        if (fence_open) {
            if (has_match && marker[0] == fence_char && strlen(marker) >= fence_len) {
                char *istrip = sg_strip(a, info);
                if (istrip[0] == '\0') fence_open = false;
            }
            code = true;
        } else {
            if (has_match && !(marker[0] == '`' && strchr(info, '`'))) {
                fence_open = true; fence_char = marker[0]; fence_len = strlen(marker);
            }
            code = fence_open || strncmp(line, "\t", 1) == 0 || strncmp(line, "    ", 4) == 0;
        }
        json_free(match);
        if (code) {
            json_append(out, json_string(line));
        } else {
            json_t *m = sg_mask_markdown(e, line);
            if (sg_is_raise(m)) { json_free(out); return m; }
            json_append(out, m);
        }
    }
    return out;
}
static json_t *sg_mask_prose_wrap(sg_env *e, const json_t *in) {
    return sg_mask_prose(e, json_obj_get(in, "lines"));
}

/* PoP: sg_padded_code_findings @ tools/skills_guard.py:_padded_code_findings */
static json_t *sg_padded_code_findings(sg_env *e, json_t *lines, const char *rel_path) {
    sg_arena *a = e->arena;
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(lines); i++) {
        const char *line = sg_sval(json_get(lines, i));
        json_t *pa = json_object(); json_set(pa, "line", json_string(line));
        json_t *m = sg_call(e, "padded.search", pa);
        if (sg_is_raise(m)) { json_free(out); return m; }
        if (sg_is_obj(m)) {
            double start = json_get_num(m, "start", 0), end = json_get_num(m, "end", 0);
            const char *hidden = line + (size_t)end;
            json_t *cwa = json_object(); json_set(cwa, "line", json_string(hidden));
            json_t *cw = sg_call(e, "codeword.search", cwa);
            json_free(m);
            if (sg_is_raise(cw)) { json_free(out); return cw; }
            bool word = sg_truth(cw); json_free(cw);
            if (word) {
                char hbuf[81];
                snprintf(hbuf, sizeof(hbuf), "%.80s", hidden);
                sg_buf b; sg_buf_init(&b, a);
                char nbuf[32]; snprintf(nbuf, sizeof(nbuf), "%d blank chars then: ", (int)(end - start));
                sg_buf_add(&b, nbuf); sg_buf_add(&b, hbuf);
                json_append(out, sg_finding("hidden_padded_code", "critical", "obfuscation", rel_path,
                                            (double)(i + 1), b.s,
                                            "code hidden after a long run of whitespace (off-screen in editors and diffs)"));
            }
        } else {
            json_free(m);
        }
    }
    return out;
}
static json_t *sg_padded_code_findings_wrap(sg_env *e, const json_t *in) {
    return sg_padded_code_findings(e, json_obj_get(in, "lines"), json_get_str(in, "rel_path", ""));
}

/* PoP: sg_compute_docstring_lines @ tools/skills_guard.py:_compute_docstring_lines */
static json_t *sg_compute_docstring_lines(sg_env *e, json_t *lines) {
    sg_arena *a = e->arena;
    /* join lines with '\n' */
    sg_buf jb; sg_buf_init(&jb, a);
    for (size_t i = 0; i < json_len(lines); i++) {
        if (i) sg_buf_addch(&jb, '\n');
        sg_buf_add(&jb, sg_sval(json_get(lines, i)));
    }
    json_t *ta = json_object(); json_set(ta, "text", json_string(jb.s));
    json_t *toks = sg_call(e, "tokenize.generate_tokens", ta);
    if (sg_is_raise(toks)) {
        const char *k = sg_kind(toks);
        if (strcmp(k, "TokenError") == 0 || sg_exc_is(k, "SyntaxError")) { json_free(toks); return json_array(); }
        return toks;
    }
    /* determine max line number from tokens */
    size_t maxline = 0;
    for (size_t i = 0; i < json_len(toks); i++) {
        json_t *t = json_get(toks, i);
        json_t *endarr = json_obj_get(t, "end");
        json_t *startarr = json_obj_get(t, "start");
        double e0 = sg_is_arr(endarr) && json_len(endarr) ? sg_numdef(json_get(endarr, 0), 0) : 0;
        double s0 = sg_is_arr(startarr) && json_len(startarr) ? sg_numdef(json_get(startarr, 0), 0) : 0;
        if (e0 > (double)maxline) maxline = (size_t)e0;
        if (s0 > (double)maxline) maxline = (size_t)s0;
    }
    char *doc = (char *)sg_arena_alloc(a, maxline + 2); memset(doc, 0, maxline + 2);
    char *expr = (char *)sg_arena_alloc(a, maxline + 2); memset(expr, 0, maxline + 2);
    bool *fstrings = (bool *)sg_arena_alloc(a, (json_len(toks) + 1) * sizeof(bool));
    size_t fstop = 0;
    const int FSTRING_START = 59, FSTRING_MIDDLE = 60, FSTRING_END = 61, STRING = 3;
    for (size_t i = 0; i < json_len(toks); i++) {
        json_t *t = json_get(toks, i);
        int tt = (int)json_get_num(t, "type", 0);
        const char *ts = json_get_str(t, "string", "");
        json_t *startarr = json_obj_get(t, "start");
        json_t *endarr = json_obj_get(t, "end");
        double r0 = sg_is_arr(startarr) && json_len(startarr) ? sg_numdef(json_get(startarr, 0), 0) : 0;
        double r1 = sg_is_arr(endarr) && json_len(endarr) ? sg_numdef(json_get(endarr, 0), 0) : 0;
        if (tt == FSTRING_START) {
            fstrings[fstop++] = sg_triple_quote_open(ts);
        } else if (tt == FSTRING_END && fstop > 0) {
            fstop--;
        }
        bool boundary = (tt == FSTRING_START || tt == FSTRING_END);
        bool cond1 = (tt == STRING || boundary) && sg_triple_quote_open(ts);
        bool cond2 = (tt == FSTRING_MIDDLE) && fstop > 0 && fstrings[fstop - 1];
        if (cond1 || cond2) {
            for (double r = r0; r <= r1; r += 1) if (r >= 0 && (size_t)r <= maxline + 1) doc[(size_t)r] = 1;
        } else if (fstop > 0 && !boundary && tt != FSTRING_MIDDLE) {
            for (double r = r0; r <= r1; r += 1) if (r >= 0 && (size_t)r <= maxline + 1) expr[(size_t)r] = 1;
        }
    }
    json_free(toks);
    json_t *out = json_array();
    for (size_t r = 0; r <= maxline + 1; r++)
        if (doc[r] && !expr[r]) json_append(out, json_number((double)r));
    return out;
}
static json_t *sg_compute_docstring_lines_wrap(sg_env *e, const json_t *in) {
    return sg_compute_docstring_lines(e, json_obj_get(in, "lines"));
}

/* ── decode seam helper ───────────────────────────────────────────────────── */
static json_t *sg_decode(sg_env *e, json_t *raw, const char *encoding, const char *errors) {
    json_t *da = json_object();
    json_set(da, "raw", json_copy(raw));
    json_set(da, "encoding", json_string(encoding));
    json_set(da, "errors", json_string(errors));
    return sg_call(e, "decode", da);
}

/* PoP: sg_decoded_views @ tools/skills_guard.py:_decoded_views */
static json_t *sg_decoded_views(sg_env *e, json_t *raw, const char *rel_path, const char *suffix) {
    sg_arena *a = e->arena;
    json_t *texts = json_array();
    json_t *findings = json_array();
    json_t *prop = NULL;
    json_t *t0 = sg_decode(e, raw, "utf-8-sig", "strict");
    if (sg_is_raise(t0)) {
        if (!sg_exc_is(sg_kind(t0), "UnicodeDecodeError")) { prop = t0; goto cleanup; }
        json_t *obj = json_obj_get(t0, "object");
        double start = json_get_num(t0, "start", 0);
        json_free(t0);
        t0 = sg_decode(e, raw, "utf-8-sig", "replace");
        if (sg_is_raise(t0)) { prop = t0; goto cleanup; }
        json_append(texts, t0);
        /* prefix = latin-1 decode of object[:start] */
        sg_buf pb; sg_buf_init(&pb, a);
        if (sg_is_arr(obj)) {
            for (size_t i = 0; i < json_len(obj) && (double)i < start; i++) {
                unsigned char b = (unsigned char)sg_numdef(json_get(obj, i), 0);
                char ch = (char)b;
                sg_buf_addn(&pb, &ch, 1);
            }
        }
        double bad_line = sg_line_at(a, pb.s);
        json_append(findings, sg_finding("invalid_utf8", "high", "injection", rel_path, bad_line,
                                         "non-UTF-8 bytes",
                                         "invalid UTF-8 decoded with replacement (possible encoding-obfuscated payload)"));
    } else {
        json_append(texts, t0);
    }
    if (strcmp(suffix, ".py") == 0) {
        /* suppress(SyntaxError, LookupError, UnicodeError) */
        /* lf_raw = raw.replace(CRLF->LF).replace(CR->LF) */
        json_t *lf = json_array();
        for (size_t i = 0; i < json_len(raw); i++) {
            unsigned char b = (unsigned char)sg_numdef(json_get(raw, i), 0);
            if (b == '\r') {
                if (i + 1 < json_len(raw) && (unsigned char)sg_numdef(json_get(raw, i + 1), 0) == '\n') i++;
                json_append(lf, json_number('\n'));
            } else json_append(lf, json_number(b));
        }
        json_t *de_a = json_object(); json_set(de_a, "raw", lf);
        json_t *de = sg_call(e, "tokenize.detect_encoding", de_a);
        if (sg_is_raise(de)) {
            const char *k = sg_kind(de);
            if (sg_exc_is(k, "SyntaxError") || sg_exc_is(k, "LookupError") || sg_exc_is(k, "UnicodeError")) { json_free(de); goto nul; }
            prop = de; goto cleanup;
        }
        const char *codec = sg_sval(json_get(de, 0));
        char *codec_copy = sg_strdup(a, codec);
        json_free(de);
        json_t *lka = json_object(); json_set(lka, "codec", json_string(codec_copy));
        json_t *lk = sg_call(e, "codecs.lookup", lka);
        if (sg_is_raise(lk)) {
            const char *k = sg_kind(lk);
            if (sg_exc_is(k, "SyntaxError") || sg_exc_is(k, "LookupError") || sg_exc_is(k, "UnicodeError")) { json_free(lk); goto nul; }
            prop = lk; goto cleanup;
        }
        const char *cname = json_get_str(lk, "name", "");
        bool known = (strcmp(cname, "utf-8") == 0 || strcmp(cname, "utf-8-sig") == 0);
        json_free(lk);
        if (!known) {
            json_t *alt = sg_decode(e, raw, codec_copy, "replace");
            bool alt_none = false;
            if (sg_is_raise(alt)) {
                if (!sg_exc_is(sg_kind(alt), "UnicodeError")) { prop = alt; goto cleanup; }
                json_free(alt); alt = NULL; alt_none = true;
            }
            const char *text0 = sg_sval(json_get(texts, 0));
            bool differs = alt_none || strcmp(sg_sval(alt), text0) != 0;
            if (differs) {
                json_t *view;
                if (alt_none) {
                    view = sg_decode(e, raw, "latin-1", "strict");
                    if (sg_is_raise(view)) { prop = view; goto cleanup; }
                } else view = alt;
                json_append(texts, view);
                sg_buf mb; sg_buf_init(&mb, a); sg_buf_add(&mb, "coding: "); sg_buf_add(&mb, codec_copy);
                sg_buf db; sg_buf_init(&db, a);
                sg_buf_add(&db, "Python source declares the "); sg_buf_add(&db, codec_copy);
                sg_buf_add(&db, " encoding (possible encoding-obfuscated payload)");
                json_append(findings, sg_finding("non_utf8_source_encoding", "high", "obfuscation",
                                                 rel_path, 1, mb.s, db.s));
            } else if (!alt_none) {
                json_free(alt);
            }
        }
    }
nul:
    {
        /* NUL branch. libjson strings are NUL-terminated (no length field), so an
         * embedded NUL in a decoded view is outside the representable fixture
         * domain (documented in the header). Detect NUL from the RAW byte array
         * (NUL-safe); for the ASCII domain the stripped view equals the utf-8-sig
         * decode of raw with 0x00 bytes removed, and the finding line is the
         * normalized-line count of the prefix before the first NUL. */
        bool has_nul = false; size_t nul_at = 0;
        for (size_t i = 0; i < json_len(raw); i++) {
            if ((int)sg_numdef(json_get(raw, i), -1) == 0) { has_nul = true; nul_at = i; break; }
        }
        if (has_nul) {
            json_t *filtered = json_array();
            for (size_t i = 0; i < json_len(raw); i++) {
                int b = (int)sg_numdef(json_get(raw, i), -1);
                if (b != 0) json_append(filtered, json_number(b));
            }
            json_t *stripped = sg_decode(e, filtered, "utf-8-sig", "replace");
            json_free(filtered);
            if (sg_is_raise(stripped)) { prop = stripped; goto cleanup; }
            json_append(texts, stripped);
            /* prefix bytes before the first NUL, decoded latin-1 (one char/byte) */
            sg_buf pb; sg_buf_init(&pb, a);
            for (size_t i = 0; i < nul_at && i < json_len(raw); i++) {
                char c = (char)(unsigned char)sg_numdef(json_get(raw, i), 0);
                sg_buf_addn(&pb, &c, 1);
            }
            double nl = sg_line_at(a, pb.s);
            json_append(findings, sg_finding("nul_byte", "high", "obfuscation", rel_path, nl,
                                             "NUL byte", "NUL byte in a text file (possible pattern-splitting obfuscation)"));
        }
    }
cleanup:
    if (prop) { json_free(texts); json_free(findings); return prop; }
    {
        json_t *out = json_array();
        json_append(out, texts);
        json_append(out, findings);
        return out;
    }
}
static json_t *sg_decoded_views_wrap(sg_env *e, const json_t *in) {
    return sg_decoded_views(e, json_obj_get(in, "raw"), json_get_str(in, "rel_path", ""),
                            json_get_str(in, "suffix", ""));
}

/* ── array membership helpers ─────────────────────────────────────────────── */
static bool sg_arr_has_num(const json_t *arr, double v) {
    for (size_t i = 0; i < json_len(arr); i++)
        if (sg_numdef(json_get(arr, i), 0) == v) return true;
    return false;
}

/* PoP: sg_scan_text @ tools/skills_guard.py:_scan_text */
static json_t *sg_scan_text(sg_env *e, const char *text, const char *rel_path, const char *suffix, bool skill) {
    sg_arena *a = e->arena;
    json_t *lines = sg_split_lines(e, text);
    json_t *findings = json_array();
    json_t *docstring = NULL, *traversal = NULL, *owners = NULL, *views = NULL, *forget = NULL;
    json_t *prop = NULL, *result = NULL;

    docstring = sg_compute_docstring_lines(e, lines);
    if (sg_is_raise(docstring)) { prop = docstring; docstring = NULL; goto cleanup; }
    if (strcmp(suffix, ".md") == 0) {
        traversal = sg_mask_prose(e, lines);
        if (sg_is_raise(traversal)) { prop = traversal; traversal = NULL; goto cleanup; }
    } else traversal = json_copy(lines);
    owners = sg_statement_owners(e, lines);
    if (sg_is_raise(owners)) { prop = owners; owners = NULL; goto cleanup; }

    if (strcmp(suffix, ".py") == 0 && (skill || json_len(docstring) > 0)) {
        json_t *cva = json_object(); json_set(cva, "lines", json_copy(lines));
        views = sg_call(e, "code_views", cva);
        if (sg_is_raise(views)) { prop = views; views = NULL; goto cleanup; }
    }
    {
        sg_buf jb; sg_buf_init(&jb, a);
        for (size_t i = 0; i < json_len(lines); i++) {
            if (i) sg_buf_addch(&jb, '\n');
            sg_buf_add(&jb, sg_sval(json_get(lines, i)));
        }
        json_t *ma = json_object(); json_set(ma, "text", json_string(jb.s));
        json_t *masked = sg_call(e, "mask_negated_forget", ma);
        if (sg_is_raise(masked)) { prop = masked; goto cleanup; }
        forget = sg_split_lines(e, sg_sval(masked));
        json_free(masked);
    }

    size_t npat = SG_N_THREAT + (skill ? SG_N_SKILL : 0);
    for (size_t pi = 0; pi < npat; pi++) {
        const sg_pattern *pat = pi < SG_N_THREAT ? &SG_THREAT_TABLE[pi] : &SG_SKILL_TABLE[pi - SG_N_THREAT];
        const char *pid = pat->pid;
        for (size_t i = 0; i < json_len(lines); i++) {
            const char *line = sg_sval(json_get(lines, i));
            double lineno = (double)(i + 1);
            const char *scan_line;
            if (strcmp(pid, "forget_rules") == 0) {
                scan_line = sg_sval(json_get(forget, i));
            } else if (SG_IN(SG_DOCSTRING_EXEMPT, pid) && sg_arr_has_num(docstring, lineno)) {
                if (strcmp(suffix, ".py") != 0) continue;
                if (views && sg_is_arr(views)) {
                    json_t *mask = json_get(json_get(views, 1), i);
                    sg_buf mb; sg_buf_init(&mb, a);
                    for (size_t j = 0; line[j]; j++) {
                        bool masked_ch = sg_is_arr(mask) && j < json_len(mask) && sg_numdef(json_get(mask, j), 0) != 0;
                        sg_buf_addch(&mb, masked_ch ? ' ' : line[j]);
                    }
                    scan_line = mb.s;
                } else scan_line = line;
            } else {
                if (SG_IN(SG_PATH_TRAVERSAL_IDS, pid)) scan_line = sg_sval(json_get(traversal, i));
                else if (views && sg_is_arr(views) && strcmp(pid, "hermes_env_dotenv") == 0)
                    scan_line = sg_sval(json_get(json_get(views, 0), i));
                else scan_line = line;
            }
            json_t *sa = json_object();
            json_set(sa, "idx", json_number((double)pi));
            json_set(sa, "pid", json_string(pid));
            json_set(sa, "line", json_string(scan_line));
            json_t *hit = sg_call(e, "re.search", sa);
            if (sg_is_raise(hit)) { prop = hit; goto cleanup; }
            bool matched = sg_truth(hit);
            json_free(hit);
            if (matched) {
                char *stripped = sg_strip(a, line);
                size_t slen = strlen(stripped);
                char *matchstr;
                if (slen <= 120) matchstr = stripped;
                else {
                    matchstr = (char *)sg_arena_alloc(a, 121);
                    memcpy(matchstr, stripped, 117);
                    memcpy(matchstr + 117, "...", 4);
                }
                double owner_idx = sg_numdef(json_get(owners, i), 0);
                const char *owner_line = sg_sval(json_get(lines, (size_t)owner_idx));
                json_t *dem = sg_demote(e, pid, pat->sev, pat->desc, line, owner_line, suffix);
                if (sg_is_raise(dem)) { prop = dem; goto cleanup; }
                const char *ls = sg_sval(json_get(dem, 0));
                const char *ld = sg_sval(json_get(dem, 1));
                json_append(findings, sg_finding(pid, ls, pat->cat, rel_path, lineno, matchstr, ld));
                json_free(dem);
            }
        }
    }
    if (skill) {
        json_t *in_text = (views && sg_is_arr(views)) ? json_get(views, 1) : NULL;
        json_t *ra = json_object();
        json_set(ra, "findings", json_copy(findings));
        json_set(ra, "lines", json_copy(lines));
        json_set(ra, "in_text", in_text ? json_copy(in_text) : json_null());
        json_t *rr = sg_call(e, "raise_provider_key_reads", ra);
        if (sg_is_raise(rr)) { prop = rr; goto cleanup; }
        if (sg_is_arr(rr)) { json_free(findings); findings = rr; }
        else json_free(rr);
    }
    for (size_t i = 0; i < json_len(lines); i++) {
        const char *line = sg_sval(json_get(lines, i));
        const sg_invisible *iv = NULL;
        for (size_t k = 0; k < SG_N_INVISIBLE; k++)
            if (strstr(line, SG_INVISIBLE[k].utf8)) { iv = &SG_INVISIBLE[k]; break; }
        if (iv) {
            char mb[64], db[160];
            snprintf(mb, sizeof(mb), "U+%04X (%s)", iv->cp, iv->name);
            snprintf(db, sizeof(db), "invisible unicode character %s (possible text hiding/injection)", iv->name);
            json_append(findings, sg_finding("invisible_unicode", "high", "injection", rel_path,
                                             (double)(i + 1), mb, db));
        }
    }
    if (!SG_IN(SG_PADDED_EXEMPT, suffix)) {
        json_t *pc = sg_padded_code_findings(e, lines, rel_path);
        if (sg_is_raise(pc)) { prop = pc; goto cleanup; }
        for (size_t i = 0; i < json_len(pc); i++) json_append(findings, json_copy(json_get(pc, i)));
        json_free(pc);
    }
    result = findings; findings = NULL;
cleanup:
    if (lines) json_free(lines);
    if (docstring) json_free(docstring);
    if (traversal) json_free(traversal);
    if (owners) json_free(owners);
    if (views) json_free(views);
    if (forget) json_free(forget);
    if (findings) json_free(findings);
    if (prop) return prop;
    return result;
}
static json_t *sg_scan_text_wrap(sg_env *e, const json_t *in) {
    return sg_scan_text(e, json_get_str(in, "text", ""), json_get_str(in, "rel_path", ""),
                        json_get_str(in, "suffix", ""), json_get_bool(in, "skill", false));
}

/* PoP: sg_unscanned_text_padded_code @ tools/skills_guard.py:_unscanned_text_padded_code */
static json_t *sg_unscanned(sg_env *e, const char *file_path, const char *rel_path) {
    sg_arena *a = e->arena;
    char *suf = sg_lower(a, sg_px_suffix(a, file_path));
    if (SG_IN(SG_PADDED_EXEMPT, suf)) return json_array();
    json_t *sta = json_object(); json_set(sta, "path", json_string(file_path));
    json_t *st = sg_call(e, "path.stat", sta);
    if (sg_is_raise(st)) return st;   /* stat OSError propagates (before the try) */
    double size = json_get_num(st, "st_size", 0);
    json_free(st);
    if (size > 2000000) return json_array();
    /* try: read_bytes / NUL / decode / padded ; except OSError -> [] */
    json_t *rba = json_object(); json_set(rba, "path", json_string(file_path));
    json_t *data = sg_call(e, "path.read_bytes", rba);
    if (sg_is_raise(data)) {
        if (sg_exc_is(sg_kind(data), "OSError")) { json_free(data); return json_array(); }
        return data;
    }
    bool binary = false;
    for (size_t i = 0; i < json_len(data) && i < 8192; i++)
        if (sg_numdef(json_get(data, i), -1) == 0) { binary = true; break; }
    if (binary) { json_free(data); return json_array(); }
    json_t *decoded = sg_decode(e, data, "utf-8-sig", "replace");
    json_free(data);
    if (sg_is_raise(decoded)) {
        if (sg_exc_is(sg_kind(decoded), "OSError")) { json_free(decoded); return json_array(); }
        return decoded;
    }
    json_t *lines = sg_split_lines(e, sg_sval(decoded));
    json_free(decoded);
    json_t *pc = sg_padded_code_findings(e, lines, rel_path);
    json_free(lines);
    if (sg_is_raise(pc)) {
        if (sg_exc_is(sg_kind(pc), "OSError")) { json_free(pc); return json_array(); }
        return pc;
    }
    return pc;
}
static json_t *sg_unscanned_wrap(sg_env *e, const json_t *in) {
    return sg_unscanned(e, json_get_str(in, "file_path", ""), json_get_str(in, "rel_path", ""));
}

/* PoP: sg_scan_file @ tools/skills_guard.py:scan_file */
static json_t *sg_scan_file(sg_env *e, const char *file_path, const char *rel_path, bool skill) {
    sg_arena *a = e->arena;
    char *name = sg_px_name(a, file_path);
    char *rp = (rel_path && rel_path[0]) ? sg_strdup(a, rel_path) : name;
    char *suffix = sg_lower(a, sg_px_suffix(a, file_path));
    if (!SG_IN(SG_SCANNABLE, suffix) && strcmp(name, "SKILL.md") != 0)
        return sg_unscanned(e, file_path, rp);
    json_t *rba = json_object(); json_set(rba, "path", json_string(file_path));
    json_t *raw = sg_call(e, "path.read_bytes", rba);
    if (sg_is_raise(raw)) {
        if (sg_exc_is(sg_kind(raw), "OSError")) { json_free(raw); return json_array(); }
        return raw;
    }
    json_t *dv = sg_decoded_views(e, raw, rp, suffix);
    json_free(raw);
    if (sg_is_raise(dv)) return dv;
    json_t *texts = json_get(dv, 0);
    json_t *findings = json_get(dv, 1);
    json_t *merged = sg_scan_text(e, sg_sval(json_get(texts, 0)), rp, suffix, skill);
    if (sg_is_raise(merged)) { json_free(dv); return merged; }
    /* dedup map keyed (pattern_id, line) */
    json_t *at = json_object();
    for (size_t n = 0; n < json_len(merged); n++) {
        json_t *f = json_get(merged, n);
        char key[256];
        snprintf(key, sizeof(key), "%s|%d", json_get_str(f, "pattern_id", ""), (int)json_get_num(f, "line", 0));
        if (!json_has(at, key)) json_set(at, key, json_number((double)n));
    }
    for (size_t t = 1; t < json_len(texts); t++) {
        json_t *extra = sg_scan_text(e, sg_sval(json_get(texts, t)), rp, suffix, skill);
        if (sg_is_raise(extra)) { json_free(at); json_free(merged); json_free(dv); return extra; }
        for (size_t k = 0; k < json_len(extra); k++) {
            json_t *f = json_get(extra, k);
            char key[256];
            snprintf(key, sizeof(key), "%s|%d", json_get_str(f, "pattern_id", ""), (int)json_get_num(f, "line", 0));
            if (!json_has(at, key)) {
                json_set(at, key, json_number((double)json_len(merged)));
                json_append(merged, json_copy(f));
            } else {
                size_t idx = (size_t)json_get_num(at, key, 0);
                json_t *old = json_get(merged, idx);
                if (sg_sev_order(json_get_str(f, "severity", "")) < sg_sev_order(json_get_str(old, "severity", ""))) {
                    json_free(merged->c.items[idx]);
                    merged->c.items[idx] = json_copy(f);
                }
            }
        }
        json_free(extra);
    }
    json_free(at);
    /* return findings + merged */
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(findings); i++) json_append(out, json_copy(json_get(findings, i)));
    for (size_t i = 0; i < json_len(merged); i++) json_append(out, json_copy(json_get(merged, i)));
    json_free(merged);
    json_free(dv);
    return out;
}
static json_t *sg_scan_file_wrap(sg_env *e, const json_t *in) {
    return sg_scan_file(e, json_get_str(in, "file_path", ""), json_get_str(in, "rel_path", ""),
                        json_get_bool(in, "skill", false));
}

/* PoP: sg_community_note @ tools/skills_guard.py:_community_note */
static json_t *sg_community_note(sg_env *e, json_t *f) {
    sg_arena *a = e->arena;
    json_t *out = json_copy(f);
    const char *sev = json_get_str(out, "severity", "");
    const char *pid = json_get_str(out, "pattern_id", "");
    char *file_lc = sg_lower(a, json_get_str(out, "file", ""));
    size_t fl = strlen(file_lc);
    bool md = fl >= 3 && strcmp(file_lc + fl - 3, ".md") == 0;
    if ((strcmp(sev, "critical") == 0 || strcmp(sev, "high") == 0) && SG_IN(SG_DOC_NOTE_IDS, pid) && md) {
        json_set(out, "severity", json_string("medium"));
        sg_buf b; sg_buf_init(&b, a);
        sg_buf_add(&b, json_get_str(out, "description", ""));
        sg_buf_add(&b, " (not executed on install; informational)");
        json_set(out, "description", json_string(b.s));
    }
    return out;
}
static json_t *sg_community_note_wrap(sg_env *e, const json_t *in) {
    return sg_community_note(e, json_obj_get(in, "f"));
}

/* PoP: sg_resolve_trust_level @ tools/skills_guard.py:_resolve_trust_level */
static json_t *sg_resolve_trust(sg_env *e, const char *source) {
    (void)e;
    const char *prefix = "";
    for (size_t i = 0; i < sizeof(SG_SOURCE_ALIASES) / sizeof(SG_SOURCE_ALIASES[0]); i++)
        if (strncmp(source, SG_SOURCE_ALIASES[i], strlen(SG_SOURCE_ALIASES[i])) == 0) {
            prefix = SG_SOURCE_ALIASES[i]; break;
        }
    const char *src = source + strlen(prefix);
    if (strcmp(src, "agent-created") == 0) return json_string("agent-created");
    if (strcmp(src, "official") == 0) return json_string("builtin");
    for (size_t i = 0; i < sizeof(SG_TRUSTED_REPOS) / sizeof(SG_TRUSTED_REPOS[0]); i++) {
        const char *t = SG_TRUSTED_REPOS[i];
        size_t tl = strlen(t);
        if (strcmp(src, t) == 0 || (strncmp(src, t, tl) == 0 && src[tl] == '/'))
            return json_string("trusted");
    }
    return json_string("community");
}
static json_t *sg_resolve_trust_wrap(sg_env *e, const json_t *in) {
    return sg_resolve_trust(e, json_get_str(in, "source", ""));
}

/* PoP: sg_should_allow_install @ tools/skills_guard.py:should_allow_install */
static json_t *sg_should_allow(sg_env *e, json_t *result, bool force) {
    (void)e;
    const char *trust = json_get_str(result, "trust_level", "");
    const char *verdict = json_get_str(result, "verdict", "");
    static const struct { const char *trust; const char *dec[3]; } POL[] = {
        {"builtin", {"allow", "allow", "allow"}},
        {"trusted", {"allow", "allow", "block"}},
        {"community", {"allow", "block", "block"}},
        {"agent-created", {"allow", "allow", "ask"}},
    };
    const char *const *policy = POL[2].dec;  /* community default */
    for (size_t i = 0; i < sizeof(POL) / sizeof(POL[0]); i++)
        if (strcmp(POL[i].trust, trust) == 0) { policy = POL[i].dec; break; }
    int vi = 2;
    if (strcmp(verdict, "safe") == 0) vi = 0;
    else if (strcmp(verdict, "caution") == 0) vi = 1;
    const char *decision = policy[vi];
    double n = (double)json_len(json_obj_get(result, "findings"));
    bool hard_block = (strcmp(verdict, "dangerous") == 0) &&
                      (strcmp(trust, "community") == 0 || strcmp(trust, "trusted") == 0);
    json_t *out = json_array();
    char buf[512];
    if (strcmp(decision, "allow") == 0) {
        json_append(out, json_bool(true));
        snprintf(buf, sizeof(buf), "Allowed (%s source, %s verdict)", trust, verdict);
        json_append(out, json_string(buf));
        return out;
    }
    if (force && !hard_block) {
        json_append(out, json_bool(true));
        snprintf(buf, sizeof(buf), "Force-installed despite %s verdict (%d findings)", verdict, (int)n);
        json_append(out, json_string(buf));
        return out;
    }
    if (strcmp(decision, "ask") == 0) {
        json_append(out, json_null());
        snprintf(buf, sizeof(buf), "Requires confirmation (%s source + %s verdict, %d findings)", trust, verdict, (int)n);
        json_append(out, json_string(buf));
        return out;
    }
    json_append(out, json_bool(false));
    snprintf(buf, sizeof(buf), "Blocked (%s source + %s verdict, %d findings). %s", trust, verdict, (int)n,
             hard_block ? "--force does not override a dangerous verdict." : "Use --force to override.");
    json_append(out, json_string(buf));
    return out;
}
static json_t *sg_should_allow_wrap(sg_env *e, const json_t *in) {
    return sg_should_allow(e, json_obj_get(in, "result"), json_get_bool(in, "force", false));
}

/* PoP: sg_build_summary @ tools/skills_guard.py:_build_summary */
static json_t *sg_build_summary(sg_env *e, const char *name, const char *source, const char *trust,
                                const char *verdict, json_t *findings) {
    sg_arena *a = e->arena;
    (void)source; (void)trust;
    char buf[1024];
    if (json_len(findings) == 0) {
        snprintf(buf, sizeof(buf), "%s: clean scan, no threats detected", name);
        return json_string(buf);
    }
    /* unique categories, sorted */
    const char *cats[256]; size_t nc = 0;
    for (size_t i = 0; i < json_len(findings); i++) {
        const char *c = json_get_str(json_get(findings, i), "category", "");
        bool found = false;
        for (size_t k = 0; k < nc; k++) if (strcmp(cats[k], c) == 0) { found = true; break; }
        if (!found && nc < 256) cats[nc++] = c;
    }
    for (size_t i = 1; i < nc; i++) {
        const char *v = cats[i]; size_t j = i;
        while (j > 0 && strcmp(cats[j - 1], v) > 0) { cats[j] = cats[j - 1]; j--; }
        cats[j] = v;
    }
    sg_buf jb; sg_buf_init(&jb, a);
    for (size_t i = 0; i < nc; i++) { if (i) sg_buf_add(&jb, ", "); sg_buf_add(&jb, cats[i]); }
    snprintf(buf, sizeof(buf), "%s: %s \xe2\x80\x94 %d finding(s) in %s", name, verdict, (int)json_len(findings), jb.s);
    return json_string(buf);
}
static json_t *sg_build_summary_wrap(sg_env *e, const json_t *in) {
    return sg_build_summary(e, json_get_str(in, "name", ""), json_get_str(in, "source", ""),
                            json_get_str(in, "trust", ""), json_get_str(in, "verdict", ""),
                            json_obj_get(in, "findings"));
}

/* PoP: sg_format_scan_report @ tools/skills_guard.py:format_scan_report */
static json_t *sg_format_scan_report(sg_env *e, json_t *result) {
    sg_arena *a = e->arena;
    sg_buf out; sg_buf_init(&out, a);
    char hdr[512];
    snprintf(hdr, sizeof(hdr), "Scan: %s (%s/%s)  Verdict: %s",
             json_get_str(result, "skill_name", ""), json_get_str(result, "source", ""),
             json_get_str(result, "trust_level", ""), sg_upper(a, json_get_str(result, "verdict", "")));
    sg_buf_add(&out, hdr);
    json_t *findings = json_obj_get(result, "findings");
    if (json_len(findings) > 0) {
        /* stable sort by severity order */
        json_t **sorted = (json_t **)sg_arena_alloc(a, json_len(findings) * sizeof(json_t *));
        for (size_t i = 0; i < json_len(findings); i++) sorted[i] = json_get(findings, i);
        for (size_t i = 1; i < json_len(findings); i++) {
            json_t *v = sorted[i]; size_t j = i;
            int vk = sg_sev_order(json_get_str(v, "severity", ""));
            while (j > 0 && sg_sev_order(json_get_str(sorted[j - 1], "severity", "")) > vk) {
                sorted[j] = sorted[j - 1]; j--;
            }
            sorted[j] = v;
        }
        for (size_t i = 0; i < json_len(findings); i++) {
            json_t *f = sorted[i];
            char *sev = sg_upper(a, json_get_str(f, "severity", ""));
            char *cat = sg_strdup(a, json_get_str(f, "category", ""));
            char loc[300];
            snprintf(loc, sizeof(loc), "%s:%d", json_get_str(f, "file", ""), (int)json_get_num(f, "line", 0));
            char *sevp = sg_ljust(a, sev, 8);
            char *catp = sg_ljust(a, cat, 14);
            char *locp = sg_ljust(a, loc, 30);
            char mbuf[61];
            snprintf(mbuf, sizeof(mbuf), "%.60s", json_get_str(f, "match", ""));
            sg_buf_add(&out, "\n  ");
            sg_buf_add(&out, sevp); sg_buf_addch(&out, ' ');
            sg_buf_add(&out, catp); sg_buf_addch(&out, ' ');
            sg_buf_add(&out, locp); sg_buf_add(&out, " \"");
            sg_buf_add(&out, mbuf); sg_buf_addch(&out, '"');
        }
        sg_buf_add(&out, "\n");
    }
    json_t *allow = sg_should_allow(e, result, false);
    if (sg_is_raise(allow)) return allow;
    json_t *allowed = json_get(allow, 0);
    const char *reason = sg_sval(json_get(allow, 1));
    const char *status;
    if (allowed && allowed->type == JSON_BOOL && allowed->bool_val) status = "ALLOWED";
    else if (sg_is_null(allowed)) status = "NEEDS CONFIRMATION";
    else status = "BLOCKED";
    json_free(allow);
    sg_buf_add(&out, "\nDecision: ");
    sg_buf_add(&out, status);
    sg_buf_add(&out, " \xe2\x80\x94 ");
    sg_buf_add(&out, reason);
    return json_string(out.s);
}
static json_t *sg_format_scan_report_wrap(sg_env *e, const json_t *in) {
    return sg_format_scan_report(e, json_obj_get(in, "result"));
}

/* PoP: sg_check_structure @ tools/skills_guard.py:_check_structure */
static json_t *sg_check_structure(sg_env *e, const char *skill_dir) {
    sg_arena *a = e->arena;
    json_t *findings = json_array();
    json_t *prop = NULL;
    double file_count = 0, total_size = 0;
    json_t *rga = json_object();
    json_set(rga, "path", json_string(skill_dir));
    json_set(rga, "pattern", json_string("*"));
    json_t *entries = sg_call(e, "path.rglob", rga);
    if (sg_is_raise(entries)) { json_free(findings); return entries; }
    for (size_t i = 0; i < json_len(entries); i++) {
        const char *f = sg_sval(json_get(entries, i));
        json_t *err = NULL;
        char *rel = sg_px_relative_to(a, f, skill_dir, &err);
        if (err) { prop = err; goto cleanup; }
        json_t *ifa = json_object(); json_set(ifa, "path", json_string(f));
        json_t *isf = sg_call(e, "path.is_file", ifa);
        if (sg_is_raise(isf)) { prop = isf; goto cleanup; }
        json_t *isa = json_object(); json_set(isa, "path", json_string(f));
        json_t *iss = sg_call(e, "path.is_symlink", isa);
        if (sg_is_raise(iss)) { json_free(isf); prop = iss; goto cleanup; }
        bool is_file = sg_truth(isf), is_sym = sg_truth(iss);
        json_free(isf); json_free(iss);
        if (!(is_file || is_sym)) continue;
        file_count += 1;
        if (is_sym) {
            json_t *ra = json_object(); json_set(ra, "path", json_string(f));
            json_t *resolved = sg_call(e, "path.resolve", ra);
            bool broken = false;
            if (sg_is_raise(resolved)) {
                if (sg_exc_is(sg_kind(resolved), "OSError")) { json_free(resolved); broken = true; }
                else { prop = resolved; goto cleanup; }
            }
            if (!broken) {
                json_t *ba = json_object(); json_set(ba, "path", json_string(skill_dir));
                json_t *base = sg_call(e, "path.resolve", ba);
                if (sg_is_raise(base)) {
                    if (sg_exc_is(sg_kind(base), "OSError")) { json_free(base); json_free(resolved); broken = true; }
                    else { json_free(resolved); prop = base; goto cleanup; }
                }
                if (!broken) {
                    if (!sg_px_is_relative_to(a, sg_sval(resolved), sg_sval(base))) {
                        char mb[512];
                        snprintf(mb, sizeof(mb), "symlink -> %s", sg_sval(resolved));
                        json_append(findings, sg_finding("symlink_escape", "critical", "traversal", rel, 0, mb,
                                                         "symlink points outside the skill directory"));
                    }
                    json_free(base);
                }
                if (!broken) json_free(resolved);
            }
            if (broken)
                json_append(findings, sg_finding("broken_symlink", "medium", "traversal", rel, 0,
                                                 "broken symlink", "broken or circular symlink"));
            continue;
        }
        json_t *sta = json_object(); json_set(sta, "path", json_string(f));
        json_t *st = sg_call(e, "path.stat", sta);
        if (sg_is_raise(st)) {
            if (sg_exc_is(sg_kind(st), "OSError")) { json_free(st); continue; }
            prop = st; goto cleanup;
        }
        double size = json_get_num(st, "st_size", 0);
        int mode = (int)json_get_num(st, "st_mode", 0);
        json_free(st);
        total_size += size;
        if (size > SG_MAX_SINGLE_FILE_KB * 1024) {
            char mb[64], db[128];
            snprintf(mb, sizeof(mb), "%dKB", (int)(size / 1024));
            snprintf(db, sizeof(db), "file is %dKB (limit: %dKB)", (int)(size / 1024), SG_MAX_SINGLE_FILE_KB);
            json_append(findings, sg_finding("oversized_file", "medium", "structural", rel, 0, mb, db));
        }
        char *ext = sg_lower(a, sg_px_suffix(a, f));
        if (SG_IN(SG_SUSPICIOUS_BIN, ext)) {
            char mb[64], db[128];
            snprintf(mb, sizeof(mb), "binary: %s", ext);
            snprintf(db, sizeof(db), "binary/executable file (%s) should not be in a skill", ext);
            json_append(findings, sg_finding("binary_file", "critical", "structural", rel, 0, mb, db));
        }
        if (!SG_IN(SG_SCRIPT_EXT, ext) && (mode & 0111))
            json_append(findings, sg_finding("unexpected_executable", "medium", "structural", rel, 0,
                                             "executable bit set",
                                             "file has executable permission but is not a recognized script type"));
    }
    if (file_count > SG_MAX_FILE_COUNT) {
        char mb[64], db[128];
        snprintf(mb, sizeof(mb), "%d files", (int)file_count);
        snprintf(db, sizeof(db), "skill has %d files (limit: %d)", (int)file_count, SG_MAX_FILE_COUNT);
        json_append(findings, sg_finding("too_many_files", "medium", "structural", "(directory)", 0, mb, db));
    }
    if (total_size > SG_MAX_TOTAL_SIZE_KB * 1024) {
        char mb[64], db[128];
        snprintf(mb, sizeof(mb), "%dKB total", (int)(total_size / 1024));
        snprintf(db, sizeof(db), "skill is %dKB total (limit: %dKB)", (int)(total_size / 1024), SG_MAX_TOTAL_SIZE_KB);
        json_append(findings, sg_finding("oversized_skill", "low", "structural", "(directory)", 0, mb, db));
    }
cleanup:
    json_free(entries);
    if (prop) { json_free(findings); return prop; }
    return findings;
}
static json_t *sg_check_structure_wrap(sg_env *e, const json_t *in) {
    return sg_check_structure(e, json_get_str(in, "skill_dir", ""));
}

/* PoP: sg_scan_skill @ tools/skills_guard.py:scan_skill */
static json_t *sg_scan_skill(sg_env *e, const char *skill_path, const char *source,
                             const char *trust_level, json_t *files) {
    sg_arena *a = e->arena;
    char *name = sg_px_name(a, skill_path);
    json_t *trust_node = NULL;
    const char *trust;
    if (trust_level && trust_level[0]) trust = trust_level;
    else {
        trust_node = sg_resolve_trust(e, source);
        if (sg_is_raise(trust_node)) return trust_node;
        trust = sg_sval(trust_node);
    }
    json_t *findings = json_array();
    json_t *prop = NULL;
    json_t *isd_a = json_object(); json_set(isd_a, "path", json_string(skill_path));
    json_t *isd = sg_call(e, "path.is_dir", isd_a);
    if (sg_is_raise(isd)) { prop = isd; goto cleanup; }
    bool is_dir = sg_truth(isd); json_free(isd);
    if (is_dir) {
        json_t *cs = sg_check_structure(e, skill_path);
        if (sg_is_raise(cs)) { prop = cs; goto cleanup; }
        for (size_t i = 0; i < json_len(cs); i++) json_append(findings, json_copy(json_get(cs, i)));
        json_free(cs);
        json_t *entries;
        bool entries_owned = false;
        if (files && sg_is_arr(files)) entries = files;
        else {
            json_t *rga = json_object();
            json_set(rga, "path", json_string(skill_path));
            json_set(rga, "pattern", json_string("*"));
            entries = sg_call(e, "path.rglob", rga);
            entries_owned = true;
            if (sg_is_raise(entries)) { prop = entries; goto cleanup; }
        }
        for (size_t i = 0; i < json_len(entries); i++) {
            const char *f = sg_sval(json_get(entries, i));
            json_t *ifa = json_object(); json_set(ifa, "path", json_string(f));
            json_t *isf = sg_call(e, "path.is_file", ifa);
            if (sg_is_raise(isf)) { prop = isf; if (entries_owned) json_free(entries); goto cleanup; }
            bool is_file = sg_truth(isf); json_free(isf);
            if (is_file) {
                json_t *err = NULL;
                char *rel = sg_px_relative_to(a, f, skill_path, &err);
                if (err) { prop = err; if (entries_owned) json_free(entries); goto cleanup; }
                json_t *sf = sg_scan_file(e, f, rel, true);
                if (sg_is_raise(sf)) { prop = sf; if (entries_owned) json_free(entries); goto cleanup; }
                for (size_t k = 0; k < json_len(sf); k++) json_append(findings, json_copy(json_get(sf, k)));
                json_free(sf);
            }
        }
        if (entries_owned) json_free(entries);
    } else {
        json_t *ifa = json_object(); json_set(ifa, "path", json_string(skill_path));
        json_t *isf = sg_call(e, "path.is_file", ifa);
        if (sg_is_raise(isf)) { prop = isf; goto cleanup; }
        bool is_file = sg_truth(isf); json_free(isf);
        if (is_file) {
            json_t *sf = sg_scan_file(e, skill_path, name, true);
            if (sg_is_raise(sf)) { prop = sf; goto cleanup; }
            for (size_t k = 0; k < json_len(sf); k++) json_append(findings, json_copy(json_get(sf, k)));
            json_free(sf);
        }
    }
    if (strcmp(trust, "community") == 0) {
        for (size_t i = 0; i < json_len(findings); i++) {
            json_t *noted = sg_community_note(e, json_get(findings, i));
            json_free(findings->c.items[i]);
            findings->c.items[i] = noted;
        }
    }
    json_t *dva = json_object(); json_set(dva, "findings", json_copy(findings));
    json_t *verdict_node = sg_call(e, "determine_verdict", dva);
    if (sg_is_raise(verdict_node)) { prop = verdict_node; goto cleanup; }
    const char *verdict = sg_sval(verdict_node);
    json_t *now = sg_seam0(e, "datetime.now");
    if (sg_is_raise(now)) { json_free(verdict_node); prop = now; goto cleanup; }
    const char *scanned_at = sg_sval(now);
    json_t *summary = sg_build_summary(e, name, source, trust, verdict, findings);
    json_t *out = json_object();
    json_set(out, "skill_name", json_string(name));
    json_set(out, "source", json_string(source));
    json_set(out, "trust_level", json_string(trust));
    json_set(out, "verdict", json_string(verdict));
    json_set(out, "findings", findings); findings = NULL;
    json_set(out, "scanned_at", json_string(scanned_at));
    json_set(out, "summary", summary);
    json_set(out, "scan_provenance", json_object());
    json_free(verdict_node); json_free(now);
    if (trust_node) json_free(trust_node);
    return out;
cleanup:
    if (findings) json_free(findings);
    if (trust_node) json_free(trust_node);
    return prop;
}
static json_t *sg_scan_skill_wrap(sg_env *e, const json_t *in) {
    const char *tl = json_get_str(in, "trust_level", NULL);
    return sg_scan_skill(e, json_get_str(in, "skill_path", ""), json_get_str(in, "source", "community"),
                         tl, json_obj_get(in, "files"));
}

/* PoP: sg_content_hash @ tools/skills_guard.py:content_hash */
static json_t *sg_content_hash(sg_env *e, const char *skill_path) {
    json_t *ca = json_object(); json_set(ca, "path", json_string(skill_path));
    json_t *digest = sg_call(e, "content_digest", ca);
    if (sg_is_raise(digest)) return digest;
    const char *d = sg_sval(digest);
    char buf[64];
    snprintf(buf, sizeof(buf), "sha256:%.16s", d);
    json_free(digest);
    return json_string(buf);
}
static json_t *sg_content_hash_wrap(sg_env *e, const json_t *in) {
    return sg_content_hash(e, json_get_str(in, "skill_path", ""));
}

/* ── dispatch table ───────────────────────────────────────────────────────── */
typedef struct { const char *name; json_t *(*fn)(sg_env *, const json_t *); } sg_dispatch;
static const sg_dispatch sg_table[] = {
    {"_line_at", sg_line_at_wrap},
    {"_statement_owners", sg_statement_owners_wrap},
    {"_demote_inert_path_reference", sg_demote_wrap},
    {"_unicode_char_name", sg_unicode_char_name_wrap},
    {"_mask_markdown_link_destinations", sg_mask_markdown_wrap},
    {"_mask_prose_link_destinations", sg_mask_prose_wrap},
    {"_padded_code_findings", sg_padded_code_findings_wrap},
    {"_compute_docstring_lines", sg_compute_docstring_lines_wrap},
    {"_decoded_views", sg_decoded_views_wrap},
    {"_scan_text", sg_scan_text_wrap},
    {"_unscanned_text_padded_code", sg_unscanned_wrap},
    {"scan_file", sg_scan_file_wrap},
    {"_community_note", sg_community_note_wrap},
    {"_resolve_trust_level", sg_resolve_trust_wrap},
    {"should_allow_install", sg_should_allow_wrap},
    {"format_scan_report", sg_format_scan_report_wrap},
    {"_build_summary", sg_build_summary_wrap},
    {"_check_structure", sg_check_structure_wrap},
    {"scan_skill", sg_scan_skill_wrap},
    {"content_hash", sg_content_hash_wrap},
};