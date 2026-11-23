/*
 * terminal_tool_guards.c — C port of upstream Hermes tools/terminal_tool_guards.py.
 * Gate: w14_terminalto_c154c3
 *
 * PoP: w14_terminalto_c154c3_validate_workdir @ tools/terminal_tool_guards.py:_validate_workdir
 * PoP: w14_terminalto_c154c3_safe_command_preview @ tools/terminal_tool_guards.py:_safe_command_preview
 * PoP: w14_terminalto_c154c3_blocked_json @ tools/terminal_tool_guards.py:_blocked_json
 * PoP: w14_terminalto_c154c3_looks_like_help_or_version_command @ tools/terminal_tool_guards.py:_looks_like_help_or_version_command
 * PoP: w14_terminalto_c154c3_strip_quotes @ tools/terminal_tool_guards.py:_strip_quotes
 * PoP: w14_terminalto_c154c3_strip_trailing_amp @ tools/terminal_tool_guards.py:_strip_trailing_amp
 * PoP: w14_terminalto_c154c3_has_flag @ tools/terminal_tool_guards.py:_has_flag
 * PoP: w14_terminalto_c154c3_is_data_command @ tools/terminal_tool_guards.py:_is_data_command
 * PoP: w14_terminalto_c154c3_returns_promptly @ tools/terminal_tool_guards.py:_returns_promptly
 * PoP: w14_terminalto_c154c3_command_word @ tools/terminal_tool_guards.py:_command_word
 *
 * Caveat: strip_inert_heredoc_bodies() (imported at module load in the live
 * module) is stubbed to identity in the oracle; fixtures avoid inert heredocs.
 * Unicode str.isalnum() is approximated by treating any byte >= 0x80 as alnum
 * (fixtures use ASCII plus CJK letters).
 */
#include "noc_w14_terminalto_c154c3.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static char *dupstr_n(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? (v->str_val ? v->str_val : "") : NULL;
}

static int is_space_c(unsigned char c) {
    return isspace((unsigned char)c) != 0;
}

static char *rstrip_ws(const char *s) {
    size_t n = s ? strlen(s) : 0;
    while (n > 0 && is_space_c((unsigned char)s[n - 1])) n--;
    return dupstr_n(s ? s : "", n);
}

/* Python repr() of a single character (ASCII / CJK-safe for our fixtures). */
static char *py_repr_char(unsigned char c) {
    char tmp[16];
    if (c == '\n') return dupstr("'\\n'");
    if (c == '\r') return dupstr("'\\r'");
    if (c == '\t') return dupstr("'\\t'");
    if (c < 32 || c == 127) { snprintf(tmp, sizeof(tmp), "'\\x%02x'", c); return dupstr(tmp); }
    if (c == '\'') return dupstr("\"'\"");
    if (c == '"') return dupstr("'\"'");
    snprintf(tmp, sizeof(tmp), "'%c'", c);
    return dupstr(tmp);
}

static int safe_workdir_char(unsigned char c) {
    if (c == 0 || c < 32 || c == 127) return 0;
    if (c >= 0x80) return 1;  /* Unicode alnum (CJK) */
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return 1;
    return strchr("/\\:_-.~ +@=,", c) != NULL;
}

json_t *w14_terminalto_c154c3_validate_workdir(const struct json_t *workdir) {
    const char *p = jstr(workdir);
    if (!p) p = "";
    for (size_t i = 0; p[i]; i++) {
        unsigned char c = (unsigned char)p[i];
        if (!safe_workdir_char(c)) {
            char *r = py_repr_char(c);
            size_t n = r ? strlen(r) : 0;
            char *msg = (char *)malloc(n + 128);
            if (msg) {
                snprintf(msg, n + 128,
                         "Blocked: workdir contains disallowed character %s. Use a simple filesystem path without shell metacharacters.",
                         r ? r : "");
            }
            free(r);
            json_t *out = json_string(msg ? msg : "");
            free(msg);
            return out;
        }
    }
    return json_null();
}

json_t *w14_terminalto_c154c3_safe_command_preview(const struct json_t *command, int limit) {
    if (!command || command->type == JSON_NULL) return json_string("<None>");
    const char *p = jstr(command);
    if (!p) p = "";
    size_t n = strlen(p);
    if (limit >= 0 && n > (size_t)limit) n = (size_t)limit;
    char *out = dupstr_n(p, n);
    json_t *j = json_string(out ? out : "");
    free(out);
    return j;
}

json_t *w14_terminalto_c154c3_blocked_json(const struct json_t *error, const struct json_t *status) {
    const char *e0 = jstr(error) ? jstr(error) : "";
    const char *s0 = jstr(status) ? jstr(status) : "";
    char *e = json_escape_string(e0);
    char *s = json_escape_string(s0);
    size_t total = strlen("{\"output\": \"\", \"exit_code\": 1, \"error\": , \"status\": }") +
                   (e ? strlen(e) : 0) + (s ? strlen(s) : 0);
    char *buf = (char *)malloc(total + 2);
    if (buf) snprintf(buf, total + 2,
                      "{\"output\": \"\", \"exit_code\": 1, \"error\": %s, \"status\": %s}",
                      e ? e : "\"\"", s ? s : "\"\"");
    json_t *out = json_string(buf ? buf : "{}");
    free(buf); free(e); free(s);
    return out;
}

static char *normalize_lower_ws(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *buf = (char *)malloc(n + 2);
    if (!buf) return dupstr("");
    size_t j = 0;
    int pending = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (is_space_c(c)) { if (j) pending = 1; continue; }
        if (pending && j) buf[j++] = ' ';
        pending = 0;
        buf[j++] = (char)tolower(c);
    }
    buf[j] = '\0';
    return buf;
}

json_t *w14_terminalto_c154c3_looks_like_help_or_version_command(const struct json_t *command) {
    char *n = normalize_lower_ws(jstr(command) ? jstr(command) : "");
    size_t len = strlen(n);
    int hit = strstr(n, " --help") != NULL ||
              (len >= 3 && !strcmp(n + len - 3, " -h")) ||
              strstr(n, " --version") != NULL ||
              (len >= 3 && !strcmp(n + len - 3, " -v"));
    free(n);
    return json_bool(hit);
}

/* Three quote-masking passes, in the live order. Inert heredoc bodies are
 * identity-stubbed (see header caveat). */
static char *mask_single(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return dupstr("");
    size_t j = 0;
    for (size_t i = 0; i < n;) {
        if (s[i] == '\'') {
            size_t k = i + 1;
            while (k < n && s[k] != '\'') k++;
            if (k < n) { out[j++] = '\''; out[j++] = '\''; i = k + 1; }
            else out[j++] = s[i++];
        } else out[j++] = s[i++];
    }
    out[j] = '\0';
    return out;
}

static char *mask_double(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return dupstr("");
    size_t j = 0;
    for (size_t i = 0; i < n;) {
        if (s[i] == '"') {
            size_t k = i + 1;
            int closed = 0;
            while (k < n) {
                if (s[k] == '\\') { if (k + 1 < n) { k += 2; continue; } else break; }
                if (s[k] == '"') { closed = 1; break; }
                k++;
            }
            if (closed) { out[j++] = '"'; out[j++] = '"'; i = k + 1; }
            else out[j++] = s[i++];
        } else out[j++] = s[i++];
    }
    out[j] = '\0';
    return out;
}

static char *mask_backtick(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return dupstr("");
    size_t j = 0;
    for (size_t i = 0; i < n;) {
        if (s[i] == '`') {
            size_t k = i + 1;
            while (k < n && s[k] != '`') k++;
            if (k < n) { out[j++] = '`'; out[j++] = '`'; i = k + 1; }
            else out[j++] = s[i++];
        } else out[j++] = s[i++];
    }
    out[j] = '\0';
    return out;
}

json_t *w14_terminalto_c154c3_strip_quotes(const struct json_t *command) {
    const char *p = jstr(command) ? jstr(command) : "";
    char *a = mask_single(p);
    char *b = mask_double(a);
    char *c = mask_backtick(b);
    json_t *out = json_string(c ? c : "");
    free(a); free(b); free(c);
    return out;
}

json_t *w14_terminalto_c154c3_strip_trailing_amp(const struct json_t *command) {
    char *r = rstrip_ws(jstr(command) ? jstr(command) : "");
    size_t n = strlen(r);
    if (n == 0) { free(r); return json_string(""); }
    char *body = dupstr_n(r, n - 1);  /* command.rstrip()[:-1] */
    free(r);
    size_t end = strlen(body);
    while (end > 0 && is_space_c((unsigned char)body[end - 1])) {
        size_t bs = 0, p = end - 1;
        while (p > 0 && body[p - 1] == '\\') { bs++; p--; }
        if (bs % 2) break;
        end--;
    }
    body[end] = '\0';  /* NUL at the shortened length */
    json_t *out = json_string(body);
    free(body);
    return out;
}

static const char *arr_str(const json_t *arr, size_t i) {
    if (!json_is_array(arr) || i >= json_len(arr)) return NULL;
    return jstr(json_get(arr, i));
}

static int arr_has(const json_t *arr, const char *s) {
    if (!json_is_array(arr)) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *v = arr_str(arr, i);
        if (v && !strcmp(v, s)) return 1;
    }
    return 0;
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

json_t *w14_terminalto_c154c3_has_flag(const struct json_t *args, const char *short_flags,
                                      const struct json_t *long_flags) {
    if (!json_is_array(args)) return json_bool(false);
    for (size_t i = 0; i < json_len(args); i++) {
        const char *a = arr_str(args, i);
        if (!a) continue;
        if (!strncmp(a, "--", 2)) {
            const char *eq = strchr(a, '=');
            size_t n = eq ? (size_t)(eq - a) : strlen(a);
            if (json_is_array(long_flags)) {
                for (size_t k = 0; k < json_len(long_flags); k++) {
                    const char *lg = arr_str(long_flags, k);
                    if (lg && strlen(lg) == n && !strncmp(a, lg, n)) return json_bool(true);
                }
            }
        } else if (a[0] == '-' && short_flags) {
            for (const char *q = short_flags; *q; q++) if (strchr(a, *q)) return json_bool(true);
        }
    }
    return json_bool(false);
}

static int duration_match(const char *s) {
    if (!s) return 0;
    size_t i = 0;
    while (s[i] >= '0' && s[i] <= '9') i++;
    if (i == 0) return 0;
    if (s[i] == '.') {
        i++;
        size_t d = 0;
        while (s[i] >= '0' && s[i] <= '9') { i++; d++; }
        if (!d) return 0;
    }
    if (s[i] && strchr("smhd", s[i])) i++;
    return s[i] == '\0';
}

static int arr_slice_eq(const json_t *arr, size_t start, const char *const *vals, size_t n) {
    for (size_t i = 0; i < n; i++) {
        const char *a = arr_str(arr, start + i);
        if (!a || strcmp(a, vals[i])) return 0;
    }
    return 1;
}

static int git_runs_command(const json_t *args) {
    size_t n = json_is_array(args) ? json_len(args) : 0, i = 0;
    const char *plain = "--no-pager -P -p --paginate --bare --no-replace-objects --literal-pathspecs";
    while (i < n) {
        const char *a = arr_str(args, i);
        if (!a || a[0] != '-') break;
        if (!strcmp(a, "-C")) { i += 2; }
        else if (word_in(plain, a)) { i++; }
        else return 1;
    }
    const char *sub = i < n ? arr_str(args, i) : NULL;
    if (!sub) return 0;
    if (!strcmp(sub, "bisect") || !strcmp(sub, "difftool") ||
        !strcmp(sub, "mergetool") || !strcmp(sub, "filter-branch")) return 1;
    if (!strcmp(sub, "submodule")) {
        for (size_t k = i + 1; k < n; k++) {
            const char *a = arr_str(args, k);
            if (a && !strcmp(a, "foreach")) return 1;
        }
    }
    if (!strcmp(sub, "rebase")) {
        const char *shorts = "x";
        json_t *la = json_array();
        json_append(la, json_string("--exec"));
        int hit = json_is_true(w14_terminalto_c154c3_has_flag(args, shorts, la));
        json_free(la);
        if (hit) return 1;
        for (size_t k = i + 1; k < n; k++) {
            const char *a = arr_str(args, k);
            if (a && !strncmp(a, "--exec", 6)) return 1;
        }
    }
    if (!strcmp(sub, "grep")) {
        for (size_t k = i + 1; k < n; k++) {
            const char *a = arr_str(args, k);
            if (a && (!strncmp(a, "-O", 2) || !strncmp(a, "--open-files-in-pager", 21))) return 1;
        }
    }
    return 0;
}

static int is_python_word(const char *w) {
    if (!strcmp(w, "py")) return 1;
    if (strncmp(w, "python", 6)) return 0;
    for (const char *p = w + 6; *p; p++)
        if (!((*p >= '0' && *p <= '9') || *p == '.')) return 0;
    return 1;
}

static int data_exception(const char *word, const json_t *args) {
    size_t n = json_is_array(args) ? json_len(args) : 0;
    (void)n;
    if (!strcmp(word, "find")) {
        const char *bad[] = {"-exec", "-execdir", "-ok", "-okdir"};
        for (size_t i = 0; i < 4; i++) if (arr_has(args, bad[i])) return 1;
        return 0;
    }
    if (!strcmp(word, "tail")) {
        if (w14_terminalto_c154c3_has_flag(args, "fF", NULL)) return 1;
        for (size_t i = 0; i < json_len(args); i++) {
            const char *a = arr_str(args, i);
            if (a && !strncmp(a, "--f", 3)) return 1;
        }
        return 0;
    }
    if (!strcmp(word, "git")) return git_runs_command(args);
    if (!strcmp(word, "fd")) {
        json_t *l = json_array();
        json_append(l, json_string("--exec"));
        json_append(l, json_string("--exec-batch"));
        int r = json_is_true(w14_terminalto_c154c3_has_flag(args, "xX", l));
        json_free(l);
        return r;
    }
    if (!strcmp(word, "rg")) {
        json_t *l = json_array();
        json_append(l, json_string("--pre"));
        json_append(l, json_string("--hostname-bin"));
        int r = json_is_true(w14_terminalto_c154c3_has_flag(args, "", l));
        json_free(l);
        return r;
    }
    if (!strcmp(word, "ag")) {
        json_t *l = json_array();
        json_append(l, json_string("--pager"));
        int r = json_is_true(w14_terminalto_c154c3_has_flag(args, "", l));
        json_free(l);
        return r;
    }
    if (!strcmp(word, "sort")) {
        json_t *l = json_array();
        json_append(l, json_string("--compress-program"));
        int r = json_is_true(w14_terminalto_c154c3_has_flag(args, "", l));
        json_free(l);
        return r;
    }
    if (!strcmp(word, "tar")) {
        if (arr_has(args, "-I")) return 1;
        json_t *l = json_array();
        json_append(l, json_string("--to-command"));
        json_append(l, json_string("--checkpoint-action"));
        json_append(l, json_string("--use-compress-program"));
        int r = json_is_true(w14_terminalto_c154c3_has_flag(args, "", l));
        json_free(l);
        return r;
    }
    return 0;
}

/* json_len()==0 for objects; these are arrays, so json_len() is their size. */
json_t *w14_terminalto_c154c3_is_data_command(const struct json_t *word, const struct json_t *args) {
    const char *w = jstr(word);
    if (!w) return json_bool(false);
    size_t n = json_is_array(args) ? json_len(args) : 0;
    if (!strcmp(w, "command")) {
        const char *v[] = {"-v"}; const char *V[] = {"-V"};
        return json_bool((n == 1 && arr_slice_eq(args, 0, v, 1)) ||
                         (n == 1 && arr_slice_eq(args, 0, V, 1)));
    }
    if ((!strcmp(w, "uv") && n && arr_str(args, 0) && !strcmp(arr_str(args, 0), "pip")) ||
        (is_python_word(strrchr(w, '/') ? strrchr(w, '/') + 1 : w) &&
         n >= 2 && arr_str(args, 0) && !strcmp(arr_str(args, 0), "-m") &&
         arr_str(args, 1) && !strcmp(arr_str(args, 1), "pip"))) {
        size_t pip = n;
        for (size_t i = 0; i < n; i++) {
            const char *a = arr_str(args, i);
            if (a && !strcmp(a, "pip")) { pip = i + 1; break; }
        }
        size_t keep = n - pip;
        json_t *tail = json_array();
        for (size_t i = 0; i < keep; i++) {
            const char *a = arr_str(args, pip + i);
            json_append(tail, json_string(a ? a : ""));
        }
        json_t *res = w14_terminalto_c154c3_is_data_command(json_string("pip"), tail);
        json_free(tail);
        return res;
    }
    const char *package_tools = "npm pnpm yarn bun pip pip3 uv poetry pdm pipx brew apt apt-get apk dnf conda cargo gem";
    if (!word_in(package_tools, w)) {
        int is_data = word_in(
            "echo printf cat ls ll dir type grep egrep fgrep rg ag ack head wc stat du df file which "
            "whereis whence rm rmdir mkdir cp mv ln touch chmod chown cd pushd test [ sed awk sort uniq "
            "cut tr jq yq tree basename dirname realpath readlink diff cmp tar zip unzip fd find tail tee "
            "locate pgrep ps lsof curl wget git export unset column nl base64 xxd od fold paste tac rev "
            "comm md5sum sha256sum shasum true false done fi esac", w);
        return json_bool(is_data && !data_exception(w, args));
    }
    const char *plain = "-q --quiet -y --yes -g --global -v --verbose --no-cache --no-cache-dir "
                        "--no-input --disable-pip-version-check";
    size_t i = 0;
    while (i < n) {
        const char *a = arr_str(args, i);
        if (a && word_in(plain, a)) i++;
        else break;
    }
    const char *sub = i < n ? arr_str(args, i) : NULL;
    if (!sub) sub = "";
    return json_bool(word_in(
        "install i add update up upgrade remove rm uninstall ls list view info show why outdated audit "
        "ci pkg search freeze download sync lock", sub));
}

json_t *w14_terminalto_c154c3_returns_promptly(const struct json_t *word, const struct json_t *args) {
    const char *w = jstr(word);
    if (!w) return json_bool(false);
    size_t n = json_is_array(args) ? json_len(args) : 0;
    if (!strcmp(w, "sleep")) {
        if (!n) return json_bool(false);
        for (size_t i = 0; i < n; i++) if (!duration_match(arr_str(args, i))) return json_bool(false);
        return json_bool(true);
    }
    if (!strcmp(w, "docker")) {
        size_t start = 0;
        if (n && arr_str(args, 0) && !strcmp(arr_str(args, 0), "compose")) start = 1;
        const char *value_flags = "-f --file -p --project-name --project-directory --profile --env-file";
        size_t i = start;
        while (i < n) {
            const char *a = arr_str(args, i);
            if (!a || a[0] != '-') break;
            i += word_in(value_flags, a) ? 2 : 1;
        }
        const char *sub = i < n ? arr_str(args, i) : "";
        if (!sub) sub = "";
        return json_bool(word_in("ps port config images version ls inspect", sub));
    }
    return json_bool(false);
}

json_t *w14_terminalto_c154c3_command_word(const struct json_t *tokens) {
    size_t n = json_is_array(tokens) ? json_len(tokens) : 0;
    size_t i = 0;
    const char *word = "";
    while (i < n) {
        const char *tok = arr_str(tokens, i);
        if (!tok) { i++; continue; }
        char *buf = dupstr(tok);
        size_t k = 0;
        while (buf[k] == '(' || buf[k] == '{') k++;
        for (size_t q = k; buf[q]; q++) buf[q] = (char)tolower((unsigned char)buf[q]);
        char *w = dupstr(buf + k);
        free(buf);
        const char *system_bins[] = {"/bin/", "/sbin/", "/usr/bin/", "/usr/sbin/",
                                     "/usr/local/bin/", "/opt/homebrew/bin/"};
        int sysbin = 0;
        for (size_t b = 0; b < 6; b++) if (!strncmp(w, system_bins[b], strlen(system_bins[b]))) sysbin = 1;
        if (sysbin && !strstr(w, "..")) {
            const char *slash = strrchr(w, '/');
            char *base = dupstr(slash ? slash + 1 : w);
            free(w); w = base;
        }
        int transparent = word_in("time then do else elif if while until ! env", w);
        int assignment = 0;
        if (tok[0] && ((tok[0] >= 'A' && tok[0] <= 'Z') || (tok[0] >= 'a' && tok[0] <= 'z') || tok[0] == '_')) {
            size_t p = 1;
            while (tok[p] && ((tok[p] >= 'A' && tok[p] <= 'Z') || (tok[p] >= 'a' && tok[p] <= 'z') ||
                              (tok[p] >= '0' && tok[p] <= '9') || tok[p] == '_')) p++;
            if (tok[p] == '=') assignment = 1;
        }
        if (!*w) { i++; free(w); continue; }
        if (!strcmp(w, "sudo")) {
            int bail = 0;
            i++;
            while (i < n) {
                const char *f = arr_str(tokens, i);
                if (!f || f[0] != '-') break;
                if (!strcmp(f, "-u") || !strcmp(f, "-g")) i += 2;
                else if (word_in("-E -H -n -k -S -b", f)) i++;
                else { bail = 1; break; }
            }
            free(w);
            if (bail) {
                /* Unknown sudo option: the live function returns (word, [])
                 * with word still "sudo" instead of continuing the walk. */
                json_t *out = json_array();
                json_append(out, json_string("sudo"));
                json_append(out, json_array());
                return out;
            }
            continue;  /* next token is the command sudo executes */
        }
        if (!strcmp(w, "timeout") && duration_match(i + 1 < n ? arr_str(tokens, i + 1) : "")) {
            i += 2; free(w); continue;
        }
        if (transparent || assignment) { i++; free(w); continue; }
        word = w;
        i++;
        break;
    }
    json_t *out = json_array();
    json_append(out, json_string(word));
    json_t *rest = json_array();
    for (size_t k = i; k < n; k++) {
        const char *a = arr_str(tokens, k);
        json_append(rest, json_string(a ? a : ""));
    }
    json_append(out, rest);
    return out;
}
