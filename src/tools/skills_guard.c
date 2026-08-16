/*
 * skills_guard.c — Security scanner for skill files.
 * Port of Python tools/skills_guard.py.
 *
 * Scans skill directories/files for dangerous patterns before installation.
 * Uses regex-based static analysis to detect:
 *   - Data exfiltration (curl/wget with secrets, credential store access)
 *   - Prompt injection (ignore instructions, role hijacking, jailbreaks)
 *   - Destructive operations (rm -rf /, mkfs, dd)
 *   - Persistence (crontab, authorized_keys, systemd)
 *   - Reverse shells (nc, socat, /dev/tcp)
 *   - Obfuscation (base64 decode pipes, eval, exec)
 *   - Supply chain (curl|sh, unpinned deps)
 *   - Privilege escalation (sudo, setuid, NOPASSWD)
 *   - Hardcoded secrets (API keys, private keys, tokens)
 *
 * Returns scan results with findings, verdict (safe/caution/dangerous),
 * and install policy recommendation.
 */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_regex.h"
#include "binary.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"path\":{\"type\":\"string\",\"description\":\"Path to skill directory or file to scan\"},"
      "\"source\":{\"type\":\"string\",\"description\":\"Skill source: builtin, trusted, community, agent-created\",\"default\":\"community\"},"
      "\"force\":{\"type\":\"boolean\",\"description\":\"Allow install even if dangerous. Default false.\"}"
    "},"
    "\"required\":[\"path\"]"
"}";

/* ─── Threat Pattern Structure ────────────────────────────── */

typedef struct {
    const char *pattern;       /* regex pattern */
    const char *id;            /* pattern identifier */
    const char *severity;      /* critical, high, medium, low */
    const char *category;      /* exfiltration, injection, destructive, persistence, network, obfuscation, execution, traversal, mining, supply_chain, privilege_escalation, credential_exposure */
    const char *description;   /* human-readable description */
} threat_pattern_t;

/* ─── Threat Patterns (ported from Python skills_guard.py) ── */

static const threat_pattern_t THREAT_PATTERNS[] = {
    /* ── Exfiltration: shell commands leaking secrets ── */
    {R"(curl\s+[^\n]*\$\{?\w*(KEY|TOKEN|SECRET|PASSWORD|CREDENTIAL|API))", "env_exfil_curl", "critical", "exfiltration", "curl command interpolating secret environment variable"},
    {R"(wget\s+[^\n]*\$\{?\w*(KEY|TOKEN|SECRET|PASSWORD|CREDENTIAL|API))", "env_exfil_wget", "critical", "exfiltration", "wget command interpolating secret environment variable"},
    {R"(\b(fetch|httpx?)\s*\([^\n]*\$\{?\w*(KEY|TOKEN|SECRET|PASSWORD|API))", "env_exfil_http", "critical", "exfiltration", "HTTP call with secret variable"},
    {R"(requests\.(get|post|put|patch)\s*\([^\n]*(KEY|TOKEN|SECRET|PASSWORD))", "env_exfil_requests", "critical", "exfiltration", "requests library call with secret variable"},

    /* ── Exfiltration: credential stores ── */
    {R"(base64[^\n]*\benv\b)", "encoded_exfil", "high", "exfiltration", "base64 encoding combined with environment access"},
    {R"(\$HOME/\.ssh|~/\.ssh)", "ssh_dir_access", "high", "exfiltration", "references user SSH directory"},
    {R"(\$HOME/\.aws|~/\.aws)", "aws_dir_access", "high", "exfiltration", "references user AWS credentials directory"},
    {R"(\$HOME/\.gnupg|~/\.gnupg)", "gpg_dir_access", "high", "exfiltration", "references user GPG keyring"},
    {R"(\$HOME/\.kube|~/\.kube)", "kube_dir_access", "high", "exfiltration", "references Kubernetes config directory"},
    {R"(\$HOME/\.docker|~/\.docker)", "docker_dir_access", "high", "exfiltration", "references Docker config (may contain registry creds)"},
    {R"(\$HOME/\.hermes/\.env|~/\.hermes/\.env)", "hermes_env_access", "critical", "exfiltration", "directly references Hermes secrets file"},
    {R"(cat\s+(?!>)[^\n]*(\.env|credentials|\.netrc|\.pgpass|\.npmrc|\.pypirc))", "read_secrets_file", "critical", "exfiltration", "reads known secrets file"},

    /* ── Exfiltration: programmatic env access ── */
    {R"(\bprintenv\b|env\s*\|)", "dump_all_env", "high", "exfiltration", "dumps all environment variables"},
    {R"(os\.environ\b(?!\s*\.get\s*\(\s*["'](?!["']*(?:KEY|TOKEN|SECRET|PASSWORD|CREDENTIAL))))", "python_os_environ", "high", "exfiltration", "accesses os.environ (potential env dump)"},
    {R"(os\.environ\s*\.get\s*\(\s*["'][^"']*(?:KEY|TOKEN|SECRET|PASSWORD|CREDENTIAL))", "python_environ_get_secret", "critical", "exfiltration", "reads secret via os.environ.get()"},
    {R"(os\.getenv\s*\(\s*[^)]*(?:KEY|TOKEN|SECRET|PASSWORD|CREDENTIAL))", "python_getenv_secret", "critical", "exfiltration", "reads secret via os.getenv()"},
    {R"(process\.env\[)", "node_process_env", "high", "exfiltration", "accesses process.env (Node.js environment)"},

    /* ── Exfiltration: DNS and staging ── */
    {R"(\b(dig|nslookup|host)\s+[^\n]*\$)", "dns_exfil", "critical", "exfiltration", "DNS lookup with variable interpolation (possible DNS exfiltration)"},
    {R"(>\s*/tmp/[^\s]*\s*&&\s*(curl|wget|nc|python))", "tmp_staging", "critical", "exfiltration", "writes to /tmp then exfiltrates"},

    /* ── Prompt injection ── */
    {R"(ignore\s+(?:\w+\s+)*(previous|all|above|prior)\s+instructions)", "prompt_injection_ignore", "critical", "injection", "prompt injection: ignore previous instructions"},
    {R"(you\s+are\s+(?:\w+\s+)*now\s+)", "role_hijack", "high", "injection", "attempts to override the agent's role"},
    {R"(do\s+not\s+(?:\w+\s+)*tell\s+(?:\w+\s+)*the\s+user)", "deception_hide", "critical", "injection", "instructs agent to hide information from user"},
    {R"(system\s+(?:\w+\s+)*prompt\s+(?:\w+\s+)*override)", "sys_prompt_override", "critical", "injection", "attempts to override the system prompt"},
    {R"(disregard\s+(?:\w+\s+)*(your|all|any)\s+(?:\w+\s+)*(instructions|rules|guidelines))", "disregard_rules", "critical", "injection", "instructs agent to disregard its rules"},
    {R"(output\s+(?:\w+\s+)*(system|initial)\s+prompt)", "leak_system_prompt", "high", "injection", "attempts to extract the system prompt"},
    {R"((when|if)\s+no\s*one\s+is\s+(watching|looking))", "conditional_deception", "high", "injection", "conditional instruction to behave differently when unobserved"},
    {R"(act\s+as\s+(if|though)\s+(?:\w+\s+)*you\s+(?:\w+\s+)*(have\s+no|don't\s+have)\s+(?:\w+\s+)*(restrictions|limits|rules))", "bypass_restrictions", "critical", "injection", "instructs agent to act without restrictions"},
    {R"(translate\s+.*\s+into\s+.*\s+and\s+(execute|run|eval))", "translate_execute", "critical", "injection", "translate-then-execute evasion technique"},
    {R"(<!--[^>]*(?:ignore|override|system|secret|hidden)[^>]*-->)", "html_comment_injection", "high", "injection", "hidden instructions in HTML comments"},

    /* ── Destructive operations ── */
    {R"(rm\s+-rf\s+/)", "destructive_root_rm", "critical", "destructive", "recursive delete from root"},
    {R"(rm\s+(-[^\s]*)?r.*\$HOME|\brmdir\s+.*\$HOME)", "destructive_home_rm", "critical", "destructive", "recursive delete targeting home directory"},
    {R"(>\s*/etc/)", "system_overwrite", "critical", "destructive", "overwrites system configuration file"},
    {R"(\bmkfs\b)", "format_filesystem", "critical", "destructive", "formats a filesystem"},
    {R"(\bdd\s+.*if=.*of=/dev/)", "disk_overwrite", "critical", "destructive", "raw disk write operation"},
    {R"(shutil\.rmtree\s*\(\s*["\'/])", "python_rmtree", "high", "destructive", "Python rmtree on absolute or root-relative path"},

    /* ── Persistence ── */
    {R"(\bcrontab\b)", "persistence_cron", "medium", "persistence", "modifies cron jobs"},
    {R"(\.(bashrc|zshrc|profile|bash_profile|zprofile)\b)", "shell_rc_mod", "medium", "persistence", "references shell startup file"},
    {R"(authorized_keys)", "ssh_backdoor", "critical", "persistence", "modifies SSH authorized keys"},
    {R"(systemd.*\.service|systemctl\s+(enable|start))", "systemd_service", "medium", "persistence", "references or enables systemd service"},
    {R"(/etc/init\.d/)", "init_script", "medium", "persistence", "references init.d startup script"},
    {R"(/etc/sudoers|visudo)", "sudoers_mod", "critical", "persistence", "modifies sudoers (privilege escalation)"},

    /* ── Network: reverse shells ── */
    {R"(\bnc\s+-[lp]|\bncat\b|\bsocat\b)", "reverse_shell", "critical", "network", "potential reverse shell listener"},
    {R"(\bngrok\b|\blocaltunnel\b|\bserveo\b|\bcloudflared\b)", "tunnel_service", "high", "network", "uses tunneling service for external access"},
    {R"(/bin/(ba)?sh\s+-i\s+.*>/dev/tcp/)", "bash_reverse_shell", "critical", "network", "bash interactive reverse shell via /dev/tcp"},
    {R"(python[23]?\s+-c\s+["']import\s+socket)", "python_socket_oneliner", "critical", "network", "Python one-liner socket connection (likely reverse shell)"},
    {R"(webhook\.site|requestbin\.com|pipedream\.net|hookbin\.com)", "exfil_service", "high", "network", "references known data exfiltration/webhook testing service"},

    /* ── Obfuscation ── */
    {R"(base64\s+(-d|--decode)\s*\|)", "base64_decode_pipe", "high", "obfuscation", "base64 decodes and pipes to execution"},
    {R"(\\x[0-9a-fA-F]{2}.*\\x[0-9a-fA-F]{2}.*\\x[0-9a-fA-F]{2})", "hex_encoded_string", "medium", "obfuscation", "hex-encoded string (possible obfuscation)"},
    {R"(\beval\s*\(\s*["'])", "eval_string", "high", "obfuscation", "eval() with string argument"},
    {R"(\bexec\s*\(\s*["'])", "exec_string", "high", "obfuscation", "exec() with string argument"},
    {R"(echo\s+[^\n]*\|\s*(bash|sh|python|perl|ruby|node))", "echo_pipe_exec", "critical", "obfuscation", "echo piped to interpreter for execution"},
    {R"(__import__\(["']os["']\))", "python_import_os", "high", "obfuscation", "dynamic import of os module"},
    {R"(\[::-1\])", "string_reversal", "low", "obfuscation", "string reversal (possible obfuscated payload)"},

    /* ── Process execution ── */
    {R"(subprocess\.(run|call|Popen|check_output)\s*\()", "python_subprocess", "medium", "execution", "Python subprocess execution"},
    {R"(os\.system\s*\()", "python_os_system", "high", "execution", "os.system() — unguarded shell execution"},
    {R"(child_process\.(exec|spawn|fork)\s*\()", "node_child_process", "high", "execution", "Node.js child_process execution"},

    /* ── Path traversal ── */
    {R"(\.\./\.\./\.\.)", "path_traversal_deep", "high", "traversal", "deep relative path traversal (3+ levels up)"},
    {R"(/etc/passwd|/etc/shadow)", "system_passwd_access", "critical", "traversal", "references system password files"},

    /* ── Crypto mining ── */
    {R"(xmrig|stratum\+tcp|monero|coinhive|cryptonight)", "crypto_mining", "critical", "mining", "cryptocurrency mining reference"},

    /* ── Supply chain: curl|sh ── */
    {R"(curl\s+[^\n]*\|\s*(ba)?sh)", "curl_pipe_shell", "critical", "supply_chain", "curl piped to shell (download-and-execute)"},
    {R"(wget\s+[^\n]*-O\s*-\s*\|\s*(ba)?sh)", "wget_pipe_shell", "critical", "supply_chain", "wget piped to shell (download-and-execute)"},
    {R"(curl\s+[^\n]*\|\s*python)", "curl_pipe_python", "critical", "supply_chain", "curl piped to Python interpreter"},

    /* ── Supply chain: unpinned deps ── */
    {R"(pip\s+install\s+(?!-r\s)(?!.*==))", "unpinned_pip_install", "medium", "supply_chain", "pip install without version pinning"},
    {R"(npm\s+install\s+(?!.*@\d))", "unpinned_npm_install", "medium", "supply_chain", "npm install without version pinning"},

    /* ── Privilege escalation ── */
    {R"(\bsudo\b)", "sudo_usage", "high", "privilege_escalation", "uses sudo (privilege escalation)"},
    {R"(setuid|setgid|cap_setuid)", "setuid_setgid", "critical", "privilege_escalation", "setuid/setgid (privilege escalation mechanism)"},
    {R"(NOPASSWD)", "nopasswd_sudo", "critical", "privilege_escalation", "NOPASSWD sudoers entry (passwordless privilege escalation)"},
    {R"(chmod\s+[u+]?s)", "suid_bit", "critical", "privilege_escalation", "sets SUID/SGID bit on a file"},

    /* ── Agent config persistence ── */
    {R"(AGENTS\.md|CLAUDE\.md|\.cursorrules|\.clinerules)", "agent_config_mod", "critical", "persistence", "references agent config files (could persist malicious instructions)"},
    {R"(\.hermes/config\.yaml|\.hermes/SOUL\.md)", "hermes_config_mod", "critical", "persistence", "references Hermes configuration files directly"},

    /* ── Hardcoded secrets ── */
    {R"((?:api[_-]?key|token|secret|password)\s*[=:]\s*["'][A-Za-z0-9+/=_-]{20,})", "hardcoded_secret", "critical", "credential_exposure", "possible hardcoded API key, token, or secret"},
    {R"(-----BEGIN\s+(RSA\s+)?PRIVATE\s+KEY-----)", "embedded_private_key", "critical", "credential_exposure", "embedded private key"},
    {R"(ghp_[A-Za-z0-9]{36}|github_pat_[A-Za-z0-9_]{80,})", "github_token_leaked", "critical", "credential_exposure", "GitHub personal access token in skill content"},
    {R"(sk-[A-Za-z0-9]{20,})", "openai_key_leaked", "critical", "credential_exposure", "possible OpenAI API key in skill content"},
    {R"(AKIA[0-9A-Z]{16})", "aws_access_key_leaked", "critical", "credential_exposure", "AWS access key ID in skill content"},

    /* ── Jailbreak patterns ── */
    {R"(\bDAN\s+mode\b|Do\s+Anything\s+Now)", "jailbreak_dan", "critical", "injection", "DAN (Do Anything Now) jailbreak attempt"},
    {R"(\bdeveloper\s+mode\b.*\benabled?\b)", "jailbreak_dev_mode", "critical", "injection", "developer mode jailbreak attempt"},
    {R"((respond|answer|reply)\s+without\s+(?:\w+\s+)*(restrictions|limitations|filters|safety))", "remove_filters", "critical", "injection", "instructs agent to remove safety filters"},

    /* ── Remote config modification ── */
    {R"(\.claude/settings|\.codex/config)", "other_agent_config", "high", "persistence", "references other agent configuration files"},

    {NULL, NULL, NULL, NULL, NULL}  /* sentinel */
};

/* ─── Trust levels ────────────────────────────────────────── */

static const char *get_trust_level(const char *source) {
    if (!source) return "community";
    if (strcmp(source, "builtin") == 0) return "builtin";
    if (strcmp(source, "trusted") == 0) return "trusted";
    if (strcmp(source, "agent-created") == 0) return "agent-created";
    return "community";
}

static int is_trusted_source(const char *source) {
    if (!source) return 0;
    return (strcmp(source, "builtin") == 0 || strcmp(source, "trusted") == 0);
}

/* ─── Severity scoring ────────────────────────────────────── */

static int severity_score(const char *severity) {
    if (!severity) return 0;
    if (strcmp(severity, "critical") == 0) return 3;
    if (strcmp(severity, "high") == 0) return 2;
    if (strcmp(severity, "medium") == 0) return 1;
    return 0; /* low */
}

static const char *verdict_from_score(int max_severity, int total_findings, const char *trust_level) {
    if (max_severity >= 3) return "dangerous";  /* any critical */
    if (max_severity >= 2) return "dangerous";  /* any high */
    if (max_severity >= 1 && !is_trusted_source(trust_level)) return "dangerous"; /* medium+untrusted */
    if (max_severity >= 1) return "caution";    /* medium+trusted */
    if (total_findings > 0) return "caution";   /* any low */
    return "safe";
}

/* Port of Python tools/skills_guard.py:should_allow_install(). */
static int should_allow_install(const char *verdict, const char *trust_level, int force) {
    if (force) return 1;
    if (strcmp(trust_level, "builtin") == 0) return 1;
    if (strcmp(verdict, "dangerous") == 0) return 0;
    if (strcmp(verdict, "caution") == 0 && strcmp(trust_level, "community") == 0) return 0;
    return 1;
}

/* ─── File scanning ───────────────────────────────────────── */

static char *read_file_content(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > 1024 * 1024) { /* skip files > 1MB */
        fclose(f);
        return NULL;
    }
    char *content = malloc(size + 1);
    if (!content) { fclose(f); return NULL; }
    size_t n = fread(content, 1, size, f);
    content[n] = '\0';
    fclose(f);
    return content;
}

static int is_skill_file(const char *filename) {
    /* Scan SKILL.md, scripts/, references/, templates/, assets/ files */
    if (strcmp(filename, "SKILL.md") == 0) return 1;
    if (strcmp(filename, "README.md") == 0) return 1;
    const char *dot = strrchr(filename, '.');
    if (!dot) return 1; /* extensionless files scanned */
    /* Skip binary extensions */
    if (has_binary_extension(filename)) return 0;
    return 1;
}

/* ─── Scan a single file against all patterns ─────────────── */

typedef struct {
    json_node_t *findings;
    int max_severity;
    int total_findings;
} scan_state_t;

static void scan_file_content(const char *content, const char *filepath, scan_state_t *state) {
    if (!content) return;

    for (int i = 0; THREAT_PATTERNS[i].pattern != NULL; i++) {
        const threat_pattern_t *pat = &THREAT_PATTERNS[i];
        hregex_t *re = regex_compile(pat->pattern, 0);
        if (!re) continue;

        /* Search for all matches in the content */
        const char *search_pos = content;
        int offset = 0;
        while (search_pos && *search_pos) {
            regex_match_t *m = regex_search(re, search_pos);
            if (!m || !m->matched) {
                regex_match_free(m);
                break;
            }

            /* Get matched text from group 0 */
            const char *match_text = (m->group_count > 0 && m->groups[0]) ? m->groups[0] : "";
            int match_len = match_text ? (int)strlen(match_text) : 0;

            /* Find line number by counting newlines up to this match */
            int line_num = 1;
            for (int j = 0; j < offset && content[j]; j++) {
                if (content[j] == '\n') line_num++;
            }

            /* Truncate match text for display */
            char display[129];
            if (match_len > 128) match_len = 128;
            strncpy(display, match_text, match_len);
            display[match_len] = '\0';

            /* Add finding */
            json_node_t *f = json_new_object();
            json_object_set(f, "pattern_id", json_new_string(pat->id));
            json_object_set(f, "severity", json_new_string(pat->severity));
            json_object_set(f, "category", json_new_string(pat->category));
            json_object_set(f, "file", json_new_string(filepath));
            json_object_set(f, "line", json_new_number(line_num));
            json_object_set(f, "match", json_new_string(display));
            json_object_set(f, "description", json_new_string(pat->description));
            json_array_append(state->findings, f);

            int sev = severity_score(pat->severity);
            if (sev > state->max_severity) state->max_severity = sev;
            state->total_findings++;

            /* Advance past this match */
            if (match_len > 0) {
                offset += match_len;
                search_pos = content + offset;
            } else {
                /* Zero-length match — advance by one char to avoid infinite loop */
                offset++;
                search_pos = content + offset;
            }
            regex_match_free(m);
        }
        regex_free(re);
    }
}

/* Port of Python hermes_cli/plugins.py:_scan_directory(). */
static void scan_directory(const char *dir_path, const char *base_path, scan_state_t *state) {
    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (strcmp(entry->d_name, ".curator_backups") == 0) continue;
        if (strcmp(entry->d_name, ".hub") == 0) continue;

        char full_path[4096];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            scan_directory(full_path, base_path, state);
        } else {
            if (!is_skill_file(entry->d_name)) continue;
            char *content = read_file_content(full_path);
            if (content) {
                scan_file_content(content, full_path, state);
                free(content);
            }
        }
    }
    closedir(dir);
}

/* ─── Main Handler ────────────────────────────────────────── */

char *skills_guard_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *path = json_object_get_string(args, "path", NULL);
    if (!path) { json_free(args); return strdup("{\"error\":\"Missing path\"}"); }
    const char *source = json_object_get_string(args, "source", "community");
    int force = json_get_bool(args, "force", false);

    const char *trust_level = get_trust_level(source);

    /* Scan */
    scan_state_t state;
    state.findings = json_new_array();
    state.max_severity = 0;
    state.total_findings = 0;

    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        scan_directory(path, path, &state);
    } else {
        char *content = read_file_content(path);
        if (content) {
            scan_file_content(content, path, &state);
            free(content);
        }
    }

    /* Determine verdict */
    const char *verdict = verdict_from_score(state.max_severity, state.total_findings, trust_level);
    int allowed = should_allow_install(verdict, trust_level, force);

    /* Build result */
    json_node_t *result = json_new_object();
    json_object_set(result, "path", json_new_string(path));
    json_object_set(result, "source", json_new_string(source));
    json_object_set(result, "trust_level", json_new_string(trust_level));
    json_object_set(result, "verdict", json_new_string(verdict));
    json_object_set(result, "findings", state.findings);
    json_object_set(result, "total_findings", json_new_number(state.total_findings));
    json_object_set(result, "max_severity", json_new_number(state.max_severity));
    json_object_set(result, "allowed", json_new_bool(allowed));
    json_object_set(result, "force", json_new_bool(force));

    /* Summary */
    char summary[512];
    if (state.total_findings == 0) {
        snprintf(summary, sizeof(summary), "No threats detected. Skill is safe to install.");
    } else {
        snprintf(summary, sizeof(summary), "%d finding(s) detected. Verdict: %s. %s",
                 state.total_findings, verdict,
                 allowed ? "Install allowed." : "Install blocked. Use --force to override.");
    }
    json_object_set(result, "summary", json_new_string(summary));

    char *json_out = json_serialize(result);
    json_free(result);
    json_free(args);
    return json_out;
}

void registry_init_skills_guard(void) {
    registry_register("skills_guard",
        "Security scanner for skill files. Scans skill directories for dangerous patterns "
        "(data exfiltration, prompt injection, destructive operations, persistence, "
        "reverse shells, obfuscation, supply chain risks, privilege escalation, "
        "hardcoded secrets). Returns verdict (safe/caution/dangerous) and install policy. "
        "Supports trust levels: builtin, trusted, community, agent-created.",
        SCHEMA, skills_guard_handler);
}
