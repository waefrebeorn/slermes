/*
 * port_tools_terminal_hints.c — C11 port of tools/terminal_hints.py
 *
 * Output-pattern failure hints for the terminal tool.
 * Extends the exit-code semantics table in terminal_tool with an
 * *output-pattern* tier: a bounded scan of command output that maps
 * well-known failure shapes to one short, actionable recovery hint.
 *
 * Design rules (kept identical to the Python original):
 *   * Only fires on non-zero exit codes — never annotate success.
 *   * At most ONE hint per result, first match wins; patterns ordered by
 *     observed frequency in production trajectories (state.db mining, Aug 2026).
 *   * Scans only the first _SCAN_CHARS of output — hints key on error headers.
 *   * Hints state the *next action*, not a diagnosis essay.
 *   * Pure function, no I/O, no config reads — trivially unit-testable.
 */

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "port_tools_terminal_hints.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "libregex/hermes_regex.h"

/* Bounded scan window: error headers appear early; deep output is noise. */
#define _SCAN_CHARS 4000

/* Exit-code-only hints for codes the semantics table does not cover. */
static const char _EXIT_CODE_HINTS[][512] = {
    [126] = "Exit 126: the file was found but is not executable — `chmod +x` it or invoke it via its interpreter (e.g. `bash script.sh`).",
    [124] = "Exit 124: the command hit its timeout. Raise timeout= (foreground max 600s) or run it with background=true and notify_on_complete=true.",
    [137] = "Exit 137: the process was SIGKILLed — usually out-of-memory or an external kill. Reduce memory use or check `dmesg | tail` before retrying.",
};
#define _EXIT_CODE_HINTS_LEN (sizeof(_EXIT_CODE_HINTS)/sizeof(_EXIT_CODE_HINTS[0]))

/* --- per-pattern hint functions (PoP: tools/terminal_hints.py:<fn>) ---
 * Each takes (command, output_window) and returns a malloc'd hint or NULL.
 * The window is already the first _SCAN_CHARS of the output. */

/* PoP: thint_hint_gh_unknown_json_field @ tools/terminal_hints.py:_hint_gh_unknown_json_field */
/* ~9,175x: gh CLI version drift — model asks for fields the installed gh
 * doesn't know. gh already prints the valid field list.
 * Pattern: r'Unknown JSON field: "?(\w+)' */
char *thint_hint_gh_unknown_json_field(const char *command, const char *output) {
    (void)command;
    if (!output) return NULL;
    char *val = regex_extract("Unknown JSON field: ?\"?([[:alnum:]_]+)", output, 1);
    if (!val || !*val) { free(val); return NULL; }
    char *hint = NULL;
    asprintf(&hint,
        "The installed gh does not support the JSON field '%s'. "
        "The valid field list is printed in the output above — retry using "
        "only fields from that list.", val);
    free(val);
    return hint;
}

/* PoP: thint_hint_command_not_found @ tools/terminal_hints.py:_hint_command_not_found */
/* ~1,010x generic; 837x bare `python` on python3-only distros.
 * Pattern: r"(?:bash: line \d+: |bash: |sh: \d*:? ?)?([\w.+-]+): command not found" */
char *thint_hint_command_not_found(const char *command, const char *output) {
    (void)command;
    if (!output) return NULL;
    /* regex_search finds the missing command token anywhere in output */
    hregex_t *re = regex_compile(
        "(?:bash: line [0-9]+: |bash: |sh: [0-9]*:? ?)?([[:alnum:]_.+\\-]+): command not found",
        0);
    if (!re) return NULL;
    regex_match_t *m = regex_search(re, output);
    char *missing = (m && m->matched && m->groups[1]) ? strdup(m->groups[1]) : NULL;
    regex_match_free(m);
    regex_free(re);
    if (!missing || !*missing) { free(missing); return NULL; }

    char *hint = NULL;
    if (strcmp(missing, "python") == 0) {
        asprintf(&hint,
            "This system has no bare `python` — use `python3`, or the "
            "project venv's interpreter (e.g. .venv/bin/python).");
    } else if (strcmp(missing, "pip") == 0) {
        asprintf(&hint,
            "This system has no bare `pip` — use `pip3`, `python3 -m pip`, "
            "or the project venv's pip (e.g. .venv/bin/pip).");
    } else {
        asprintf(&hint,
            "`%s` is not installed or not on PATH. Verify with "
            "`which %s`; install it or use an absolute path instead of "
            "retrying the same command.", missing, missing);
    }
    free(missing);
    return hint;
}

/* PoP: thint_hint_module_not_found @ tools/terminal_hints.py:_hint_module_not_found */
/* ~739x: almost always a venv-activation slip, not a missing dependency.
 * Pattern: r'(?:ModuleNotFoundError|ImportError): No module named '?([\w.]+)' */
char *thint_hint_module_not_found(const char *command, const char *output) {
    (void)command;
    if (!output) return NULL;
    char *val = regex_extract(
        "(ModuleNotFoundError|ImportError): No module named '?([[:alnum:]_.]+)",
        output, 2);
    if (!val || !*val) { free(val); return NULL; }
    char *hint = NULL;
    asprintf(&hint,
        "Python cannot import '%s'. Most often the wrong "
        "interpreter is running: activate the project venv (e.g. `source "
        ".venv/bin/activate`) or invoke its python directly. Only pip "
        "install if the package is genuinely absent from that venv.", val);
    free(val);
    return hint;
}

/* PoP: thint_hint_merge_conflict @ tools/terminal_hints.py:_hint_merge_conflict */
/* ~1,172x: models sometimes re-run the failing merge/rebase verbatim.
 * Pattern: r"^CONFLICT |Automatic merge failed|needs merge" with re.M */
char *thint_hint_merge_conflict(const char *command, const char *output) {
    (void)command;
    if (!output) return NULL;
    /* Python uses re.search(pattern, output, re.M): the ^ anchor matches at
     * each line start. POSIX regexec (without REG_NEWLINE) only honours ^ at
     * offset 0, so we honour the line-start semantics manually for the
     * CONFLICT alternative while still using regex_search for the phrases. */
    bool hit = false;
    if (strstr(output, "Automatic merge failed") || strstr(output, "needs merge"))
        hit = true;
    if (!hit) {
        const char *line = output;
        while (*line) {
            if (strncmp(line, "CONFLICT ", 9) == 0) { hit = true; break; }
            const char *nl = strchr(line, '\n');
            line = nl ? nl + 1 : NULL;
            if (!line) break;
        }
    }
    if (!hit) return NULL;
    return strdup(
        "Git merge conflict. Do not retry this command. Resolve the "
        "conflicted files listed above (edit, then `git add`), then continue "
        "(`git rebase --continue` / commit the merge) — or abort with "
        "`--abort`.");
}

/* PoP: thint_hint_already_exists @ tools/terminal_hints.py:_hint_already_exists */
/* ~633x: branch/dir/file already exists → retrying unchanged always fails.
 * Pattern: r"(?:fatal|error):.*?'([^']+)' already exists" */
char *thint_hint_already_exists(const char *command, const char *output) {
    (void)command;
    if (!output) return NULL;
    char *val = regex_extract(
        "(?:fatal|error):.*?'([^']+)' already exists", output, 1);
    if (!val || !*val) { free(val); return NULL; }
    char *hint = NULL;
    asprintf(&hint,
        "'%s' already exists — retrying unchanged will keep "
        "failing. Reuse it, choose another name, or delete it first if it is "
        "genuinely stale.", val);
    free(val);
    return hint;
}

/* PoP: thint_hint_gh_rate_limit @ tools/terminal_hints.py:_hint_gh_rate_limit */
/* ~133x: immediate retries burn turns; the limit is time-based. */
char *thint_hint_gh_rate_limit(const char *command, const char *output) {
    (void)command;
    if (!output) return NULL;
    if (strstr(output, "API rate limit") == NULL
        && strstr(output, "was submitted too quickly") == NULL)
        return NULL;
    return strdup(
        "GitHub API rate limit hit — immediate retries will keep failing. "
        "Continue with other work and retry this operation later.");
}

/* PoP: thint_hint_permission_denied @ tools/terminal_hints.py:_hint_permission_denied */
char *thint_hint_permission_denied(const char *command, const char *output) {
    (void)command;
    if (!output) return NULL;
    if (strstr(output, "Permission denied") == NULL
        && strstr(output, "EACCES") == NULL)
        return NULL;
    return strdup(
        "Permission denied. Check ownership/mode of the target path "
        "(`ls -la`); prefer a user-writable location. Only escalate to sudo "
        "if the task genuinely requires it.");
}

/* Ordered by production frequency — first match wins. Mirrors _OUTPUT_HINTS. */
typedef char *(*hint_fn)(const char *command, const char *output);
static const hint_fn _OUTPUT_HINTS[] = {
    thint_hint_gh_unknown_json_field,
    thint_hint_merge_conflict,
    thint_hint_command_not_found,
    thint_hint_module_not_found,
    thint_hint_already_exists,
    thint_hint_gh_rate_limit,
    thint_hint_permission_denied,
};
#define _N_HINTS (sizeof(_OUTPUT_HINTS)/sizeof(_OUTPUT_HINTS[0]))

/* PoP: thint_annotate_failure @ tools/terminal_hints.py:annotate_failure */
/* Return one short recovery hint for a failed command, or NULL.
 * Returns NULL for exit_code == 0. */
char *thint_annotate_failure(const char *command, int exit_code, const char *output) {
    if (exit_code == 0)
        return NULL;
    const char *window = output ? output : "";
    size_t wlen = strlen(window);
    char *scan = (wlen > _SCAN_CHARS) ? strndup(window, _SCAN_CHARS) : strdup(window);
    if (!scan) return NULL;

    char *hint = NULL;
    if (*scan) {
        for (size_t i = 0; i < _N_HINTS; i++) {
            hint = _OUTPUT_HINTS[i](command ? command : "", scan);
            if (hint) break;
        }
    }
    free(scan);

    if (hint)
        return hint;

    /* Exit-code-only hints for codes not covered by output patterns. */
    if (exit_code >= 0 && exit_code < (int)_EXIT_CODE_HINTS_LEN
        && _EXIT_CODE_HINTS[exit_code][0]) {
        return strdup(_EXIT_CODE_HINTS[exit_code]);
    }
    return NULL;
}

/* ── masked-success annotation (battleground 8/10 → 10/10) ─────────────── */

#define THINT_SCAN_CHARS 4000

/* PoP: _first_token @ tools/terminal_hints.py:_first_token */
static void thint_first_token(const char *command, char *out, size_t outsz)
{
    out[0] = '\0';
    if (!command) return;
    const char *s = command;
    while (*s == ' ' || *s == '\t') s++;

    while (*s) {
        /* token ends at whitespace */
        const char *e = s;
        while (*e && *e != ' ' && *e != '\t') e++;
        size_t len = (size_t)(e - s);
        if (len >= outsz) len = outsz - 1;

        /* skip env-var assignments (= present, not starting =, ./, /) */
        bool has_eq = memchr(s, '=', len) != NULL;
        bool wrapper = (s[0] == '=' ) || (len > 1 && s[0] == '.' && s[1] == '/') ||
                       (s[0] == '/');
        if (has_eq && !wrapper) {
            s = e;
            while (*s == ' ' || *s == '\t') s++;
            continue;
        }
        memcpy(out, s, len);
        out[len] = '\0';
        /* rsplit("/", 1)[-1] */
        char *slash = strrchr(out, '/');
        if (slash) memmove(out, slash + 1, strlen(slash + 1) + 1);
        return;
    }
}

/* PoP: annotate_masked_success @ tools/terminal_hints.py:annotate_masked_success
 * Advisory note when an exit-0 result likely masks a failure. Returns a
 * malloc'd note or NULL. Caller frees. */
char *thint_annotate_masked_success(const char *command, const char *output)
{
    static const char *const READONLY_HEADS[] = {
        "grep", "rg", "ag", "find", "ls", "cat", "head", "tail", "jq", "awk",
        "sed", "strings", "zcat", "journalctl", "dmesg", "echo", "printf",
        NULL,
    };
    static const char *const PASSTHROUGH[] = {
        "tail", "head", "cat", "tee", "less", "more", "wc", "sort", "uniq",
        NULL,
    };

    const char *cmd = command ? command : "";
    char window[THINT_SCAN_CHARS + 1];
    size_t wlen = output ? strlen(output) : 0;
    if (wlen > THINT_SCAN_CHARS) wlen = THINT_SCAN_CHARS;
    if (output) { memcpy(window, output, wlen); }
    window[wlen] = '\0';

    if (!*cmd || !wlen) return NULL;

    char head[128];
    thint_first_token(cmd, head, sizeof(head));
    for (int i = 0; READONLY_HEADS[i]; i++)
        if (strcmp(head, READONLY_HEADS[i]) == 0) return NULL;

    /* _FAILURE_SHAPES: strong tool-specific failure markers. */
    static const char *const SHAPES[] = {
        "error[E", "error: could not compile", "error: aborting due to",
        "Traceback (most recent call last)", "compilation terminated.",
        "npm ERR!", "BUILD FAILED", "Build FAILED", "FAILED: ",
        NULL,
    };
    bool shape_hit = false;
    for (int i = 0; SHAPES[i]; i++)
        if (strstr(window, SHAPES[i])) { shape_hit = true; break; }
    /* pytest summary: "(=+ )?\d+ failed" at line start */
    for (const char *p = window; p && !shape_hit; ) {
        if (p == window || p[-1] == '\n') {
            size_t eq = 0;
            while (p[eq] == '=') eq++;
            if (eq > 0 && p[eq] == ' ') eq++;
            if (p[eq] >= '0' && p[eq] <= '9') {
                size_t d = eq;
                while (p[d] >= '0' && p[d] <= '9') d++;
                if (strncmp(p + d, " failed", 7) == 0) shape_hit = true;
            }
        }
        p = strchr(p, '\n');
        if (p) p++;
    }
    /* pytest per-test: line starts "FAILED " then word:: or word.py */
    for (const char *p = window; p && !shape_hit; ) {
        if ((p == window || p[-1] == '\n') && strncmp(p, "FAILED ", 7) == 0) {
            const char *w = p + 7;
            const char *dc = strstr(w, "::");
            const char *dp = strstr(w, ".py");
            if ((dc && dc < w + 200) || (dp && dp < w + 200)) shape_hit = true;
        }
        p = strchr(p, '\n');
        if (p) p++;
    }
    /* make: "***" at line start with optional [N] */
    for (const char *p = window; p && !shape_hit; ) {
        if ((p == window || p[-1] == '\n') && strncmp(p, "make", 4) == 0) {
            const char *q = p + 4;
            if (*q == '[') { q = strchr(q, ']'); if (q) q++; }
            if (q && strncmp(q, ": ***", 5) == 0) shape_hit = true;
        }
        p = strchr(p, '\n');
        if (p) p++;
    }
    if (!shape_hit) return NULL;

    /* masking pipe: last segment is a passthrough consumer */
    bool masking_pipe = false;
    {
        const char *last_pipe = NULL;
        for (const char *p = cmd; *p; p++) {
            if (*p == '|' && *(p + 1) != '|' &&
                !(p > cmd && p[-1] == '|')) {
                /* single pipe — check it's the start of the last segment */
                last_pipe = p;
            }
        }
        if (last_pipe) {
            const char *seg = last_pipe + 1;
            while (*seg == ' ') seg++;
            for (int i = 0; PASSTHROUGH[i]; i++) {
                size_t l = strlen(PASSTHROUGH[i]);
                if (strncmp(seg, PASSTHROUGH[i], l) == 0 &&
                    !strchr(seg, '|')) {
                    masking_pipe = true;
                    break;
                }
            }
        }
    }
    if (masking_pipe) {
        return strdup(
            "exit_code 0 here is the status of the last pipeline command "
            "(tail/head/cat/...), NOT of the command before the pipe — and "
            "the output contains failure indicators. Treat this run as "
            "FAILED until proven otherwise: re-run the command WITHOUT the "
            "pipe (output is auto-truncated and the full text is saved to a "
            "file, so piping through tail/head is never needed) to get the "
            "real exit code.");
    }

    /* masking ||: fallback swallows the failure status */
    if (strstr(cmd, "|| echo ") || strstr(cmd, "|| echo\t") ||
        strstr(cmd, "|| printf") || strstr(cmd, "|| true") ||
        strstr(cmd, "|| : ") || strstr(cmd, "|| :")) {
        return strdup(
            "exit_code 0 here is the status of the `||` fallback (echo/true), "
            "NOT of the command before it — and the output contains failure "
            "indicators. Treat this run as FAILED until proven otherwise: "
            "re-run the command bare to get its real exit code.");
    }
    return NULL;
}
