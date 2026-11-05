/* t_port_access_policy.c — oracle harness for access_policy_mixin pure
 * helpers: _open_dm_opted_in and _entry_matches. */
#include "port_access_policy.h"

#include <stdio.h>
#include <stdlib.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* _open_dm_opted_in: (env0, env1). */
    static const char *const ov[][2] = {
        {"true", ""}, {"", "yes"}, {"", "0"}, {"on", ""}, {"  TRUE  ", ""},
        {"1", "no"}, {"", ""}, {"maybe", "yes"},
    };
    for (int i = 0; i < 8; i++) {
        int out = apm_open_dm_opted_in(ov[i][0], ov[i][1]);
        printf("{\"family\":\"optin\",\"case\":%d,\"e0\":", i);
        jstr(ov[i][0]);
        printf(",\"e1\":");
        jstr(ov[i][1]);
        printf(",\"out\":%d}\n", out);
    }

    /* _entry_matches: (entries, target). */
    static const char *const em[][2] = {
        {"[\"a\",\"b\",\"c\"]", "b"},
        {"[\"a\",\"b\"]", "z"},
        {"[]", "x"},
        {"[\"x\",\"y\"]", "x"},
        {"[\"a\"]", "aa"},
    };
    for (int i = 0; i < 5; i++) {
        int out = apm_entry_matches(em[i][0], em[i][1]);
        printf("{\"family\":\"match\",\"case\":%d,\"entries\":%s,\"target\":", i, em[i][0]);
        jstr(em[i][1]);
        printf(",\"out\":%d}\n", out);
    }
    return 0;
}
