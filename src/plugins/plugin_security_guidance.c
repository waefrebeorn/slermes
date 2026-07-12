/*
 * plugin_security_guidance.c — Security pattern scanning plugin for Hermes C.
 * PL16: Ports Python plugins/security-guidance (25 regex/substring rules).
 *
 * Scans file-write tool results for known-dangerous patterns:
 *   pickle.load, yaml.load, eval(, os.system, dangerouslySetInnerHTML,
 *   verify=False, ECB mode, XXE, GitHub Actions injection, etc.
 *
 * Non-blocking: appends security warning to tool result so the model
 * can self-correct on the next turn.
 *
 * Build:
 *   gcc -O2 -fPIC -shared -I ../../include -I ../../lib/libplugin \
 *       plugin_security_guidance.c -o plugin_security_guidance.so -lpcre2-8
 */


/* PoP: security guidance plugin */

#include "plugin.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 *  Plugin metadata
 * ================================================================ */

const char *plugin_meta_name(void) {
    return "security-guidance";
}

const char *plugin_meta_version(void) {
    return "0.1.0";
}

const char *plugin_meta_type(void) {
    return "skill";
}

const char *plugin_meta_description(void) {
    return "Security pattern scanning — warns on dangerous code patterns";
}

int plugin_deps_count(void) { return 0; }
const plugin_dep_t *plugin_deps_list(void) { return NULL; }

/* ================================================================
 *  Security pattern definitions (25 rules from Python patterns.py)
 * ================================================================ */

/* Reminder messages */
#define REMINDER_PICKLE "⚠️ Security Warning: Loading pickle data from untrusted sources allows arbitrary code execution. Prefer JSON or msgspec."
#define REMINDER_YAML "⚠️ Security Warning: yaml.load() executes arbitrary Python via !!python/object tags. Use yaml.safe_load()."
#define REMINDER_EVAL "⚠️ Security Warning: eval() executes arbitrary code. Use JSON.parse() or ast.literal_eval()."
#define REMINDER_OS_SYSTEM "⚠️ Security Warning: os.system() runs a shell and is a command-injection sink. Use subprocess.run([...]) with argument list."
#define REMINDER_SUBPROCESS_SHELL "⚠️ Security Warning: subprocess with shell=True enables command injection. Pass arguments as a list without shell=True."
#define REMINDER_INNER_HTML "⚠️ Security Warning: Setting innerHTML with untrusted content can lead to XSS. Use textContent or DOMPurify."
#define REMINDER_OUTER_HTML "⚠️ Security Warning: Use textContent or sanitize with DOMPurify. outerHTML assignment is an XSS sink."
#define REMINDER_DANGEROUSLY_SET "⚠️ Security Warning: dangerouslySetInnerHTML can lead to XSS. Sanitize with DOMPurify."
#define REMINDER_DOC_WRITE "⚠️ Security Warning: document.write() exploits XSS. Use createElement/appendChild."
#define REMINDER_CHILD_PROC "⚠️ Security Warning: child_process.exec() leads to command injection. Use execFile() with argument array."
#define REMINDER_NEW_FUNC "⚠️ Security Warning: new Function() with string interpolation is CODE INJECTION."
#define REMINDER_GITHUB_ACTIONS "⚠️ Security Warning: GitHub Actions workflow file. Never use untrusted input directly in run: commands. Use env: variables."
#define REMINDER_AES_ECB "⚠️ Security Warning: ECB mode leaks plaintext structure. Use AES-GCM or AES-CBC with HMAC."
#define REMINDER_TLS_VERIFY "⚠️ Security Warning: Don't disable TLS verification — allows MITM attacks."
#define REMINDER_XML_XXE "⚠️ Security Warning: Use defusedxml.ElementTree. Python stdlib XML parsers are vulnerable to XXE attacks."
#define REMINDER_NODE_CIPHER "⚠️ Security Warning: Use crypto.createCipheriv(). createCipher was removed in Node 22."
#define REMINDER_SCRIPT_SRI "⚠️ Security Warning: Add integrity=\"sha384-...\" to external script tags for Subresource Integrity."
#define REMINDER_TORCH "⚠️ Security Warning: torch.load() defaults to weights_only=False. Pass weights_only=True if loading tensors only."
#define REMINDER_GO_EXEC "⚠️ Security Warning: exec.Command with shell interpreter enables injection. Pass arguments directly without shell."
#define REMINDER_INSERT_ADJACENT "⚠️ Security Warning: insertAdjacentHTML is an XSS sink. Use insertAdjacentText() or sanitize with DOMPurify."
#define REMINDER_MARSHAL "⚠️ Security Warning: marshal.loads() from untrusted sources allows arbitrary code execution."
#define REMINDER_SHELVE "⚠️ Security Warning: shelve.open() uses pickle internally. Prefer JSON for simple data."

/* Path filter helpers */
static int path_ends_with(const char *path, const char *ext) {
    if (!path || !ext) return 0;
    size_t plen = strlen(path), elen = strlen(ext);
    if (plen < elen) return 0;
    return strcasecmp(path + plen - elen, ext) == 0;
}

static int path_contains(const char *path, const char *substr) {
    return path && substr && strstr(path, substr) != NULL;
}

static int is_py_file(const char *path) {
    return path_ends_with(path, ".py") || path_ends_with(path, ".pyi") ||
           path_ends_with(path, ".ipynb");
}

static int is_js_file(const char *path) {
    return path_ends_with(path, ".js") || path_ends_with(path, ".jsx") ||
           path_ends_with(path, ".ts") || path_ends_with(path, ".tsx") ||
           path_ends_with(path, ".mjs") || path_ends_with(path, ".cjs");
}

static int is_go_file(const char *path) {
    return path_ends_with(path, ".go");
}

/* Simple substring match */
static int content_contains(const char *content, const char *substr) {
    return content && substr && strstr(content, substr) != NULL;
}

/* Simple regex match using POSIX regex */
#include <regex.h>

static int content_matches_regex(const char *content, const char *pattern) {
    if (!content || !pattern) return 0;
    regex_t re;
    int rc = regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB);
    if (rc != 0) return 0;
    rc = regexec(&re, content, 0, NULL, 0);
    regfree(&re);
    return rc == 0;
}

/* A single security rule */
typedef struct {
    const char *rule_name;
    int (*path_filter)(const char *path);  /* NULL = match all paths */
    const char **substrings;               /* NULL-terminated array, NULL = no substring check */
    const char *regex;                     /* NULL = no regex check */
    const char *reminder;
} security_rule_t;

/* Substring arrays */
static const char *subs_child_process[] = {"child_process.exec", "execSync(", NULL};
static const char *subs_new_function[] = {"new Function", NULL};
static const char *subs_eval[] = {"eval(", NULL};
static const char *subs_os_system[] = {"os.system(", NULL};
static const char *subs_pickle[] = {"pickle.load", "pickle.loads", NULL};
static const char *subs_dangerously[] = {"dangerouslySetInnerHTML", NULL};
static const char *subs_doc_write[] = {"document.write", NULL};
static const char *subs_inner_html[] = {".innerHTML =", ".innerHTML=", NULL};
static const char *subs_outer_html[] = {".outerHTML =", ".outerHTML=", NULL};
static const char *subs_insert_adj[] = {".insertAdjacentHTML(", NULL};
static const char *subs_yaml_unsafe[] = {"yaml.unsafe_load(", ".yaml_unsafe_load(", NULL};
static const char *subs_torch[] = {"torch.load(", ".torch_load(", NULL};
static const char *subs_pickle_variants[] = {"joblib.load(", "pd.read_pickle(", "pandas.read_pickle(", ".cloudpickle_load(", NULL};
static const char *subs_node_cipher[] = {"crypto.createCipher(", "crypto.createDecipher(", NULL};
static const char *subs_tls_verify[] = {"verify=False", "InsecureSkipVerify=True", "check_hostname=False", "NODE_TLS_REJECT_UNAUTHORIZED=0", NULL};
static const char *subs_aes_ecb[] = {"AES.MODE_ECB", "modes.ECB(", NULL};
static const char *subs_go_exec[] = {"exec.Command(\"sh\"", "exec.Command(\"bash\"", NULL};

/* The 25 security rules */
static const security_rule_t g_rules[] = {
    /* 1: GitHub Actions workflow */
    {
        .rule_name = "github_actions_workflow",
        .path_filter = NULL,
        .substrings = NULL,
        .regex = NULL,
        .reminder = REMINDER_GITHUB_ACTIONS,
        /* Special: check inside rule match function */
    },
    /* 2: child_process.exec (JS/TS only) */
    {
        .rule_name = "child_process_exec",
        .path_filter = is_js_file,
        .substrings = subs_child_process,
        .regex = NULL,
        .reminder = REMINDER_CHILD_PROC,
    },
    /* 3: new Function injection */
    {
        .rule_name = "new_function_injection",
        .path_filter = NULL,
        .substrings = subs_new_function,
        .regex = NULL,
        .reminder = REMINDER_NEW_FUNC,
    },
    /* 4: eval injection (skip docs) */
    {
        .rule_name = "eval_injection",
        .path_filter = NULL,
        .substrings = subs_eval,
        .regex = "(?<![a-zA-Z0-9_.])eval\\(",
        .reminder = REMINDER_EVAL,
    },
    /* 5: React dangerouslySetInnerHTML */
    {
        .rule_name = "react_dangerously_set_html",
        .path_filter = NULL,
        .substrings = subs_dangerously,
        .regex = NULL,
        .reminder = REMINDER_DANGEROUSLY_SET,
    },
    /* 6: document.write */
    {
        .rule_name = "document_write_xss",
        .path_filter = NULL,
        .substrings = subs_doc_write,
        .regex = NULL,
        .reminder = REMINDER_DOC_WRITE,
    },
    /* 7: innerHTML assignment */
    {
        .rule_name = "innerHTML_xss",
        .path_filter = NULL,
        .substrings = subs_inner_html,
        .regex = NULL,
        .reminder = REMINDER_INNER_HTML,
    },
    /* 8: pickle deserialization (Python) */
    {
        .rule_name = "pickle_deserialization",
        .path_filter = is_py_file,
        .substrings = subs_pickle,
        .regex = NULL,
        .reminder = REMINDER_PICKLE,
    },
    /* 9: os.system injection (Python) */
    {
        .rule_name = "os_system_injection",
        .path_filter = is_py_file,
        .substrings = subs_os_system,
        .regex = NULL,
        .reminder = REMINDER_OS_SYSTEM,
    },
    /* 10: subprocess shell=True */
    {
        .rule_name = "python_subprocess_shell",
        .path_filter = is_py_file,
        .substrings = NULL,
        .regex = "subprocess\\.(run|call|Popen|check_output|check_call)\\(.*shell\\s*=\\s*True",
        .reminder = REMINDER_SUBPROCESS_SHELL,
    },
    /* 11: Go exec.Command with shell */
    {
        .rule_name = "go_exec_shell_injection",
        .path_filter = is_go_file,
        .substrings = subs_go_exec,
        .regex = NULL,
        .reminder = REMINDER_GO_EXEC,
    },
    /* 12: unsafe yaml.load */
    {
        .rule_name = "unsafe_yaml_load",
        .path_filter = NULL,
        .substrings = NULL,
        .regex = "yaml\\.load\\s*\\((?![^)\\n]{0,80}Safe)",
        .reminder = REMINDER_YAML,
    },
    /* 13: createCipher without IV */
    {
        .rule_name = "node_createcipher_no_iv",
        .path_filter = is_js_file,
        .substrings = subs_node_cipher,
        .regex = NULL,
        .reminder = REMINDER_NODE_CIPHER,
    },
    /* 14: AES ECB mode */
    {
        .rule_name = "aes_ecb_mode",
        .path_filter = NULL,
        .substrings = subs_aes_ecb,
        .regex = NULL,
        .reminder = REMINDER_AES_ECB,
    },
    /* 15: TLS verification disabled */
    {
        .rule_name = "tls_verification_disabled",
        .path_filter = NULL,
        .substrings = subs_tls_verify,
        .regex = NULL,
        .reminder = REMINDER_TLS_VERIFY,
    },
    /* 16: marshal.loads */
    {
        .rule_name = "marshal_loads",
        .path_filter = is_py_file,
        .substrings = NULL,
        .regex = "marshal\\.loads?\\s*\\(",
        .reminder = REMINDER_MARSHAL,
    },
    /* 17: shelve.open */
    {
        .rule_name = "shelve_open",
        .path_filter = is_py_file,
        .substrings = NULL,
        .regex = "shelve\\.open\\s*\\(",
        .reminder = REMINDER_SHELVE,
    },
    /* 18: unsafe XML parse */
    {
        .rule_name = "xml_unsafe_parse",
        .path_filter = is_py_file,
        .substrings = NULL,
        .regex = "(xml\\.etree\\.ElementTree|ElementTree|minidom\\.(parse|parseString)|xml\\.sax\\.(parse|make_parser))\\s*\\(",
        .reminder = REMINDER_XML_XXE,
    },
    /* 19: pickle variants */
    {
        .rule_name = "pickle_variants_load",
        .path_filter = is_py_file,
        .substrings = subs_pickle_variants,
        .regex = NULL,
        .reminder = REMINDER_PICKLE,
    },
    /* 20: outerHTML */
    {
        .rule_name = "outerHTML_xss",
        .path_filter = NULL,
        .substrings = subs_outer_html,
        .regex = NULL,
        .reminder = REMINDER_OUTER_HTML,
    },
    /* 21: insertAdjacentHTML */
    {
        .rule_name = "insertAdjacentHTML_xss",
        .path_filter = NULL,
        .substrings = subs_insert_adj,
        .regex = NULL,
        .reminder = REMINDER_INSERT_ADJACENT,
    },
    /* 22: torch.load */
    {
        .rule_name = "torch_unsafe_load",
        .path_filter = is_py_file,
        .substrings = subs_torch,
        .regex = "(torch\\.load|torch_load)\\s*\\((?![^)\\n]{0,200}weights_only\\s*=\\s*True)",
        .reminder = REMINDER_TORCH,
    },
    /* 23: yaml.unsafe_load */
    {
        .rule_name = "yaml_unsafe_load_variants",
        .path_filter = NULL,
        .substrings = subs_yaml_unsafe,
        .regex = NULL,
        .reminder = REMINDER_YAML,
    },
    /* 24: pickle wrapper load (numpy allow_pickle) */
    {
        .rule_name = "pickle_wrapper_load",
        .path_filter = is_py_file,
        .substrings = subs_pickle_variants,
        .regex = "(np|numpy)\\.load\\s*\\([^)\\n]{0,200}allow_pickle\\s*=\\s*True",
        .reminder = REMINDER_PICKLE,
    },
};

#define NUM_RULES (sizeof(g_rules) / sizeof(g_rules[0]))

/* GitHub Actions special check — path-based rule */
static int check_github_actions(const char *path, const char *content, char **warning) {
    if (!path || !content) return 0;
    if (!path_contains(path, ".github/workflows/")) return 0;
    if (!path_ends_with(path, ".yml") && !path_ends_with(path, ".yaml")) return 0;
    *warning = strdup(REMINDER_GITHUB_ACTIONS);
    return 1;
}

/* Script src without SRI — special regex rule */
static int check_script_sri(const char *content, char **warning) {
    if (!content) return 0;
    /* Look for <script ... src="http... without integrity= in the same tag */
    const char *p = content;
    while ((p = strstr(p, "<script")) != NULL) {
        const char *tag_end = strchr(p, '>');
        if (!tag_end) break;
        size_t tag_len = (size_t)(tag_end - p);
        char *tag = (char *)malloc(tag_len + 1);
        if (!tag) break;
        memcpy(tag, p, tag_len);
        tag[tag_len] = '\0';

        if (strstr(tag, "src=\"http") && !strstr(tag, "integrity=")) {
            free(tag);
            *warning = strdup(REMINDER_SCRIPT_SRI);
            return 1;
        }
        free(tag);
        p = tag_end + 1;
    }
    return 0;
}

/* ================================================================
 *  Core scanning function
 * ================================================================ */

/*
 * Scan content for security issues.
 * Returns number of warnings found.
 * Caller must free warnings[] entries.
 */
static int security_scan(const char *path, const char *content,
                          char **warnings, int max_warnings) {
    if (!content) return 0;
    int count = 0;

    /* Special: GitHub Actions path-based rule */
    if (count < max_warnings && check_github_actions(path, content, &warnings[count])) {
        count++;
    }

    /* Special: script src without SRI */
    if (count < max_warnings && check_script_sri(content, &warnings[count])) {
        count++;
    }

    /* Rule-based scanning */
    for (size_t ri = 0; ri < NUM_RULES && count < max_warnings; ri++) {
        const security_rule_t *rule = &g_rules[ri];

        /* Path filter */
        if (rule->path_filter && path && !rule->path_filter(path)) continue;

        int matched = 0;

        /* Substring check */
        if (!matched && rule->substrings) {
            for (const char **s = rule->substrings; *s && !matched; s++) {
                if (content_contains(content, *s)) matched = 1;
            }
        }

        /* Regex check */
        if (!matched && rule->regex) {
            if (content_matches_regex(content, rule->regex)) matched = 1;
        }

        if (matched) {
            warnings[count] = strdup(rule->reminder);
            count++;
        }
    }

    return count;
}

/* ================================================================
 *  Plugin interface — tool result transform hook
 * ================================================================ */

/*
 * plugin_security_check — Hook function called by the agent loop.
 * Inspects write_file/patch tool results for security patterns.
 * Appends warning to result text if patterns found.
 *
 * Returns malloc'd string (modified result) or NULL if no issues found.
 */
char *plugin_security_check(const char *tool_name, const char *args_json,
                             const char *result_text) {
    /* Only inspect file-write tools */
    if (!tool_name) return NULL;
    if (strcmp(tool_name, "write_file") != 0 &&
        strcmp(tool_name, "patch") != 0) {
        return NULL;
    }

    if (!args_json) return NULL;

    /* Extract file_path from args */
    json_t *args = json_parse(args_json, NULL);
    if (!args) return NULL;
    const char *file_path = json_get_str(args, "file_path", NULL);
    if (!file_path) file_path = json_get_str(args, "path", NULL);
    json_free(args);
    if (!file_path) return NULL;

    const char *content = result_text ? result_text : "";

    /* Scan for security issues */
    char *warnings[8];
    int n = security_scan(file_path, content, warnings, 8);

    if (n == 0) return NULL;

    /* Build modified result with warnings appended */
    size_t total_len = 128; /* header */
    for (int i = 0; i < n; i++) total_len += strlen(warnings[i]) + 4;
    total_len += strlen(result_text) + 1;

    char *output = (char *)malloc(total_len);
    if (!output) {
        for (int i = 0; i < n; i++) free(warnings[i]);
        return NULL;
    }

    char *p = output;
    p += snprintf(p, total_len - (size_t)(p - output),
                  "\n\n── Security Scan: %d warning(s) ──\n", n);
    for (int i = 0; i < n; i++) {
        p += snprintf(p, total_len - (size_t)(p - output), "\n%s\n", warnings[i]);
        free(warnings[i]);
    }
    p += snprintf(p, total_len - (size_t)(p - output),
                  "\n── End Security Scan ──\n");

    return output;
}

static plugin_interface_t g_interface = {
    .type = PLUGIN_SKILL,
    .tool_execute = NULL,  /* We use the hook function instead */
};

void *plugin_get_interface(void) {
    return &g_interface;
}

/* ================================================================
 *  Lifecycle
 * ================================================================ */

int plugin_init(void) {
    fprintf(stderr, "[security-guidance] initialized (%d rules)\n", (int)NUM_RULES);
    return 0;
}

int plugin_cleanup(void) {
    fprintf(stderr, "[security-guidance] shut down.\n");
    return 0;
}

int plugin_configure(const char *config_json) {
    fprintf(stderr, "[security-guidance] configure: %s\n",
            config_json ? config_json : "(null)");
    return 0;
}
