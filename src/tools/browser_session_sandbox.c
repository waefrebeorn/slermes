/*
 * browser_session_sandbox.c — C port of upstream Hermes tools/browser_session_sandbox.py.
 *
 * PoP: w17_browserses_dec4eb_shared_key @ tools/browser_session_sandbox.py:_shared_key
 * PoP: w17_browserses_dec4eb_read_devtools @ tools/browser_session_sandbox.py:_read_devtools
 * PoP: w17_browserses_dec4eb_live_devtools @ tools/browser_session_sandbox.py:_live_devtools
 * PoP: w17_browserses_dec4eb_constants @ tools/browser_session_sandbox.py:GONE/_GONE_RC/_NOT_OURS_RC/_CONFIRMED_TTL_S/_LAUNCH_TIMEOUT_S/_START_TIME_OF_P/_STOP_SCRIPT
 */
#include "noc_w17_browserses_dec4eb.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int GONE_RC = 3;
static const int NOT_OURS_RC = 4;
static const char *const GONE = "gone";
static const double CONFIRMED_TTL_S = 30.0;
static const double LAUNCH_TIMEOUT_S = 60.0;
static const char *const DEVTOOLS_PREFIX = "/devtools/browser/";

static const char START_TIME_OF_P[] =
    "s=$(cat /proc/\"$p\"/stat 2>/dev/null) && s=${s##*\")\"} && set -- $s && printf %s \"${20}\"";
static const char STOP_SCRIPT[] =
    "p=$1; t=$(" "s=$(cat /proc/\"$p\"/stat 2>/dev/null) && s=${s##*\")\"} && set -- $s && printf %s \"${20}\""
    "); [ -n \"$t\" ] && [ \"$t\" = \"$2\" ] || exit 4; "
    "ok=; while IFS= read -r -d \"\" a; do [ \"$a\" = \"$3\" ] && ok=1; done < /proc/\"$p\"/cmdline; "
    "[ -n \"$ok\" ] || exit 4; kill -TERM \"$p\"";

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}
static char *strip(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
static bool all_digits(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; p++) if (!isdigit((unsigned char)*p)) return false;
    return true;
}

json_t *w17_browserses_dec4eb_shared_key(const char *env_id, const char *udd) {
    size_t n = strlen("sandbox:") + strlen(env_id ? env_id : "") + 1
               + strlen(udd ? udd : "") + 1;
    char *out = (char *)malloc(n);
    if (out) snprintf(out, n, "sandbox:%s:%s", env_id ? env_id : "", udd ? udd : "");
    json_t *r = json_string(out ? out : "");
    free(out);
    return r;
}

/* Python: lines = stdout.decode("utf-8","replace").splitlines() when rc==0;
 * require len>=2, lines[0].strip().isdigit(), lines[1].startswith(prefix);
 * return (int(lines[0].strip()), lines[1].strip()). */
static json_t *parse_devtools(const char *text) {
    char *copy = dupstr(text ? text : "");
    char *lines[64];
    size_t nlines = 0;
    char *p = copy;
    while (*p && nlines < 64) {
        lines[nlines++] = p;
        char *nl = strchr(p, '\n');
        if (!nl) break;
        *nl = '\0';   /* strip() below also removes a trailing '\r' */
        p = nl + 1;
    }
    json_t *result = json_null();
    if (nlines >= 2) {
        char *l0 = strip(lines[0]);
        const char *l1 = lines[1];
        if (all_digits(l0) && strncmp(l1, DEVTOOLS_PREFIX, strlen(DEVTOOLS_PREFIX)) == 0) {
            char *path = strip(l1);
            json_t *arr = json_array();
            json_append(arr, json_number((double)strtoll(l0, NULL, 10)));
            json_append(arr, json_string(path ? path : ""));
            result = arr;
            free(path);
        }
        free(l0);
    }
    free(copy);
    return result;
}

json_t *w17_browserses_dec4eb_read_devtools(int returncode, const char *stdout_text) {
    if (returncode != 0) return json_null();
    return parse_devtools(stdout_text);
}

json_t *w17_browserses_dec4eb_live_devtools(int returncode, const char *stdout_text, bool raise_exec) {
    if (raise_exec) return json_null();
    if (returncode == GONE_RC) return json_string(GONE);
    if (returncode != 0) return json_null();
    return parse_devtools(stdout_text);
}

json_t *w17_browserses_dec4eb_constants(void) {
    json_t *out = json_object();
    json_set(out, "GONE", json_string(GONE));
    json_set(out, "_GONE_RC", json_number(GONE_RC));
    json_set(out, "_NOT_OURS_RC", json_number(NOT_OURS_RC));
    json_set(out, "_CONFIRMED_TTL_S", json_number(CONFIRMED_TTL_S));
    json_set(out, "_LAUNCH_TIMEOUT_S", json_number(LAUNCH_TIMEOUT_S));
    json_set(out, "_START_TIME_OF_P", json_string(START_TIME_OF_P));
    json_set(out, "_STOP_SCRIPT", json_string(STOP_SCRIPT));
    return out;
}
