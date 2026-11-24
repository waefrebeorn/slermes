/*
 * plugins_cmd_install.c -- faithful C11 port of the deterministic
 * known-issue warning builder in hermes_cli/plugins_cmd_install.py
 * (wave18 gate w18_pluginscmd_a1d82a).
 *
 * PoP: w18_pluginscmd_a1d82a_known_issue_warnings @ hermes_cli/plugins_cmd_install.py:_known_issue_warnings
 *
 * Only the pure list-comprehension is ported; the installer core stays in
 * Python. The input is a JSON array of strings, the output a JSON array.
 */
#include "noc_w18_pluginscmd_a1d82a.h"

#include <stdlib.h>
#include <string.h>

json_t *w18_pluginscmd_a1d82a_known_issue_warnings(json_t *known_issues)
{
    json_t *out = json_array();
    if (!out) return NULL;
    size_t n = json_is_array(known_issues) ? json_len(known_issues) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *it = json_get(known_issues, i);
        const char *s = (it && json_is_string(it) && it->str_val) ? it->str_val : "";
        size_t sl = strlen(s);
        char *buf = (char *)malloc(13 + sl + 1);   /* "Known issue: " is 13 bytes */
        if (!buf) { json_free(out); return NULL; }
        memcpy(buf, "Known issue: ", 13);
        memcpy(buf + 13, s, sl);
        buf[13 + sl] = '\0';                        /* write NUL at the new length */
        json_append(out, json_string(buf));
        free(buf);
    }
    return out;
}
