/* t_port_micro_env.c — oracle harness for the micro pure helpers. */
#include "port_micro_env.h"

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

    /* strip_env_keys: always empty. */
    {
        char *out = tep_strip_env_keys();
        printf("{\"family\":\"strip\",\"out\":%s}\n", out ? out : "null");
        free(out);
    }

    /* env_description: display_name. */
    static const char *const dn[] = {"Daytona", "local", "", "A&B"};
    for (int i = 0; i < 4; i++) {
        char *out = tep_env_description(dn[i]);
        printf("{\"family\":\"desc\",\"case\":%d,\"name\":", i);
        jstr(dn[i]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free(out);
    }

    /* _allow_all_env_names: prefix. */
    static const char *const px[] = {"WHATSAPP", "TELEGRAM", "", "X"};
    for (int i = 0; i < 4; i++) {
        char *out = apm_allow_all_env_names(px[i]);
        printf("{\"family\":\"aaen\",\"case\":%d,\"prefix\":", i);
        jstr(px[i]);
        printf(",\"out\":%s}\n", out ? out : "null");
        free(out);
    }

    /* _normalized. */
    static const char *const nm[] = {"ProFile Name", "  mysite  ", "", "ALLCAPS", "under_score"};
    for (int i = 0; i < 5; i++) {
        char *out = ht_normalized(nm[i]);
        printf("{\"family\":\"norm\",\"case\":%d,\"name\":", i);
        jstr(nm[i]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free(out);
    }

    /* serves: (profiles, profile_name). */
    static const char *const sp[][2] = {
        {"[\"default\",\"coder\"]", "default"},
        {"[\"default\",\"coder\"]", "CODER"},
        {"[\"default\",\"coder\"]", "gone"},
        {"[\"default\",\"coder\"]", ""},
        {"[\"A B\"]", "a b"},
    };
    for (int i = 0; i < 5; i++) {
        int out = hgt_serves(sp[i][0], sp[i][1]);
        printf("{\"family\":\"serves\",\"case\":%d,\"profiles\":%s,\"profile\":", i, sp[i][0]);
        jstr(sp[i][1]);
        printf(",\"out\":%d}\n", out);
    }
    return 0;
}
