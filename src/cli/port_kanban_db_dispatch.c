/*
 * port_kanban_db_dispatch.c
 *
 * Pure helpers ported from hermes_cli/kanban_db_dispatch.py (82 defs, previously
 * 0 ported).
 *
 *   kbd_wait_status_from_returncode <- _wait_status_from_returncode
 *   kbd_exit_code_kind             <- _exit_code_kind
 *   kbd_error_fingerprint          <- _error_fingerprint
 *   kbd_worker_survived_termination<- _worker_survived_termination
 *
 * The sqlite / process / signal half stays in Python.
 */

#include "port_kanban_db_dispatch.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ===========================================================================
 *  _wait_status_from_returncode  (kanban_db_dispatch.py:_wait_status_from_returncode)
 *
 *      return (int(returncode) & 0xFF) << 8
 *
 *  Encodes a Popen.returncode in the wait-status layout the registry stores:
 *  the low byte is the signal/exit number and the upper byte is zero for a
 *  normal exit. `& 0xFF` discards the platform's high bits first, so a negative
 *  returncode (-15, i.e. killed by SIGTERM) becomes 240, not 0xFFFFFFxx.
 * =========================================================================== */

/* PoP: _wait_status_from_returncode @ hermes_cli/kanban_db_dispatch.py:_wait_status_from_returncode */
int kbd_wait_status_from_returncode(int returncode)
{
    return (returncode & 0xFF) << 8;
}

/* ===========================================================================
 *  _exit_code_kind  (kanban_db_dispatch.py:_exit_code_kind)
 *
 *      if code == 0: return ("clean_exit", 0)
 *      if code == KANBAN_RATE_LIMIT_EXIT_CODE: return ("rate_limited", code)
 *      if code == KANBAN_TERMINAL_PROVIDER_EXIT_CODE: return ("terminal_provider", code)
 *      return ("nonzero_exit", code)
 *
 *  The two special codes come from the kanban package; they are parameters here
 *  so the C side does not depend on that module's import.
 *  `out_kind` receives 0=clean_exit, 1=rate_limited, 2=terminal_provider,
 *  3=nonzero_exit; `out_code` receives the code as given.
 * =========================================================================== */

/* PoP: _exit_code_kind @ hermes_cli/kanban_db_dispatch.py:_exit_code_kind */
int kbd_exit_code_kind(int code, int rate_limit_code, int terminal_provider_code,
                       int *out_code)
{
    if (out_code) *out_code = code;
    if (code == 0) return 0;                                     /* clean_exit */
    if (code == rate_limit_code) return 1;                       /* rate_limited */
    if (code == terminal_provider_code) return 2;                /* terminal_provider */
    return 3;                                                    /* nonzero_exit */
}

/* ===========================================================================
 *  _error_fingerprint  (kanban_db_dispatch.py:_error_fingerprint)
 *
 *      fp = re.sub(r'\bpid \d+\b', 'pid N', error_text[:80])
 *      fp = re.sub(r'\b\d{10,}\b', '<TS>', fp)
 *      return fp.lower().strip()
 *
 *  Three steps, in order, and the ORDER matters: the pid rewrite runs on the
 *  TRUNCATED text, then the timestamp rewrite runs on the RESULT. A 10+ digit
 *  number becomes "<TS>"; a shorter one is left alone. Caller frees.
 * =========================================================================== */

/* PoP: _error_fingerprint @ hermes_cli/kanban_db_dispatch.py:_error_fingerprint */
char *kbd_error_fingerprint(const char *error_text)
{
    const char *src = error_text ? error_text : "";
    size_t n = strlen(src);
    if (n > 80) n = 80;                       /* error_text[:80] */
    char *fp = (char *)malloc(n + 1);
    if (!fp) return NULL;
    memcpy(fp, src, n);
    fp[n] = '\0';

    /* \bpid \d+\b -> "pid N" */
    size_t cap = n + 8;
    char *out = (char *)malloc(cap);
    if (!out) { free(fp); return NULL; }
    size_t o = 0;
    for (size_t i = 0; i < n;) {
        if ((i == 0 || !(isalnum((unsigned char)fp[i-1]) || fp[i-1] == '_')) &&
            strncmp(fp + i, "pid ", 4) == 0 &&
            (i + 4 < n) && isdigit((unsigned char)fp[i + 4])) {
            size_t j = i + 4;
            while (j < n && isdigit((unsigned char)fp[j])) j++;
            if (j >= n || !(isalnum((unsigned char)fp[j]) || fp[j] == '_')) {
                if (o + 5 >= cap) { cap = (o + 5) * 2; out = (char *)realloc(out, cap); if (!out) { free(fp); return NULL; } }
                memcpy(out + o, "pid N", 5);
                o += 5;
                i = j;
                continue;
            }
        }
        if (o + 1 >= cap) { cap *= 2; out = (char *)realloc(out, cap); if (!out) { free(fp); return NULL; } }
        out[o++] = fp[i++];
    }
    out[o] = '\0';
    free(fp);

    /* \b\d{10,}\b -> "<TS>" */
    cap = o + 8;
    char *out2 = (char *)malloc(cap);
    if (!out2) { free(out); return NULL; }
    size_t o2 = 0;
    for (size_t i = 0; i < o;) {
        if ((i == 0 || !(isalnum((unsigned char)out[i-1]) || out[i-1] == '_')) &&
            isdigit((unsigned char)out[i])) {
            size_t j = i;
            while (j < o && isdigit((unsigned char)out[j])) j++;
            if (j - i >= 10 &&
                (j >= o || !(isalnum((unsigned char)out[j]) || out[j] == '_'))) {
                if (o2 + 5 >= cap) { cap = (o2 + 5) * 2; out2 = (char *)realloc(out2, cap); if (!out2) { free(out); return NULL; } }
                memcpy(out2 + o2, "<TS>", 4);
                o2 += 4;
                i = j;
                continue;
            }
            if (o2 + (j - i) >= cap) { cap = (o2 + (j - i)) * 2; out2 = (char *)realloc(out2, cap); if (!out2) { free(out); return NULL; } }
            memcpy(out2 + o2, out + i, j - i);
            o2 += j - i;
            i = j;
            continue;
        }
        if (o2 + 1 >= cap) { cap *= 2; out2 = (char *)realloc(out2, cap); if (!out2) { free(out); return NULL; } }
        out2[o2++] = out[i++];
    }
    out2[o2] = '\0';
    free(out);

    /* .lower().strip() */
    for (size_t i = 0; i < o2; i++) out2[i] = (char)tolower((unsigned char)out2[i]);
    size_t start = 0, end = o2;
    while (start < end && isspace((unsigned char)out2[start])) start++;
    while (end > start && isspace((unsigned char)out2[end-1])) end--;
    memmove(out2, out2 + start, end - start);
    out2[end - start] = '\0';
    return out2;
}

/* ===========================================================================
 *  _worker_survived_termination  (kanban_db_dispatch.py:_worker_survived_termination)
 *
 *      return bool(
 *          termination.get("host_local")
 *          and (termination.get("termination_attempted") or termination.get("signal_refused"))
 *          and not termination.get("terminated"))
 *
 *  All three conditions are required. A non-local lock or a no-op attempt (no
 *  os.kill) falls through to the normal release path, because we cannot manage
 *  that worker anyway. `termination` is a flat [key, value, ...] array; values
 *  are "1"/"0" for the boolean fields.
 * =========================================================================== */

/* PoP: _worker_survived_termination @ hermes_cli/kanban_db_dispatch.py:_worker_survived_termination */
int kbd_worker_survived_termination(const char *const *termination)
{
    int host_local = 0, attempted = 0, refused = 0, terminated = 0;
    for (int i = 0; termination && termination[i]; i += 2) {
        const char *v = termination[i + 1];
        int truthy = (v && (v[0] == '1' || (v[0] && v[1] == '\0' && v[0] != '0')));
        if (strcmp(termination[i], "host_local") == 0) host_local = truthy;
        else if (strcmp(termination[i], "termination_attempted") == 0) attempted = truthy;
        else if (strcmp(termination[i], "signal_refused") == 0) refused = truthy;
        else if (strcmp(termination[i], "terminated") == 0) terminated = truthy;
    }
    return host_local && (attempted || refused) && !terminated;
}
