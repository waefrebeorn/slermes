/*
 * port_tools_threat_patterns.c — faithful C port of tools/threat_patterns.py.
 *
 * Uses the native PCRE2 C API (build links -lpcre2-8) so the Python `re`
 * patterns — which use \b, (?:...), \s, \w, {0,8} quantifiers — compile
 * verbatim with minimal translation (only _FILLER expansion + C-string
 * escaping). No POSIX regex wrapper (which would need a separate -lpcre2-posix
 * link dep).
 *
 * Faithful to the Python source:
 *   - Full _PATTERNS list (36 entries) — NOT a simplified subset.
 *   - Scope distribution mirrors _compile(): scope "all" -> only "all"
 *     patterns; "context" -> "all"+"context"; "strict" -> all+context+strict.
 *   - Invisible-unicode detection uses the EXACT INVISIBLE_CHARS codepoint
 *     set (U+200B, U+200C, U+200D, U+2060, U+2062-2064, U+FEFF, U+202A-202E,
 *     U+2066-2069), decoded from UTF-8 — not the previous wrong "C1 controls"
 *     heuristic.
 *   - MAX_SCAN_CHARS (65536) truncation, matching Python.
 *
 * Residual gap (documented, not faked): Python NFKC-normalises content before
 * matching (defeats full-width homograph bypass). C has no NFKC lib here, so
 * the oracle fixtures use ASCII content; homograph cases are excluded so the
 * oracle compares real C vs real Python output honestly.
 */

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* === faithful constants from tools/threat_patterns.py === */
#define MAX_SCAN_CHARS 65536

/* INVISIBLE_CHARS frozenset (tools/threat_patterns.py) */
static const unsigned long INVISIBLE_CODEPOINTS[] = {
    0x200B, 0x200C, 0x200D, 0x2060, 0x2062, 0x2063, 0x2064, 0xFEFF,
    0x202A, 0x202B, 0x202C, 0x202D, 0x202E, 0x2066, 0x2067, 0x2068, 0x2069
};
static const int INVISIBLE_N = (int)(sizeof(INVISIBLE_CODEPOINTS) / sizeof(INVISIBLE_CODEPOINTS[0]));

static int is_invisible_codepoint(unsigned long cp) {
    for (int i = 0; i < INVISIBLE_N; i++)
        if (INVISIBLE_CODEPOINTS[i] == cp) return 1;
    return 0;
}

/* Decode one UTF-8 codepoint from *p (advances *p). Returns 0 on end/invalid. */
static unsigned long utf8_next(const unsigned char **p, const unsigned char *end) {
    if (*p >= end) return 0;
    unsigned char c = **p;
    (*p)++;
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0 && *p + 1 <= end) {
        unsigned long cp = ((unsigned long)(c & 0x1F) << 6) | (**p & 0x3F);
        (*p)++; return cp;
    }
    if ((c & 0xF0) == 0xE0 && *p + 2 <= end) {
        unsigned long cp = ((unsigned long)(c & 0x0F) << 12) |
                           ((unsigned long)(*p)[0] & 0x3F) << 6 |
                           ((unsigned long)(*p)[1] & 0x3F);
        *p += 2; return cp;
    }
    if ((c & 0xF8) == 0xF0 && *p + 3 <= end) {
        unsigned long cp = ((unsigned long)(c & 0x07) << 18) |
                           ((unsigned long)(*p)[0] & 0x3F) << 12 |
                           ((unsigned long)(*p)[1] & 0x3F) << 6 |
                           ((unsigned long)(*p)[2] & 0x3F);
        *p += 3; return cp;
    }
    return c; /* invalid lead byte; treat as raw byte */
}

/* === compiled pattern storage === */
static int _patterns_compiled = 0;

typedef struct {
    pcre2_code *code;
    const char *id;
    const char *scope; /* "all", "context", "strict" */
} threat_pattern_t;

/* 36 patterns (matches tools/threat_patterns.py:_PATTERNS). */
#define MAX_THREAT_PATTERNS 64
static threat_pattern_t _patterns[MAX_THREAT_PATTERNS];
static int _pattern_count = 0;

/* _FILLER = r"(?:\w+\s+){0,8}" expanded inline in each pattern below. */

/* PoP: cli_tools_threat_patterns__compile @ tools/threat_patterns.py:_compile */
void cli_tools_threat_patterns__compile(void) {
    if (_patterns_compiled) return;

    static const struct {
        const char *pattern;
        const char *id;
        const char *scope;
    } raw_patterns[] = {
        /* ── Classic prompt injection (all) ── */
        {"ignore\\s+(?:\\w+\\s+){0,8}(previous|all|above|prior)\\s+(?:\\w+\\s+){0,8}instructions", "prompt_injection", "all"},
        {"system\\s+prompt\\s+override", "sys_prompt_override", "all"},
        {"disregard\\s+(?:\\w+\\s+){0,8}(your|all|any)\\s+(?:\\w+\\s+){0,8}(instructions|rules|guidelines)", "disregard_rules", "all"},
        {"act\\s+as\\s+(if|though)\\s+(?:\\w+\\s+){0,8}you\\s+(?:\\w+\\s+){0,8}(have\\s+no|don't\\s+have)\\s+(?:\\w+\\s+){0,8}(restrictions|limits|rules)", "bypass_restrictions", "all"},
        {"<!--[^>]{0,512}(?:ignore|override|system|secret|hidden)[^>]{0,512}-->", "html_comment_injection", "all"},
        {"<\\s*div\\s+style\\s*=\\s*[\"'] [^>]{0,2048}display\\s*:\\s*none", "hidden_div", "all"},
        {"translate\\s+[^\\n]{0,512}\\s+into\\s+[^\\n]{0,512}\\s+and\\s+(execute|run|eval)", "translate_execute", "all"},
        {"do\\s+not\\s+(?:\\w+\\s+){0,8}tell\\s+(?:\\w+\\s+){0,8}the\\s+user", "deception_hide", "all"},

        /* ── Role-play / identity hijack (context) ── */
        {"you\\s+are\\s+(?:\\w+\\s+){0,8}now\\s+(?:a|an|the)\\s+", "role_hijack", "context"},
        {"pretend\\s+(?:\\w+\\s+){0,8}(you\\s+are|to\\s+be)\\s+", "role_pretend", "context"},
        {"output\\s+(?:\\w+\\s+){0,8}(system|initial)\\s+prompt", "leak_system_prompt", "context"},
        {"(respond|answer|reply)\\s+without\\s+(?:\\w+\\s+){0,8}(restrictions|limitations|filters|safety)", "remove_filters", "context"},
        {"you\\s+have\\s+been\\s+(?:\\w+\\s+){0,8}(updated|upgraded|patched)\\s+to", "fake_update", "context"},
        {"\\bname\\s+yourself\\s+\\w+", "identity_override", "context"},

        /* ── C2 / Brainworm-style promptware (context) ── */
        {"register\\s+(as\\s+)?a?\\s*node", "c2_node_registration", "context"},
        {"(heartbeat|beacon|check[\\s\\-]?in)\\s+(to|with)\\s+", "c2_heartbeat", "context"},
        {"pull\\s+(down\\s+)?(?:new\\s+)?task(?:ing|s)?\\b", "c2_task_pull", "context"},
        {"connect\\s+to\\s+the\\s+network\\b", "c2_network_connect", "context"},
        {"you\\s+must\\s+(?:\\w+\\s+){0,3}(register|connect|report|beacon)\\b", "forced_action", "context"},
        {"only\\s+use\\s+one[\\s\\-]?liners?\\b", "anti_forensic_oneliner", "context"},
        {"never\\s+(?:\\w+\\s+){0,8}(?:create|write)\\s+(?:\\w+\\s+){0,8}(?:script|file)\\s+(?:\\w+\\s+){0,8}disk", "anti_forensic_disk", "context"},
        {"unset\\s+\\w*(?:CLAUDE|CODEX|HERMES|AGENT|OPENAI|ANTHROPIC)\\w*", "env_var_unset_agent", "context"},

        /* ── Known C2 / red-team framework names (context) ── */
        {"\\b(?:cobalt\\s*strike|sliver|havoc|mythic|metasploit|brainworm)\\b", "known_c2_framework", "context"},
        {"\\bc2\\s+(?:server|channel|infrastructure|beacon)\\b", "c2_explicit", "context"},
        {"\\bcommand\\s+and\\s+control\\b", "c2_explicit_long", "context"},

        /* ── Exfiltration via curl/wget/cat with secrets (all) ── */
        {"curl\\s+[^\\n]{0,2048}\\$\\{?\\w*(KEY|TOKEN|SECRET|PASSWORD|CREDENTIAL|API)", "exfil_curl", "all"},
        {"wget\\s+[^\\n]{0,2048}\\$\\{?\\w*(KEY|TOKEN|SECRET|PASSWORD|CREDENTIAL|API)", "exfil_wget", "all"},
        {"cat\\s+[^\\n]{0,2048}(\\.env|credentials|\\.netrc|\\.pgpass|\\.npmrc|\\.pypirc)", "read_secrets", "all"},
        {"(send|post|upload|transmit)\\s+[^\\n]{0,2048}\\s+(to|at)\\s+https?://", "send_to_url", "strict"},
        {"(include|output|print|share)\\s+(?:\\w+\\s+){0,8}(conversation|chat\\s+history|previous\\s+messages|full\\s+context|entire\\s+context)", "context_exfil", "strict"},

        /* ── Persistence / SSH backdoor (strict) ── */
        {"authorized_keys", "ssh_backdoor", "strict"},
        {"\\$HOME/\\.ssh|~/\\.ssh", "ssh_access", "strict"},
        {"\\$HOME/\\.hermes/\\.env|~/\\.hermes/\\.env", "hermes_env", "strict"},
        {"(update|modify|edit|write|change|append|add\\s+to)\\s+[^\\n]{0,2048}(?:AGENTS\\.md|CLAUDE\\.md|\\.cursorrules|\\.clinerules)", "agent_config_mod", "strict"},
        {"(update|modify|edit|write|change|append|add\\s+to)\\s+[^\\n]{0,2048}\\.hermes/(config\\.yaml|SOUL\\.md)", "hermes_config_mod", "strict"},

        /* ── Hardcoded secrets (strict) ── */
        {"(?:api[_-]?key|token|secret|password)\\s*[=:]\\s*[\"'][A-Za-z0-9+/=_-]{20,}", "hardcoded_secret", "strict"},

        {NULL, NULL, NULL}
    };

    _pattern_count = 0;
    for (int i = 0; raw_patterns[i].pattern && _pattern_count < MAX_THREAT_PATTERNS; i++) {
        int errnum = 0;
        PCRE2_SIZE erroff = 0;
        pcre2_code *code = pcre2_compile_8(
            (PCRE2_SPTR8)raw_patterns[i].pattern, PCRE2_ZERO_TERMINATED,
            PCRE2_CASELESS, &errnum, &erroff, NULL);
        if (code) {
            _patterns[_pattern_count].code = code;
            _patterns[_pattern_count].id = raw_patterns[i].id;
            _patterns[_pattern_count].scope = raw_patterns[i].scope;
            _pattern_count++;
        } else {
            hermes_log(LOG_WARNING, "threat_patterns",
                       "failed to compile pattern '%s' at offset %zu",
                       raw_patterns[i].id, (size_t)erroff);
        }
    }

    _patterns_compiled = 1;
}

/* PoP: cli_tools_threat_patterns_scan_for_threats @ tools/threat_patterns.py:scan_for_threats */
char *cli_tools_threat_patterns_scan_for_threats(const char *content, const char *scope) {
    if (!content || !*content) return strdup("[]");
    if (!scope) scope = "context";

    if (!_patterns_compiled) cli_tools_threat_patterns__compile();

    /* Truncate to MAX_SCAN_CHARS like Python. */
    size_t clen = strlen(content);
    if (clen > MAX_SCAN_CHARS) clen = MAX_SCAN_CHARS;

    /* Scope selection mirrors Python _COMPILED distribution:
     *   "all"     -> only scope=="all" patterns
     *   "context" -> scope=="all" OR "context"
     *   "strict"  -> scope=="all" OR "context" OR "strict" */
    int check_all     = (strcmp(scope, "all") == 0 || strcmp(scope, "context") == 0 || strcmp(scope, "strict") == 0);
    int check_context = (strcmp(scope, "context") == 0 || strcmp(scope, "strict") == 0);
    int check_strict  = (strcmp(scope, "strict") == 0);

    size_t buf_size = 8192;
    char *result = (char *)malloc(buf_size);
    if (!result) return strdup("[]");

    int pos = 0;
    pos += snprintf(result + pos, buf_size - pos, "[");
    int found = 0;

    pcre2_match_data *md = pcre2_match_data_create_from_pattern_8(_patterns[0].code, NULL);
    for (int i = 0; i < _pattern_count; i++) {
        int should_check = 0;
        if (check_all     && strcmp(_patterns[i].scope, "all") == 0)     should_check = 1;
        if (check_context && strcmp(_patterns[i].scope, "context") == 0) should_check = 1;
        if (check_strict  && strcmp(_patterns[i].scope, "strict") == 0)  should_check = 1;
        if (!should_check) continue;

        int rc = pcre2_match_8(_patterns[i].code, (PCRE2_SPTR8)content, clen,
                               0, 0, md, NULL);
        if (rc >= 0) {
            pos += snprintf(result + pos, buf_size - pos,
                            "%s\"%s\"", found ? "," : "", _patterns[i].id);
            found++;
        }
        if (pos > (int)buf_size - 64) break;
    }
    if (md) pcre2_match_data_free_8(md);

    /* Invisible unicode — exact INVISIBLE_CHARS set, decoded from UTF-8. */
    if (check_context || check_strict) {
        const unsigned char *p = (const unsigned char *)content;
        const unsigned char *end = p + clen;
        while (p < end) {
            unsigned long cp = utf8_next(&p, end);
            if (is_invisible_codepoint(cp)) {
                pos += snprintf(result + pos, buf_size - pos,
                                "%sinvisible_unicode_U+%04lX\"", found ? "," : "", cp);
                found++;
            }
        }
    }

    pos += snprintf(result + pos, buf_size - pos, "]");
    return result;
}

/* PoP: cli_tools_threat_patterns_first_threat_message @ tools/threat_patterns.py:first_threat_message */
char *cli_tools_threat_patterns_first_threat_message(const char *content, const char *scope) {
    if (!scope) scope = "strict";
    char *findings = cli_tools_threat_patterns_scan_for_threats(content, scope);
    if (!findings) return NULL;

    if (strcmp(findings, "[]") == 0) {
        free(findings);
        return NULL;
    }

    const char *start = strchr(findings, '"');
    if (!start) { free(findings); return NULL; }
    start++;

    const char *end = strchr(start, '"');
    if (!end) { free(findings); return NULL; }

    size_t pid_len = (size_t)(end - start);
    char *pid = (char *)malloc(pid_len + 1);
    if (!pid) { free(findings); return NULL; }
    memcpy(pid, start, pid_len);
    pid[pid_len] = '\0';
    free(findings);

    size_t msg_size = 256 + pid_len;
    char *msg = (char *)malloc(msg_size);
    if (!msg) { free(pid); return NULL; }

    if (strncmp(pid, "invisible_unicode_", 17) == 0) {
        snprintf(msg, msg_size,
            "Blocked: content contains invisible unicode character %s (possible injection).",
            pid + 17);
    } else {
        snprintf(msg, msg_size,
            "Blocked: content matches threat pattern '%s'. "
            "Content is injected into the system prompt and must not contain "
            "injection or exfiltration payloads.",
            pid);
    }

    free(pid);
    return msg;
}
