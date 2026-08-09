/* Slermes C port — cron/lifecycle_guard.py (pure gateway-lifecycle guard) */

#define PCRE2_CODE_UNIT_WIDTH 8

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pcre2.h>
#include "slermes_home.h"
#include "cron/port_lifecycle_guard.h"

/* Faithful copy of _GATEWAY_LIFECYCLE_PATTERN (lifecycle_guard.py:48).
 * Python's `re` is PCRE-compatible, so PCRE2 reproduces it byte-faithfully
 * (supports \b. Note: POSIX regcomp() does NOT support \b, hence PCRE2).
 * Branch B includes submit|bootstrap (persistent keepalive laundering, #62891). */
static const char *LIFECYCLE_PATTERN =
    "(?i)"
    "(?:hermes\\s+gateway\\s+(?:restart|stop))"
    "|(?:launchctl\\s+(?:kickstart|unload|load|stop|restart|submit|bootstrap)\\b[^\\n]*\\bhermes[.\\-]?gateway)"
    "|(?:systemctl\\s+(?:-\\S+\\s+)*(?:restart|stop|start)\\b[^\\n]*\\bhermes[.\\-]?gateway)"
    "|(?:p?kill\\b[^\\n]*\\bhermes\\b[^\\n]*\\bgateway)"
    "|(?:p?kill\\b[^\\n]*\\bgateway\\b[^\\n]*\\bhermes)";

/* _SHELL_LINE_CONTINUATION: a backslash immediately followed by a newline is
 * a POSIX shell line continuation — the shell joins the lines before parsing.
 * Collapse to a single space before matching (mirrors _SHELL_LINE_CONTINUATION). */
static const char *LINE_CONTINUATION_PATTERN = "\\\\\\r?\\n[ \\t]*";

/* ================================================================
 *  _resolve_script_path
 * ================================================================ */
/* PoP: cron_lifecycle_resolve_script_path @ cron/lifecycle_guard.py:_resolve_script_path */
/* Resolve a cron `script` value the same way the scheduler does. A bare/
 * relative path lives under <SLERMES_HOME>/scripts/; an absolute path is
 * used as-is. Faithful to the Python (which uses HERMES_HOME).
 * Returns a malloc'd string (caller frees). */
char *cron_lifecycle_resolve_script_path(const char *script_path)
{
    if (!script_path) return NULL;
    /* expand leading ~ */
    char expanded[4096];
    const char *raw = script_path;
    if (raw[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) home = "/";
        snprintf(expanded, sizeof(expanded), "%s%s", home, raw + 1);
        raw = expanded;
    }
    if (raw[0] == '/') {
        return strdup(raw);
    }
    const char *home = slermes_home();
    size_t need = strlen(home ? home : ".slermes") + strlen("/scripts/") + strlen(raw) + 1;
    char *buf = malloc(need);
    if (buf)
        snprintf(buf, need, "%s/scripts/%s", home ? home : ".slermes", raw);
    return buf;
}

/* ================================================================
 *  _read_script_for_scanning
 * ================================================================ */
/* PoP: cron_lifecycle_read_script_for_scanning @ cron/lifecycle_guard.py:_read_script_for_scanning */
/* Read a cron script with the bounded terminal-script scanner. Non-regular or
 * oversized inputs fail closed by returning the lifecycle-shaped sentinel
 * "hermes gateway restart"; missing/unreadable paths return "".
 * Returns a malloc'd string (caller frees). */
char *cron_lifecycle_read_script_for_scanning(const char *script_path)
{
    if (!script_path) return strdup("");
    char *rp = cron_lifecycle_resolve_script_path(script_path);
    if (!rp) return strdup("");
    bool unsafe = false;
    char *text = cron_lifecycle_read_referenced_script(rp, &unsafe);
    free(rp);
    if (unsafe) return strdup("hermes gateway restart");
    return text ? text : strdup("");
}

/* ================================================================
 *  check_gateway_lifecycle
 * ================================================================ */
/* PoP: cron_lifecycle_check_gateway_lifecycle @ cron/lifecycle_guard.py:check_gateway_lifecycle */
/* Raise-equivalent: returns a malloc'd error message describing the block
 * when `prompt` or `script` contains a gateway-lifecycle command, else
 * NULL. The caller should surface the returned string as a tool error /
 * ValueError-shaped failure. Faithful to the Python (which raises
 * GatewayLifecycleBlocked). */
char *cron_lifecycle_check_gateway_lifecycle(const char *prompt,
                                              const char *script)
{
    const char *combined = prompt && prompt[0] ? prompt : "";
    char *script_text = NULL;
    bool python_script = false;
    if (script && script[0]) {
        /* python_script = _resolve_script_path(script).suffix == ".py" */
        char *resolved = cron_lifecycle_resolve_script_path(script);
        if (resolved) {
            size_t rl = strlen(resolved);
            if (rl >= 3 && strcmp(resolved + rl - 3, ".py") == 0)
                python_script = true;
            free(resolved);
        }
        script_text = cron_lifecycle_read_script_for_scanning(script);
        if (script_text && script_text[0]) {
            size_t need = strlen(combined) + 1 + strlen(script_text) + 1;
            char *cat = (char *)malloc(need);
            snprintf(cat, need, "%s\n%s", combined, script_text);
            combined = cat;
        }
    }
    bool unsafe;
    if (python_script) {
        /* Python executed by the interpreter, never through a POSIX shell:
         * the direct command regex still scans the full text. */
        unsafe = cron_lifecycle_contains_gateway_lifecycle_command(combined);
    } else {
        char *script_dir = script && script[0]
            ? cron_lifecycle_resolve_script_directory(script) : NULL;
        unsafe = cron_lifecycle_contains_gateway_lifecycle_command_or_referenced_script(
            combined, script_dir, NULL, NULL);
        free(script_dir);
    }
    char *ret = NULL;
    if (unsafe) {
        ret = strdup(
            "Blocked: cron job contains a gateway lifecycle command or persistent "
            "launchctl submit operation. This is blocked to prevent agent-driven "
            "SIGTERM-respawn loops under launchd/systemd supervision "
            "(#30719). Run `hermes gateway restart` from a shell outside "
            "the running gateway instead.");
    }
    if (script_text) free(script_text);
    if (combined != (prompt && prompt[0] ? prompt : "")) free((void *)combined);
    return ret;
}

/* ================================================================
 *  contains_gateway_lifecycle_command
 * ================================================================ */
/* PoP: contains_gateway_lifecycle_command @ cron/lifecycle_guard.py:contains_gateway_lifecycle_command */
bool cron_lifecycle_contains_gateway_lifecycle_command(const char *text)
{
    if (!text || text[0] == '\0') return false;

    /* Python normalizes line continuations (backslash-newline → space)
     * before matching (_SHELL_LINE_CONTINUATION). */
    char *normalized = NULL;
    {
        int err; PCRE2_SIZE erroff;
        pcre2_code *re = pcre2_compile((PCRE2_SPTR)LINE_CONTINUATION_PATTERN,
                                       PCRE2_ZERO_TERMINATED, 0, &err, &erroff, NULL);
        if (re) {
            pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
            PCRE2_SIZE len = strlen(text);
            PCRE2_SIZE outlen = 0;
            uint32_t subs_opt = PCRE2_SUBSTITUTE_GLOBAL | PCRE2_SUBSTITUTE_OVERFLOW_LENGTH;
            int rc1 = pcre2_substitute(re, (PCRE2_SPTR)text, len, 0, subs_opt,
                                       md, NULL, (PCRE2_SPTR)" ", PCRE2_ZERO_TERMINATED,
                                       NULL, &outlen);
            if (rc1 == PCRE2_ERROR_NOMEMORY && outlen > 0) {
                normalized = malloc(outlen + 1);
                if (normalized) {
                    pcre2_substitute(re, (PCRE2_SPTR)text, len, 0, subs_opt,
                                     md, NULL, (PCRE2_SPTR)" ", PCRE2_ZERO_TERMINATED,
                                     (PCRE2_SPTR)normalized, &outlen);
                    normalized[outlen] = '\0';
                }
            }
            pcre2_match_data_free(md);
            pcre2_code_free(re);
        }
    }
    const char *subject = normalized ? normalized : text;

    int err; PCRE2_SIZE erroff;
    pcre2_code *re = pcre2_compile((PCRE2_SPTR)LIFECYCLE_PATTERN,
                                   PCRE2_ZERO_TERMINATED, 0, &err, &erroff, NULL);
    if (!re) {
        free(normalized);
        return false;
    }
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
    int rc = pcre2_match(re, (PCRE2_SPTR)subject, strlen(subject), 0, 0, md, NULL);
    pcre2_match_data_free(md);
    pcre2_code_free(re);
    free(normalized);
    return rc >= 0;
}

/* ================================================================
 *  shlex port (posix=True, whitespace_split=True, commenters='#',
 *  punctuation_chars=';&|()') — used by _iter_command_segments.
 * ================================================================ */

static int is_wordchar(char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_') return 1;
    /* Python adds ~-./*?= then removes ;&|() from wordchars. */
    switch (c) {
        case '~': case '-': case '.': case '/': case '*': case '?':
        case '=': return 1;
        default: return 0;
    }
}
static int is_punct(char c) {
    return c == ';' || c == '&' || c == '|' || c == '(' || c == ')';
}
static int is_whitespace_char(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
static int is_quote(char c) { return c == '\'' || c == '"'; }

/* Tokenize one line (no trailing newline) with the shlex state machine.
 * Returns a NULL-terminated array of malloc'd tokens; *out_count set.
 * On unterminated quote (ValueError in Python) returns NULL. */
static char **shlex_tokenize(const char *line, size_t *out_count)
{
    size_t cap = 16, n = 0;
    char **tokens = malloc(cap * sizeof(char *));
    if (!tokens) return NULL;

    /* token builder */
    size_t tcap = 32, tlen = 0;
    char *tok = malloc(tcap);
    if (!tok) { free(tokens); return NULL; }

    /* state: ' ' whitespace, 'a' word, 'c' punctuation, quote char, '\\' escape */
    char state = ' ';
    char escapedstate = ' ';   /* state before escape */
    int quoted = 0;
    int pushback = -1;         /* single-char pushback (input is ASCII) */
    size_t i = 0;
    size_t linelen = strlen(line);

    /* helper: emit current token */
    #define EMIT() do { \
        tok[tlen] = '\0'; \
        if (tlen > 0 || (quoted && tlen == 0)) { \
            if (n + 1 >= cap) { \
                cap *= 2; \
                char **nt = realloc(tokens, cap * sizeof(char *)); \
                if (!nt) goto oom; \
                tokens = nt; \
            } \
            tokens[n++] = strdup(tok); \
            if (!tokens[n-1]) goto oom; \
        } \
        tlen = 0; quoted = 0; \
    } while (0)

    while (1) {
        char nextchar;
        if (pushback >= 0) {
            nextchar = (char)pushback;
            pushback = -1;
        } else if (i < linelen) {
            nextchar = line[i++];
        } else {
            nextchar = '\0';  /* eof */
        }

        if (state == ' ') {
            if (!nextchar) { state = '\0'; break; }
            else if (is_whitespace_char(nextchar)) {
                if (tlen > 0 || (quoted && tlen == 0)) break;  /* emit */
                continue;
            }
            else if (nextchar == '#') {
                /* comment: skip rest of line */
                while (i < linelen && line[i] != '\n') i++;
                if (tlen > 0 || (quoted && tlen == 0)) break;
                continue;
            }
            else if (nextchar == '\\') { escapedstate = 'a'; state = '\\'; }
            else if (is_wordchar(nextchar)) { tok[tlen++] = nextchar; state = 'a'; }
            else if (is_punct(nextchar)) { tok[tlen++] = nextchar; state = 'c'; }
            else if (is_quote(nextchar)) { state = nextchar; quoted = 1; }
            else { tok[tlen++] = nextchar; state = 'a'; }  /* whitespace_split */
        }
        else if (state == '\'' || state == '"') {
            char quote = state;
            quoted = 1;
            if (!nextchar) { free(tok); free(tokens); return NULL; }  /* ValueError */
            if (nextchar == quote) {
                state = 'a';  /* posix: quote consumed, token continues */
            }
            else if (nextchar == '\\' && quote == '"') {
                escapedstate = state;
                state = '\\';
            }
            else {
                tok[tlen++] = nextchar;
            }
        }
        else if (state == '\\') {
            if (!nextchar) { free(tok); free(tokens); return NULL; }  /* ValueError */
            /* In posix shells, only the quote itself or the escape char may
             * be escaped within quotes. */
            if (escapedstate == '\'' || escapedstate == '"') {
                if (nextchar != '\\' && nextchar != escapedstate)
                    tok[tlen++] = '\\';
            }
            tok[tlen++] = nextchar;
            state = escapedstate;
        }
        else {  /* state 'a' word or 'c' punctuation */
            if (!nextchar) { state = '\0'; EMIT(); break; }
            else if (is_whitespace_char(nextchar)) {
                state = ' ';
                if (tlen > 0 || (quoted && tlen == 0)) { EMIT(); continue; }
                continue;
            }
            else if (nextchar == '#') {
                while (i < linelen && line[i] != '\n') i++;
                state = ' ';
                EMIT();
                break;
            }
            else if (state == 'c') {
                if (is_punct(nextchar)) { tok[tlen++] = nextchar; }
                else {
                    if (!is_whitespace_char(nextchar)) pushback = (int)nextchar;
                    state = ' ';
                    EMIT();
                    continue;
                }
            }
            else if (is_quote(nextchar)) {
                state = nextchar; quoted = 1;
            }
            else if (nextchar == '\\') {
                escapedstate = 'a';
                state = '\\';
            }
            else if (is_wordchar(nextchar) || is_quote(nextchar) ||
                     (!is_punct(nextchar))) {
                tok[tlen++] = nextchar;
            }
            else {
                pushback = (int)nextchar;
                state = ' ';
                EMIT();
                continue;
            }
        }
    }
    EMIT();
    if (tokens) { tokens[n] = NULL; }
    free(tok);
    *out_count = n;
    return tokens;
oom:
    if (tok) free(tok);
    for (size_t k = 0; k < n; k++) free(tokens[k]);
    free(tokens);
    return NULL;
    #undef EMIT
}

/* PoP: _iter_command_segments @ cron/lifecycle_guard.py:_iter_command_segments */
char ***cron_lifecycle_iter_command_segments(const char *command)
{
    if (!command) return NULL;
    /* normalized = command.replace("\\\n", "") */
    size_t clen = strlen(command);
    char *normalized = malloc(clen + 1);
    if (!normalized) return NULL;
    size_t nlen = 0;
    for (size_t k = 0; k < clen; k++) {
        if (command[k] == '\\' && k + 1 < clen && command[k+1] == '\n') {
            k++;  /* skip both */
            continue;
        }
        normalized[nlen++] = command[k];
    }
    normalized[nlen] = '\0';

    /* segments: NULL-terminated array of NULL-terminated token arrays */
    size_t scap = 8, sn = 0;
    char ***segments = malloc(scap * sizeof(char **));
    if (!segments) { free(normalized); return NULL; }

    /* split into lines like splitlines() (handle \r\n, \n, \r) */
    char *line = normalized;
    while (1) {
        char *nl = strchr(line, '\n');
        size_t linelen;
        if (nl) linelen = (size_t)(nl - line);
        else linelen = strlen(line);
        /* strip trailing \r */
        while (linelen > 0 && line[linelen-1] == '\r') linelen--;
        char *linebuf = malloc(linelen + 1);
        if (!linebuf) goto oom_seg;
        memcpy(linebuf, line, linelen);
        linebuf[linelen] = '\0';

        size_t ntokens = 0;
        char **tokens = shlex_tokenize(linebuf, &ntokens);
        if (tokens) {
            /* build segment: split on control-char-only tokens */
            size_t sc = 8, sn2 = 0;
            char **seg = malloc(sc * sizeof(char *));
            if (!seg) {
                for (size_t k = 0; k < ntokens; k++) free(tokens[k]);
                free(tokens); free(linebuf); goto oom_seg;
            }
            for (size_t k = 0; k < ntokens; k++) {
                char *token = tokens[k];
                int control_only = 1;
                if (!token[0]) control_only = 0;
                for (const char *p = token; *p; p++)
                    if (!is_punct(*p)) { control_only = 0; break; }
                if (control_only) {
                    if (sn2 > 0) { seg[sn2] = NULL; if (sn+1 >= scap) { scap*=2; char ***ns = realloc(segments, scap*sizeof(char**)); if (!ns) goto oom_seg; segments = ns; } segments[sn++] = seg; seg = NULL; sc = 8; sn2 = 0; seg = malloc(sc*sizeof(char*)); if (!seg) goto oom_seg; }
                    continue;
                }
                if (sn2 + 1 >= sc) { sc *= 2; char **ns = realloc(seg, sc*sizeof(char*)); if (!ns) { free(seg); goto oom_seg; } seg = ns; }
                seg[sn2++] = strdup(token);
                if (!seg[sn2-1]) goto oom_seg;
            }
            if (sn2 > 0) {
                seg[sn2] = NULL;
                if (sn+1 >= scap) { scap *= 2; char ***ns = realloc(segments, scap*sizeof(char**)); if (!ns) goto oom_seg; segments = ns; }
                segments[sn++] = seg;
            } else {
                if (seg) free(seg);
            }
            for (size_t k = 0; k < ntokens; k++) free(tokens[k]);
            free(tokens);
        }
        free(linebuf);
        if (!nl) break;
        line = nl + 1;
    }
    segments[sn] = NULL;
    free(normalized);
    return segments;

oom_seg:
    free(normalized);
    if (segments) {
        for (size_t k = 0; k < sn; k++) {
            char **seg = segments[k];
            if (!seg) continue;
            for (size_t j = 0; seg[j]; j++) free(seg[j]);
            free(seg);
        }
        free(segments);
    }
    return NULL;
}

void cron_lifecycle_free_segments(char ***segments)
{
    if (!segments) return;
    for (size_t k = 0; segments[k]; k++) {
        char **seg = segments[k];
        for (size_t j = 0; seg[j]; j++) free(seg[j]);
        free(seg);
    }
    free(segments);
}

/* PoP: _command_token_index @ cron/lifecycle_guard.py:_command_token_index */
int cron_lifecycle_command_token_index(char *const *segment)
{
    if (!segment) return -1;
    for (int index = 0; segment[index]; index++) {
        /* skip FOO=bar env assignments */
        const char *t = segment[index];
        if (t[0] && (isalpha((unsigned char)t[0]) || t[0] == '_')) {
            const char *p = t;
            int is_assign = 0;
            for (; *p; p++) {
                if (!(isalnum((unsigned char)*p) || *p == '_')) {
                    if (*p == '=') is_assign = 1;
                    break;
                }
            }
            if (is_assign && *p == '=') continue;
        }
        return index;
    }
    return -1;
}

/* PoP: contains_launchctl_submit_command @ cron/lifecycle_guard.py:contains_launchctl_submit_command */
bool cron_lifecycle_contains_launchctl_submit_command(const char *command)
{
    if (!command) return false;
    char ***segments = cron_lifecycle_iter_command_segments(command);
    if (!segments) return false;
    bool found = false;
    for (size_t s = 0; segments[s] && !found; s++) {
        char **segment = segments[s];
        int index = cron_lifecycle_command_token_index(segment);
        if (index < 0 || !segment[index]) continue;
        const char *exe = strrchr(segment[index], '/');
        exe = exe ? exe + 1 : segment[index];
        if (strcmp(exe, "launchctl") != 0) continue;
        /* arguments = segment[index+1:] */
        if (segment[index+1]) {
            const char *verb = segment[index+1];
            char vbuf[32];
            size_t vlen = strlen(verb);
            if (vlen >= sizeof(vbuf)) vlen = sizeof(vbuf) - 1;
            for (size_t k = 0; k < vlen; k++)
                vbuf[k] = (char)tolower((unsigned char)verb[k]);
            vbuf[vlen] = '\0';
            if (strcmp(vbuf, "submit") == 0 || strcmp(vbuf, "bootstrap") == 0)
                found = true;
        }
    }
    cron_lifecycle_free_segments(segments);
    return found;
}

/* PoP: _resolve_terminal_script_path @ cron/lifecycle_guard.py:_resolve_terminal_script_path */
char *cron_lifecycle_resolve_terminal_script_path(const char *candidate,
                                                  const char *cwd)
{
    if (!candidate) return NULL;
    /* Path(candidate).expanduser() */
    char *expanded = NULL;
    if (candidate[0] == '~') {
        const char *home = getenv("HOME");
        if (home) {
            size_t need = strlen(home) + strlen(candidate);
            expanded = malloc(need + 1);
            if (!expanded) return NULL;
            snprintf(expanded, need + 1, "%s%s", home, candidate + 1);
            candidate = expanded;
        }
    }
    char *result = NULL;
    if (candidate[0] == '/') {
        result = strdup(candidate);
    } else {
        const char *base = (cwd && cwd[0]) ? cwd : ".";
        size_t need = strlen(base) + 1 + strlen(candidate) + 1;
        result = malloc(need);
        if (result) snprintf(result, need, "%s/%s", base, candidate);
    }
    /* pathlib join semantics: drop "." segments (Path('/tmp')/'./x' -> /tmp/x,
     * Path('/tmp')/'.' -> /tmp) but KEEP ".." (Path('/tmp')/'..' -> /tmp/..),
     * and never leave a trailing slash. */
    if (result) {
        int is_abs = result[0] == '/';
        const char *s = result;
        /* tokenize into components on '/' */
        char **parts = NULL;
        size_t np = 0, cap = 0;
        while (*s) {
            while (*s == '/') s++;
            if (!*s) break;
            const char *seg_start = s;
            while (*s && *s != '/') s++;
            size_t seglen = (size_t)(s - seg_start);
            if (seglen == 1 && seg_start[0] == '.') continue;  /* drop "." */
            if (np + 1 >= cap) {
                cap = cap ? cap * 2 : 8;
                char **np2 = realloc(parts, cap * sizeof(char *));
                if (!np2) { free(parts); free(result); return NULL; }
                parts = np2;
            }
            parts[np] = malloc(seglen + 1);
            if (!parts[np]) { for (size_t k = 0; k < np; k++) free(parts[k]); free(parts); free(result); return NULL; }
            memcpy(parts[np], seg_start, seglen);
            parts[np][seglen] = '\0';
            np++;
        }
        size_t outlen = (is_abs ? 1 : 0);
        for (size_t k = 0; k < np; k++) outlen += strlen(parts[k]) + 1;
        char *out = malloc(outlen + 1);
        if (out) {
            char *o = out;
            if (is_abs) *o++ = '/';
            for (size_t k = 0; k < np; k++) {
                if (k) *o++ = '/';
                size_t l = strlen(parts[k]);
                memcpy(o, parts[k], l);
                o += l;
            }
            *o = '\0';
            /* empty result with absolute prefix: "/" ; empty relative: "." */
            if (o == out) {
                if (is_abs) { out[0] = '/'; out[1] = '\0'; }
                else { out[0] = '.'; out[1] = '\0'; }
            }
        }
        for (size_t k = 0; k < np; k++) free(parts[k]);
        free(parts);
        free(result);
        if (out) result = out;
    }
    free(expanded);
    return result;
}

/* PoP: _iter_referenced_shell_scripts @ cron/lifecycle_guard.py:_iter_referenced_shell_scripts */
char **cron_lifecycle_iter_referenced_shell_scripts(const char *command,
                                                    const char *cwd)
{
    size_t cap = 8, n = 0;
    char **paths = malloc(cap * sizeof(char *));
    if (!paths) return NULL;
    if (!command) { paths[0] = NULL; return paths; }

    static const char *SHELL_EXECUTABLES[] = { "sh","bash","dash","ksh","zsh", NULL };
    static const char *SHELL_OPTIONS_WITH_VALUES[] = { "-O","+O","-o","+o", NULL };

    char ***segments = cron_lifecycle_iter_command_segments(command);
    if (!segments) { paths[0] = NULL; return paths; }

    for (size_t s = 0; segments[s]; s++) {
        char **segment = segments[s];
        int index = cron_lifecycle_command_token_index(segment);
        if (index < 0 || !segment[index]) continue;
        const char *executable = segment[index];
        const char *exe_name = strrchr(executable, '/');
        exe_name = exe_name ? exe_name + 1 : executable;
        /* Python's Path('.').name / Path('..').name are '' — a bare "." or
         * ".." executable never triggers the source/. branch. */
        if (strcmp(exe_name, ".") == 0 || strcmp(exe_name, "..") == 0)
            exe_name = "";

        if (strcmp(exe_name, ".") == 0 || strcmp(exe_name, "source") == 0) {
            if (segment[index+1]) {
                char *p = cron_lifecycle_resolve_terminal_script_path(segment[index+1], cwd);
                if (p) {
                    if (n+1 >= cap) { cap *= 2; char **np = realloc(paths, cap*sizeof(char*)); if (!np) goto oom; paths = np; }
                    paths[n++] = p;
                }
            }
            continue;
        }

        int is_shell = 0;
        for (int sh = 0; SHELL_EXECUTABLES[sh]; sh++)
            if (strcmp(exe_name, SHELL_EXECUTABLES[sh]) == 0) { is_shell = 1; break; }

        if (is_shell) {
            /* walk args skipping options; the first non-option (non -c) is a script */
            char **arguments = &segment[index + 1];
            int arg_index = 0;
            while (arguments[arg_index]) {
                const char *argument = arguments[arg_index];
                if (strcmp(argument, "--") == 0) { arg_index++; break; }
                if (strcmp(argument, "-c") == 0 || strcmp(argument, "--command") == 0) break;
                int is_opt_with_val = 0;
                for (int o = 0; SHELL_OPTIONS_WITH_VALUES[o]; o++)
                    if (strcmp(argument, SHELL_OPTIONS_WITH_VALUES[o]) == 0) { is_opt_with_val = 1; break; }
                if (is_opt_with_val) { arg_index += 2; continue; }
                if (argument[0] == '-') { arg_index++; continue; }
                break;
            }
            if (arguments[arg_index] &&
                strcmp(arguments[arg_index], "-c") != 0 &&
                strcmp(arguments[arg_index], "--command") != 0) {
                char *p = cron_lifecycle_resolve_terminal_script_path(arguments[arg_index], cwd);
                if (p) {
                    if (n+1 >= cap) { cap *= 2; char **np = realloc(paths, cap*sizeof(char*)); if (!np) { free(p); goto oom; } paths = np; }
                    paths[n++] = p;
                }
            }
            continue;
        }

        /* bare executable path or script-suffixed */
        if (executable[0] == '/' || strchr(executable, '/') ||
            strstr(executable, ".sh") || strstr(executable, ".bash") || strstr(executable, ".zsh")) {
            /* skip pure-separator tokens like "/" */
            int pure_sep = 1;
            for (const char *p = executable; *p; p++)
                if (*p != '/') { pure_sep = 0; break; }
            if (!pure_sep) {
                char *p = cron_lifecycle_resolve_terminal_script_path(executable, cwd);
                if (p) {
                    if (n+1 >= cap) { cap *= 2; char **np = realloc(paths, cap*sizeof(char*)); if (!np) { free(p); goto oom; } paths = np; }
                    paths[n++] = p;
                }
            }
        }
    }
    cron_lifecycle_free_segments(segments);
    paths[n] = NULL;
    return paths;
oom:
    cron_lifecycle_free_segments(segments);
    for (size_t k = 0; k < n; k++) free(paths[k]);
    free(paths);
    return NULL;
}

/* PoP: _iter_shell_command_payloads @ cron/lifecycle_guard.py:_iter_shell_command_payloads */
char **cron_lifecycle_iter_shell_command_payloads(const char *command)
{
    size_t cap = 4, n = 0;
    char **payloads = malloc(cap * sizeof(char *));
    if (!payloads) return NULL;
    if (!command) { payloads[0] = NULL; return payloads; }

    static const char *SHELL_EXECUTABLES[] = { "sh","bash","dash","ksh","zsh", NULL };

    char ***segments = cron_lifecycle_iter_command_segments(command);
    if (!segments) { payloads[0] = NULL; return payloads; }

    for (size_t s = 0; segments[s]; s++) {
        char **segment = segments[s];
        int index = cron_lifecycle_command_token_index(segment);
        if (index < 0 || !segment[index]) continue;
        const char *exe_name = strrchr(segment[index], '/');
        exe_name = exe_name ? exe_name + 1 : segment[index];
        int is_shell = 0;
        for (int sh = 0; SHELL_EXECUTABLES[sh]; sh++)
            if (strcmp(exe_name, SHELL_EXECUTABLES[sh]) == 0) { is_shell = 1; break; }
        if (!is_shell) continue;

        char **arguments = &segment[index + 1];
        for (int ai = 0; arguments[ai]; ai++) {
            if (strcmp(arguments[ai], "-c") == 0 || strcmp(arguments[ai], "--command") == 0) {
                if (arguments[ai+1]) {
                    if (n+1 >= cap) { cap *= 2; char **np = realloc(payloads, cap*sizeof(char*)); if (!np) goto oom; payloads = np; }
                    payloads[n++] = strdup(arguments[ai+1]);
                }
                break;
            }
        }
    }
    cron_lifecycle_free_segments(segments);
    payloads[n] = NULL;
    return payloads;
oom:
    cron_lifecycle_free_segments(segments);
    for (size_t k = 0; k < n; k++) free(payloads[k]);
    free(payloads);
    return NULL;
}

/* PoP: _resolve_script_directory @ cron/lifecycle_guard.py:_resolve_script_directory */
char *cron_lifecycle_resolve_script_directory(const char *script_path)
{
    if (!script_path) return NULL;
    char *resolved = cron_lifecycle_resolve_script_path(script_path);
    if (!resolved) return NULL;
    if (resolved[0] != '/') { free(resolved); return NULL; }
    char *slash = strrchr(resolved, '/');
    char *dir = NULL;
    if (slash) {
        if (slash == resolved) dir = strdup("/");
        else {
            size_t len = (size_t)(slash - resolved);
            dir = malloc(len + 1);
            if (dir) { memcpy(dir, resolved, len); dir[len] = '\0'; }
        }
    }
    free(resolved);
    return dir;
}

/* PoP: _read_referenced_script @ cron/lifecycle_guard.py:_read_referenced_script */
char *cron_lifecycle_read_referenced_script(const char *path, bool *out_unsafe)
{
    if (out_unsafe) *out_unsafe = false;
    if (!path) return NULL;
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return NULL;
    struct stat st;
    if (fstat(fd, &st) != 0) { close(fd); return NULL; }
    if (!S_ISREG(st.st_mode)) { close(fd); if (out_unsafe) *out_unsafe = true; return NULL; }

    const size_t MAX_BYTES = 1024 * 1024;  /* _MAX_REFERENCED_SCRIPT_BYTES */
    char *data = malloc(MAX_BYTES + 1);
    if (!data) { close(fd); return NULL; }
    ssize_t got = read(fd, data, MAX_BYTES + 1);
    close(fd);
    if (got < 0) { free(data); return NULL; }

    /* binary (NUL byte) → nothing to scan */
    for (ssize_t k = 0; k < got; k++)
        if (data[k] == '\0') { free(data); return NULL; }
    if ((size_t)got > MAX_BYTES) {
        free(data);
        if (out_unsafe) *out_unsafe = true;
        return NULL;
    }
    /* UTF-8 decode with errors=replace */
    char *out = malloc((size_t)got + 1);
    if (!out) { free(data); return NULL; }
    size_t o = 0;
    for (ssize_t k = 0; k < got; ) {
        unsigned char c = (unsigned char)data[k];
        if (c < 0x80) { out[o++] = (char)c; k++; }
        else if ((c >> 5) == 0x6 && k + 1 < got) { out[o++] = '?'; k += 2; }
        else if ((c >> 4) == 0xe && k + 2 < got) { out[o++] = '?'; k += 3; }
        else if ((c >> 3) == 0x1e && k + 3 < got) { out[o++] = '?'; k += 4; }
        else { out[o++] = '?'; k++; }
    }
    out[o] = '\0';
    free(data);
    return out;
}

/* PoP: _contains_unsafe_gateway_action @ cron/lifecycle_guard.py:_contains_unsafe_gateway_action */
static bool contains_unsafe_gateway_action(const char *command,
                                           const char *cwd, int depth,
                                           char **visited, size_t *vcount,
                                           cron_lifecycle_read_remote_fn read_remote,
                                           void *read_ctx)
{
    if (!command) return false;
    if (cron_lifecycle_contains_gateway_lifecycle_command(command) ||
        cron_lifecycle_contains_launchctl_submit_command(command))
        return true;
    if (depth >= 8) return true;  /* _MAX_REFERENCED_SCRIPT_DEPTH */

    /* shell -c payloads */
    char **payloads = cron_lifecycle_iter_shell_command_payloads(command);
    if (payloads) {
        for (size_t k = 0; payloads[k]; k++) {
            if (contains_unsafe_gateway_action(payloads[k], cwd, depth + 1,
                                               visited, vcount, read_remote, read_ctx)) {
                for (size_t j = 0; payloads[j]; j++) free(payloads[j]);
                free(payloads);
                return true;
            }
        }
        for (size_t j = 0; payloads[j]; j++) free(payloads[j]);
        free(payloads);
    }

    /* referenced scripts */
    char **scripts = cron_lifecycle_iter_referenced_shell_scripts(command, cwd);
    if (scripts) {
        for (size_t k = 0; scripts[k]; k++) {
            char *resolved = scripts[k];
            /* visited check (best-effort path string compare) */
            int seen = 0;
            for (size_t v = 0; v < *vcount; v++)
                if (strcmp(visited[v], resolved) == 0) { seen = 1; break; }
            if (seen) { free(resolved); continue; }
            if (*vcount + 1 >= 64) { free(resolved); continue; }
            visited[*vcount] = strdup(resolved);
            (*vcount)++;

            bool unsafe = false;
            char *script_text = cron_lifecycle_read_referenced_script(resolved, &unsafe);
            if (unsafe) { free(resolved); goto unsafe_out; }
            if (!script_text && read_remote)
                script_text = read_remote(resolved, read_ctx);
            if (!script_text || !script_text[0]) { free(resolved); free(script_text); continue; }

            char *script_dir = cron_lifecycle_resolve_script_directory(resolved);
            const char *sub_cwd = script_dir ? script_dir : cwd;
            if (contains_unsafe_gateway_action(script_text, sub_cwd, depth + 1,
                                               visited, vcount, read_remote, read_ctx)) {
                free(script_dir);
                free(script_text);
                free(resolved);
                goto unsafe_out;
            }
            free(script_dir);
            free(script_text);
            free(resolved);
        }
        free(scripts);
    }
    return false;

unsafe_out:
    free(scripts);
    return true;
}

/* PoP: contains_gateway_lifecycle_command_or_referenced_script @ cron/lifecycle_guard.py:contains_gateway_lifecycle_command_or_referenced_script */
bool cron_lifecycle_contains_gateway_lifecycle_command_or_referenced_script(
    const char *command, const char *cwd,
    cron_lifecycle_read_remote_fn read_remote_script, void *read_ctx)
{
    char *visited[64] = {0};
    size_t vcount = 0;
    bool result = contains_unsafe_gateway_action(
        command, cwd, 0, visited, &vcount, read_remote_script, read_ctx);
    for (size_t v = 0; v < vcount; v++) free(visited[v]);
    return result;
}
