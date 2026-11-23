/*
 * systemd_stop_mark.c — C port of upstream Hermes gateway/systemd_stop_mark.py.
 *
 * PoP: w12_systemdsto_89d0b9_resolve_pid @ gateway/systemd_stop_mark.py:_resolve_target_pid
 * PoP: w12_systemdsto_89d0b9_main @ gateway/systemd_stop_mark.py:main
 *
 * `mainpid_value` is an explicit seam for os.environ["MAINPID"]: the process
 * environment is never mutated here (Windows CRT setenv("") unsets, Python
 * os.environ[""] keeps), so the fixture injects the resolved env value.
 */
#include "noc_w12_systemdsto_89d0b9.h"
#include "hermes_json.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Faithful model of Python int(str(raw).strip()): ASCII whitespace strip,
 * optional +/-, digits with Python's between-digits underscores, base 10. */
static int parse_py_int(const char *s, long long *out) {
    if (!s) return 0;
    while (*s && isspace((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    size_t span = (size_t)(e - s);
    if (span == 0 || span >= 128) return 0;
    char buf[160];
    size_t n = 0;
    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') { buf[n++] = s[i++]; }
    int digits = 0, prev_digit = 0;
    for (; i < span; i++) {
        char c = s[i];
        if (c >= '0' && c <= '9') { buf[n++] = c; digits++; prev_digit = 1; }
        else if (c == '_') {
            if (!prev_digit || i + 1 >= span || s[i + 1] < '0' || s[i + 1] > '9') return 0;
        } else return 0;
    }
    if (!digits) return 0;
    buf[n] = '\0';
    errno = 0;
    char *end = NULL;
    long long v = strtoll(buf, &end, 10);
    if (end != buf + n || errno == ERANGE) return 0;
    *out = v;
    return 1;
}

static const char *elem_text(const json_t *v, char *buf, size_t cap) {
    if (!v) return "";
    if (v->type == JSON_STRING) return v->str_val;
    if (v->type == JSON_NUMBER) { snprintf(buf, cap, "%.0f", v->num_val); return buf; }
    if (v->type == JSON_BOOL) return v->bool_val ? "True" : "False";
    return "";
}

json_t *w12_systemdsto_89d0b9_resolve_pid(const json_t *argv, const char *mainpid_value) {
    char numbuf[64];
    const char *raw = NULL;
    if (json_is_array(argv) && json_len(argv) > 1) {
        const char *t = elem_text(json_get(argv, 1), numbuf, sizeof(numbuf));
        if (t && t[0]) raw = t;
    }
    if (!raw) raw = mainpid_value ? mainpid_value : "";
    long long pid = 0;
    if (!parse_py_int(raw, &pid)) return json_null();
    if (pid <= 0) return json_null();
    return json_number((double)pid);
}

json_t *w12_systemdsto_89d0b9_main(const json_t *argv, const char *mainpid_value) {
    /* The live main() resolves the pid and calls write_planned_stop_marker(pid)
     * inside try/except, then unconditionally returns 0. */
    (void)w12_systemdsto_89d0b9_resolve_pid(argv, mainpid_value);
    return json_number(0.0);
}
