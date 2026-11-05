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

    /* _is_dm_allowed: (policy, sender, allow, env0, env1); optin derived via
     * the already-verified apm_open_dm_opted_in. */
    static const char *const dm[][5] = {
        {"allowlist", "alice", "[\"alice\",\"bob\"]", "", ""},
        {"allowlist", "eve", "[\"alice\",\"bob\"]", "", ""},
        {"open", "alice", "[]", "true", ""},
        {"open", "alice", "[]", "", "0"},
        {"disabled", "alice", "[]", "yes", ""},
        {"", "alice", "[]", "", ""},
    };
    for (int i = 0; i < 6; i++) {
        int optin = apm_open_dm_opted_in(dm[i][3], dm[i][4]);
        int out = apm_is_dm_allowed(dm[i][0], dm[i][1], dm[i][2], optin);
        printf("{\"family\":\"dma\",\"case\":%d,\"policy\":", i);
        jstr(dm[i][0]);
        printf(",\"sender\":");
        jstr(dm[i][1]);
        printf(",\"allow\":%s,\"e0\":", dm[i][2]);
        jstr(dm[i][3]);
        printf(",\"e1\":");
        jstr(dm[i][4]);
        printf(",\"optin\":%d,\"out\":%d}\n", optin, out);
    }

    /* _is_dm_intake_allowed. */
    static const char *const di[][5] = {
        {"pairing", "alice", "[]", "", ""},
        {"pairing", "  alice  ", "[]", "", ""},
        {"pairing", "", "[]", "", ""},
        {"pairing", "   ", "[]", "", ""},
        {"open", "alice", "[]", "1", ""},
        {"open", "alice", "[]", "", "no"},
        {"allowlist", "alice", "[\"alice\"]", "", ""},
        {"allowlist", "bob", "[\"alice\"]", "", ""},
    };
    for (int i = 0; i < 8; i++) {
        int optin = apm_open_dm_opted_in(di[i][3], di[i][4]);
        int out = apm_is_dm_intake_allowed(di[i][0], di[i][1], di[i][2], optin);
        printf("{\"family\":\"dmi\",\"case\":%d,\"policy\":", i);
        jstr(di[i][0]);
        printf(",\"sender\":");
        jstr(di[i][1]);
        printf(",\"allow\":%s,\"e0\":", di[i][2]);
        jstr(di[i][3]);
        printf(",\"e1\":");
        jstr(di[i][4]);
        printf(",\"optin\":%d,\"out\":%d}\n", optin, out);
    }

    /* _is_group_allowed: (policy, chat, allow). */
    static const char *const gr[][3] = {
        {"allowlist", "g1", "[\"g1\",\"g2\"]"},
        {"allowlist", "g9", "[\"g1\",\"g2\"]"},
        {"open", "g1", "[]"},
        {"disabled", "g1", "[]"},
        {"pairing", "g1", "[]"},
        {"", "g1", "[]"},
    };
    for (int i = 0; i < 6; i++) {
        int out = apm_is_group_allowed(gr[i][0], gr[i][1], gr[i][2]);
        printf("{\"family\":\"grp\",\"case\":%d,\"policy\":", i);
        jstr(gr[i][0]);
        printf(",\"chat\":");
        jstr(gr[i][1]);
        printf(",\"allow\":%s,\"out\":%d}\n", gr[i][2], out);
    }
    return 0;
}
