/*
 * approval_wmic.c -- faithful C11 port of upstream Hermes tools/approval_wmic.py.
 *
 * PoP: w7_approvalwmi_9d4dd1_has_wmic             @ tools/approval_wmic.py:WmicShadowDeleteMatcher.search
 * PoP: w7_approvalwmi_9d4dd1_carrier_script        @ tools/approval_wmic.py:WmicShadowDeleteMatcher._carrier_script
 * PoP: w7_approvalwmi_9d4dd1_wmic_delete_in_segment @ tools/approval_wmic.py:_WMIC_DELETE_IN_SEGMENT_RE
 *
 * WmicShadowDeleteMatcher.search() borrows a large shell word/segment parser
 * from tools.approval_detection; that parser is out of scope here. This port
 * covers the matcher's own logic: its case-insensitive "wmic" guard, the
 * carrier-switch/start-title/ssh-host/Command stripping of _carrier_script
 * (module-level regexes transcribed to linear scans), and the
 * `\sshadowcopy\b.*\sdelete\b` DOTALL/IGNORECASE segment pattern.
 */
#include "noc_w7_approvalwmi_9d4dd1.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int is_ws(char c) { return isspace((unsigned char)c) != 0; }
static int is_wordc(char c) {
    unsigned char u = (unsigned char)c;
    return isalnum(u) || c == '_';
}
static int ci_char(char c) { return tolower((unsigned char)c); }

static int ci_prefix(const char *s, const char *lit, size_t *len_out) {
    size_t i = 0;
    while (lit[i]) {
        if (!s[i] || ci_char(s[i]) != ci_char(lit[i])) return 0;
        i++;
    }
    if (len_out) *len_out = i;
    return 1;
}

static char *dup_range(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

/* Python `str.rstrip()` (whitespace). */
static char *rstrip_ws(const char *s) {
    size_t n = strlen(s);
    while (n && is_ws(s[n - 1])) n--;
    return dup_range(s, n);
}

/* _CARRIER_SWITCHES_RE.sub("", rest, count=1).  The pattern is a possessive
 * repetition of [\s*+ SWITCH]; an empty match always exists at position 0, so
 * the effect is to greedily consume the leading run of (whitespace + switch)
 * units.  Switches: /word[:value] or a known option+value or -/--flag. */
static char *strip_carrier_switches(const char *rest) {
    static const char *const OPTS[] = {
        "ExecutionPolicy", "ep", "ex", "WindowStyle", "w", "Version", "v",
        "WorkingDirectory", "wd", "ConfigurationName", "config",
        "SettingsFile", "settings", "OutputFormat", "of", "InputFormat",
        "inp", "if", "u", "p", "i", "o", "l", "J", "F", "a", NULL
    };
    size_t i = 0, n = strlen(rest);
    for (;;) {
        size_t j = i;
        while (j < n && is_ws(rest[j])) j++;
        size_t k = j;
        int matched = 0;

        if (k < n && rest[k] == '/') {
            size_t p = k + 1;
            if (p < n && is_wordc(rest[p])) {
                while (p < n && is_wordc(rest[p])) p++;
                if (p < n && rest[p] == ':') {
                    p++;
                    while (p < n && !is_ws(rest[p])) p++; /* \S*+ includes ;&| */
                }
                k = p;
                matched = 1;
            }
        } else if (k < n && rest[k] == '-') {
            size_t optlen = 0;
            for (int q = 0; OPTS[q]; q++) {
                size_t L = strlen(OPTS[q]);
                size_t got = 0;
                if (ci_prefix(rest + k + 1, OPTS[q], &got) && got == L
                        && k + 1 + L < n && is_ws(rest[k + 1 + L])) {
                    optlen = L;
                    break;
                }
            }
            if (optlen) {
                size_t v = k + 1 + optlen;
                while (v < n && is_ws(rest[v])) v++; /* \s++ */
                if (v < n && (rest[v] == '"' || rest[v] == '\'')) {
                    char qc = rest[v];
                    size_t p = v + 1;
                    while (p < n && rest[p] != qc && rest[p] != '\n') p++;
                    if (p < n) p++; /* include closing quote */
                    k = p;
                    matched = 1;
                } else {
                    size_t p = v;
                    while (p < n && !is_ws(rest[p]) && rest[p] != ';' && rest[p] != '&'
                           && rest[p] != '|') p++;
                    if (p > v) { k = p; matched = 1; }
                }
            }
            if (!matched) {
                /* --?\w*+ : '-' or '--' then zero or more word chars. */
                size_t p = k + 1;
                if (p < n && rest[p] == '-') p++;
                while (p < n && is_wordc(rest[p])) p++;
                k = p;
                matched = 1;
            }
        }

        if (!matched) break;
        i = k;
    }
    return dup_range(rest + i, n - i);
}

/* _START_TITLE_RE.sub("", rest, count=1): remove the leftmost leading-whitespace
 * + double-quoted-title run that is followed by whitespace and a ["\w\/]. */
static char *strip_start_title(const char *rest) {
    size_t n = strlen(rest);
    for (size_t p = 0; p < n; p++) {
        size_t i = p;
        while (i < n && is_ws(rest[i])) i++;
        if (i >= n || rest[i] != '"') continue;
        size_t j = i + 1;
        while (j < n && rest[j] != '"' && rest[j] != '\\' && rest[j] != '\n') j++;
        if (j >= n || rest[j] != '"') continue;
        size_t k = j + 1, m = k;
        while (m < n && is_ws(rest[m])) m++;
        if (m < n && (rest[m] == '"' || is_wordc(rest[m]) || rest[m] == '\\'
                      || rest[m] == '/')) {
            return dup_range(rest + k, n - k);
        }
    }
    return dup_range(rest, n);
}

/* re.sub(r"^[^\s;&|]+", "", rest): drop the leading ssh host word. */
static char *strip_ssh_host(const char *rest) {
    size_t n = strlen(rest), i = 0;
    while (i < n && !is_ws(rest[i]) && rest[i] != ';' && rest[i] != '&' && rest[i] != '|') i++;
    return dup_range(rest + i, n - i);
}

/* re.sub(r"^-(?:Command|c)\b\s*", "", rest, IGNORECASE). */
static char *strip_command_switch(const char *rest) {
    size_t n = strlen(rest);
    if (n >= 1 && rest[0] == '-') {
        size_t len = 0;
        if (ci_prefix(rest + 1, "Command", &len) && len == 7) {
            if (1 + len == n || !is_wordc(rest[1 + len])) {
                size_t v = 1 + len;
                while (v < n && is_ws(rest[v])) v++;
                return dup_range(rest + v, n - v);
            }
        }
        if (ci_prefix(rest + 1, "c", &len) && len == 1) {
            if (2 == n || !is_wordc(rest[2])) {
                size_t v = 2;
                while (v < n && is_ws(rest[v])) v++;
                return dup_range(rest + v, n - v);
            }
        }
    }
    return dup_range(rest, n);
}

/* rest.replace('\\"', '"') */
static char *unescape_quotes(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\\' && i + 1 < n && s[i + 1] == '"') { out[o++] = '"'; i++; }
        else out[o++] = s[i];
    }
    out[o] = '\0';
    return out;
}

int w7_approvalwmi_9d4dd1_has_wmic(const char *text) {
    if (!text) return 0;
    /* case-insensitive substring "wmic" */
    for (const char *p = text; *p; p++)
        if (ci_prefix(p, "wmic", NULL)) return 1;
    return 0;
}

json_t *w7_approvalwmi_9d4dd1_carrier_script(const char *name, const char *rest) {
    json_t *out = json_array();
    if (!name) name = "";
    if (!rest) rest = "";

    char *s = strip_carrier_switches(rest);
    if (!strcmp(name, "start")) {
        char *t = strip_start_title(s);
        free(s);
        s = strip_carrier_switches(t);
        free(t);
    }
    /* rest = rest.lstrip()  (unconditional in _carrier_script) */
    {
        size_t st = 0;
        while (s[st] && is_ws(s[st])) st++;
        if (st) {
            char *t = dup_range(s + st, strlen(s) - st);
            free(s);
            s = t;
        }
    }
    if (!strcmp(name, "ssh")) {
        char *h = strip_ssh_host(s);
        free(s);
        s = strip_carrier_switches(h);
        free(h);
        /* lstrip() after the second carrier-switch pass. */
        size_t st = 0;
        while (s[st] && is_ws(s[st])) st++;
        if (st) {
            char *t = dup_range(s + st, strlen(s) - st);
            free(s);
            s = t;
        }
    }
    char *c = strip_command_switch(s);
    free(s);
    s = c;

    char *r = rstrip_ws(s);
    free(s);
    s = unescape_quotes(r);
    free(r);
    if (!s) return out;

    if (s[0] != '"' && s[0] != '\'') {
        json_append(out, json_string(s));
    } else {
        json_append(out, json_string(s));
        size_t n = strlen(s);
        char q = s[0];
        char *v;
        if (n > 1 && s[n - 1] == q) v = dup_range(s + 1, n - 2);
        else v = dup_range(s + 1, n - 1);
        json_append(out, json_string(v));
        free(v);
    }
    free(s);
    return out;
}

int w7_approvalwmi_9d4dd1_wmic_delete_in_segment(const char *segment) {
    if (!segment) segment = "";
    /* Build " " + segment to mirror the prepended space in search(). */
    size_t n = strlen(segment);
    char *s = (char *)malloc(n + 2);
    if (!s) return 0;
    s[0] = ' ';
    memcpy(s + 1, segment, n + 1);

    for (size_t i = 0; s[i]; i++) {
        if (!is_ws(s[i])) continue;
        const char *w = s + i + 1;
        size_t L = 0;
        if (!ci_prefix(w, "shadowcopy", &L) || L != 10) continue;
        if (w[10] && is_wordc(w[10])) continue; /* \b after shadowcopy */
        for (size_t k = i + 1 + 10; s[k]; k++) {
            if (!is_ws(s[k])) continue;
            const char *d = s + k + 1;
            size_t DL = 0;
            if (ci_prefix(d, "delete", &DL) && DL == 6) {
                if (!d[6] || !is_wordc(d[6])) { free(s); return 1; } /* \b after delete */
            }
        }
    }
    free(s);
    return 0;
}
