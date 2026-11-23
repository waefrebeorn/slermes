/*
 * update_stage.c — C home for upstream Hermes hermes_cli/update_stage.py.
 *
 * PoP: w9_updatestage_53df9d_ui_present_in_log @ hermes_cli/update_stage.py:_ui_present_in_log
 * PoP: w9_updatestage_53df9d_status_file @ hermes_cli/update_stage.py:status_file
 * PoP: w9_updatestage_53df9d_status_from_marker @ hermes_cli/update_stage.py:_status_from_marker
 *
 * The process environment, the marker file and the temp directory are injected
 * as fixture values; the pure decision layers are ported verbatim.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w9_updatestage_53df9d.h"

struct json_t *w9_updatestage_53df9d_ui_present_in_log(const char *log_text) {
    const char *p = log_text ? log_text : "";
    /* Collect the last 40 lines.  str.splitlines() drops \r in CRLF pairs. */
    const char *lines[40];
    size_t nline = 0;
    const char *cur = p;
    while (*cur) {
        const char *end = cur;
        while (*end && *end != '\n' && *end != '\r') end++;
        lines[nline % 40] = cur;
        size_t len = (size_t)(end - cur);
        nline++;
        if (*end == '\r' && end[1] == '\n') end++;
        if (*end) end++;
        cur = end;
        (void)len;
    }
    /* Build the joined tail in order (skip lines that fell out of the window). */
    char joined[32768];
    size_t j = 0;
    size_t start = nline > 40 ? nline - 40 : 0;
    for (size_t i = start; i < nline; i++) {
        const char *s = lines[i % 40];
        const char *e = s;
        while (*e && *e != '\n' && *e != '\r') e++;
        size_t len = (size_t)(e - s);
        if (j + len + 1 >= sizeof(joined)) break;
        if (i > start) joined[j++] = '\n';
        memcpy(joined + j, s, len);
        j += len;
    }
    joined[j] = '\0';
    int present = (strstr(joined, "app window") != NULL) ||
                  (strstr(joined, "status panel pid=") != NULL);
    return json_bool(present ? true : false);
}

struct json_t *w9_updatestage_53df9d_status_file(const char *exported,
                                                 const char *marker_result) {
    /* Python: os.environ.get(ENV, "").strip() -- ASCII whitespace strip. */
    const char *s = exported ? exported : "";
    while (*s && isspace((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    size_t len = (size_t)(e - s);
    if (len > 0) {
        char buf[32768];
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        memcpy(buf, s, len);
        buf[len] = '\0';
        return json_string(buf);
    }
    if (marker_result && *marker_result) return json_string(marker_result);
    return json_null();
}

struct json_t *w9_updatestage_53df9d_status_from_marker(const char *marker_text,
                                                       int candidate_exists) {
    const char *p = marker_text ? marker_text : "";
    /* utf-8-sig: strip a leading BOM (EF BB BF). */
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB &&
        (unsigned char)p[2] == 0xBF) p += 3;
    /* splitlines()[0] -- first line, with surrounding whitespace stripped by
     * Python's .strip() before int(). */
    char first[512];
    size_t n = 0;
    while (*p && *p != '\n' && *p != '\r' && n + 1 < sizeof(first)) {
        first[n++] = *p++;
    }
    first[n] = '\0';
    size_t b = 0, e2 = n;
    while (b < e2 && isspace((unsigned char)first[b])) b++;
    while (e2 > b && isspace((unsigned char)first[e2 - 1])) e2--;
    if (b == e2) return json_null();
    char num[512];
    memcpy(num, first + b, e2 - b);
    num[e2 - b] = '\0';
    char *end = NULL;
    long long pid = strtoll(num, &end, 10);
    if (end == num || (end && *end)) return json_null();
    if (pid <= 0) return json_null();
    if (!candidate_exists) return json_null();
    char out[256];
    snprintf(out, sizeof(out), "hermes-update-status.%lld", pid);
    return json_string(out);
}

void hermes_cli_update_stage_port_pending(void) { (void)0; }
