/**
 * port_approval.c — Port of Python: tools/approval.py
 *
 * Real C implementations for command approval functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
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
    if (!word) return false;
    for (const char *p = word; *p; p++) {
        if (*p == '$' || *p == '`' || *p == '\'' || *p == '"' || *p == '\\')
            return false;
        if (*p == '|' || *p == '&' || *p == ';' || *p == '>' || *p == '<')
            return false;
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
    if (!command) return false;
    if (strstr(command, " | ") || strstr(command, " && ") ||
        strstr(command, " || ") || strstr(command, " ; ") ||
        strstr(command, " > ") || strstr(command, " >> ") ||
        strstr(command, " < ")) {
        hermes_log(LOG_DEBUG, "port", "has_allowlist_shell_operator: operators found in '%s'",
                   command);
        return true;
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

/* PoP: _hardline_rm_path @ tools/approval.py:_hardline_rm_path */
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

/* PoP: _home_prefix_fold_regex @ tools/approval.py:_home_prefix_fold_regex */
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

/* PoP: _fold_home_prefixes @ tools/approval.py:_fold_home_prefixes */
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