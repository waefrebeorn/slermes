/*
 * port_cronjob_prompt_scan.c — Port of tools/cronjob_prompt_scan.py::
 * _first_pattern_error: scan a prompt text against one or more (regex, pid)
 * pattern sets; return the fixed block message naming the FIRST pid whose
 * case-insensitive re.search matches, else the empty string.
 *
 *   message = "Blocked: prompt matches threat pattern '<pid>'. Cron prompts "
 *             "must not contain injection or exfiltration payloads."
 */

#include "port_cronjob_prompt_scan.h"

#include "hermes_regex.h"

#include <stdlib.h>
#include <string.h>

#define PREAMBLE "Blocked: prompt matches threat pattern '"
#define TAIL     "'. Cron prompts must not contain injection or exfiltration payloads."

static char g_out[256 + 96];

const char *cps_first_pattern_error(const char *text, const char *patterns_json)
{
    if (!text || !patterns_json)
        return "";
    /* patterns_json encodes a JSON array of [pattern, pid] entries. We scan it
       with a tiny strstr partitioner: each entry is bounded by "[" ... "],".
       Simpler contract: entries separated by "\n" lines, each "PATTERN\tPID". */
    const char *line = patterns_json;
    while (line && *line) {
        const char *nl = strchr(line, '\n');
        size_t len = nl ? (size_t)(nl - line) : strlen(line);
        if (len == 0) { if (!nl) break; line = nl + 1; continue; }
        /* split on tab */
        char *buf = (char *)malloc(len + 1);
        if (!buf) break;
        memcpy(buf, line, len); buf[len] = '\0';
        char *tab = strchr(buf, '\t');
        if (tab) {
            *tab = '\0';
            const char *pat = buf;
            const char *pid = tab + 1;
            hregex_t *re = regex_compile(pat, 1); /* ICASE */
            if (re) {
                regex_match_t *m = regex_search(re, text);
                int hit = m && m->matched;
                regex_match_free(m);
                regex_free(re);
                if (hit) {
                    const char *pre = PREAMBLE;
                    const char *tail = TAIL;
                    size_t n = 0;
                    while (*pre && n < sizeof(g_out) - 1) g_out[n++] = *pre++;
                    const char *p = pid;
                    while (*p && n < sizeof(g_out) - 1) g_out[n++] = *p++;
                    while (*tail && n < sizeof(g_out) - 1) g_out[n++] = *tail++;
                    g_out[n] = '\0';
                    free(buf);
                    return g_out;
                }
            }
        }
        free(buf);
        if (!nl) break;
        line = nl + 1;
    }
    return "";
}
