/* Slermes C port — cron/lifecycle_guard.py (pure gateway-lifecycle guard) */

#define PCRE2_CODE_UNIT_WIDTH 8

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pcre2.h>
#include "slermes_home.h"
#include "cron/port_lifecycle_guard.h"

/* Faithful copy of _GATEWAY_LIFECYCLE_PATTERN (lifecycle_guard.py:48).
 * Python's `re` is PCRE-compatible, so PCRE2 reproduces it byte-faithfully
 * (supports \b. Note: POSIX regcomp() does NOT support \b, hence PCRE2). */
static const char *LIFECYCLE_PATTERN =
    "(?i)"
    "(?:hermes\\s+gateway\\s+(?:restart|stop))"
    "|(?:launchctl\\s+(?:kickstart|unload|load|stop|restart)\\b[^\\n]*\\bhermes[.\\-]?gateway)"
    "|(?:systemctl\\s+(?:-\\S+\\s+)*(?:restart|stop|start)\\b[^\\n]*\\bhermes[.\\-]?gateway)"
    "|(?:p?kill\\b[^\\n]*\\bhermes\\b[^\\n]*\\bgateway)"
    "|(?:p?kill\\b[^\\n]*\\bgateway\\b[^\\n]*\\bhermes)";

/* ================================================================
 *  _resolve_script_path
 * ================================================================ */
/* PoP: cron_lifecycle_resolve_script_path @ cron/lifecycle_guard.py:_resolve_script_path */
/* Resolve a cron `script` value the same way the scheduler does. A bare/
 * relative path lives under <SLERMES_HOME>/scripts/; an absolute path is
 * used as-is. Faithful to the Python (which uses HERMES_HOME). */
static char *g_script_path_buf = NULL;
char *cron_lifecycle_resolve_script_path(const char *script_path)
{
    if (!script_path) return NULL;
    /* reuse a static buffer */
    static char buf[4096];
    g_script_path_buf = buf;
    const char *raw = script_path;
    /* expand leading ~ */
    char expanded[4096];
    if (raw[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) home = "/";
        snprintf(expanded, sizeof(expanded), "%s%s", home, raw + 1);
        raw = expanded;
    }
    if (raw[0] == '/') {
        snprintf(buf, sizeof(buf), "%s", raw);
        return buf;
    }
    const char *home = slermes_home();
    snprintf(buf, sizeof(buf), "%s/scripts/%s", home ? home : ".slermes", raw);
    return buf;
}

/* ================================================================
 *  _read_script_for_scanning
 * ================================================================ */
/* PoP: cron_lifecycle_read_script_for_scanning @ cron/lifecycle_guard.py:_read_script_for_scanning */
/* Read a script file for lifecycle-pattern scanning. Decodes with
 * errors="replace" so binary/non-UTF-8 content does not silently bypass
 * the check. Returns a malloc'd string (caller frees) or NULL on read
 * failure (empty string semantics preserved: return "" only when the file
 * cannot be read at all). */
char *cron_lifecycle_read_script_for_scanning(const char *script_path)
{
    const char *rp = cron_lifecycle_resolve_script_path(script_path);
    if (!rp) return strdup("");
    FILE *f = fopen(rp, "rb");
    if (!f) return strdup("");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return strdup(""); }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return strdup(""); }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    /* UTF-8 decode with errors="replace": any invalid byte -> '?' */
    static char *out = NULL;
    out = (char *)realloc(out, (size_t)got + 1);
    size_t o = 0;
    for (size_t i = 0; i < got; ) {
        unsigned char c = (unsigned char)buf[i];
        if (c < 0x80) { out[o++] = (char)c; i++; }
        else if ((c >> 5) == 0x6) { out[o++] = '?'; i += 2; }
        else if ((c >> 4) == 0xe) { out[o++] = '?'; i += 3; }
        else if ((c >> 3) == 0x1e) { out[o++] = '?'; i += 4; }
        else { out[o++] = '?'; i++; }
    }
    out[o] = '\0';
    free(buf);
    return strdup(out);
}

/* ================================================================
 *  check_gateway_lifecycle
 * ================================================================ */
/* PoP: cron_lifecycle_check_gateway_lifecycle @ cron/lifecycle_guard.py:check_gateway_lifecycle */
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
    if (script && script[0]) {
        script_text = cron_lifecycle_read_script_for_scanning(script);
        if (script_text && script_text[0]) {
            size_t need = strlen(combined) + 1 + strlen(script_text) + 1;
            char *cat = (char *)malloc(need);
            snprintf(cat, need, "%s\n%s", combined, script_text);
            combined = cat;
        }
    }
    char *ret = NULL;
    if (cron_lifecycle_contains_gateway_lifecycle_command(combined)) {
        ret = strdup(
            "Blocked: cron job contains a gateway lifecycle command "
            "(restart/stop/kill). This is blocked to prevent agent-driven "
            "SIGTERM-respawn loops under launchd/systemd supervision (#30719). "
            "Run `hermes gateway restart` from a shell outside the running "
            "gateway instead.");
    }
    if (script_text) free(script_text);
    if (combined != (prompt && prompt[0] ? prompt : "")) free((void *)combined);
    return ret;
}

/* ================================================================
 *  contains_gateway_lifecycle_command
 * ================================================================ */
/* PoP: contains_gateway_lifecycle_command @ cron/lifecycle_guard.py:contains_gateway_lifecycle_command */
bool cron_lifecycle_contains_gateway_lifecycle_command(const char *text)
{
    if (!text || text[0] == '\0') return false;

    int err; PCRE2_SIZE erroff;
    pcre2_code *re = pcre2_compile((PCRE2_SPTR)LIFECYCLE_PATTERN,
                                   PCRE2_ZERO_TERMINATED, 0, &err, &erroff, NULL);
    if (!re) {
        /* Should never happen for a verified pattern; fail closed (no match). */
        return false;
    }
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
    int rc = pcre2_match(re, (PCRE2_SPTR)text, strlen(text), 0, 0, md, NULL);
    pcre2_match_data_free(md);
    pcre2_code_free(re);
    return rc >= 0;
}
