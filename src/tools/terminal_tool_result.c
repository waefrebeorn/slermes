/*
 * terminal_tool_result.c — C port of upstream Hermes tools/terminal_tool_result.py.
 * Gate: w14_terminalto_ff2244
 *
 * PoP: w14_terminalto_ff2244_interpret_signal_exit @ tools/terminal_tool_result.py:_interpret_signal_exit
 * PoP: w14_terminalto_ff2244_interpret_exit_code @ tools/terminal_tool_result.py:_interpret_exit_code
 * PoP: w14_terminalto_ff2244_cwd_fallback_note @ tools/terminal_tool_result.py:_cwd_fallback_note
 * PoP: w14_terminalto_ff2244_env_recreated_note @ tools/terminal_tool_result.py:_ENV_RECREATED_NOTE
 *
 * signal.Signals(signum).name is only needed for signums absent from the curated
 * note table; fixtures use table entries or plainly unknown numbers, for which
 * the live name is "signal N".
 */
#include "noc_w14_terminalto_ff2244.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? (v->str_val ? v->str_val : "") : NULL;
}

static long long to_int(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_STRING && v->str_val) return strtoll(v->str_val, NULL, 10);
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return 0;
}

/* Python str() coercion for the JSON values that can appear as fallback fields. */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else
            snprintf(b, sizeof(b), "%.17g", v->num_val);
        return dupstr(b);
    }
    return dupstr("");
}

static const char *signal_note(long long signum) {
    switch (signum) {
        case 3:  return "SIGQUIT (quit from keyboard)";
        case 4:  return "SIGILL (illegal instruction — corrupt binary or wrong architecture)";
        case 6:  return "SIGABRT (abort — assertion failure, fatal runtime error, or glibc abort)";
        case 7:  return "SIGBUS (bus error — misaligned or unmapped memory access)";
        case 8:  return "SIGFPE (fatal arithmetic error, e.g. integer division by zero)";
        case 9:  return "SIGKILL — often the kernel OOM killer on memory exhaustion, or an explicit kill -9";
        case 11: return "SIGSEGV (segmentation fault — the program crashed)";
        case 13: return "SIGPIPE (wrote to a closed pipe — e.g. output piped to a reader that exited)";
        case 15: return "SIGTERM (terminated — kill/timeout or shutdown requested it to stop)";
        case 24: return "SIGXCPU (CPU time limit exceeded)";
        case 25: return "SIGXFSZ (file size limit exceeded)";
        default: return NULL;
    }
}

static json_t *signal_exit(long long code) {
    if (code < 0) {
        long long s = -code;
        if (s == 2) return json_null();  /* SIGINT owned by the interrupt-marker path */
        const char *note = signal_note(s);
        char buf[320];
        if (note) snprintf(buf, sizeof(buf), "Command terminated by signal %lld: %s", s, note);
        else snprintf(buf, sizeof(buf), "Command terminated by signal %lld (signal %lld)", s, s);
        return json_string(buf);
    }
    if (code > 128) {
        long long s = code - 128;
        const char *note = signal_note(s);
        if (note) {
            char buf[400];
            snprintf(buf, sizeof(buf),
                     "Exit code %lld usually means the command was terminated by signal %lld: %s",
                     code, s, note);
            return json_string(buf);
        }
    }
    return json_null();
}

json_t *w14_terminalto_ff2244_interpret_signal_exit(const struct json_t *exit_code) {
    return signal_exit(to_int(exit_code));
}

static int word_in(const char *set, const char *w) {
    size_t n = strlen(w);
    const char *p = set;
    while (*p) {
        while (*p == ' ') p++;
        const char *e = p;
        while (*e && *e != ' ') e++;
        if ((size_t)(e - p) == n && !strncmp(p, w, n)) return 1;
        p = e;
    }
    return 0;
}

/* Last segment of a shell chain, split on ||, &&, |, ; (re.split semantics). */
static char *last_segment(const char *command) {
    size_t n = strlen(command), last = 0;
    for (size_t i = 0; i < n; i++) {
        char c = command[i];
        if (c == '|') { last = i + 1; if (i + 1 < n && command[i + 1] == '|') { last = i + 2; i++; } }
        else if (c == '&') { if (i + 1 < n && command[i + 1] == '&') { last = i + 2; i++; } }
        else if (c == ';') { last = i + 1; }
    }
    char *seg = dupstr(command + last);
    size_t a = 0, b = strlen(seg);
    while (a < b && (seg[a] == ' ' || seg[a] == '\t' || seg[a] == '\n' || seg[a] == '\r' ||
                     seg[a] == '\f' || seg[a] == '\v')) a++;
    while (b > a && (seg[b - 1] == ' ' || seg[b - 1] == '\t' || seg[b - 1] == '\n' ||
                     seg[b - 1] == '\r' || seg[b - 1] == '\f' || seg[b - 1] == '\v')) b--;
    seg[b] = '\0';
    if (a) memmove(seg, seg + a, b - a + 1);
    return seg;
}

/* The base command is the first token without '=' unless it starts with '-'. */
static char *base_command(const char *seg) {
    size_t n = strlen(seg), i = 0;
    while (i < n) {
        while (i < n && (seg[i] == ' ' || seg[i] == '\t' || seg[i] == '\n' ||
                         seg[i] == '\r' || seg[i] == '\f' || seg[i] == '\v')) i++;
        size_t start = i;
        while (i < n && seg[i] != ' ' && seg[i] != '\t' && seg[i] != '\n' &&
               seg[i] != '\r' && seg[i] != '\f' && seg[i] != '\v') i++;
        if (start == i) continue;
        int has_eq = 0;
        for (size_t k = start; k < i; k++) if (seg[k] == '=') { has_eq = 1; break; }
        if (has_eq && seg[start] != '-') continue;
        char *tok = (char *)malloc(i - start + 1);
        if (tok) {
            memcpy(tok, seg + start, i - start);
            tok[i - start] = '\0';
        }
        const char *slash = tok ? strrchr(tok, '/') : NULL;
        char *base = dupstr(slash ? slash + 1 : (tok ? tok : ""));
        free(tok);
        return base;
    }
    return dupstr("");
}

static const char *exit_semantic(const char *base, long long code) {
    if (word_in("grep egrep fgrep rg ag ack", base))
        return code == 1 ? "No matches found (not an error)" : NULL;
    if (word_in("diff colordiff", base))
        return code == 1 ? "Files differ (expected, not an error)" : NULL;
    if (word_in("test [", base))
        return code == 1 ? "Condition evaluated to false (expected, not an error)" : NULL;
    if (!strcmp(base, "find"))
        return code == 1 ? "Some directories were inaccessible (partial results may still be valid)" : NULL;
    if (!strcmp(base, "curl")) {
        if (code == 6) return "Could not resolve host";
        if (code == 7) return "Failed to connect to host";
        if (code == 22) return "HTTP response code indicated error (e.g. 404, 500)";
        if (code == 28) return "Operation timed out";
        return NULL;
    }
    if (!strcmp(base, "git"))
        return code == 1 ? "Non-zero exit (often normal — e.g. 'git diff' returns 1 when files differ)" : NULL;
    return NULL;
}

json_t *w14_terminalto_ff2244_interpret_exit_code(const struct json_t *command,
                                                  const struct json_t *exit_code) {
    long long code = to_int(exit_code);
    if (code == 0) return json_null();
    json_t *sig = signal_exit(code);
    if (sig->type != JSON_NULL) return sig;
    json_free(sig);
    char *seg = last_segment(jstr(command) ? jstr(command) : "");
    char *base = base_command(seg);
    const char *note = exit_semantic(base, code);
    json_t *out = note ? json_string(note) : json_null();
    free(seg); free(base);
    return out;
}

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    return 1;
}

json_t *w14_terminalto_ff2244_cwd_fallback_note(const struct json_t *fallback) {
    if (!json_is_object(fallback)) return json_null();
    json_t *used = json_obj_get(fallback, "used");
    if (!truthy(used)) return json_null();
    char *u = py_str(used);
    json_t *req_node = json_obj_get(fallback, "requested");
    char *req = py_str(req_node);
    size_t total = strlen(u) + strlen(req) + 256;
    char *msg = (char *)malloc(total);
    if (msg) snprintf(msg, total,
        "Working directory %s no longer exists or is not accessible; this command ran in %s instead, so relative paths resolved there. Check where you are before anything destructive and `cd` to the intended directory explicitly.",
        req ? req : "None", u ? u : "");
    json_t *out = json_string(msg ? msg : "");
    free(msg); free(u); free(req);
    return out;
}

json_t *w14_terminalto_ff2244_env_recreated_note(void) {
    return json_string(
        "The execution environment was recreated while running this command "
        "(the previous container/sandbox was gone). Persistent files were restored "
        "where the backend supports it, but background processes and any files "
        "outside persisted paths from earlier commands may be lost — verify state "
        "before relying on prior work.");
}
