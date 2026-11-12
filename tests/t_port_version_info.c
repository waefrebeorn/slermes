/* t_port_version_info.c — oracle harness for hermes_cli/version_info.py. */
#include "port_version_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr_escape(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') {
            putchar('\\');
            putchar(ch);
        } else if (ch < 0x20) {
            printf("\\u%04x", ch);
        } else {
            putchar(ch);
        }
    }
    putchar('"');
}

static void emit_str(int cs, const char *fn, const char *in, const char *out)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":{\"str\":", cs, fn, in);
    jstr_escape(out ? out : "");
    printf("}}\n");
    fflush(stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    char b[256];

    /* _derived_version. dist_json is the JSON text for the distance field, so
     * has_distance=0 is emitted as null (Python None) and not as 0. */
    {
        struct { const char *base; const char *dist_json; long dist;
                 int has; int dirty; const char *sha; } cases[] = {
            {"1.2.3", "5", 5, 1, 0, "abc1234"},
            {"1.2.3", "5", 5, 1, 1, "abc1234"},
            {"1.2.3", "5", 5, 1, 0, NULL},
            {"1.2.3", "5", 5, 1, 1, NULL},
            {"1.2.3", "0", 0, 1, 0, "abc1234"},
            {"1.2.3", "0", 0, 1, 1, "abc1234"},
            {"1.2.3", "null", 0, 0, 0, NULL},
            {"1.2.3", "null", 0, 0, 1, NULL},
            {"unknown", "null", 0, 0, 0, "abc1234"},
            {"unknown", "null", 0, 0, 1, "abc1234"},
            {"2.0.0", "100000", 100000, 1, 1, "deadbee"},
            {"0.20.5", "1", 1, 1, 0, ""},
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            char in[256];
            if (cases[i].sha && *cases[i].sha)
                snprintf(in, sizeof(in),
                         "{\"base\":\"%s\",\"distance\":%s,"
                         "\"dirty\":%d,\"short_commit\":\"%s\"}",
                         cases[i].base, cases[i].dist_json, cases[i].dirty,
                         cases[i].sha);
            else
                snprintf(in, sizeof(in),
                         "{\"base\":\"%s\",\"distance\":%s,"
                         "\"dirty\":%d,\"short_commit\":null}",
                         cases[i].base, cases[i].dist_json, cases[i].dirty);
            version_derived(cases[i].base, cases[i].dist, cases[i].has,
                            cases[i].dirty, cases[i].sha, b, sizeof(b));
            emit_str((int)i + 1, "derived", in, b);
        }
    }

    /* _parse_nonnegative — a number, or null when it does not parse */
    {
        static const char *vals[] = {
            "5", "0", "-1", "", "  42  ", "abc", "1.5", "+7", NULL,
            "0x10", "  ", "007",
        };
        for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
            char in[192];
            if (vals[i])
                snprintf(in, sizeof(in), "{\"value\":\"%s\"}", vals[i]);
            else
                snprintf(in, sizeof(in), "{\"value\":null}");
            int ok = 0;
            long v = version_parse_nonnegative(vals[i], &ok);
            char out[64];
            snprintf(out, sizeof(out), "%s%ld",
                     ok ? "" : "null", ok ? v : 0L);
            /* rewrite the emitter: number or null, not a string */
            printf("{\"case\":%d,\"fn\":\"parse_nonnegative\",\"in\":%s,"
                   "\"out\":%s}\n",
                   (int)i + 14, in, out);
            fflush(stdout);
        }
    }

    /* display_version */
    {
        struct { const char *base; const char *derived; long dist; int has; }
        cases[] = {
            {"1.2.3", "1.2.3+5.gabc1234", 5, 1},
            {"unknown", "git.abc1234", 0, 0},
            {"unknown", "git.abc1234.dirty", 0, 0},
            {"0.20.5", "0.20.5", 0, 1},
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            char in[256];
            snprintf(in, sizeof(in),
                     "{\"base\":\"%s\",\"derived\":\"%s\",\"distance\":%ld,"
                     "\"has_distance\":%d}",
                     cases[i].base, cases[i].derived, cases[i].dist,
                     cases[i].has);
            version_display(cases[i].base, cases[i].derived, cases[i].dist,
                            cases[i].has, b, sizeof(b));
            emit_str((int)i + 27, "display", in, b);
        }
    }

    /* hermes_user_agent */
    {
        struct { const char *product; const char *base; } cases[] = {
            {"hermes-cli", "0.20.5"},
            {"hermes-gateway", "unknown"},
            {"", "1.2.3"},
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            char in[192];
            snprintf(in, sizeof(in),
                     "{\"product\":\"%s\",\"base_version\":\"%s\"}",
                     cases[i].product, cases[i].base);
            version_user_agent(cases[i].product, cases[i].base, b, sizeof(b));
            emit_str((int)i + 31, "user_agent", in, b);
        }
    }
    return 0;
}
