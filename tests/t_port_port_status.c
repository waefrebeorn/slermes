/* t_port_port_status.c -- harness for the gateway/status.py port. Emits JSONL. */
#include "port_status.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else putchar(*p);
        }
    }
    putchar('"');
}

static const char *const CMDS[] = {
    "", " ", "HERMES_HOME=/root/.hermes",
    "HERMES_HOME=/root/.hermes ", " HERMES_HOME=/x", "a HERMES_HOME=/x b",
    "FOO=hermes_home=/x", "hermes_home=/x", "HERMES_HOME=",
    "HERMES_HOME=\"/root/.hermes\"", "HERMES_HOME='/root/.hermes'",
    "HERMES_HOME=\"/root/.hermes\" HERMES_HOME=/y",
    "x HERMES_HOME=/root/.hermes/ y", "HERMES_HOME=/root/.hermes//",
    "env HERMES_HOME=C:/Users/John Doe/.hermes",
    "env HERMES_HOME=\"C:/Users/John Doe/.hermes\"",
    "HERMES_HOME=/a HERMES_HOME=/b HERMES_HOME=/c",
    "HERMES_HOME = /x", "hermes_home=/x HERMES_HOME=/y",
};

static const char *const HOMES[] = {
    "/root/.hermes", "/root/.hermes/", "C:/Users/John Doe/.hermes",
    "C:/Users/John Doe/.hermes/", "/root/profiles/ops", "/x", "", "/",
};

static const char *const PATHS[] = {
    "", "/", "/root/.hermes", "/root/profiles/ops", "/root/profiles/ops/",
    "/root/profiles/ops/nested", "/root/other/ops", "profiles/ops",
    "/root/profiles/", "/root/profiles/default", "relative/profiles/x",
};

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    for (size_t i = 0; i < sizeof(CMDS) / sizeof(CMDS[0]); i++) {
        char *vals[16];
        int n = st_hermes_home_assignments(CMDS[i], vals, 16);
        printf("{\"fn\":\"assign\",\"arg\":");
        jstr(CMDS[i]);
        printf(",\"out\":[");
        for (int k = 0; k < n; k++) {
            if (k) putchar(',');
            jstr(vals[k]);
            free(vals[k]);
        }
        fputs("]}\n", stdout);
    }

    for (size_t i = 0; i < sizeof(CMDS) / sizeof(CMDS[0]); i++) {
        for (size_t h = 0; h < sizeof(HOMES) / sizeof(HOMES[0]); h++) {
            printf("{\"fn\":\"names\",\"cmd\":");
            jstr(CMDS[i]);
            printf(",\"home\":");
            jstr(HOMES[h]);
            printf(",\"out\":%d}\n",
                   st_command_line_names_home(CMDS[i], HOMES[h]));
        }
    }

    for (size_t i = 0; i < sizeof(PATHS) / sizeof(PATHS[0]); i++) {
        char *name = st_profile_name_for_home(PATHS[i]);
        printf("{\"fn\":\"profile\",\"arg\":");
        jstr(PATHS[i]);
        printf(",\"out\":");
        if (name) { jstr(name); free(name); } else fputs("null", stdout);
        fputs("}\n", stdout);
    }
    return 0;
}
