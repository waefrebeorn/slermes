/* Slermes C port — gateway/cgroup_cleanup.py:_own_cgroup_path parser rule */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <regex.h>

/* PoP: gateway_cgroup_cleanup_own_cgroup_path @ gateway/cgroup_cleanup.py:_own_cgroup_path */
int gateway_cgroup_cleanup_own_cgroup_path(const char *buf, char *out, size_t outsz)
{
    out[0] = '\0';
    if (!buf || !*buf) return 0;
    regex_t re;
    if (regcomp(&re, "^0::(.+)$", REG_EXTENDED | REG_NEWLINE) != 0) return 0;
    regmatch_t m[2];
    const char *p = buf;
    int rc;
    while ((rc = regexec(&re, p, 1, m, 0)) == 0) {
        const char *line = p + m[0].rm_so;
        const char *colon = strstr(line, "0::");
        if (colon) {
            const char *val = colon + 3;
            const char *nl = strpbrk(val, "\n\r");
            size_t len = nl ? (size_t)(nl - val) : strlen(val);
            while (len > 0 && (val[len-1] == ' ' || val[len-1] == '\t')) len--;
            if (len > 0 && len < outsz) { memcpy(out, val, len); out[len] = '\0'; }
            regfree(&re);
            return 1;
        }
        p += m[0].rm_eo;
        if (*p == '\0') break;
    }
    regfree(&re);
    return 0;
}
