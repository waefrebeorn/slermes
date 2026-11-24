/*
 * process_registry_output_log.c - C11 port of upstream
 * tools/process_registry_output_log.py (gate w26_processreg_5c33c6).
 *
 * PoP: w26_processreg_5c33c6_output_dir @ tools/process_registry_output_log.py:output_dir
 * PoP: w26_processreg_5c33c6_adoptable_log @ tools/process_registry_output_log.py:adoptable_log
 * PoP: w26_processreg_5c33c6_exit_path @ tools/process_registry_output_log.py:_exit_path
 * PoP: w26_processreg_5c33c6_record_exit_command @ tools/process_registry_output_log.py:record_exit_command
 * PoP: w26_processreg_5c33c6_constants @ tools/process_registry_output_log.py:_TAIL_SECONDS/_FINAL_DRAIN_MAX_BYTES
 *
 * The file that holds a worker-spawned process's output is a pure function of
 * values: the profile home, the checkpointed path, the log name.  What is
 * ported here is exactly that -- the directory, the adoption test, the exit
 * sidecar name, the shell prefix that writes it, and the drain budgets.  The
 * descriptor, the directory creation, the retirement sweep and the tail loop
 * are seams and stay in the caller.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_processreg_5c33c6.h"

static char *pr_xstrdup(const char *s) {
    size_t n;
    char *out;
    if (!s) return NULL;
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* A JSON string's bytes, or NULL when the value is not a string. */
static const char *pr_str(const json_t *v) {
    if (!v || v->type != JSON_STRING || !v->str_val) return NULL;
    return v->str_val;
}

/* str(PurePosixPath(s)): collapse repeated slashes, drop "." components, drop a
 * trailing slash.  ".." is kept (Path does not resolve it).  "" -> ".". */
static char *pr_path_norm(const char *in) {
    char *out;
    size_t out_n = 0, i = 0;
    int absolute;

    if (!in || !*in) return pr_xstrdup(".");
    out = (char *)malloc(strlen(in) + 2);
    if (!out) return NULL;
    absolute = in[0] == '/';
    if (absolute) out[out_n++] = '/';
    while (in[i]) {
        size_t start = i, clen;
        while (in[i] && in[i] != '/') i++;
        clen = i - start;
        if (clen > 0 && !(clen == 1 && in[start] == '.')) {
            if (out_n > (absolute ? (size_t)1 : (size_t)0)) out[out_n++] = '/';
            memcpy(out + out_n, in + start, clen);
            out_n += clen;
        }
        if (in[i] == '/') i++;
    }
    if (out_n == 0) out[out_n++] = '.';
    out[out_n] = '\0';
    return out;
}

/* Index (within name) where the suffix starts; n when the name has no suffix
 * Python keeps (a leading dot is not a suffix separator). */
static size_t pr_suffix_start(const char *name, size_t n) {
    size_t i = n;
    while (i > 0 && name[i - 1] != '.') i--;
    if (i == 0) return n;        /* no dot at all */
    i -= 1;                      /* index of the last dot */
    return i <= 0 ? n : i;
}

/* Path.with_suffix(suffix) as text; NULL where Python's Path would raise. */
static char *pr_with_suffix(const char *path, const char *suffix) {
    char *n, *out;
    size_t len, i, dir_end = 0, name_len, ss, base, slen;

    n = pr_path_norm(path);
    if (!n) return NULL;
    len = strlen(n);
    for (i = 0; i < len; i++)
        if (n[i] == '/') dir_end = i + 1;
    name_len = len - dir_end;
    if (name_len == 0 || (name_len == 1 && n[dir_end] == '.')) { free(n); return NULL; }
    ss = pr_suffix_start(n + dir_end, name_len);
    base = dir_end + ss;
    slen = strlen(suffix);
    out = (char *)malloc(base + slen + 1);
    if (!out) { free(n); return NULL; }
    memcpy(out, n, base);
    memcpy(out + base, suffix, slen + 1);
    free(n);
    return out;
}

/* shlex.quote (Python 3.14): bare when every byte is in the safe set, else
 * single-quoted with the '\'' dance. */
static int pr_shlex_safe(char c) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) return 1;
    return c != '\0' && strchr("%+,-./:=@_", c) != NULL;
}

static char *pr_shlex_quote(const char *s) {
    size_t i, n, out_n = 0;
    char *out;
    int safe = 1;

    if (!s || !*s) return pr_xstrdup("''");
    for (i = 0; s[i]; i++)
        if (!pr_shlex_safe(s[i])) { safe = 0; break; }
    if (safe) return pr_xstrdup(s);
    n = strlen(s);
    out = (char *)malloc(n * 5 + 3);
    if (!out) return NULL;
    out[out_n++] = '\'';
    for (i = 0; i < n; i++) {
        if (s[i] == '\'') { memcpy(out + out_n, "'\"'\"'", 5); out_n += 5; }
        else out[out_n++] = s[i];
    }
    out[out_n++] = '\'';
    out[out_n] = '\0';
    return out;
}

json_t *w26_processreg_5c33c6_output_dir(const json_t *hermes_home) {
    const char *home = pr_str(hermes_home);
    char *norm, *out;
    size_t need;
    json_t *res;

    if (!home) return json_null();
    norm = pr_path_norm(home);
    if (!norm) return json_null();
    if (!strcmp(norm, ".")) {                    /* Path(".") / "logs" -> "logs" */
        free(norm);
        return json_string("logs/process-output");
    }
    need = strlen(norm) + strlen("/logs/process-output") + 2;
    out = (char *)malloc(need);
    if (!out) { free(norm); return json_null(); }
    if (!strcmp(norm, "/")) snprintf(out, need, "/logs/process-output");
    else snprintf(out, need, "%s/logs/process-output", norm);
    res = json_string(out);
    free(out);
    free(norm);
    return res;
}

json_t *w26_processreg_5c33c6_adoptable_log(const json_t *path, const json_t *output_dir) {
    const char *p = pr_str(path), *d = pr_str(output_dir);
    char *n, *dn, *parent = NULL;
    size_t len, i, dir_end = 0, name_len, ss;
    int ok;

    if (!p || !*p || !d) return json_string("");
    n = pr_path_norm(p);
    dn = pr_path_norm(d);
    if (!n || !dn) { free(n); free(dn); return json_string(""); }
    len = strlen(n);
    for (i = 0; i < len; i++)
        if (n[i] == '/') dir_end = i + 1;
    name_len = len - dir_end;
    ss = pr_suffix_start(n + dir_end, name_len);
    ok = (name_len - ss) == 4 && !strcmp(n + dir_end + ss, ".log");
    if (dir_end == 0) {
        parent = pr_xstrdup(".");
    } else {
        parent = (char *)malloc(dir_end);        /* dir_end-1 chars + NUL */
        if (parent) { memcpy(parent, n, dir_end - 1); parent[dir_end - 1] = '\0'; }
    }
    if (ok && parent && strcmp(parent, dn) != 0) ok = 0;
    free(parent);
    free(dn);
    if (!ok) { free(n); return json_string(""); }
    {
        json_t *res = json_string(n);
        free(n);
        return res;
    }
}

json_t *w26_processreg_5c33c6_exit_path(const json_t *log) {
    const char *s = pr_str(log);
    char *out;
    json_t *res;

    if (!s) return json_null();
    out = pr_with_suffix(s, ".exit");
    if (!out) return json_null();
    res = json_string(out);
    free(out);
    return res;
}

json_t *w26_processreg_5c33c6_record_exit_command(const json_t *log, const json_t *command) {
    const char *s = pr_str(log), *cmd = pr_str(command);
    char *exit_path, *quoted, *out;
    size_t need;
    json_t *res;

    if (!s || !cmd) return json_null();
    exit_path = pr_with_suffix(s, ".exit");
    if (!exit_path) return json_null();
    quoted = pr_shlex_quote(exit_path);
    free(exit_path);
    if (!quoted) return json_null();
    need = strlen(quoted) + strlen(cmd) + 128;
    out = (char *)malloc(need);
    if (!out) { free(quoted); return json_null(); }
    snprintf(out, need,
             "__hermes_exit_file=%s; trap 'printf \"%%s\\n\" \"$?\" > \"$__hermes_exit_file\"' EXIT\n%s",
             quoted, cmd);
    res = json_string(out);
    free(out);
    free(quoted);
    return res;
}

json_t *w26_processreg_5c33c6_constants(void) {
    json_t *o = json_object();
    json_set(o, "tail_seconds", json_number(0.2));
    json_set(o, "final_drain_max_bytes", json_number(1048576.0));
    return o;
}
