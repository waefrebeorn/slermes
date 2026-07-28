/*
 * t_port_web_server_cron_dash.c — oracle harness for the dashboard cron
 * adapter layer. Ops:
 *   {"op":"script","value":...,"home":"..."}
 *   {"op":"validate_job","job":{...}}
 *   {"op":"normalize","updates":{...},"home":"..."}
 *   {"op":"profile_home","profile":...}      (SLERMES_HOME sandbox)
 *   {"op":"default_profile"}
 *   {"op":"annotate","job":{...},"profile":"...","home":"..."}
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "web_server_cron_dash.h"

static void pjs(const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else if (*p == '\n') fputs("\\n", stdout);
        else if (*p < 0x20) printf("\\u%04x", *p);
        else putchar(*p);
    }
}

static void emit_str_or_null(char *s) {
    if (s) { putchar('"'); pjs(s); putchar('"'); free(s); }
    else fputs("null", stdout);
}

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 2;
    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    json_t *fx = json_parse(buf, NULL);
    if (!fx) return 2;
    const char *op = json_get_str(fx, "op", "");
    int status = 0;
    char *detail = NULL;

    if (strcmp(op, "script") == 0) {
        json_t *v = json_object_get(fx, "value");
        const char *val = (v && v->type == JSON_STRING) ? v->str_val : NULL;
        char *rel = ws_cron_normalize_script(val, json_get_str(fx, "home", ""),
                                             &status, &detail);
        printf("{\"rel\":");
        emit_str_or_null(rel);
        printf(",\"status\":%d,\"detail\":", status);
        emit_str_or_null(detail);
        printf("}\n");
    } else if (strcmp(op, "validate_job") == 0) {
        bool ok = ws_cron_validate_effective_job(json_object_get(fx, "job"),
                                                 &status, &detail);
        printf("{\"ok\":%s,\"status\":%d,\"detail\":", ok ? "true" : "false",
               status);
        emit_str_or_null(detail);
        printf("}\n");
    } else if (strcmp(op, "normalize") == 0) {
        json_t *norm = ws_cron_normalize_updates(
            json_object_get(fx, "updates"), json_get_str(fx, "home", ""),
            &status, &detail);
        printf("{\"norm\":");
        if (norm) { char *d = json_dumps(norm, 0); fputs(d, stdout); free(d); json_free(norm); }
        else fputs("null", stdout);
        printf(",\"status\":%d,\"detail\":", status);
        emit_str_or_null(detail);
        printf("}\n");
    } else if (strcmp(op, "profile_home") == 0) {
        json_t *p = json_object_get(fx, "profile");
        const char *prof = (p && p->type == JSON_STRING) ? p->str_val : NULL;
        char *name = NULL, *home = NULL;
        bool ok = ws_cron_profile_home(prof, &name, &home, &status, &detail);
        printf("{\"ok\":%s,\"name\":", ok ? "true" : "false");
        emit_str_or_null(name);
        printf(",\"home\":");
        emit_str_or_null(home);
        printf(",\"status\":%d,\"detail\":", status);
        emit_str_or_null(detail);
        printf("}\n");
    } else if (strcmp(op, "default_profile") == 0) {
        char *p = ws_cron_default_profile();
        printf("{\"profile\":");
        emit_str_or_null(p);
        printf("}\n");
    } else if (strcmp(op, "annotate") == 0) {
        json_t *r = ws_cron_annotate_job(json_object_get(fx, "job"),
                                         json_get_str(fx, "profile", ""),
                                         json_get_str(fx, "home", ""));
        char *d = json_dumps(r, 0);
        printf("%s\n", d);
        free(d);
        json_free(r);
    } else {
        return 2;
    }
    json_free(fx);
    return 0;
}
