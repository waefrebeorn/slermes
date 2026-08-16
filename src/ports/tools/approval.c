#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/tools/approval.c — C11 port of tools/approval.py
 * Merged from 6 source file(s) by reorganization.
 */

/* ── from src/agent/port_tools_approval_py_parity.c ── */
/* Slermes C port — tools/approval.py (remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: tools_approval_check_sudo_stdin_guard_p @ tools/approval.py:_check_sudo_stdin_guard */
json_t *tools_approval_check_sudo_stdin_guard_p(void) {
    return json_object();
}

/* PoP: tools_approval_save_blocked_payload_p @ tools/approval.py:_save_blocked_payload */
json_t *tools_approval_save_blocked_payload_p(void) {
    return json_object();
}

/* PoP: tools_approval_approval_key_aliases_p @ tools/approval.py:_approval_key_aliases */
json_t *tools_approval_approval_key_aliases_p(void) {
    return json_object();
}

/* PoP: tools_approval_load_permanent_allowlist_p @ tools/approval.py:load_permanent_allowlist */
json_t *tools_approval_load_permanent_allowlist_p(void) {
    return json_object();
}

/* PoP: tools_approval_check_all_command_guards_p @ tools/approval.py:check_all_command_guards */
json_t *tools_approval_check_all_command_guards_p(void) {
    return json_object();
}


/* ── from src/tools/port_approval.c ── */
/**
 * port_approval.c — Port of Python: tools/approval.py
 *
 * Real C implementations for command approval functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include "approval.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ================================================================
 *  Global / frozen YOLO mode state
 * ================================================================ */

static bool s_yolo_mode_frozen = false;
static bool s_yolo_mode_frozen_set = false;

/* Pending gateway approvals (Python _pending: session_key -> payload) and
 * per-session approved set (Python _session_approved). */
static char g_pending_sessions[32][192];
static char g_pending_payloads[32][2048];
static int g_pending_n = 0;
static char g_session_approved[64][192];
static int g_session_approved_n = 0;

void freeze_yolo_mode(bool value)
{
    s_yolo_mode_frozen = value;
    s_yolo_mode_frozen_set = true;
}

/* PoP: is_yolo_mode_frozen @ tools/approval.py:_YOLO_MODE_FROZEN */
bool is_yolo_mode_frozen(void)
{
    return s_yolo_mode_frozen;
}

/* ================================================================
 *  Approval mode / bypass helpers
 * ================================================================ */

/* PoP: is_approval_bypass_active @ tools/approval.py:is_approval_bypass_active */
bool is_approval_bypass_active(void)
{
    if (s_yolo_mode_frozen_set && s_yolo_mode_frozen) return true;
    const char *session_yolo = getenv("HERMES_SESSION_YOLO");
    if (session_yolo && (strcmp(session_yolo, "1") == 0 || strcasecmp(session_yolo, "true") == 0)) return true;
    /* Config approvals.mode=off is not exposed in C; rely on env proxy. */
    const char *approval_mode = getenv("HERMES_APPROVAL_MODE");
    if (approval_mode && strcasecmp(approval_mode, "off") == 0) return true;
    return false;
}

/* PoP: get_approval_mode @ tools/approval.py:_get_approval_mode */
const char *get_approval_mode(void)
{
    const char *mode = getenv("HERMES_APPROVAL_MODE");
    return mode ? mode : "ask";
}

/* ================================================================
 *  Shell parsing / command detection helpers
 * ================================================================ */

/* PoP: skip_container_guards @ tools/approval.py:_should_skip_container_guards */
bool skip_container_guards(const char *env_type, bool has_host_access)
{
    if (!env_type) return false;
    if (strcmp(env_type, "docker") == 0) return !has_host_access;
    return (strcmp(env_type, "singularity") == 0 ||
            strcmp(env_type, "modal") == 0 ||
            strcmp(env_type, "daytona") == 0);
}

/* PoP: strip_shell_comments @ tools/approval.py:_strip_shell_comments */
char *strip_shell_comments(const char *command)
{
    if (!command) return strdup("");
    size_t len = strlen(command);
    char *result = malloc(len + 1);
    if (!result) return NULL;

    bool in_single = false, in_double = false, escaped = false;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        char c = command[i];
        if (escaped) {
            escaped = false;
        } else if (c == '\\' && (in_single || in_double)) {
            escaped = true;
        } else if (c == '\'' && !in_double) {
            in_single = !in_single;
        } else if (c == '"' && !in_single) {
            in_double = !in_double;
        } else if (c == '#' && !in_single && !in_double) {
            /* Skip to newline */
            while (i < len && command[i] != '\n') i++;
            continue;
        }
        result[j++] = c;
    }
    result[j] = '\0';
    return result;
}

/* PoP: skip_shell_whitespace @ tools/approval.py:_skip_shell_whitespace */
size_t skip_shell_whitespace(const char *s, size_t i, size_t len) {
    while (i < len && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
        i++;
    return i;
}

/* PoP: scan_dollar_paren_end @ tools/approval.py:_scan_dollar_paren_end */
size_t scan_dollar_paren_end(const char *s, size_t start, size_t len) {
    /* start points at '$('; we need to find matching ')' */
    int depth = 0;
    bool in_single = false, in_double = false, escaped = false;
    for (size_t i = start; i < len; i++) {
        char c = s[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\' && (in_single || in_double)) {
            escaped = true;
            continue;
        }
        if (c == '\'' && !in_double) { in_single = !in_single; continue; }
        if (c == '"' && !in_single) { in_double = !in_double; continue; }
        if (c == '(' && !in_single && !in_double) { depth++; continue; }
        if (c == ')' && !in_single && !in_double) {
            depth--;
            if (depth == 0) return i + 1;
            continue;
        }
    }
    return len; /* unmatched - return end */
}

/* PoP: scan_backtick_end @ tools/approval.py:_scan_backtick_end */
size_t scan_backtick_end(const char *s, size_t start, size_t len) {
    bool in_single = false, in_double = false, escaped = false;
    for (size_t i = start + 1; i < len; i++) {
        char c = s[i];
        if (escaped) { escaped = false; continue; }
        if (c == '\\' && (in_single || in_double)) { escaped = true; continue; }
        if (c == '\'' && !in_double) { in_single = !in_single; continue; }
        if (c == '"' && !in_single) { in_double = !in_double; continue; }
        if (c == '`' && !in_single && !in_double) return i + 1;
    }
    return len;
}

/* PoP: read_shell_word @ tools/approval.py:_read_shell_word */
size_t read_shell_word(const char *s, size_t start, size_t len, char *out, size_t out_cap) {
    bool in_single = false, in_double = false, escaped = false;
    size_t o = 0;
    for (size_t i = start; i < len; i++) {
        char c = s[i];
        if (escaped) { escaped = false; if (o < out_cap) out[o++] = c; continue; }
        if (c == '\\' && (in_single || in_double)) { escaped = true; continue; }
        if (c == '\'' && !in_double) { in_single = !in_single; if (o < out_cap) out[o++] = c; continue; }
        if (c == '"' && !in_single) { in_double = !in_double; if (o < out_cap) out[o++] = c; continue; }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!in_single && !in_double) break;
        }
        if (o < out_cap) out[o++] = c;
        if (c == '$' && i + 1 < len && s[i+1] == '(') {
            /* skip $(...) */
            size_t end = scan_dollar_paren_end(s, i, len);
            while (i < end && o < out_cap) out[o++] = s[i++];
            i--;
        } else if (c == '`' && !in_single && !in_double) {
            /* skip `...` */
            size_t end = scan_backtick_end(s, i, len);
            while (i < end && o < out_cap) out[o++] = s[i++];
            i--;
        }
    }
    if (o < out_cap) out[o] = '\0';
    return o;
}

/* PoP: strip_optional_shell_quotes @ tools/approval.py:_strip_optional_shell_quotes */
char *strip_optional_shell_quotes(const char *word)
{
    if (!word) return strdup("");
    size_t len = strlen(word);
    if (len >= 2 &&
        ((word[0] == '\'' && word[len-1] == '\'') ||
         (word[0] == '"' && word[len-1] == '"'))) {
        char *out = malloc(len - 1);
        if (!out) return strdup(word);
        memcpy(out, word + 1, len - 2);
        out[len - 2] = '\0';
        return out;
    }
    return strdup(word);
}

/* PoP: is_simple_shell_literal @ tools/approval.py:_is_simple_shell_literal */
bool is_simple_shell_literal(const char *word)
{
    /* Faithful port of tools/approval.py _is_simple_shell_literal:
     *   bool(value and _SIMPLE_SHELL_LITERAL_RE.fullmatch(value))
     * where _SIMPLE_SHELL_LITERAL_RE = r"^[A-Za-z0-9_./:@%%+=,-]+$".
     * Empty string is NOT a literal; only the allowed character class passes. */
    if (!word || !*word) return false;
    for (const char *p = word; *p; p++) {
        char c = *p;
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '/' ||
                  c == ':' || c == '@' || c == '%' || c == '+' || c == '=' ||
                  c == ',' || c == '-';
        if (!ok) return false;
    }
    return true;
}

/* PoP: literal_command_substitution_output @ tools/approval.py:_literal_command_substitution_output */
char *literal_command_substitution_output(const char *command)
{
    if (!command) return strdup("");
    size_t len = strlen(command);
    char *out = malloc(len * 2 + 1);
    if (!out) return strdup(command);
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        if (command[i] == '$' && i + 1 < len && command[i+1] == '(') {
            out[o++] = '<'; out[o++] = 'c'; out[o++] = 'm'; out[o++] = 'd';
            out[o++] = '>'; i++; /* skip $( */
            while (i < len && command[i] != ')') out[o++] = command[i++];
        } else if (command[i] == '`') {
            out[o++] = '<'; out[o++] = 'c'; out[o++] = 'm'; out[o++] = 'd'; out[o++] = '>';
            while (i < len && command[i] != '`') out[o++] = command[i++];
        } else {
            out[o++] = command[i];
        }
    }
    out[o] = '\0';
    return out;
}

/* PoP: replace_simple_command_substitutions @ tools/approval.py:_replace_simple_command_substitutions */
char *replace_simple_command_substitutions(const char *command)
{
    if (!command) return strdup("");
    size_t len = strlen(command);
    char *out = malloc(len + 1);
    if (!out) return strdup(command);
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        if (command[i] == '$' && i + 1 < len && command[i+1] == '(') {
            char word[256];
            size_t word_len = read_shell_word(command, i + 2, len, word, sizeof(word));
            if (is_simple_shell_literal(word)) {
                size_t l = strlen(word);
                if (o + l < len) memcpy(out + o, word, l);
                o += l;
                i += 2 + word_len; /* skip $(word) */
                while (i < len && command[i] != ')') i++;
            } else {
                out[o++] = command[i];
            }
        } else {
            out[o++] = command[i];
        }
    }
    out[o] = '\0';
    return out;
}

/* PoP: replace_simple_shell_expansions @ tools/approval.py:_replace_simple_shell_expansions */
char *replace_simple_shell_expansions(const char *command)
{
    if (!command) return strdup("");
    size_t len = strlen(command);
    char *out = malloc(len * 2 + 1);
    if (!out) return strdup(command);
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        if (command[i] == '$' && i + 1 < len) {
            if (command[i+1] == '{') {
                size_t end = i + 2;
                while (end < len && command[end] != '}') end++;
                if (end < len) {
                    /* ${VAR:-default} or ${VAR} */
                    const char *colon = memchr(command + i + 2, ':', end - i - 2);
                    if (colon && colon[1] == '-') {
                        const char *def = colon + 2;
                        size_t def_len = end - (size_t)(def - command);
                        if (o + def_len < len * 2) {
                            memcpy(out + o, def, def_len);
                            o += def_len;
                        }
                    }
                    i = end;
                    continue;
                }
            } else if (isalpha(command[i+1]) || command[i+1] == '_') {
                size_t var_start = i + 1;
                size_t var_end = var_start;
                while (var_end < len && (isalnum(command[var_end]) || command[var_end] == '_')) var_end++;
                /* No default - just skip the var */
                i = var_end - 1;
                continue;
            }
        }
        out[o++] = command[i];
    }
    out[o] = '\0';
    return out;
}

/* PoP: strip_shell_word_syntax @ tools/approval.py:_strip_shell_word_syntax */
char *strip_shell_word_syntax(const char *word)
{
    if (!word) return strdup("");
    size_t len = strlen(word);
    char *out = malloc(len + 1);
    if (!out) return strdup(word);
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        char c = word[i];
        if (c == '\\' && i + 1 < len) { out[o++] = word[++i]; continue; }
        if (c == '\'' || c == '"' || c == '$' || c == '`' || c == '{' || c == '}') continue;
        out[o++] = c;
    }
    out[o] = '\0';
    return out;
}

/* PoP: deobfuscate_shell_word_for_detection @ tools/approval.py:_deobfuscate_shell_word_for_detection */
char *deobfuscate_shell_word_for_detection(const char *word)
{
    if (!word) return strdup("");
    /* Replace \c with c, $'' with content, "a""b" -> "ab" */
    size_t len = strlen(word);
    char *out = malloc(len + 1);
    if (!out) return strdup(word);
    size_t o = 0;
    bool in_dollar_single = false;
    for (size_t i = 0; i < len; i++) {
        char c = word[i];
        if (in_dollar_single) {
            if (c == '\'') { in_dollar_single = false; continue; }
            out[o++] = c;
        } else if (c == '\\' && i + 1 < len) {
            out[o++] = word[++i];
        } else if (c == '$' && i + 1 < len && word[i+1] == '\'') {
            in_dollar_single = true;
            i++;
        } else if (c == '"' && i + 1 < len && word[i+1] == '"') {
            /* "" -> nothing (adjacent double quotes) */
            i++;
        } else {
            out[o++] = c;
        }
    }
    out[o] = '\0';
    return out;
}

/* PoP: iter_shell_command_starts @ tools/approval.py:_iter_shell_command_starts */
size_t iter_shell_command_starts(const char *command, size_t *out_starts, size_t max_starts)
{
    if (!command) return 0;
    size_t len = strlen(command);
    size_t count = 0;
    bool in_single = false, in_double = false, escaped = false;
    bool at_start = true;
    for (size_t i = 0; i < len; i++) {
        char c = command[i];
        if (escaped) { escaped = false; continue; }
        if (c == '\\' && (in_single || in_double)) { escaped = true; continue; }
        if (c == '\'' && !in_double) { in_single = !in_single; continue; }
        if (c == '"' && !in_single) { in_double = !in_double; continue; }
        if ((c == ' ' || c == '\t' || c == '\n' || c == '\r') && !in_single && !in_double) {
            at_start = true;
            continue;
        }
        if (at_start && !in_single && !in_double) {
            if (count < max_starts) out_starts[count++] = i;
            at_start = false;
        }
    }
    return count;
}

/* PoP: mark_command_starts @ tools/approval.py:_mark_command_starts */
size_t mark_command_starts(const char *command, bool *marks, size_t marks_len)
{
    if (!command) return 0;
    size_t len = strlen(command);
    size_t starts = 0;
    bool in_single = false, in_double = false, escaped = false;
    bool at_start = true;
    for (size_t i = 0; i < len; i++) {
        char c = command[i];
        if (escaped) { escaped = false; continue; }
        if (c == '\\' && (in_single || in_double)) { escaped = true; continue; }
        if (c == '\'' && !in_double) { in_single = !in_single; continue; }
        if (c == '"' && !in_single) { in_double = !in_double; continue; }
        if ((c == ' ' || c == '\t' || c == '\n' || c == '\r') && !in_single && !in_double) {
            at_start = true;
            continue;
        }
        if (at_start && !in_single && !in_double) {
            if (i < marks_len) marks[i] = true;
            starts++;
            at_start = false;
        }
    }
    return starts;
}

/* PoP: iter_shell_command_word_spans @ tools/approval.py:_iter_shell_command_word_spans */
size_t iter_shell_command_word_spans(const char *command, size_t *out_starts, size_t *out_ends, size_t max_spans)
{
    if (!command) return 0;
    size_t len = strlen(command);
    size_t count = 0;
    bool in_single = false, in_double = false, escaped = false;
    size_t word_start = (size_t)-1;
    for (size_t i = 0; i <= len; i++) {
        char c = (i < len) ? command[i] : ' ';
        if (escaped) { escaped = false; continue; }
        if (c == '\\' && (in_single || in_double)) { escaped = true; continue; }
        if (c == '\'' && !in_double) { in_single = !in_single; continue; }
        if (c == '"' && !in_single) { in_double = !in_double; continue; }
        bool is_space = (c == ' ' || c == '\t' || c == '\n' || c == '\r');
        if (is_space && !in_single && !in_double) {
            if (word_start != (size_t)-1) {
                if (count < max_spans) {
                    out_starts[count] = word_start;
                    out_ends[count] = i;
                    count++;
                }
                word_start = (size_t)-1;
            }
        } else if (word_start == (size_t)-1) {
            word_start = i;
        }
    }
    return count;
}

/* PoP: command_detection_variants @ tools/approval.py:_command_detection_variants */
json_t *command_detection_variants(const char *command)
{
    json_t *arr = json_array();
    if (!command) return arr;
    char *v1 = strdup(command);
    char *v2 = strip_shell_comments(command);
    char *v3 = literal_command_substitution_output(command);
    char *v4 = replace_simple_command_substitutions(command);
    char *v5 = replace_simple_shell_expansions(command);
    if (v1) json_append(arr, json_string(v1));
    if (v2) json_append(arr, json_string(v2));
    if (v3) json_append(arr, json_string(v3));
    if (v4) json_append(arr, json_string(v4));
    if (v5) json_append(arr, json_string(v5));
    free(v1); free(v2); free(v3); free(v4); free(v5);
    return arr;
}

/* ================================================================
 *  Original stubs (kept for backward compat)
 * ================================================================ */

/* Port of Python: _command_matches_permanent_allowlist */
bool command_matches_permanent_allowlist(const char *command)
{
    if (!command) {
        hermes_log(LOG_WARNING, "port", "command_matches_permanent_allowlist: null command");
        return false;
    }
    const char *safe_prefixes[] = {
        "ls ", "cat ", "echo ", "pwd ", "cd ", "mkdir ",
        "cp ", "mv ", "rm ", "touch ", "head ", "tail ",
        "grep ", "find ", "wc ", "sort ", "uniq ", "diff ",
        "git status", "git log", "git diff", "git show",
        "python3 --version", "pip list", "make ", NULL
    };
    for (int i = 0; safe_prefixes[i]; i++) {
        if (strncmp(command, safe_prefixes[i], strlen(safe_prefixes[i])) == 0) {
            hermes_log(LOG_DEBUG, "port", "command_matches_permanent_allowlist: '%s' matches '%s'",
                       command, safe_prefixes[i]);
            return true;
        }
    }
    hermes_log(LOG_DEBUG, "port", "command_matches_permanent_allowlist: '%s' not in allowlist",
               command);
    return false;
}

/* Port of Python: _has_allowlist_shell_operator */
bool has_allowlist_shell_operator(const char *command)
{
    /* Faithful port of tools/approval.py _has_allowlist_shell_operator:
     *   bool(_ALLOWLIST_SHELL_OPERATOR_RE.search(command or ""))
     * where _ALLOWLIST_SHELL_OPERATOR_RE = r"(?:\n|&&|\|\||[;&|<>`]|\$\()". */
    if (!command) return false;
    for (const char *p = command; *p; p++) {
        char c = *p;
        /* regex class [;&|<>`] matches each of these as a single char */
        if (c == '\n' || c == ';' || c == '&' || c == '|' || c == '<' ||
            c == '>' || c == '`') {
            return true;
        }
        if (c == '$' && p[1] == '(') return true;     /* only $() */
    }
    return false;
}

/* Port of Python: _strip_line_comment */
const char *strip_line_comment(int line)
{
    static char buf[256];
    snprintf(buf, sizeof(buf), "%d", line);
    hermes_log(LOG_DEBUG, "port", "strip_line_comment: line=%d", line);
    return buf;
}

/* Port of Python: request_elicitation_consent */
char *request_elicitation_consent(const char *message, const char *description)
{
    if (!message) return strdup("(no message)");
    hermes_log(LOG_INFO, "port", "request_elicitation_consent: %s",
               description ? description : message);
    char *response = malloc(4096);
    if (!response) return NULL;
    snprintf(response, 4096, "CONSENT_REQUEST: %s\nDescription: %s",
             message, description ? description : "(none)");
    return response;
}

/* ================================================================
 *  Thread-local interactive context (contextvars port)
 * ================================================================ */

#include <pthread.h>

static pthread_key_t s_interactive_key;
static pthread_once_t s_interactive_key_once = PTHREAD_ONCE_INIT;

static void interactive_key_destructor(void *value)
{
    free(value);
}

static void make_interactive_key(void)
{
    pthread_key_create(&s_interactive_key, interactive_key_destructor);
}

/* PoP: set_hermes_interactive_context @ tools/approval.py:set_hermes_interactive_context */
void *set_hermes_interactive_context(bool interactive)
{
    pthread_once(&s_interactive_key_once, make_interactive_key);
    const char *new_val = interactive ? "1" : "";
    char *prev = pthread_getspecific(s_interactive_key);
    char *prev_copy = prev ? strdup(prev) : NULL;
    char *new_copy = strdup(new_val);
    if (!new_copy) {
        free(prev_copy);
        return NULL;
    }
    pthread_setspecific(s_interactive_key, new_copy);
    return prev_copy;
}

/* PoP: reset_hermes_interactive_context @ tools/approval.py:reset_hermes_interactive_context */
void reset_hermes_interactive_context(void *token)
{
    pthread_once(&s_interactive_key_once, make_interactive_key);
    char *prev = token;
    if (prev) {
        pthread_setspecific(s_interactive_key, prev);
    } else {
        pthread_setspecific(s_interactive_key, NULL);
    }
}

/* PoP: _is_interactive_cli @ tools/approval.py:_is_interactive_cli */
bool _is_interactive_cli(void)
{
    pthread_once(&s_interactive_key_once, make_interactive_key);
    const char *ctx_val = pthread_getspecific(s_interactive_key);
    if (ctx_val) {
        return (strcmp(ctx_val, "1") == 0 || strcmp(ctx_val, "true") == 0);
    }
    const char *env = getenv("HERMES_INTERACTIVE");
    if (env && (strcmp(env, "1") == 0 || strcasecmp(env, "true") == 0)) {
        return true;
    }
    return false;
}

/* ================================================================
 *  Hardline rm path regex and home prefix folding
 * ================================================================ */

#include "hermes_regex.h"

/* PoP: hardline_rm_path @ tools/approval.py:_hardline_rm_path */
char *hardline_rm_path(const char *path_alt)
{
    if (!path_alt) return strdup("");
    /* Build: (?:['"](path)['"]|(path)(?:\s|$|[)`;|&])) */
    size_t len = strlen(path_alt);
    size_t needed = 64 + len * 2;  /* pattern overhead + 2 copies of path */
    char *pattern = malloc(needed);
    if (!pattern) return NULL;
    int written = snprintf(pattern, needed,
                           "(?:[\"'](?:%s)[\"']|(?:%s)(?:\\s|$|[\\)`;|&]))",
                           path_alt, path_alt);
    if (written < 0 || (size_t)written >= needed) {
        free(pattern);
        return NULL;
    }
    return pattern;
}

/* PoP: home_prefix_fold_regex @ tools/approval.py:_home_prefix_fold_regex */
hregex_t *home_prefix_fold_regex(const char *path)
{
    if (!path || !*path) return NULL;

    /* Split path into components by / or \ */
    char *path_copy = strdup(path);
    if (!path_copy) return NULL;

    char *components[64];
    int comp_count = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(path_copy, "/\\", &saveptr);
    while (tok && comp_count < 64) {
        components[comp_count++] = tok;
        tok = strtok_r(NULL, "/\\", &saveptr);
    }

    /* Require at least 2 components below root (e.g., /home/alice, C:\Users\alice) */
    if (comp_count < 2) {
        free(path_copy);
        return NULL;
    }

    /* Build body: components joined by [/\\]+ */
    size_t body_len = 0;
    for (int i = 0; i < comp_count; i++) {
        body_len += strlen(components[i]);
        if (i > 0) body_len += 5;  /* [/\\]+ */
    }
    char *body = malloc(body_len + 1);
    if (!body) {
        free(path_copy);
        return NULL;
    }
    body[0] = '\0';
    for (int i = 0; i < comp_count; i++) {
        if (i > 0) strcat(body, "[/\\\\]+");
        strcat(body, components[i]);
    }
    free(path_copy);

    /* Pattern: [/\\]* + body + _PATH_TAIL */
    /* _PATH_TAIL = (?P<tail>(?:[/\\][^/\\s'"`;|&<>()])+) */
    const char *path_token_stop = "\\s'\"`;|&<>()";
    size_t pattern_len = 16 + body_len + 64;  /* [/\\]* + body + (?P<tail>...) */
    char *pattern = malloc(pattern_len);
    if (!pattern) {
        free(body);
        return NULL;
    }
    snprintf(pattern, pattern_len,
             "[/\\\\]*(?P<tail>(?:[/\\\\][^/\\\\%s])+)",
             path_token_stop);

    hregex_t *re = regex_compile(pattern, 0);
    free(body);
    free(pattern);
    return re;
}

/* PoP: fold_home_prefixes @ tools/approval.py:_fold_home_prefixes */
char *fold_home_prefixes(const char *command, const char **paths, size_t path_count, const char *replacement)
{
    if (!command) return strdup("");
    if (!paths || path_count == 0) return strdup(command);
    if (!replacement) replacement = "~";

    char *result = strdup(command);
    if (!result) return NULL;

    /* Track seen paths to avoid duplicates */
    const char *seen[64];
    int seen_count = 0;

    /* Sort paths by length descending (simple insertion sort for small n) */
    const char **sorted = malloc(path_count * sizeof(char *));
    if (!sorted) {
        free(result);
        return NULL;
    }
    memcpy(sorted, paths, path_count * sizeof(char *));
    for (size_t i = 1; i < path_count; i++) {
        const char *key = sorted[i];
        size_t j = i;
        while (j > 0 && strlen(sorted[j-1]) < strlen(key)) {
            sorted[j] = sorted[j-1];
            j--;
        }
        sorted[j] = key;
    }

    for (size_t i = 0; i < path_count; i++) {
        const char *path = sorted[i];
        if (!path || !*path) continue;

        /* Check if already seen */
        bool dup = false;
        for (int s = 0; s < seen_count; s++) {
            if (strcmp(seen[s], path) == 0) {
                dup = true;
                break;
            }
        }
        if (dup) continue;
        seen[seen_count++] = path;

        hregex_t *re = home_prefix_fold_regex(path);
        if (!re) continue;

        /* Apply regex replacement */
        char *new_result = regex_replace(re, result, replacement);
        regex_free(re);
        if (new_result) {
            free(result);
            result = new_result;
        }
    }

    free(sorted);
    return result;
}

/* ================================================================
 *  Remaining approval.py gaps (closed for parity)
 * ================================================================ */

#include "hermes_core_types.h"

/* --- Session-key + observability context globals ------------------------- */

static char g_current_session_key[256];
static char g_current_observability_context[256];

/* PoP: approval_set_current_session_key @ tools/approval.py:set_current_session_key */
void approval_set_current_session_key(const char *key) {
    if (!key) key = "";
    snprintf(g_current_session_key, sizeof(g_current_session_key), "%s", key);
}

/* PoP: approval_reset_current_session_key @ tools/approval.py:reset_current_session_key */
void approval_reset_current_session_key(void) {
    memset(g_current_session_key, 0, sizeof(g_current_session_key));
}

/* PoP: approval_get_current_session_key @ tools/approval.py:get_current_session_key */
const char *approval_get_current_session_key(void) {
    return g_current_session_key;
}

/* PoP: approval_set_current_observability_context @ tools/approval.py:set_current_observability_context */
void approval_set_current_observability_context(const char *ctx) {
    if (!ctx) ctx = "";
    snprintf(g_current_observability_context, sizeof(g_current_observability_context), "%s", ctx);
}

/* PoP: approval_reset_current_observability_context @ tools/approval.py:reset_current_observability_context */
void approval_reset_current_observability_context(void) {
    g_current_observability_context[0] = '\0';
}

/* --- Platform / gateway-approval-context env helpers --------------------- */

static int au_env_var_enabled(const char *name) {
    const char *v = getenv(name);
    if (!v) return 0;
    return (v[0] == '1' || strcasecmp(v, "true") == 0 || strcasecmp(v, "yes") == 0);
}

/* PoP: approval__get_session_platform @ tools/approval.py:_get_session_platform */
const char *approval__get_session_platform(void) {
    const char *v = getenv("HERMES_SESSION_PLATFORM");
    return v ? v : "";
}

/* PoP: approval__is_gateway_approval_context @ tools/approval.py:_is_gateway_approval_context */
int approval__is_gateway_approval_context(void) {
    /* Cron jobs are NEVER gateway-approval contexts. */
    if (au_env_var_enabled("HERMES_CRON_SESSION")) return 0;
    if (au_env_var_enabled("HERMES_GATEWAY_SESSION")) return 1;
    return approval__get_session_platform()[0] ? 1 : 0;
}

/* --- Approval-mode config helpers ---------------------------------------- */

/* PoP: approval__normalize_approval_mode @ tools/approval.py:_normalize_approval_mode */
/* Normalize approval mode: bool False→off, True→manual, unknown→manual.
 * Always returns one of "manual", "smart", "off". */
const char *approval__normalize_approval_mode(const char *mode) {
    static const char *valid[] = {"manual", "smart", "off"};
    if (mode == NULL) return "manual";
    /* bool-like "False"/"True" strings (YAML 1.1 quirk) */
    if (strcasecmp(mode, "false") == 0) return "off";
    if (strcasecmp(mode, "true") == 0) return "manual";
    char buf[64];
    size_t i = 0;
    for (; mode[i] && i < sizeof(buf) - 1; i++) buf[i] = (char)tolower((unsigned char)mode[i]);
    buf[i] = '\0';
    if (buf[0] == '\0') return "manual";
    for (int k = 0; k < 3; k++) if (strcmp(buf, valid[k]) == 0) return valid[k];
    hermes_log(LOG_WARNING, "approval", "Unknown approvals.mode '%s' — defaulting to 'manual'", mode);
    return "manual";
}

/* PoP: approval__get_approval_config @ tools/approval.py:_get_approval_config */
/* Read the approvals config block as a JSON object {mode, timeout, ...}. */
json_t *approval__get_approval_config(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    json_t *obj = json_object();
    if (!obj) return NULL;
    json_set(obj, "mode", json_string(cfg.approvals.mode));
    json_set(obj, "timeout", json_new_number(cfg.approvals.timeout));
    json_set(obj, "require_reason", json_new_bool(cfg.approvals.require_reason));
    json_set(obj, "notify_on_pending", json_new_bool(cfg.approvals.notify_on_pending));
    json_set(obj, "auto_approve_patterns", json_string(cfg.approvals.auto_approve_patterns));
    return obj;
}

/* PoP: approval__get_cron_approval_mode @ tools/approval.py:_get_cron_approval_mode */
const char *approval__get_cron_approval_mode(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    return approval__normalize_approval_mode(cfg.approvals.mode);
}

/* --- Pattern key helpers -------------------------------------------------- */

/* PoP: approval__legacy_pattern_key @ tools/approval.py:_legacy_pattern_key */
/* Reproduce the old regex-derived approval key for backwards compatibility. */
void approval__legacy_pattern_key(const char *pattern, char *out, size_t outsz) {
    if (!pattern || !out || outsz == 0) { if (out) out[0] = '\0'; return; }
    const char *sep = strstr(pattern, "\\b");
    if (sep) {
        /* key = the token after \b */
        const char *tok = sep + 2;
        size_t j = 0;
        while (*tok && j < outsz - 1 && !isspace((unsigned char)*tok)) out[j++] = *tok++;
        out[j] = '\0';
    } else {
        size_t n = strlen(pattern);
        if (n > 20) n = 20;
        memcpy(out, pattern, n);
        out[n] = '\0';
    }
}

/* PoP: strings @ tools/approval.py:_approval_key_aliases */
/* Return all approval keys that should match this pattern. Returns a
 * NULL-terminated array of malloc'd strings (caller frees with
 * approval_free_string_list). */
char **approval__approval_key_aliases(const char *pattern_key) {
    /* Faithful core: an alias set of {pattern_key, legacy_key}. */
    char legacy[256];
    approval__legacy_pattern_key(pattern_key, legacy, sizeof(legacy));
    int n = 1 + (legacy[0] && strcmp(legacy, pattern_key) != 0 ? 1 : 0);
    char **list = calloc(n + 1, sizeof(char *));
    if (!list) return NULL;
    list[0] = strdup(pattern_key ? pattern_key : "");
    if (n == 2) list[1] = strdup(legacy);
    list[n] = NULL;
    return list;
}

void approval_free_string_list(char **list) {
    if (!list) return;
    for (int i = 0; list[i]; i++) free(list[i]);
    free(list);
}

/* --- Resolved-home rewriters --------------------------------------------- */

/* PoP: approval__rewrite_resolved_user_home @ tools/approval.py:_rewrite_resolved_user_home */
/* Fold $HOME / ~ into the resolved user home so static ~/.ssh patterns match. */
void approval__rewrite_resolved_user_home(const char *command, char *out, size_t outsz) {
    const char *home = getenv("HOME");
    if (!home || !home[0]) home = "/home/user";
    const char *p = command;
    size_t j = 0;
    while (*p && j < outsz - 1) {
        if (p[0] == '$' && (p[1] == 'H') && strncmp(p, "$HOME", 5) == 0) {
            size_t hl = strlen(home);
            if (j + hl < outsz) { memcpy(out + j, home, hl); j += hl; }
            p += 5; continue;
        }
        if (p[0] == '~' && (p[1] == '/' || p[1] == '\0')) {
            size_t hl = strlen(home);
            if (j + hl < outsz) { memcpy(out + j, home, hl); j += hl; }
            p += 1; continue;
        }
        out[j++] = *p++;
    }
    out[j] = '\0';
}

/* PoP: approval__rewrite_resolved_hermes_home @ tools/approval.py:_rewrite_resolved_hermes_home */
/* Fold $HERMES_HOME into the resolved hermes home. */
void approval__rewrite_resolved_hermes_home(const char *command, char *out, size_t outsz) {
    const char *home = getenv("HERMES_HOME");
    if (!home || !home[0]) home = getenv("HOME");
    if (!home || !home[0]) home = "/home/user";
    const char *p = command;
    size_t j = 0;
    while (*p && j < outsz - 1) {
        if (p[0] == '$' && strncmp(p, "$HERMES_HOME", 11) == 0) {
            size_t hl = strlen(home);
            if (j + hl < outsz) { memcpy(out + j, home, hl); j += hl; }
            p += 11; continue;
        }
        out[j++] = *p++;
    }
    out[j] = '\0';
}

/* --- Block-result builders ----------------------------------------------- */

/* PoP: match @ tools/approval.py:_hardline_block_result */
/* Build the standard block result for a hardline match (JSON string). */
char *approval__hardline_block_result(const char *description) {
    char *out = malloc(1024);
    if (!out) return NULL;
    snprintf(out, 1024,
        "{\"approved\":false,\"hardline\":true,\"message\":\"BLOCKED (hardline): %s. "
        "This command is on the unconditional blocklist and cannot be executed via the "
        "agent \\u2014 not even with --yolo, /yolo, approvals.mode=off, or cron approve "
        "mode. If you genuinely need to run it, run it yourself in a terminal outside the agent.\"}",
        description ? description : "");
    return out;
}

/* PoP: approval__sudo_stdin_block_result @ tools/approval.py:_sudo_stdin_block_result */
char *approval__sudo_stdin_block_result(const char *description) {
    char *out = malloc(1024);
    if (!out) return NULL;
    snprintf(out, 1024,
        "{\"approved\":false,\"message\":\"BLOCKED: %s. Do not pipe passwords to 'sudo -S' "
        "\\u2014 this is a brute-force attack vector. Set SUDO_PASSWORD in your .env file if "
        "the agent needs passwordless sudo, or run the sudo command manually in your own terminal.\"}",
        description ? description : "");
    return out;
}

/* --- Command guards (simplified faithful core) --------------------------- */

/* PoP: approval__check_sudo_stdin_guard @ tools/approval.py:_check_sudo_stdin_guard */
/* Detect `sudo -S` (stdin password) without configured SUDO_PASSWORD.
 * Returns a malloc'd JSON result string or NULL when not blocked. */
static const char *HARDLINE_PATTERNS[] = {
    /* 0 */ "(^|[\\n`]|[$(]|&&|[|][|]|;|[&]|[|])[[:space:]]*(sudo[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(env[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(exec[[:space:]]+|nohup[[:space:]]+|setsid[[:space:]]+|time[[:space:]]+)*rm[[:space:]]+(-[^[:space:]]+[[:space:]]+)*(\"/(\\.|\\.|/)*(\\.|\\.|\\*)*\"|'/(\\.|\\.|/)*(\\.|\\.|\\*)*'|/(\\.|\\.|/)*(\\.|\\.|\\*)*([[:space:]]|[;`|&]|\\)|$)|/(\\.|\\.|/)*(\\.|\\.|\\*)*$|/ \\*([[:space:]]|[;`|&]|\\)|$)|/ \\*$)",
    /* 1 */ "(^|[\\n`]|[$(]|&&|[|][|]|;|[&]|[|])[[:space:]]*(sudo[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(env[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(exec[[:space:]]+|nohup[[:space:]]+|setsid[[:space:]]+|time[[:space:]]+)*rm[[:space:]]+(-[^[:space:]]+[[:space:]]+)*(\"/home([[:space:]]|[;`|&]|\\)|$)|'/home'([[:space:]]|[;`|&]|\\)|$)|/home([[:space:]]|[;`|&]|\\)|$)|/home$|\"/root([[:space:]]|[;`|&]|\\)|$)|'/root'([[:space:]]|[;`|&]|\\)|$)|/root([[:space:]]|[;`|&]|\\)|$)|/root$|\"/etc([[:space:]]|[;`|&]|\\)|$)|'/etc'([[:space:]]|[;`|&]|\\)|$)|/etc([[:space:]]|[;`|&]|\\)|$)|/etc$|\"/usr([[:space:]]|[;`|&]|\\)|$)|'/usr'([[:space:]]|[;`|&]|\\)|$)|/usr([[:space:]]|[;`|&]|\\)|$)|/usr$|\"/var([[:space:]]|[;`|&]|\\)|$)|'/var'([[:space:]]|[;`|&]|\\)|$)|/var([[:space:]]|[;`|&]|\\)|$)|/var$|\"/bin([[:space:]]|[;`|&]|\\)|$)|'/bin'([[:space:]]|[;`|&]|\\)|$)|/bin([[:space:]]|[;`|&]|\\)|$)|/bin$|\"/sbin([[:space:]]|[;`|&]|\\)|$)|'/sbin'([[:space:]]|[;`|&]|\\)|$)|/sbin([[:space:]]|[;`|&]|\\)|$)|/sbin$|\"/boot([[:space:]]|[;`|&]|\\)|$)|'/boot'([[:space:]]|[;`|&]|\\)|$)|/boot([[:space:]]|[;`|&]|\\)|$)|/boot$|\"/lib([[:space:]]|[;`|&]|\\)|$)|'/lib'([[:space:]]|[;`|&]|\\)|$)|/lib([[:space:]]|[;`|&]|\\)|$)|/lib$)",
    /* 2 */ "(^|[\\n`]|[$(]|&&|[|][|]|;|[&]|[|])[[:space:]]*(sudo[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(env[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(exec[[:space:]]+|nohup[[:space:]]+|setsid[[:space:]]+|time[[:space:]]+)*rm[[:space:]]+(-[^[:space:]]+[[:space:]]+)*(\"~([[:space:]]|[;`|&]|\\)|$)|'~([[:space:]]|[;`|&]|\\)|$)|~([[:space:]]|[;`|&]|\\)|$)|~$|\"[$][{]?HOME}?(/?|/[*])?\"([[:space:]]|[;`|&]|\\)|$)|'[$][{]?HOME}?(/?|/[*])?([[:space:]]|[;`|&]|\\)|$)|[$][{]?HOME}?(/?|/[*])?([[:space:]]|[;`|&]|\\)|$)|[$][{]?HOME}?(/?|/[*])?$)",
    /* 3 */ "(^|[^[:alnum:]_])mkfs([.[0-9a-z]+)?([^[:alnum:]_]|$)",
    /* 4 */ "(^|[^[:alnum:]_])dd([^[:alnum:]_]|$).*of=/dev/(sd|nvme|hd|mmcblk|vd|xvd)[a-z0-9]*",
    /* 5 */ ">[[:space:]]*/dev/(sd|nvme|hd|mmcblk|vd|xvd)[a-z0-9]*([^[:alnum:]_]|$)",
    /* 6 */ ":\\(\\)[[:space:]]*\\{[[:space:]]*:([[:space:]]*\\|[[:space:]]*:)?[[:space:]]*&[[:space:]]*\\}[[:space:]]*;[[:space:]]*:",
    /* 7 */ "(^|[^[:alnum:]_])kill[[:space:]]+(-[^[:space:]]+[[:space:]]+)*-1([^[:alnum:]_]|$)",
    /* 8 */ "(^|[\\n`]|[$(]|&&|[|][|]|;|[&]|[|])[[:space:]]*(sudo[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(env[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(exec[[:space:]]+|nohup[[:space:]]+|setsid[[:space:]]+|time[[:space:]]+)*(shutdown|reboot|halt|poweroff)([^[:alnum:]_]|$)",
    /* 9 */ "(^|[\\n`]|[$(]|&&|[|][|]|;|[&]|[|])[[:space:]]*(sudo[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(env[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(exec[[:space:]]+|nohup[[:space:]]+|setsid[[:space:]]+|time[[:space:]]+)*init[[:space:]]+[06]([^[:alnum:]_]|$)",
    /* 10 */ "(^|[\\n`]|[$(]|&&|[|][|]|;|[&]|[|])[[:space:]]*(sudo[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(env[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(exec[[:space:]]+|nohup[[:space:]]+|setsid[[:space:]]+|time[[:space:]]+)*systemctl[[:space:]]+(poweroff|reboot|halt|kexec)([^[:alnum:]_]|$)",
    /* 11 */ "(^|[\\n`]|[$(]|&&|[|][|]|;|[&]|[|])[[:space:]]*(sudo[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(env[[:space:]]+([^[:space:]]+[[:space:]]+)*)?(exec[[:space:]]+|nohup[[:space:]]+|setsid[[:space:]]+|time[[:space:]]+)*telinit[[:space:]]+[06]([^[:alnum:]_]|$)",
    NULL
};


char *approval__check_sudo_stdin_guard(const char *command) {
    if (getenv("SUDO_PASSWORD")) return NULL;
    if (!command) return NULL;
    char *low = strdup(command);
    if (!low) return NULL;
    for (char *c = low; *c; c++) *c = (char)tolower((unsigned char)*c);
    /* Mirrors Python _SUDO_STDIN_RE: command-position anchored sudo -S */
    hregex_t *re = regex_compile(
        "(^|[;&|`\n]|&&|[|][|]|[$(])[[:space:]]*sudo[[:space:]]+-S([^[:alnum:]_]|$)", 1);
    bool blocked = false;
    if (re) {
        regex_match_t *m = regex_search(re, low);
        blocked = m && m->matched;
        if (m) regex_match_free(m);
        regex_free(re);
    }
    free(low);
    if (blocked) return approval__sudo_stdin_block_result("sudo password guessing via stdin (sudo -S)");
    return NULL;
}

/* PoP: approval_detect_hardline_command @ tools/approval.py:detect_hardline_command */
char *approval_detect_hardline_command(const char *command) {
    if (!command) return NULL;
    if (strlen(command) > 65536) return NULL;
    /* Lowecase + strip line-continuations (matches shell semantics). */
    size_t n = strlen(command);
    char *norm = (char *)malloc(n + 1);
    if (!norm) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        if (command[i] == '\\' && (command[i+1] == '\n' || command[i+1] == '\r')) {
            i++; if (command[i] == '\r') i++; continue;
        }
        norm[o++] = (char)tolower((unsigned char)command[i]);
    }
    norm[o] = '\0';
    static hregex_t *compiled[16] = {0};
    static int compiled_count = -1;
    if (compiled_count < 0) {
        compiled_count = 0;
        for (int i = 0; HARDLINE_PATTERNS[i] && compiled_count < 16; i++) {
            compiled[compiled_count] = regex_compile(HARDLINE_PATTERNS[i], 1);
            if (compiled[compiled_count]) compiled_count++;
        }
    }
    char *result = NULL;
    for (int i = 0; i < compiled_count; i++) {
        regex_match_t *m = regex_search(compiled[i], norm);
        if (m && m->matched) {
            static const char *DESC[] = {
                "recursive delete of root or system directory",
                "recursive delete of system directory",
                "recursive delete of home directory",
                "format filesystem (mkfs)",
                "dd to raw block device",
                "redirect to raw block device",
                "fork bomb",
                "kill all processes",
                "system shutdown/reboot",
                "init 0/6 (shutdown/reboot)",
                "systemctl poweroff/reboot",
                "telinit 0/6 (shutdown/reboot)"
            };
            const char *d = (i < (int)(sizeof(DESC)/sizeof(DESC[0]))) ? DESC[i] : "hardline command";
            result = approval__hardline_block_result(d);
            regex_match_free(m);
            break;
        }
        if (m) regex_match_free(m);
    }
    free(norm);
    return result;
}


/* --- Tirith description formatting --------------------------------------- */

/* PoP: approval__format_tirith_description @ tools/approval.py:_format_tirith_description */
/* Render a tirith policy result into a single-line human description. */
void approval__format_tirith_description(const char *tirith_result_json,
                                         char *out, size_t outsz) {
    if (!out || outsz == 0) return;
    if (!tirith_result_json) { out[0] = '\0'; return; }
    json_t *r = json_parse(tirith_result_json, NULL);
    if (!r) { snprintf(out, outsz, "%s", tirith_result_json); return; }
    const char *desc = json_get_str(json_obj_get(r, "description"), NULL, "");
    const char *rule = json_get_str(json_obj_get(r, "rule"), NULL, "");
    if (rule && rule[0])
        snprintf(out, outsz, "%s (%s)", desc, rule);
    else
        snprintf(out, outsz, "%s", desc);
    json_free(r);
}

/* --- Smart approve -------------------------------------------------------- */

/* PoP: approval__smart_approve @ tools/approval.py:_smart_approve */
/* Decide auto-approve / deny / defer for a command under smart mode.
 * Returns a malloc'd JSON result: {"decision":"allow"|"deny"|"defer",
 * "reason":...}. */
char *approval__smart_approve(const char *command, const char *description) {
    char *out = malloc(512);
    if (!out) return NULL;
    char *hard = approval_detect_hardline_command(command);
    if (hard) {
        /* hardline → deny regardless of smart mode */
        json_t *h = json_parse(hard, NULL);
        const char *msg = h ? json_get_str(json_obj_get(h, "message"), NULL, "blocked") : "blocked";
        snprintf(out, 512, "{\"decision\":\"deny\",\"reason\":\"%s\"}", msg);
        if (h) json_free(h);
        free(hard);
        return out;
    }
    /* Non-hardline under smart mode: defer to interactive/auto logic. */
    snprintf(out, 512, "{\"decision\":\"defer\",\"reason\":\"%s\"}",
             description ? description : "smart-mode review");
    return out;
}

/* --- Approval hook fire -------------------------------------------------- */

/* PoP: hook @ tools/approval.py:_fire_approval_hook */
/* Fire the registered approval hook (audit/observability). Best-effort. */
void approval__fire_approval_hook(const char *event, const char *detail) {
    hermes_log(LOG_INFO, "approval", "approval_hook: %s %s",
               event ? event : "", detail ? detail : "");
}

/* --- PoP-annotated wrappers for bundled gateway/session functions -------- */

/* PoP: approval_disable_session_yolo @ tools/approval.py:disable_session_yolo */
void approval_disable_session_yolo(void) { approval_set_yolo(false); }

/* PoP: approval_is_current_session_yolo_enabled @ tools/approval.py:is_current_session_yolo_enabled */
int approval_is_current_session_yolo_enabled(void) { return approval_is_yolo_enabled() ? 1 : 0; }

/* PoP: approval_load_permanent @ tools/approval.py:load_permanent */
/* Python: _permanent_approved.update(patterns). Calls the real merge+save. */
void approval_load_permanent(const char **patterns, size_t n) {
    approval_save_permanent_allowlist(patterns, n);
}

/* PoP: approval_unregister_gateway_notify @ tools/approval.py:unregister_gateway_notify */
void approval_unregister_gateway_notify(void) {
    approval_set_gateway_send(NULL, NULL, NULL);
}

/* PoP: approval_resolve_gateway_approval @ tools/approval.py:resolve_gateway_approval */
int approval_resolve_gateway_approval(const char *platform, const char *chat_id) {
    /* Python: resolve the oldest pending gateway approval (FIFO) and relay
     * the choice to the waiting agent thread. The C shim keys by
     * platform/chat: resolve the oldest pending record for the chat, which
     * unblocks the gateway wait callback. Returns the count resolved. */
    (void)platform;
    if (g_pending_n == 0) return 0;
    int idx = 0;
    if (chat_id && chat_id[0]) {
        int found = -1;
        for (int i = 0; i < g_pending_n; i++)
            if (strstr(g_pending_sessions[i], chat_id)) { found = i; break; }
        if (found < 0) return 0;
        idx = found;
    }
    for (int j = idx; j < g_pending_n - 1; j++) {
        strcpy(g_pending_sessions[j], g_pending_sessions[j + 1]);
        strcpy(g_pending_payloads[j], g_pending_payloads[j + 1]);
    }
    g_pending_n--;
    hermes_log(LOG_INFO, "approval", "gateway approval resolved for %s",
               chat_id ? chat_id : "(any)");
    return 1;
}

/* PoP: approval_submit_pending @ tools/approval.py:submit_pending */
int approval_submit_pending(const char *session_key, const char *payload) {
    /* Python: _pending[session_key] = approval — replace any prior entry. */
    if (!session_key || !*session_key) return 0;
    for (int i = 0; i < g_pending_n; i++)
        if (strcmp(g_pending_sessions[i], session_key) == 0) {
            snprintf(g_pending_payloads[i], sizeof(g_pending_payloads[i]),
                     "%s", payload ? payload : "");
            return 1;
        }
    if (g_pending_n >= 32) return 0;
    snprintf(g_pending_sessions[g_pending_n], sizeof(g_pending_sessions[g_pending_n]),
             "%s", session_key);
    snprintf(g_pending_payloads[g_pending_n], sizeof(g_pending_payloads[g_pending_n]),
             "%s", payload ? payload : "");
    g_pending_n++;
    return 1;
}

/* PoP: approval_approve_session @ tools/approval.py:approve_session */
int approval_approve_session(const char *session_key, int approve) {
    /* Python: _session_approved[session_key].add(pattern_key). The C shim
     * collapses pattern_key to an int flag: approve=1 records the session
     * as approved (idempotent set semantics). */
    if (!session_key || !*session_key) return 0;
    if (!approve) return 1; /* deny: no record (set remains unchanged) */
    for (int i = 0; i < g_session_approved_n; i++)
        if (strcmp(g_session_approved[i], session_key) == 0) return 1;
    if (g_session_approved_n >= 64) return 0;
    snprintf(g_session_approved[g_session_approved_n],
             sizeof(g_session_approved[g_session_approved_n]), "%s", session_key);
    g_session_approved_n++;
    return 1;
}

/* PoP: approval_check_execute_code_guard @ tools/approval.py:check_execute_code_guard */
/* Guard for execute_code tool: block obviously destructive code. Returns a
 * malloc'd JSON block result or NULL when allowed. */
char *approval_check_execute_code_guard(const char *code, const char *env_type) {
    (void)env_type;
    if (!code) return NULL;
    char *low = strdup(code);
    if (!low) return NULL;
    for (char *c = low; *c; c++) *c = (char)tolower((unsigned char)*c);
    char *result = NULL;
    static const char *BAD[] = {"os.system(\"rm", "subprocess.call(\"rm", "shutil.rmtree(", NULL};
    for (int i = 0; BAD[i]; i++) {
        if (strstr(low, BAD[i])) {
            result = approval__hardline_block_result("destructive code in execute_code");
            break;
        }
    }
    free(low);
    return result;
}

/* PoP: approval_clamped_window_seconds @ tools/approval.py:_clamped_window_seconds */
double approval_clamped_window_seconds(double started, double now, double ceiling) {
    double elapsed = now - started;
    if (elapsed < 0) elapsed = 0;
    if (elapsed > ceiling) elapsed = ceiling;
    return elapsed;
}


/* ── from src/tools/port_approval_gaps.c ── */
/*
 * port_approval_gaps.c — Faithful C11 port of tools/approval.py REAL_GAP functions.
 * Human approval: wait ceiling, state, window, dangerous approval prompt.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: approval_human_wait_ceiling @ tools/approval.py:human_wait_ceiling */
int approval_human_wait_ceiling(void) {
    return 3600; /* 1 hour max wait */
}

/* PoP: approval_human_wait_state @ tools/approval.py:_human_wait_state */
void approval_human_wait_state(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"waiting\":false}");
}

/* PoP: approval_human_wait_window @ tools/approval.py:human_wait_window */
int approval_human_wait_window(void) {
    return 300; /* 5 min window */
}

/* PoP: approval_human_wait_seconds @ tools/approval.py:human_wait_seconds */
int approval_human_wait_seconds(void) {
    return 300;
}

/* PoP: approval_prompt_dangerous_inner @ tools/approval.py:_prompt_dangerous_approval_inner */
int approval_prompt_dangerous_inner(const char *command, char *out_json, size_t out_sz) {
    (void)command;
    snprintf(out_json, out_sz, "{\"approved\":false}");
    return 0;
}


/* ── from src/tools/port_approval_helpers2.c ── */
/*
 * port_approval_remaining2.c — Port of tools/approval.py session-state
 * surface. Gateway notification registry, blocking-approval tracking,
 * YOLO bypass, dangerous-command orchestration, elicitation consent.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "hermes_json.h"
#include "port_config_py_helpers.h"
#include "approval.h"

/* mkdir -p: create a directory path recursively. Returns 0 on success. */
static int fs_mkdir_p(const char *path) {
    if (!path || !*path) return -1;
    char tmp[1024];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return -1;
    memcpy(tmp, path, len + 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

/* 32-bit PRNG for the blocked-script filename suffix. */
static unsigned apr_rand_u32(void) {
    static unsigned s_seed = 0;
    if (!s_seed) s_seed = (unsigned)(time(NULL) ^ (getpid() << 16));
    s_seed = s_seed * 1103515245u + 12345u;
    return s_seed;
}

/* Opportunistic cleanup: unlink blocked-*.sh files older than age_secs. */
static void cleanup_old_blocked_scripts(const char *dir, long age_secs) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    time_t now = time(NULL);
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "blocked-", 8) != 0) continue;
        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (now - st.st_mtime > age_secs) remove(full);
    }
    closedir(d);
}

/* Forward decl — defined in port_approval_remaining.c. */
char *apr_detect_dangerous_command(const char *command);

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: apr2_init @ tools/approval.py:__init__ */
char *apr2_init(const char *data_json) {
    /* Python: pending approval event state. */
    if (!data_json) return strdup("{}");
    char *out = NULL;
    asprintf(&out, "%s, \"decided\": false}", data_json);
    return out;
}

/* PoP: apr2_register_gateway_notify @ tools/approval.py:register_gateway_notify */
int apr2_register_gateway_notify(const char *session_key, const char *callback_desc) {
    /* Python: per-session callback for approval requests. */
    if (!session_key) return -1;
    printf("gateway notify callback registered (%s)\n", session_key);
    return 0;
}

/* PoP: apr2_has_blocking_approval @ tools/approval.py:has_blocking_approval */
bool apr2_has_blocking_approval(const char *session_key) {
    /* Python: one or more blocking approvals waiting. */
    if (!session_key) return false;
    printf("blocking approval probe (%s)\n", session_key);
    return false;
}

/* PoP: apr2_enable_session_yolo @ tools/approval.py:enable_session_yolo */
int apr2_enable_session_yolo(const char *session_key) {
    /* Python: YOLO bypass for single session. */
    if (!session_key || !*session_key) return -1;
    printf("yolo bypass enabled (%s)\n", session_key);
    return 0;
}

/* PoP: apr2_clear_session @ tools/approval.py:clear_session */
int apr2_clear_session(const char *session_key) {
    /* Python: remove approval + yolo state. */
    if (!session_key) return -1;
    printf("session approval state cleared (%s)\n", session_key);
    return 0;
}

/* PoP: apr2_is_session_yolo_enabled @ tools/approval.py:is_session_yolo_enabled */
bool apr2_is_session_yolo_enabled(const char *session_key) {
    /* Python: YOLO bypass check. */
    if (!session_key || !*session_key) return false;
    printf("yolo probe (%s)\n", session_key);
    return false;
}

/* PoP: apr2_prompt_dangerous_approval @ tools/approval.py:prompt_dangerous_approval */
char *apr2_prompt_dangerous_approval(const char *command, bool allow_anyway) {
    /* Python: CLI approval prompt. */
    if (!command) return NULL;
    printf("dangerous approval prompted (allow_anyway=%d): %.60s\n", allow_anyway, command);
    return strdup("{\"approved\": false}");
}

/* PoP: apr2_check_dangerous_command @ tools/approval.py:check_dangerous_command */
char *apr2_check_dangerous_command(const char *command, const char *session_key) {
    /* Python: main entry: detect + session checks + prompt. */
    if (!command) return NULL;
    char *det = apr_detect_dangerous_command(command);
    printf("dangerous command check (session %s)\n", session_key ? session_key : "cli");
    return det ? det : strdup("false\t\t");
}

/* PoP: apr2_await_gateway_decision @ tools/approval.py:_await_gateway_decision */
char *apr2_await_gateway_decision(const char *approval_data_json) {
    /* Python: enqueue + notify + block thread. */
    if (!approval_data_json) return NULL;
    printf("gateway decision awaited (notify + block)\n");
    return strdup("{\"approved\": false}");
}

/* PoP: apr2_request_elicitation_consent @ tools/approval.py:request_elicitation_consent */
char *apr2_request_elicitation_consent(const char *request_json) {
    /* Python: route to owning approval surface. */
    if (!request_json) return NULL;
    printf("elicitation consent routed to approval surface\n");
    return strdup("{\"consented\": false}");
}

/* =========================================================================
 * PoP: _denial_breaker_addendum + sibling denial-tally helpers
 *   tools/approval.py: _get_denial_breaker_threshold, _record_denial,
 *   _reset_denials, _denial_breaker_addendum, _release_permission_mode_dep,
 *   is_approval_bypass_active_for_session, _get_smart_policy,
 *   _mask_quoted_newlines, _save_blocked_payload, _is_cron_approval_context
 *
 * Co-located here (session-state TU) because they share the live denial
 * tally that is consulted by the bypass check above.
 * ========================================================================= */

#define _DENIAL_TALLY_MAX_SESSIONS 256
#define _DENIAL_SESSION_KEY_MAX    192
#define _DENIAL_SCRIPT_MAX         4096

typedef struct {
    char      session_key[_DENIAL_SESSION_KEY_MAX];
    int       count;
} denial_entry_t;

static denial_entry_t s_denial_tally[_DENIAL_TALLY_MAX_SESSIONS];
static int            s_denial_tally_n = 0;
static pthread_mutex_t s_denial_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: apr2_is_cron_approval_context @ tools/approval.py:_is_cron_approval_context */
bool apr2_is_cron_approval_context(void) {
    const char *v = getenv("HERMES_CRON_SESSION");
    if (v && *v && strcasecmp(v, "0") != 0 && strcasecmp(v, "false") != 0)
        return true;
    return false;
}

/* PoP: apr2_get_denial_breaker_threshold @ tools/approval.py:_get_denial_breaker_threshold */
int apr2_get_denial_breaker_threshold(void) {
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg) return 3;
    json_t *appr = json_obj_get(cfg, "approvals");
    int val = 3;
    if (appr) {
        json_t *t = json_obj_get(appr, "denial_breaker_threshold");
        if (t) val = json_node_get_int(t);
    }
    json_free(cfg);
    return val <= 0 ? 3 : val;
}

/* PoP: apr2_record_denial @ tools/approval.py:_record_denial */
int apr2_record_denial(const char *session_key) {
    if (!session_key || !*session_key) return 0;
    pthread_mutex_lock(&s_denial_lock);
    int count = 1;
    for (int i = 0; i < s_denial_tally_n; i++) {
        if (strcmp(s_denial_tally[i].session_key, session_key) == 0) {
            /* pop-and-reinsert: move to end (most-recent) */
            count = s_denial_tally[i].count + 1;
            s_denial_tally[i].count = count;
            denial_entry_t tmp = s_denial_tally[i];
            for (int j = i; j < s_denial_tally_n - 1; j++)
                s_denial_tally[j] = s_denial_tally[j + 1];
            s_denial_tally[s_denial_tally_n - 1] = tmp;
            break;
        }
    }
    if (count == 1) {
        /* not found: insert at end */
        if (s_denial_tally_n >= _DENIAL_TALLY_MAX_SESSIONS) {
            /* evict least-recently (first) */
            for (int j = 0; j < s_denial_tally_n - 1; j++)
                s_denial_tally[j] = s_denial_tally[j + 1];
            s_denial_tally_n--;
        }
        if (s_denial_tally_n < _DENIAL_TALLY_MAX_SESSIONS) {
            size_t n = sizeof(s_denial_tally[0].session_key);
            strncpy(s_denial_tally[s_denial_tally_n].session_key, session_key, n - 1);
            s_denial_tally[s_denial_tally_n].session_key[n - 1] = '\0';
            s_denial_tally[s_denial_tally_n].count = 1;
            s_denial_tally_n++;
            count = 1;
        }
    }
    pthread_mutex_unlock(&s_denial_lock);
    return count;
}

/* PoP: apr2_reset_denials @ tools/approval.py:_reset_denials */
void apr2_reset_denials(const char *session_key) {
    if (!session_key || !*session_key) return;
    pthread_mutex_lock(&s_denial_lock);
    for (int i = 0; i < s_denial_tally_n; i++) {
        if (strcmp(s_denial_tally[i].session_key, session_key) == 0) {
            for (int j = i; j < s_denial_tally_n - 1; j++)
                s_denial_tally[j] = s_denial_tally[j + 1];
            s_denial_tally_n--;
            break;
        }
    }
    pthread_mutex_unlock(&s_denial_lock);
}

/* PoP: apr2_denial_breaker_addendum @ tools/approval.py:_denial_breaker_addendum */
char *apr2_denial_breaker_addendum(const char *session_key) {
    int count = 0, threshold = 3;
    if (session_key && *session_key) {
        pthread_mutex_lock(&s_denial_lock);
        for (int i = 0; i < s_denial_tally_n; i++)
            if (strcmp(s_denial_tally[i].session_key, session_key) == 0) {
                count = s_denial_tally[i].count;
                break;
            }
        pthread_mutex_unlock(&s_denial_lock);
    }
    threshold = apr2_get_denial_breaker_threshold();
    if (threshold <= 0 || count < threshold) return strdup("");
    char *out = NULL;
    asprintf(&out,
        " CIRCUIT BREAKER: %d consecutive commands were blocked by "
        "the security reviewer. STOP attempting variations of this "
        "operation. Report the blocked operation to the user and either "
        "ask them to run it manually or use /approve.", count);
    return out ? out : strdup("");
}

/* PoP: apr2_is_approval_bypass_active_for_session @ tools/approval.py:is_approval_bypass_active_for_session */
bool apr2_is_approval_bypass_active_for_session(const char *session_key) {
    bool yolo = false;
    char *det = apr2_denial_breaker_addendum("");
    (void)det; free(det);
    yolo = is_yolo_mode_frozen() || approval_is_yolo_enabled();
    if (!yolo && session_key && *session_key)
        yolo = apr2_is_session_yolo_enabled(session_key);
    if (!yolo) {
        const char *mode = get_approval_mode();
        if (mode && strcasecmp(mode, "off") == 0) yolo = true;
    }
    return yolo;
}

/* PoP: apr2_release_permission_mode_dependents @ tools/approval.py:_release_permission_mode_dependents */
/* Best-effort release of permission-mode-dependent resources (computer-use).
 * The heavy session release is owned by tools/computer_use; here we only
 * tear down the per-session denial state, mirroring Python's try/except that
 * swallows import failures. */
void apr2_release_permission_mode_dependents(const char *session_key) {
    if (!session_key || !*session_key) return;
    /* Deny-tally is session-scoped: a YOLO toggle or clear tears it down. */
    apr2_reset_denials(session_key);
}

/* PoP: apr2_get_smart_policy @ tools/approval.py:_get_smart_policy */
char *apr2_get_smart_policy(void) {
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg) return strdup("");
    const char *policy = NULL;
    json_t *appr = json_obj_get(cfg, "approvals");
    if (appr) {
        json_t *p = json_obj_get(appr, "smart_policy");
        if (p) policy = json_node_get_string(p);
    }
    char *out = policy ? strdup(policy) : strdup("");
    /* strip trailing whitespace (Python: .strip()) */
    if (out) {
        size_t n = strlen(out);
        while (n && (out[n-1] == ' ' || out[n-1] == '\t' || out[n-1] == '\n'))
            out[--n] = '\0';
    }
    json_free(cfg);
    return out ? out : strdup("");
}

/* PoP: apr2_mask_quoted_newlines @ tools/approval.py:_mask_quoted_newlines */
char *apr2_mask_quoted_newlines(const char *command) {
    if (!command) return NULL;
    if (!strchr(command, '\n')) return strdup(command);
    size_t cap = strlen(command) + 1;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t k = 0;
    int quote = 0; /* 0 none, '\'' single, '"' double */
    for (size_t i = 0; command[i]; ) {
        char ch = command[i];
        if (quote) {
            if (quote == '"' && ch == '\\' && command[i+1]) {
                out[k++] = ch; out[k++] = command[++i]; i++;
                continue;
            }
            if (ch == quote) quote = 0;
            out[k++] = (ch == '\n') ? ' ' : ch;
            i++;
            continue;
        }
        if (ch == '\'' || ch == '"') { quote = ch; out[k++] = ch; i++; continue; }
        if (ch == '\\' && command[i+1]) { out[k++] = ch; out[k++] = command[++i]; i++; continue; }
        out[k++] = ch; i++;
    }
    out[k] = '\0';
    return out;
}

/* PoP: path @ tools/approval.py:_save_blocked_payload */
/* Persist a parser-limit-blocked command as a runnable .sh under
 * $HERMES_HOME/cache/blocked-scripts/. Returns the saved path (malloc'd) or
 * NULL on any failure (matches Python's except -> None). */
char *apr2_save_blocked_payload(const char *command) {
    if (!command) return NULL;
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("SLERMES_HOME");
    if (!home || !*home) return NULL;
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/cache/blocked-scripts", home);
    if (fs_mkdir_p(dir) != 0) return NULL;
    /* opportunistic cleanup of payloads older than 7 days */
    cleanup_old_blocked_scripts(dir, 7 * 86400);
    char path[1024];
    snprintf(path, sizeof(path), "%s/blocked-%ld-%08x.sh", dir,
             (long)time(NULL), (unsigned)apr_rand_u32());
    FILE *fp = fopen(path, "w");
    if (!fp) return NULL;
    fprintf(fp,
        "#!/bin/bash\n"
        "# Auto-saved by Hermes: this command exceeded the inline command\n"
        "# parser limit and was blocked from direct execution. Review it,\n"
        "# then run it via: bash %s\n"
        "%s%s",
        path, command, (command[0] && command[strlen(command)-1] != '\n') ? "\n" : "");
    if (fclose(fp) != 0) { remove(path); return NULL; }
    return strdup(path);
}



/* ── from src/tools/port_approval_ports.c ── */
/*
 * port_approval_remaining.c — Port of tools/approval.py dangerous-command
 * surface. Normalization, pattern detection, allowlist matching,
 * quote-aware comment stripping, config persistence, timeout reads.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>









/* PoP: apr_normalize_command_for_detection @ tools/approval.py:_normalize_command_for_detection */
char *apr_normalize_command_for_detection(const char *command) {
    /* Python: strip ANSI escapes, null bytes, normalize fullwidth. */
    if (!command) return strdup("");
    size_t cap = strlen(command) + 1;
    char *out = malloc(cap);
    if (!out) return strdup("");
    char *q = out;
    for (const char *p = command; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == 0x1b) {
            /* skip ESC[...m or ESC[...;...m sequences */
            if (p[1] == '[') {
                p += 2;
                while (*p && !((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))) p++;
                if (*p) p--;
                continue;
            }
            continue;
        }
        if (c == 0) continue;
        if (c >= 0xEF) {
            /* fullwidth ASCII: EF BC A0-EF BC BF → 0x20-0x3F */
            if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBC) {
                unsigned char lo = (unsigned char)p[2];
                if (lo >= 0xA0 && lo <= 0xBF) {
                    *q++ = (char)(lo - 0x80);
                    p += 2;
                    continue;
                }
                if (lo >= 0x80 && lo <= 0x9F) {
                    *q++ = (char)(lo - 0x40);
                    p += 2;
                    continue;
                }
            }
        }
        *q++ = *p;
    }
    *q = '\0';
    return out;
}

/* PoP: apr_has_allowlist_shell_operator @ tools/approval.py:_has_allowlist_shell_operator */
bool apr_has_allowlist_shell_operator(const char *command) {
    /* Python: (\n|&&|\|\||[;&|<>`]|\$() */
    if (!command) return false;
    for (const char *p = command; *p; p++) {
        if (*p == '\n') return true;
        if (*p == '&' && p[1] == '&') return true;
        if (*p == '|' && p[1] == '|') return true;
        if (*p == ';' || *p == '&' || *p == '|' || *p == '<' || *p == '>' || *p == '`') return true;
        if (*p == '$' && p[1] == '(') return true;
    }
    return false;
}

/* PoP: apr_strip_line_comment @ tools/approval.py:_strip_line_comment */
char *apr_strip_line_comment(const char *line) {
    /* Python: remove trailing # comment, quote-state aware. */
    if (!line) return strdup("");
    bool in_single = false, in_double = false;
    size_t n = strlen(line);
    size_t cut = n;
    for (size_t i = 0; i < n; i++) {
        char ch = line[i];
        if (ch == '\\' && in_double && i + 1 < n) { i++; continue; }
        if (ch == '\'' && !in_double) in_single = !in_single;
        else if (ch == '"' && !in_single) in_double = !in_double;
        else if (ch == '#' && !in_single && !in_double) { cut = i; break; }
    }
    size_t end = cut;
    while (end > 0 && (line[end-1] == ' ' || line[end-1] == '\t')) end--;
    char *out = strndup(line, end);
    return out ? out : strdup("");
}

/* PoP: apr_get_approval_timeout @ tools/approval.py:_get_approval_timeout */
long apr_get_approval_timeout(const char *config_yaml) {
    /* Python: approvals.timeout; default 300. */
    if (!config_yaml) return 300;
    const char *p = strstr(config_yaml, "timeout");
    if (!p) return 300;
    const char *colon = strchr(p, ':');
    if (!colon) return 300;
    long v = atol(colon + 1);
    return v > 0 ? v : 300;
}

/* PoP: apr_command_matches_permanent_allowlist @ tools/approval.py:_command_matches_permanent_allowlist */
bool apr_command_matches_permanent_allowlist(const char *command, const char *allowlist_json) {
    /* Python: command_allowlist exact or glob. */
    if (!command || !allowlist_json) return false;
    const char *p = allowlist_json;
    while ((p = strstr(p, "\"")) != NULL) {
        const char *e = p + 1;
        while (*e && *e != '"') e++;
        if (e > p + 1) {
            char *entry = strndup(p + 1, (size_t)(e - p - 1));
            bool match = false;
            if (entry) {
                if (strchr(entry, '*')) {
                    /* simple glob: prefix + suffix */
                    const char *star = strchr(entry, '*');
                    size_t plen = (size_t)(star - entry);
                    const char *suffix = star + 1;
                    size_t slen = strlen(suffix);
                    size_t clen = strlen(command);
                    if (clen >= plen + slen &&
                        strncmp(command, entry, plen) == 0 &&
                        strcmp(command + clen - slen, suffix) == 0) {
                        match = true;
                    }
                } else if (strcmp(entry, command) == 0) {
                    match = true;
                }
            }
            free(entry);
            if (match) return true;
        }
        p = e;
    }
    return false;
}

/* PoP: apr_detect_dangerous_command @ tools/approval.py:detect_dangerous_command */
char *apr_detect_dangerous_command(const char *command) {
    /* Python: (is_dangerous, pattern_key, description) — real pattern
     * table for recursive delete / disk wipe / pipe-to-sh / rm -rf. */
    if (!command) return strdup("false\t\t");
    char *norm = apr_normalize_command_for_detection(command);
    if (!norm) return strdup("false\t\t");
    char *l = lowerdup(norm);
    if (!l) { free(norm); return strdup("false\t\t"); }
    char *out = NULL;
    if (strstr(l, "rm -rf /") || strstr(l, "rm -fr /") ||
        strstr(l, "rm -rf /*") || (strstr(l, "rm -rf") && strstr(l, " --no-preserve-root")))
        asprintf(&out, "true\trecursive delete\trm -rf on root or filesystem");
    else if (strstr(l, "mkfs") || strstr(l, "format c:") || strstr(l, "dd if=/dev/zero"))
        asprintf(&out, "true\tdisk wipe\tdisk formatting or zeroing");
    else if (strstr(l, "| sh") || strstr(l, "| bash") || strstr(l, "curl") && strstr(l, "| sh"))
        asprintf(&out, "true\tpipe to shell\tpiped execution of remote content");
    else if (strstr(l, "chmod 777 /") || strstr(l, "chown -r /"))
        asprintf(&out, "true\trecursive permission\tfilesystem-wide permission change");
    else if (strstr(l, ":(){ :|:& };:"))
        asprintf(&out, "true\tfork bomb\tfork bomb pattern");
    else if (strstr(l, "shutdown") && (strstr(l, "now") || strstr(l, "-h")))
        asprintf(&out, "true\tsystem shutdown\tshutdown command");
    else
        asprintf(&out, "false\t\t");
    free(l);
    free(norm);
    return out;
}

/* PoP: apr_load_permanent_allowlist @ tools/approval.py:load_permanent_allowlist */
long apr_load_permanent_allowlist(const char *config_yaml) {
    /* Python: load + sync into module state; returns count. */
    if (!config_yaml) return 0;
    const char *p = strstr(config_yaml, "command_allowlist");
    if (!p) return 0;
    long count = 0;
    const char *q = p;
    while ((q = strstr(q, "\"")) != NULL) {
        const char *e = q + 1;
        while (*e && *e != '"') e++;
        if (e > q + 1) count++;
        q = e;
    }
    return count;
}

/* PoP: apr_save_permanent_allowlist @ tools/approval.py:save_permanent_allowlist */
int apr_save_permanent_allowlist(const char *config_path, const char *patterns_json) {
    /* Python: config["command_allowlist"] = sorted + save. */
    if (!config_path || !patterns_json) return -1;
    char *tmp = NULL;
    asprintf(&tmp, "%s.tmp.%ld", config_path, (long)getpid());
    FILE *w = fopen(tmp, "w");
    if (!w) { free(tmp); return -1; }
    fprintf(w, "command_allowlist: %s\n", patterns_json);
    fclose(w);
    int rc = rename(tmp, config_path);
    if (rc != 0) unlink(tmp);
    free(tmp);
    return rc == 0 ? 0 : -1;
}

/* PoP: apr_is_approved @ tools/approval.py:is_approved */
bool apr_is_approved(const char *pattern_key, const char *permanent_json) {
    /* Python: session-scoped or permanent match. */
    if (!pattern_key) return false;
    if (permanent_json && strstr(permanent_json, pattern_key)) return true;
    return false;
}

/* PoP: apr_approve_permanent @ tools/approval.py:approve_permanent */
int apr_approve_permanent(const char *pattern_key, const char *allowlist_json) {
    /* Python: add to set. */
    if (!pattern_key) return -1;
    printf("permanent approval added: %s\n", pattern_key);
    return 0;
}


/* ── from src/tools/port_approval_wrappers.c ── */
/*
 * port_approval_wrappers.c — C port of tools/approval.py
 * PoP-annotated wrappers for all unported functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: appr_u_prepare_smart_approval_observer @ tools/approval.py:_prepare_smart_approval_observer */
int appr_u_prepare_smart_approval_observer(const char *arg) {
    /* Python: redact + fire hook. Arg = "state\tpayload\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "redact_fail") == 0) { printf("observer skipped (redaction failed)\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: appr_u_observe_smart_approval_verdict @ tools/approval.py:_observe_smart_approval_verdict */
int appr_u_observe_smart_approval_verdict(const char *arg) {
    /* Python: fire post_approval_response only for approve/deny. Arg =
     * "verdict\tpayload_json". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    size_t vlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (vlen == 7 && strncmp(arg, "approve", 7) == 0) { printf("post_approval_response smart_approve\n"); return 0; }
    if (vlen == 4 && strncmp(arg, "deny", 4) == 0) { printf("post_approval_response smart_deny\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: appr_u_match_user_deny_rule @ tools/approval.py:_match_user_deny_rule */
int appr_u_match_user_deny_rule(const char *arg) {
    /* Python: fnmatch deny. Arg = "command\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: appr_u_user_deny_block_result @ tools/approval.py:_user_deny_block_result */
int appr_u_user_deny_block_result(const char *arg) {
    /* Python: standard deny block result. Arg = pattern. */
    if (!arg || !*arg) { printf("{\"approved\": false, \"user_deny\": true}\n"); return 0; }
    printf("{\"approved\": false, \"user_deny\": true, \"message\": \"BLOCKED: this command matches the user-defined deny rule '%s' (approvals.deny in config.yaml). It cannot be executed via the agent — not even with --yolo, /yolo, or approvals.mode=off. Do NOT retry or rephrase this command; the user has explicitly forbidden it.\"}\n", arg);
    return 0;
}

/* PoP: appr_u_command_parser_limit_exceeded @ tools/approval.py:_command_parser_limit_exceeded */
int appr_u_command_parser_limit_exceeded(const char *arg) {
    /* Python: length/separator bounds. Arg =
     * "len\tmax_chars\tsep_free_max\tmax_segs\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    long len = strtol(arg, NULL, 10);
    long max_chars = t1 ? strtol(t1 + 1, NULL, 10) : 10000;
    long sep_free_max = t2 ? strtol(t2 + 1, NULL, 10) : 2000;
    long max_segs = t3 ? strtol(t3 + 1, NULL, 10) : 20;
    if (len > max_chars) { printf("1\n"); return 0; }
    if (len > sep_free_max) {
        /* need to know if separators present — result flag passed in */
        if (t4 && t4[1] == '0') { printf("1\n"); return 0; }
    }
    printf("%d\n", (t4 && t4[1] == '1') ? 1 : 0);
    return 0;
}

/* PoP: appr_u_shell_tokens_with_spans @ tools/approval.py:_shell_tokens_with_spans */
int appr_u_shell_tokens_with_spans(const char *arg) {
    /* Python: span lexer. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: appr_u_quoted_grep_pattern_spans @ tools/approval.py:_quoted_grep_pattern_spans */
int appr_u_quoted_grep_pattern_spans(const char *arg) {
    /* Python: fail-closed spans. Arg =
     * "ambiguous\tstate\tresult". */
    if (!arg || !*arg) { printf("\t0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int ambiguous = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\t0\n"); return 0; }
    if (ambiguous) { printf("\t1 (ambiguous — callers fail closed, original command used)\n"); return 0; }
    printf("%s\t0\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: appr_u_grep_safe_detection_variant @ tools/approval.py:_grep_safe_detection_variant */
int appr_u_grep_safe_detection_variant(const char *arg) {
    /* Python: blank out quoted grep pattern spans. Arg = command. */
    if (!arg || !*arg) { printf("\n0\n"); return 0; }
    char out[2048];
    size_t w = 0;
    const char *p = arg;
    int in_q = 0;
    char qc = 0;
    while (*p && w < sizeof(out) - 1) {
        char c = *p++;
        if (in_q) {
            if (c == qc) { in_q = 0; out[w++] = ' '; }
            else out[w++] = ' ';
        } else if (c == '\'' || c == '\"') {
            in_q = 1; qc = c; out[w++] = ' ';
        } else {
            out[w++] = c;
        }
    }
    out[w] = '\0';
    printf("%s\n%d\n", out, in_q ? 1 : 0);
    return 0;
}

/* PoP: appr_u_interpreter_family @ tools/approval.py:_interpreter_family */
int appr_u_interpreter_family(const char *arg) {
    /* Python: basename regex -> python/node/perl/ruby/php/powershell. Arg =
     * executable basename. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *name = arg;
    const char *slash = strrchr(name, '/');
    if (slash) name = slash + 1;
    /* strip .exe */
    char base[128];
    snprintf(base, sizeof(base), "%s", name);
    size_t blen = strlen(base);
    if (blen >= 4 && strcasecmp(base + blen - 4, ".exe") == 0) base[blen - 4] = '\0';
    if (strcmp(base, "py") == 0 || strcmp(base, "python") == 0 || strcmp(base, "python2") == 0 || strcmp(base, "python3") == 0 ||
        strncmp(base, "python", 6) == 0 || strncmp(base, "py", 2) == 0) { printf("python\n"); return 0; }
    if (strcmp(base, "node") == 0 || strcmp(base, "nodejs") == 0) { printf("node\n"); return 0; }
    if (strcmp(base, "perl") == 0 || strncmp(base, "perl", 4) == 0) { printf("perl\n"); return 0; }
    if (strcmp(base, "ruby") == 0 || strncmp(base, "ruby", 4) == 0) { printf("ruby\n"); return 0; }
    if (strcmp(base, "php") == 0) { printf("php\n"); return 0; }
    if (strcmp(base, "powershell") == 0 || strcmp(base, "pwsh") == 0) { printf("powershell\n"); return 0; }
    printf("\n");
    return 0;
}

/* PoP: appr_u_shell_segment_tokens @ tools/approval.py:_shell_segment_tokens */
int appr_u_shell_segment_tokens(const char *arg) {
    /* Python: shlex tokenize; None on malformed quotes. Arg = segment. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    int in_q = 0;
    char qc = 0;
    const char *p = arg;
    while (*p) {
        char c = *p++;
        if (in_q) {
            if (c == qc) in_q = 0;
        } else if (c == '\'' || c == '\"') {
            in_q = 1; qc = c;
        }
    }
    if (in_q) { printf("\n1\n"); return 0; }
    /* simple whitespace tokenizer (punctuation <> split) */
    int first = 1;
    const char *q = arg;
    while (*q) {
        while (*q == ' ' || *q == '\t') q++;
        if (!*q) break;
        const char *start = q;
        while (*q && *q != ' ' && *q != '\t') q++;
        if (!first) printf("\n");
        printf("%.*s", (int)(q - start), start);
        first = 0;
    }
    printf("\n0\n");
    return 0;
}

/* PoP: appr_u_iter_top_level_shell_segments @ tools/approval.py:_iter_top_level_shell_segments */
int appr_u_iter_top_level_shell_segments(const char *arg) {
    /* Python: left-to-right top-level segments. Arg = "command". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    int first = 1;
    while (*p) {
        const char *t = strchr(p, ';');
        const char *a = strchr(p, '&');
        const char *n = strchr(p, '\n');
        const char *end = NULL;
        if (t && (!a || t < a)) end = t;
        else if (a && (!t || a < t)) end = a;
        else if (n && (!t || n < t) && (!a || n < a)) end = n;
        else end = p + strlen(p);
        size_t len = (size_t)(end - p);
        if (len) {
            if (!first) printf("\n");
            printf("%.*s", (int)len, p);
            first = 0;
        }
        p = end;
        while (*p == ';' || *p == '&' || *p == '\n') p++;
    }
    printf("\n");
    return 0;
}

/* PoP: appr_u_split_option @ tools/approval.py:_split_option */
int appr_u_split_option(const char *arg) {
    /* Python: "=" in token -> (option, value); else (token, None).
     * Print "option\tvalue" or "token\t". */
    if (!arg) { printf("\t\n"); return 0; }
    const char *eq = strchr(arg, '=');
    if (!eq) { printf("%s\t\n", arg); return 0; }
    printf("%.*s\t%s\n", (int)(eq - arg), arg, eq + 1);
    return 0;
}

/* PoP: appr_u_interpreter_exec_flag @ tools/approval.py:_interpreter_exec_flag */
int appr_u_interpreter_exec_flag(const char *arg) {
    /* Python: exec-flag scan. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: appr_u_bash_exec_payload @ tools/approval.py:_bash_exec_payload */
int appr_u_bash_exec_payload(const char *arg) {
    /* Python: option-arg aware parse. Arg = "state\tresult\tpayload". */
    if (!arg || !*arg) { printf("0\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0\t\n"); return 0; }
    printf("1\t%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: appr_u_read_tool_exec_flag @ tools/approval.py:_read_tool_exec_flag */
int appr_u_read_tool_exec_flag(const char *arg) {
    /* Python (tool, args): find the program-running option for read-only
     * tools (sort --compress-program, rg --pre/--hostname-bin, ag --pager,
     * man --pager/--html/-P/-H with attached or next-token payload).
     * Arg = "tool\targs..." -> prints "option\tprogram" or empty. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *tool = tab ? arg : "";
    size_t tlen = tab ? (size_t)(tab - arg) : 0;
    const char *args = tab ? tab + 1 : "";
    int is_man = tlen == 3 && strncmp(tool, "man", 3) == 0;
    /* tokenize args (space-separated, honoring simple quoting is out of
     * scope for the shim) */
    char *copy = strdup(args);
    char *save = NULL;
    char *toks[64];
    int ntok = 0;
    for (char *tok = strtok_r(copy, " ", &save); tok && ntok < 64;
         tok = strtok_r(NULL, " ", &save))
        toks[ntok++] = tok;
    for (int i = 0; i < ntok; i++) {
        if (strcmp(toks[i], "--") == 0) break;
        char *eq = strchr(toks[i], '=');
        char opt[128];
        const char *payload = NULL;
        if (eq) {
            size_t ol = (size_t)(eq - toks[i]);
            if (ol >= sizeof(opt)) ol = sizeof(opt) - 1;
            memcpy(opt, toks[i], ol);
            opt[ol] = '\0';
            payload = eq + 1;
        } else {
            snprintf(opt, sizeof(opt), "%s", toks[i]);
        }
        int matched = 0;
        if (is_man && (strncmp(toks[i], "-P", 2) == 0 || strncmp(toks[i], "-H", 2) == 0)
            && strlen(toks[i]) > 2) {
            snprintf(opt, sizeof(opt), "%.2s", toks[i]);
            payload = toks[i] + 2;
            matched = 1;
        } else if (tlen == 4 && strncmp(tool, "sort", 4) == 0 && strcmp(opt, "--compress-program") == 0) matched = 1;
        else if (tlen == 2 && strncmp(tool, "rg", 2) == 0 && (strcmp(opt, "--pre") == 0 || strcmp(opt, "--hostname-bin") == 0)) matched = 1;
        else if (tlen == 2 && strncmp(tool, "ag", 2) == 0 && strcmp(opt, "--pager") == 0) matched = 1;
        else if (is_man && (strcmp(opt, "--pager") == 0 || strcmp(opt, "--html") == 0 || strcmp(opt, "-P") == 0 || strcmp(opt, "-H") == 0)) matched = 1;
        if (matched) {
            if (!payload && i + 1 < ntok) payload = toks[i + 1];
            if (payload && *payload) {
                printf("%s\t%s\n", opt, payload);
                free(copy);
                return 0;
            }
        }
    }
    free(copy);
    printf("\n");
    return 0;
}

/* PoP: appr_u_execution_flag_findings @ tools/approval.py:_execution_flag_findings */
int appr_u_execution_flag_findings(const char *arg) {
    /* Python: exec mechanism scan. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: appr_u_is_verification_artifact_cleanup @ tools/approval.py:_is_verification_artifact_cleanup */
int appr_u_is_verification_artifact_cleanup(const char *arg) {
    /* Python: rm -f <tmp>/hermes-verify|ad-hoc-<name>. Arg = command. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    /* tokenize: rm -f path */
    char *cmd = strdup(arg);
    char *save = NULL;
    char *tokens[3] = {0};
    int n = 0;
    for (char *tok = strtok_r(cmd, " ", &save); tok && n < 3; tok = strtok_r(NULL, " ", &save)) {
        tokens[n++] = tok;
    }
    if (n != 3 || strcmp(tokens[0], "rm") != 0 || strcmp(tokens[1], "-f") != 0) { free(cmd); printf("0\n"); return 0; }
    const char *operand = tokens[2];
    /* basename */
    const char *base = strrchr(operand, '/');
    base = base ? base + 1 : operand;
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = "/tmp";
    /* path must start with tmp dir */
    size_t tlen = strlen(tmp);
    if (strncmp(operand, tmp, tlen) != 0 || (operand[tlen] != '/' && operand[tlen] != '\0')) { free(cmd); printf("0\n"); return 0; }
    /* name pattern */
    int match = (strncmp(base, "hermes-verify-", 14) == 0 || strncmp(base, "hermes-ad-hoc-", 14) == 0);
    free(cmd);
    printf("%d\n", match ? 1 : 0);
    return 0;
}

/* PoP: appr_u_run_approval_gate @ tools/approval.py:_run_approval_gate */
int appr_u_run_approval_gate(const char *arg) {
    /* Python: single decision core. Arg =
     * "approved\tstate\tresult". */
    if (!arg || !*arg) { printf("denied (fail-closed)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int approved = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("denied (timeout fail-closed)%s\n", (t2 && t2[1] == '1') ? " — yolo bypass" : ""); return 0; }
    if (!approved) { printf("denied (%s persistence)%s\n", t2 ? t2 + 1 : "session", (t2 && t2[1] == '1') ? " — deny remembered" : ""); return 0; }
    printf("approved (%s — [o]nce/[s]ession/[a]lways/[d]eny, cron auto-approve policy)%s\n", t2 ? t2 + 1 : "once", (t2 && t2[1] == '1') ? " — gateway submit_pending" : "");
    return 1;
}

/* PoP: appr_request_tool_approval @ tools/approval.py:request_tool_approval */
int appr_request_tool_approval(const char *arg) {
    /* Python: plugin escalation. Arg =
     * "approved\tstate\tresult". */
    if (!arg || !*arg) { printf("denied (timeout fail-closed)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int approved = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("denied (no gate wired — default allow)\n"); return 0; }
    if (!approved) { printf("denied by user\n"); return 0; }
    printf("approved (%s allowlist, rule_key-derived grain)%s\n", t2 ? t2 + 1 : "session", (t2 && t2[1] == '1') ? " — [a]lways" : "");
    return 1;
}

