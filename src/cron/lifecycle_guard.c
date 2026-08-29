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
#include "cron/lifecycle_guard.h"

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
/* PoP: Python @ cron/lifecycle_guard.py:_resolve_script_path */
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
/* PoP: string @ cron/lifecycle_guard.py:_read_script_for_scanning */
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
/* PoP: Python @ cron/lifecycle_guard.py:check_gateway_lifecycle */
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
/* PoP: cron_lifecycle_contains_gateway_lifecycle_command @ cron/lifecycle_guard.py:contains_gateway_lifecycle_command */
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

/* PoP: cron_lifecycle_iter_command_segments @ cron/lifecycle_guard.py:_iter_command_segments */
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

/* PoP: cron_lifecycle_command_token_index @ cron/lifecycle_guard.py:_command_token_index */
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

/* PoP: cron_lifecycle_contains_launchctl_submit_command @ cron/lifecycle_guard.py:contains_launchctl_submit_command */
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

/* PoP: cron_lifecycle_resolve_terminal_script_path @ cron/lifecycle_guard.py:_resolve_terminal_script_path */
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

/* PoP: cron_lifecycle_iter_referenced_shell_scripts @ cron/lifecycle_guard.py:_iter_referenced_shell_scripts */
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

/* PoP: cron_lifecycle_iter_shell_command_payloads @ cron/lifecycle_guard.py:_iter_shell_command_payloads */
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

/* PoP: cron_lifecycle_resolve_script_directory @ cron/lifecycle_guard.py:_resolve_script_directory */
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

/* PoP: cron_lifecycle_read_referenced_script @ cron/lifecycle_guard.py:_read_referenced_script */
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

/* PoP: contains_unsafe_gateway_action @ cron/lifecycle_guard.py:_contains_unsafe_gateway_action */
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

/* PoP: cron_lifecycle_contains_gateway_lifecycle_command_or_referenced_script @ cron/lifecycle_guard.py:contains_gateway_lifecycle_command_or_referenced_script */
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

/* PoP: lifecycle_sanitize_remote_script @ cron/lifecycle_guard.py:_sanitize_remote_script_text */
/* Returns (text, ok) via out params. ok=0 means binary/oversized. */
void lifecycle_sanitize_remote_script(const char *text, char **out_text, int *ok) {
    if (!text || !*text) { *out_text = NULL; *ok = 0; return; }
    if (strchr(text, '\x00')) { *out_text = NULL; *ok = 0; return; }
    size_t len = strlen(text);
    if (len > 1048576) { *out_text = NULL; *ok = 0; return; }  /* _MAX_REFERENCED_SCRIPT_BYTES = 1MB */
    *out_text = strdup(text);
    *ok = 1;
}

/* PoP: lifecycle_expand_candidate_path @ cron/lifecycle_guard.py:_expand_candidate_path */
char *lifecycle_expand_candidate_path(const char *candidate) {
    if (!candidate || !*candidate) return NULL;
    if (strchr(candidate, '\x00')) return NULL;
    /* expanduser */
    if (candidate[0] == '~' && candidate[1] == '/') {
        const char *home = getenv("HOME");
        if (home) {
            char *out = NULL;
            asprintf(&out, "%s%s", home, candidate + 1);
            return out;
        }
    }
    return strdup(candidate);
}

/* ══════════════════════════════════════════════════════════════════
 — cron/lifecycle_guard.py (13 fns)
 * Python truth: $SLERMES_SRC/cron/lifecycle_guard.py
 * ══════════════════════════════════════════════════════════════════ */

/* ── local string helpers (file-local, mirrors other cron/*.c TUs) ──── */
static char *lg_xstrdup(const char *s) {
    size_t n = s ? strlen(s) + 1 : 1;
    char *out = malloc(n);
    if (out) memcpy(out, s ? s : "", n);
    return out;
}
static char *str_strip_dup(const char *s) {
    if (!s) return lg_xstrdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) len--;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}
#define xstrdup lg_xstrdup


#include "profile_store.h"   /* profile_get_active_name */

/* ── module constants (byte-exact with lifecycle_guard.py) ─────────── */

/* _LAUNCHCTL_LIFECYCLE_VERBS_RE (#78028 companion) */
static const char *LAUNCHCTL_LIFECYCLE_VERBS_PATTERN =
    "(?i)\\blaunchctl\\s+(?:kickstart|unload|load|stop|restart|bootout|kill|disable|remove)\\b";
/* _HERMES_GATEWAY_LABEL_RE */
static const char *HERMES_GATEWAY_LABEL_PATTERN =
    "(?i)\\bhermes[.\\-]?gateway\\b";
/* _PIPE_TO_INTERPRETER */
static const char *PIPE_TO_INTERPRETER_PATTERN =
    "\\|\\s*&?\\s*(?:sudo\\s+)?(?:sh|bash|dash|ksh|zsh|xargs|eval|source)\\b";

static const char *CLOUD_PLACEHOLDER_MARKERS[] = {
    "Mobile Documents", "CloudStorage", NULL,
};

static const char *SHELL_EXECUTABLES[] = {
    "sh", "bash", "dash", "ksh", "zsh", NULL,
};
static const char *SHELL_OPTIONS_WITH_VALUES[] = {
    "-O", "+O", "-o", "+o", NULL,
};

static const char *DATA_SINK_EXECUTABLES[] = {
    "grep", "egrep", "fgrep", "rg", "ag", "ack", "journalctl", "sqlite3",
    "psql", NULL,
};
/* _UNSAFE_DATA_ARG_MARKERS: execution-capable argument markers. */
static const char *UNSAFE_DATA_ARG_MARKERS[] = {
    "`", "$(", "<(", ">(", "\\!", NULL,
};

static const char *TRANSPARENT_COMMAND_PREFIXES[] = {
    "sudo", "doas", "env", "nohup", "setsid", "nice", "ionice", "stdbuf",
    "timeout", "exec", "command", "builtin", "eatmydata",
    "pkexec", "su", "runuser", "setpriv", "systemd-run", "nsenter",
    "unshare", NULL,
};
typedef struct { const char *name; const char *const *options; } tp_value_opts;
static const char *tp_sudo_opts[] = { "-u", "-g", "-U", "-C", "-p", "-r", "-t", "-T",
    "--user", "--group", "--prompt", NULL };
static const char *tp_doas_opts[] = { "-u", "-C", NULL };
static const char *tp_env_opts[] = { "-u", "--unset", "-S", "--split-string", "-C", "--chdir", NULL };
static const char *tp_nice_opts[] = { "-n", "--adjustment", NULL };
static const char *tp_ionice_opts[] = { "-c", "-n", "-p", "--class", "--classdata", NULL };
static const char *tp_stdbuf_opts[] = { "-i", "-o", "-e", "--input", "--output", "--error", NULL };
static const char *tp_timeout_opts[] = { "-s", "-k", "--signal", "--kill-after", NULL };
static const char *tp_pkexec_opts[] = { "--user", NULL };
static const char *tp_su_opts[] = { "-s", "--shell", "-g", "--group", "-G", "--supp-group", NULL };
static const char *tp_runuser_opts[] = { "-u", "--user", "-s", "--shell", "-g", "--group",
    "-G", "--supp-group", NULL };
static const tp_value_opts TRANSPARENT_PREFIX_VALUE_OPTIONS[] = {
    { "sudo", tp_sudo_opts },     { "doas", tp_doas_opts },
    { "env", tp_env_opts },       { "nice", tp_nice_opts },
    { "ionice", tp_ionice_opts }, { "stdbuf", tp_stdbuf_opts },
    { "timeout", tp_timeout_opts }, { "pkexec", tp_pkexec_opts },
    { "su", tp_su_opts },         { "runuser", tp_runuser_opts },
    { NULL, NULL },
};
typedef struct { const char *name; int operands; } tp_operands;
static const tp_operands TRANSPARENT_PREFIX_OPERANDS[] = {
    { "timeout", 1 }, { NULL, 0 },
};
#define MAX_PREFIX_PEELS 8

/* _BINARY_MAGIC_PREFIXES: ELF, PE/COFF, Mach-O (universal + thin, both
 * endiannesses). */
static const unsigned char BINARY_MAGICS[][4] = {
    { 0x7f, 'E', 'L', 'F' },
    { 'M', 'Z', 0, 0 },
    { 0xca, 0xfe, 0xba, 0xbe },
    { 0xcf, 0xfa, 0xed, 0xfe },
    { 0xce, 0xfa, 0xed, 0xfe },
    { 0xfe, 0xed, 0xfa, 0xce },
    { 0xfe, 0xed, 0xfa, 0xcf },
};

/* ── small helpers ─────────────────────────────────────────────────── */

/* Compile-once pcre2 search helper. Returns true on match. */
static bool lg_regex_search_once(const char *pattern, const char *subject)
{
    if (!pattern || !subject) return false;
    int errcode = 0;
    PCRE2_SIZE erroffset = 0;
    pcre2_code *re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
                                   0, &errcode, &erroffset, NULL);
    if (!re) return false;
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
    int rc = pcre2_match(re, (PCRE2_SPTR)subject, strlen(subject), 0, 0, md, NULL);
    pcre2_match_data_free(md);
    pcre2_code_free(re);
    return rc >= 0;
}

static bool str_in_set(const char *const *set, const char *s)
{
    if (!s) return false;
    for (; *set; set++)
        if (strcmp(*set, s) == 0) return true;
    return false;
}

static bool starts_with(const char *s, const char *prefix)
{
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* _ENV_ASSIGNMENT.match(token): ^[A-Za-z_][A-Za-z0-9_]*= */
static bool is_env_assignment_token(const char *token)
{
    if (!token || !(isalpha((unsigned char)token[0]) || token[0] == '_'))
        return false;
    size_t i = 1;
    while (token[i] && (isalnum((unsigned char)token[i]) || token[i] == '_'))
        i++;
    return token[i] == '=';
}

/* _DOT_COMMAND_ARGUMENT.match(arg): ^\.[A-Za-z] */
static bool is_dot_command_argument(const char *arg)
{
    return arg && arg[0] == '.' && isalpha((unsigned char)arg[1]);
}

/* ================================================================
 *  _current_profile_name
 * ================================================================ */
/* PoP: cron_lifecycle_current_profile_name @ cron/lifecycle_guard.py:_current_profile_name */
char *cron_lifecycle_current_profile_name(void)
{
    /* Prefer the explicit env markers (profile launcher / kanban spawns),
     * falling back to the HERMES_HOME-derived active profile name.
     * Returns malloc'd name or NULL when neither source yields one. */
    static const char *env_names[] = { "HERMES_PROFILE_NAME", "HERMES_PROFILE", NULL };
    for (int i = 0; env_names[i]; i++) {
        const char *value = getenv(env_names[i]);
        if (value && *value) {
            char *stripped = str_strip_dup(value);
            if (stripped && *stripped) return stripped;
            free(stripped);
        }
    }
    char *active = profile_get_active_name();
    if (active && !*active) { free(active); return NULL; }
    return active;   /* may be NULL — mirrors get_...() or None */
}

/* ================================================================
 *  _named_profile_is_current
 * ================================================================ */
/* PoP: cron_lifecycle_named_profile_is_current @ cron/lifecycle_guard.py:_named_profile_is_current */
bool cron_lifecycle_named_profile_is_current(const char *named)
{
    char *current = cron_lifecycle_current_profile_name();
    if (!current) {
        /* No profile identity available: cannot prove self-targeting, so do
         * not block — sibling restarts must stay allowed (#78028). */
        return false;
    }
    /* named.strip().casefold() == current.strip().casefold() */
    char *a = str_strip_dup(named ? named : "");
    bool result = false;
    if (a) {
        for (char *p = a; *p; p++) *p = (char)tolower((unsigned char)*p);
        for (char *p = current; *p; p++) *p = (char)tolower((unsigned char)*p);
        result = strcmp(a, current) == 0;
        free(a);
    }
    free(current);
    return result;
}

/* ================================================================
 *  _contains_launchctl_gateway_lifecycle
 * ================================================================ */
/* PoP: cron_lifecycle_contains_launchctl_gateway_lifecycle @ cron/lifecycle_guard.py:_contains_launchctl_gateway_lifecycle */
bool cron_lifecycle_contains_launchctl_gateway_lifecycle(const char *normalized_text)
{
    /* Order-independent companion to Branch B: a launchctl lifecycle verb
     * AND a hermes-gateway label anywhere in the normalized text. */
    return lg_regex_search_once(LAUNCHCTL_LIFECYCLE_VERBS_PATTERN, normalized_text) &&
           lg_regex_search_once(HERMES_GATEWAY_LABEL_PATTERN, normalized_text);
}

/* ================================================================
 *  _is_cloud_placeholder_path
 * ================================================================ */
/* PoP: cron_lifecycle_is_cloud_placeholder_path @ cron/lifecycle_guard.py:_is_cloud_placeholder_path */
bool cron_lifecycle_is_cloud_placeholder_path(const char *path)
{
    /* True when some component sits directly under a `Library` component
     * that is a FileProvider-backed subtree (iCloud Drive / CloudStorage).
     * Purely metadata-based — never opens the file (fail closed). */
    if (!path) return false;
    char *dup = xstrdup(path);
    if (!dup) return false;
    char **parts = NULL;
    size_t nparts = 0, cap = 8;
    parts = malloc(cap * sizeof(char *));
    if (!parts) { free(dup); return false; }
    for (char *tok = strtok(dup, "/"); tok; tok = strtok(NULL, "/")) {
        if (nparts + 1 >= cap) {
            cap *= 2;
            char **nt = realloc(parts, cap * sizeof(char *));
            if (!nt) break;
            parts = nt;
        }
        parts[nparts++] = tok;
    }
    bool found = false;
    for (size_t i = 1; i < nparts && !found; i++)   /* enumerate(parts) skipping index 0 */
        if (strcmp(parts[i - 1], "Library") == 0 &&
            str_in_set(CLOUD_PLACEHOLDER_MARKERS, parts[i]))
            found = true;
    free(parts);
    free(dup);
    return found;
}

/* ================================================================
 *  _split_logical_lines
 * ================================================================ */
/* PoP: cron_lifecycle_split_logical_lines @ cron/lifecycle_guard.py:_split_logical_lines */
char **cron_lifecycle_split_logical_lines(const char *text)
{
    /* Split on newlines that are not inside quotes; escaped quotes within
     * strings are data, not command separators. Returns a NULL-terminated
     * array of malloc'd lines (caller frees each + the array). */
    size_t cap = 8, n = 0;
    char **lines = malloc(cap * sizeof(char *));
    if (!lines) return NULL;
    size_t tcap = 64, tlen = 0;
    char *current = malloc(tcap);
    if (!current) { free(lines); return NULL; }
    bool in_single = false, in_double = false, escape = false;

    for (const char *p = text ? text : ""; *p; p++) {
        char ch = *p;
        if ((size_t)(tlen + 2) > tcap) {
            tcap *= 2;
            char *nt = realloc(current, tcap);
            if (!nt) goto oom;
            current = nt;
        }
        if (escape) {
            current[tlen++] = ch;
            escape = false;
            continue;
        }
        if (ch == '\\') {
            escape = true;
            current[tlen++] = ch;
            continue;
        }
        if (ch == '\'' && !in_double) {
            in_single = !in_single;
            current[tlen++] = ch;
            continue;
        }
        if (ch == '"' && !in_single) {
            in_double = !in_double;
            current[tlen++] = ch;
            continue;
        }
        if (ch == '\n' && !in_single && !in_double) {
            current[tlen] = '\0';
            if (n + 1 >= cap) {
                cap *= 2;
                char **nl2 = realloc(lines, cap * sizeof(char *));
                if (!nl2) goto oom;
                lines = nl2;
            }
            lines[n++] = xstrdup(current);
            tlen = 0;
            continue;
        }
        current[tlen++] = ch;
    }
    if (tlen > 0) {
        current[tlen] = '\0';
        if (n + 1 >= cap) {
            cap *= 2;
            char **nl2 = realloc(lines, cap * sizeof(char *));
            if (nl2) lines = nl2;
        }
        if (n + 1 < cap) lines[n++] = xstrdup(current);
    }
    free(current);
    lines[n] = NULL;
    return lines;
oom:
    free(current);
    for (size_t i = 0; i < n; i++) free(lines[i]);
    free(lines);
    return NULL;
}

/* ================================================================
 *  _executable_name
 * ================================================================ */
/* pathlib.Path(token).name-or-token. Path(".") / Path("..") / Path("/") have
 * no name component, so fall back to the raw token so the POSIX dot-source
 * builtin "." survives. Returns malloc'd string. */
char *cron_lifecycle_executable_name(const char *token)
{
    if (!token) token = "";
    /* pathlib semantics: strip trailing slashes, then take after last '/'. */
    size_t len = strlen(token);
    while (len > 1 && token[len - 1] == '/') len--;
    if (len == 1 && token[0] == '/') len = 0;   /* Path('/') -> no name */
    size_t start = 0;
    for (size_t i = 0; i < len; i++)
        if (token[i] == '/') start = i + 1;
    if (start >= len) return xstrdup(token);    /* ".", "..", "/" -> raw token */
    char *out = malloc(len - start + 1);
    if (!out) return NULL;
    memcpy(out, token + start, len - start);
    out[len - start] = '\0';
    return out;
}

/* ================================================================
 *  _peel_transparent_prefixes
 * ================================================================ */
/* PoP: cron_lifecycle_peel_transparent_prefixes @ cron/lifecycle_guard.py:_peel_transparent_prefixes */
size_t cron_lifecycle_peel_transparent_prefixes(char *const *segment, size_t index)
{
    /* Return the index of the command a wrapper chain actually executes.
     * May return the segment length when a wrapper has no operand — callers
     * must bounds-check before indexing. */
    size_t seg_len = 0;
    while (segment && segment[seg_len]) seg_len++;
    for (int peel = 0; peel < MAX_PREFIX_PEELS; peel++) {
        if (index >= seg_len) return index;
        char *name_owned = cron_lifecycle_executable_name(segment[index]);
        if (!name_owned) return index;
        bool is_wrapper = str_in_set(TRANSPARENT_COMMAND_PREFIXES, name_owned);
        const char *const *value_options = NULL;
        if (is_wrapper)
            for (const tp_value_opts *vo = TRANSPARENT_PREFIX_VALUE_OPTIONS; vo->name; vo++)
                if (strcmp(vo->name, name_owned) == 0) { value_options = vo->options; break; }
        int operand_count = 0;
        for (const tp_operands *op = TRANSPARENT_PREFIX_OPERANDS; op->name; op++)
            if (strcmp(op->name, name_owned) == 0) { operand_count = op->operands; break; }
        free(name_owned);
        if (!is_wrapper) return index;
        index++;
        while (index < seg_len) {
            const char *token = segment[index];
            if (strcmp(token, "--") == 0) {
                /* POSIX end-of-options: the command starts at the next token. */
                index++;
                break;
            }
            if (value_options && str_in_set(value_options, token)) {
                index += 2;
                continue;
            }
            if (token[0] == '-' || is_env_assignment_token(token)) {
                index++;
                continue;
            }
            break;
        }
        for (int k = 0; k < operand_count; k++) {
            if (index < seg_len && segment[index][0] != '-') index++;
        }
    }
    return index;
}

/* ================================================================
 *  _mask_data_sink_arguments
 * ================================================================ */
/* PoP: cron_lifecycle_mask_data_sink_arguments @ cron/lifecycle_guard.py:_mask_data_sink_arguments */
char *cron_lifecycle_mask_data_sink_arguments(const char *text)
{
    /* Replace data-sink executables' arguments with a neutral placeholder so
     * the lifecycle regex only blocks matches in actual COMMAND position.
     * Strictly fail-closed: masking can only ever ALLOW a command the plain
     * regex would have blocked — never block one it would have allowed. */
    if (!text) return NULL;
    char **lines = cron_lifecycle_split_logical_lines(text);
    if (!lines) return NULL;
    /* Python: text.splitlines() or [text] — empty text yields []. */
    size_t nlines = 0;
    while (lines[nlines]) nlines++;

    /* Precompile _PIPE_TO_INTERPRETER once per call (cheap, fail-open keeps line). */
    bool pipe_re_ok = true;

    size_t ocap = 16, on = 0;
    char **lines_out = malloc(ocap * sizeof(char *));
    if (!lines_out) { for (size_t i = 0; lines[i]; i++) free(lines[i]); free(lines); return NULL; }
    bool changed = false;

    for (size_t li = 0; li <= nlines; li++) {
        /* Python `for line in text.splitlines() or [text]`: when splitlines()
         * is empty (no newline-terminated content), iterate [text] itself. */
        const char *line;
        if (nlines > 0) {
            if (li >= nlines) break;
            line = lines[li];
        } else {
            if (li > 0) break;
            line = text;
        }
        if (pipe_re_ok && lg_regex_search_once(PIPE_TO_INTERPRETER_PATTERN, line)) {
            if (on + 2 >= ocap) { ocap *= 2; lines_out = realloc(lines_out, ocap * sizeof(char *)); }
            lines_out[on++] = xstrdup(line);
            continue;
        }
        char ***segments = cron_lifecycle_iter_command_segments(line);
        if (!segments) {
            /* Cannot be tokenized at all — leave unchanged (fail closed). */
            if (on + 2 >= ocap) { ocap *= 2; lines_out = realloc(lines_out, ocap * sizeof(char *)); }
            lines_out[on++] = xstrdup(line);
            continue;
        }

        /* rebuilt tokens across all segments of this line */
        size_t rcap = 16, rn = 0;
        char **rebuilt = malloc(rcap * sizeof(char *));
        if (!rebuilt) { cron_lifecycle_free_segments(segments); break; }
        for (size_t si = 0; segments[si]; si++) {
            char **segment = segments[si];
            size_t seg_len = 0;
            while (segment[seg_len]) seg_len++;
            if (seg_len == 0) continue;
            int idx = cron_lifecycle_command_token_index(segment);
            bool masked = false;
            if (idx >= 0) {
                char *exe = cron_lifecycle_executable_name(segment[idx]);
                if (exe && str_in_set(DATA_SINK_EXECUTABLES, exe)) {
                    /* arguments = segment[idx+1:] */
                    bool unsafe = false;
                    for (size_t ai = (size_t)idx + 1; ai < seg_len && !unsafe; ai++) {
                        const char *argument = segment[ai];
                        if (is_dot_command_argument(argument)) { unsafe = true; break; }
                        for (const char *const *mk = UNSAFE_DATA_ARG_MARKERS; *mk; mk++)
                            if (strstr(argument, *mk)) { unsafe = true; break; }
                    }
                    if (!unsafe) {
                        changed = true;
                        masked = true;
                        for (int ki = 0; ki <= idx; ki++) {
                            if (rn + 2 >= rcap) { rcap *= 2; rebuilt = realloc(rebuilt, rcap * sizeof(char *)); }
                            rebuilt[rn++] = xstrdup(segment[ki]);
                        }
                        for (size_t ai = (size_t)idx + 1; ai < seg_len; ai++) {
                            if (rn + 2 >= rcap) { rcap *= 2; rebuilt = realloc(rebuilt, rcap * sizeof(char *)); }
                            rebuilt[rn++] = xstrdup("arg");
                        }
                    }
                }
                free(exe);
            }
            if (!masked) {
                for (size_t ti = 0; ti < seg_len; ti++) {
                    if (rn + 2 >= rcap) { rcap *= 2; rebuilt = realloc(rebuilt, rcap * sizeof(char *)); }
                    rebuilt[rn++] = xstrdup(segment[ti]);
                }
            }
        }
        cron_lifecycle_free_segments(segments);

        /* lines_out.append(" ".join(rebuilt)) */
        size_t total = 1;
        for (size_t ti = 0; ti < rn; ti++) total += strlen(rebuilt[ti]) + 1;
        if (on + 2 >= ocap) { ocap *= 2; lines_out = realloc(lines_out, ocap * sizeof(char *)); }
        char *joined = malloc(total);
        if (joined) {
            joined[0] = '\0';
            for (size_t ti = 0; ti < rn; ti++) {
                if (ti) strcat(joined, " ");
                strcat(joined, rebuilt[ti]);
            }
            lines_out[on++] = joined;
        } else {
            lines_out[on++] = xstrdup(line);
        }
        for (size_t ti = 0; ti < rn; ti++) free(rebuilt[ti]);
        free(rebuilt);
    }
    for (size_t i = 0; lines[i]; i++) free(lines[i]);
    free(lines);

    if (!changed) {
        char *result = xstrdup(text);
        for (size_t i = 0; i < on; i++) free(lines_out[i]);
        free(lines_out);
        return result;
    }
    size_t total = 1;
    for (size_t i = 0; i < on; i++) total += strlen(lines_out[i]) + 1;
    char *result = malloc(total);
    if (result) {
        result[0] = '\0';
        for (size_t i = 0; i < on; i++) {
            if (i) strcat(result, "\n");
            strcat(result, lines_out[i]);
        }
    }
    for (size_t i = 0; i < on; i++) free(lines_out[i]);
    free(lines_out);
    return result;
}

/* ================================================================
 *  _lifecycle_command_scan_with_data_exemption
 * ================================================================ */
/* PoP: cron_lifecycle_lifecycle_command_scan_with_data_exemption @ cron/lifecycle_guard.py:_lifecycle_command_scan_with_data_exemption */
bool cron_lifecycle_lifecycle_command_scan_with_data_exemption(const char *text)
{
    /* Two-pass: cheap regex first; on a raw match re-scan with data-sink
     * arguments masked out. Only a match that survives masking — one in
     * actual command position — blocks. */
    if (!cron_lifecycle_contains_gateway_lifecycle_command(text)) return false;
    /* normalized = _SHELL_LINE_CONTINUATION.sub(" ", text) — collapse
     * backslash-newline continuations to single spaces. */
    size_t n = strlen(text);
    char *normalized = malloc(n + 1);
    if (!normalized) return true;   /* fail closed like the raw match */
    size_t o = 0;
    for (size_t i = 0; i < n;) {
        if (text[i] == '\\' && (text[i+1] == '\n' || (text[i+1] == '\r' && text[i+2] == '\n'))) {
            if (text[i+1] == '\r') i++;      /* \r\n */
            i++;                              /* newline */
            while (text[i] == ' ' || text[i] == '\t') i++;
            normalized[o++] = ' ';
            continue;
        }
        normalized[o++] = text[i++];
    }
    normalized[o] = '\0';
    char *masked = cron_lifecycle_mask_data_sink_arguments(normalized);
    bool verdict = masked
        ? cron_lifecycle_contains_gateway_lifecycle_command(masked)
        : true;   /* masking unavailable -> keep raw-match verdict (fail closed) */
    free(masked);
    free(normalized);
    return verdict;
}

/* ================================================================
 *  _direct_lifecycle_scan
 * ================================================================ */
/* PoP: cron_lifecycle_direct_lifecycle_scan @ cron/lifecycle_guard.py:_direct_lifecycle_scan */
bool cron_lifecycle_direct_lifecycle_scan(const char *command)
{
    /* Pure-string direct scans: lifecycle regex (data-exempted) + submit. */
    return cron_lifecycle_lifecycle_command_scan_with_data_exemption(command) ||
           cron_lifecycle_contains_launchctl_submit_command(command);
}

/* ================================================================
 *  _iter_option_values
 * ================================================================ */
/* PoP: cron_lifecycle_iter_option_values @ cron/lifecycle_guard.py:_iter_option_values */
char **cron_lifecycle_iter_option_values(char *const *segment, size_t start,
                                         const char *option)
{
    /* Yield values given to *option*, in both "--opt v" and "--opt=v" form.
     * Returns a NULL-terminated array of malloc'd strings (caller frees). */
    size_t cap = 4, n = 0;
    char **values = malloc(cap * sizeof(char *));
    if (!values) return NULL;
    values[n] = NULL;
    size_t prefix_len = strlen(option) + 1;
    char *prefix = malloc(prefix_len + 1);
    if (!prefix) { free(values); return NULL; }
    snprintf(prefix, prefix_len + 1, "%s=", option);
    size_t seg_len = 0;
    while (segment && segment[seg_len]) seg_len++;
    for (size_t position = start + 1; position < seg_len; position++) {
        const char *token = segment[position];
        char *found = NULL;
        if (strcmp(token, option) == 0 && position + 1 < seg_len)
            found = xstrdup(segment[position + 1]);
        else if (starts_with(token, prefix))
            found = xstrdup(token + prefix_len);
        if (found) {
            if (n + 2 >= cap) {
                cap *= 2;
                char **nv = realloc(values, cap * sizeof(char *));
                if (!nv) { free(found); break; }
                values = nv;
            }
            values[n++] = found;
            values[n] = NULL;
        }
    }
    free(prefix);
    return values;
}

/* ================================================================
 *  _references_at
 * ================================================================ */
/* PoP: cron_lifecycle_references_at @ cron/lifecycle_guard.py:_references_at */
char **cron_lifecycle_references_at(char *const *segment, size_t index,
                                    const char *cwd)
{
    /* Yield the scripts the token at *index* executes, if any. Returns a
     * NULL-terminated array of malloc'd path strings (caller frees each +
     * the array). */
    size_t cap = 4, n = 0;
    char **paths = malloc(cap * sizeof(char *));
    if (!paths) return NULL;
    paths[n] = NULL;
    #define REF_APPEND(p_) do { \
        if ((p_)) { \
            if (n + 2 >= cap) { \
                cap *= 2; \
                char **np2 = realloc(paths, cap * sizeof(char *)); \
                if (!np2) { free((p_)); goto done; } \
                paths = np2; \
            } \
            paths[n++] = (p_); \
            paths[n] = NULL; \
        } \
    } while (0)

    size_t seg_len = 0;
    while (segment && segment[seg_len]) seg_len++;
    if (index >= seg_len) return paths;
    const char *executable = segment[index];
    char *executable_name_owned = cron_lifecycle_executable_name(executable);
    if (!executable_name_owned) return paths;

    if (strcmp(executable_name_owned, ".") == 0 ||
        strcmp(executable_name_owned, "source") == 0) {
        if (seg_len > index + 1) {
            char *resolved =
                cron_lifecycle_resolve_terminal_script_path(segment[index + 1], cwd);
            REF_APPEND(resolved);
        }
        goto done;
    }

    if (str_in_set(SHELL_EXECUTABLES, executable_name_owned)) {
        /* arguments = segment[index+1:]; walk past options to the script. */
        const char **arguments = calloc(seg_len > index ? seg_len - index : 1,
                                        sizeof(char *));
        if (!arguments) goto done;
        size_t nargs = 0;
        for (size_t i = index + 1; i < seg_len; i++) arguments[nargs++] = segment[i];
        size_t arg_index = 0;
        while (arg_index < nargs) {
            const char *argument = arguments[arg_index];
            if (strcmp(argument, "--") == 0) { arg_index++; break; }
            if (strcmp(argument, "-c") == 0 || strcmp(argument, "--command") == 0)
                break;
            if (str_in_set(SHELL_OPTIONS_WITH_VALUES, argument)) { arg_index += 2; continue; }
            if (argument[0] == '-') { arg_index++; continue; }
            break;
        }
        if (arg_index < nargs &&
            strcmp(arguments[arg_index], "-c") != 0 &&
            strcmp(arguments[arg_index], "--command") != 0) {
            char *resolved =
                cron_lifecycle_resolve_terminal_script_path(arguments[arg_index], cwd);
            REF_APPEND(resolved);
        }
        free(arguments);
        goto done;
    }

    /* A bare "/" token is not an executable reference (pathlib division
     * operator in Python sources, #77131): skip pure-separator tokens. */
    {
        bool bare_separator = true;
        for (const char *p = executable; *p; p++)
            if (*p != '/') { bare_separator = false; break; }
        if (!bare_separator) {
            bool has_slash = strchr(executable, '/') != NULL;
            size_t elen = strlen(executable);
            bool shell_ext = (elen >= 3 && strcmp(executable + elen - 3, ".sh") == 0) ||
                             (elen >= 5 && strcmp(executable + elen - 5, ".bash") == 0) ||
                             (elen >= 4 && strcmp(executable + elen - 4, ".zsh") == 0);
            if (has_slash || shell_ext) {
                char *resolved =
                    cron_lifecycle_resolve_terminal_script_path(executable, cwd);
                REF_APPEND(resolved);
            }
        }
    }
done:
    free(executable_name_owned);
    return paths;
    #undef REF_APPEND
}

/* ================================================================
 *  _has_binary_magic
 * ================================================================ */
/* PoP: cron_lifecycle_has_binary_magic @ cron/lifecycle_guard.py:_has_binary_magic */
bool cron_lifecycle_has_binary_magic(const unsigned char *data, size_t len)
{
    /* A shebang always wins — an interpreted script is never a binary,
     * however odd its payload. Deliberately narrower than "contains NUL". */
    if (len >= 2 && data[0] == '#' && data[1] == '!') return false;
    for (size_t m = 0; m < sizeof(BINARY_MAGICS) / sizeof(BINARY_MAGICS[0]); m++) {
        /* "MZ" magic is effectively 2 bytes; compare only nonzero prefix bytes. */
        size_t plen = 4;
        while (plen > 0 && BINARY_MAGICS[m][plen - 1] == 0) plen--;
        if (plen == 0) continue;
        if (len >= plen && memcmp(data, BINARY_MAGICS[m], plen) == 0)
            return true;
    }
    return false;
}
