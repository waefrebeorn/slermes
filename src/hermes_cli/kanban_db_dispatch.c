/*
 * kanban_db_dispatch.c -- faithful C11 port of the pure helpers in
 * hermes_cli/kanban_db_dispatch.py (wave17 gate w17_kanbandbd_e96b7a).
 *
 * PoP: w17_kanbandbd_e96b7a_wait_status_from_returncode @ hermes_cli/kanban_db_dispatch.py:_wait_status_from_returncode
 * PoP: w17_kanbandbd_e96b7a_exit_code_kind @ hermes_cli/kanban_db_dispatch.py:_exit_code_kind
 * PoP: w17_kanbandbd_e96b7a_error_fingerprint @ hermes_cli/kanban_db_dispatch.py:_error_fingerprint
 * PoP: w17_kanbandbd_e96b7a_worker_survived_termination @ hermes_cli/kanban_db_dispatch.py:_worker_survived_termination
 *
 * The sqlite / process / signal half stays in Python. No stubs here: each
 * exported symbol does the full deterministic work of its Python counterpart.
 */
#include "noc_w17_kanbandbd_e96b7a.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* PoP: w17_kanbandbd_e96b7a_wait_status_from_returncode @ hermes_cli/kanban_db_dispatch.py:_wait_status_from_returncode */
int w17_kanbandbd_e96b7a_wait_status_from_returncode(int returncode)
{
    return (returncode & 0xFF) << 8;
}

/* PoP: w17_kanbandbd_e96b7a_exit_code_kind @ hermes_cli/kanban_db_dispatch.py:_exit_code_kind */
const char *w17_kanbandbd_e96b7a_exit_code_kind(int code, int rate_limit_code,
                                                int terminal_provider_code, int *out_code)
{
    if (out_code) *out_code = code;
    if (code == 0) return "clean_exit";
    if (code == rate_limit_code) return "rate_limited";
    if (code == terminal_provider_code) return "terminal_provider";
    return "nonzero_exit";
}

/* Python re.sub() word boundaries: a word char is [A-Za-z0-9_] (ASCII for our
 * inputs). The two passes are ordered: pid rewrite on the truncated text, then
 * the timestamp rewrite on the result of that pass. */
static char *dupstr_n(const char *s, size_t n)
{
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* PoP: w17_kanbandbd_e96b7a_error_fingerprint @ hermes_cli/kanban_db_dispatch.py:_error_fingerprint */
char *w17_kanbandbd_e96b7a_error_fingerprint(const char *error_text)
{
    const char *src = error_text ? error_text : "";
    size_t n = strlen(src);
    if (n > 80) n = 80;                       /* error_text[:80] */

    /* pass 1: \bpid \d+\b -> "pid N" */
    size_t cap = n + 16;
    char *a = (char *)malloc(cap);
    if (!a) return NULL;
    size_t oa = 0;
    for (size_t i = 0; i < n;) {
        int boundary_before = (i == 0) || !(isalnum((unsigned char)src[i-1]) || src[i-1] == '_');
        if (boundary_before && i + 4 < n && memcmp(src + i, "pid ", 4) == 0 &&
            isdigit((unsigned char)src[i + 4])) {
            size_t j = i + 4;
            while (j < n && isdigit((unsigned char)src[j])) j++;
            int boundary_after = (j >= n) || !(isalnum((unsigned char)src[j]) || src[j] == '_');
            if (boundary_after) {
                if (oa + 6 > cap) { cap = (oa + 6) * 2; char *t = (char *)realloc(a, cap); if (!t) { free(a); return NULL; } a = t; }
                memcpy(a + oa, "pid N", 5); oa += 5;
                i = j; continue;
            }
        }
        if (oa + 2 > cap) { cap *= 2; char *t = (char *)realloc(a, cap); if (!t) { free(a); return NULL; } a = t; }
        a[oa++] = src[i++];
    }
    a[oa] = '\0';

    /* pass 2: \b\d{10,}\b -> "<TS>" */
    size_t cap2 = oa + 16;
    char *b = (char *)malloc(cap2);
    if (!b) { free(a); return NULL; }
    size_t ob = 0;
    for (size_t i = 0; i < oa;) {
        int boundary_before = (i == 0) || !(isalnum((unsigned char)a[i-1]) || a[i-1] == '_');
        if (boundary_before && isdigit((unsigned char)a[i])) {
            size_t j = i;
            while (j < oa && isdigit((unsigned char)a[j])) j++;
            int boundary_after = (j >= oa) || !(isalnum((unsigned char)a[j]) || a[j] == '_');
            size_t want = (j - i >= 10 && boundary_after) ? 4 : (j - i);
            if (ob + want + 1 > cap2) { cap2 = (ob + want + 1) * 2; char *t = (char *)realloc(b, cap2); if (!t) { free(a); free(b); return NULL; } b = t; }
            if (j - i >= 10 && boundary_after) memcpy(b + ob, "<TS>", 4);
            else memcpy(b + ob, a + i, j - i);
            ob += want;
            i = j; continue;
        }
        if (ob + 2 > cap2) { cap2 *= 2; char *t = (char *)realloc(b, cap2); if (!t) { free(a); free(b); return NULL; } b = t; }
        b[ob++] = a[i++];
    }
    b[ob] = '\0';
    free(a);

    /* .lower().strip() */
    for (size_t i = 0; i < ob; i++) b[i] = (char)tolower((unsigned char)b[i]);
    size_t start = 0, end = ob;
    while (start < end && isspace((unsigned char)b[start])) start++;
    while (end > start && isspace((unsigned char)b[end - 1])) end--;
    char *out = dupstr_n(b + start, end - start);
    b[ob] = '\0';
    free(b);
    return out;
}

static int json_truthy(json_t *v)
{
    if (!v || json_is_null(v)) return 0;
    if (json_is_bool(v)) return json_bool_value(v) ? 1 : 0;
    if (json_is_number(v)) return json_number_value(v) != 0.0;
    if (json_is_string(v)) { const char *s = json_string_value(v); return s && *s; }
    if (json_is_array(v) || json_is_object(v)) return json_len(v) != 0;
    return 0;
}

/* PoP: w17_kanbandbd_e96b7a_worker_survived_termination @ hermes_cli/kanban_db_dispatch.py:_worker_survived_termination */
int w17_kanbandbd_e96b7a_worker_survived_termination(json_t *termination)
{
    if (!json_is_object(termination)) return 0;
    int host_local   = json_truthy(json_obj_get(termination, "host_local"));
    int attempted    = json_truthy(json_obj_get(termination, "termination_attempted"));
    int refused      = json_truthy(json_obj_get(termination, "signal_refused"));
    int terminated   = json_truthy(json_obj_get(termination, "terminated"));
    return host_local && (attempted || refused) && !terminated;
}

