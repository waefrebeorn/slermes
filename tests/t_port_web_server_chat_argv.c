/*
 * t_port_web_server_chat_argv.c — oracle harness for chat PTY env assembly
 * + session descendant resolution. Ops:
 *   {"op":"profile_dir","name":"...","home":"..."}   (SLERMES_HOME sandbox)
 *   {"op":"resolve_sid","db":"...","sid":"..."}
 *   {"op":"descendant","db":"...","sid":"..."}
 *   {"op":"build_env","base":{...},"resume":..,"sidecar":..,"profile_dir":..,
 *    "asf":..,"gateway":..}
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "web_server_chat_argv.h"

static void pjs(const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else if (*p == '\n') fputs("\\n", stdout);
        else if (*p < 0x20) printf("\\u%04x", *p);
        else putchar(*p);
    }
}

static const char *gs(json_t *o, const char *k) {
    json_t *v = o ? json_object_get(o, k) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
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
    /* The oracle overrides HERMES_HOME from the fixture's "home" field
     * (default /nonexistent); mirror that so both sides resolve the same
     * home. @SBX@ is substituted to TMPH by the runner. */
    {
        const char *home = json_get_str(fx, "home", NULL);
        if (home && *home) {
            setenv("HERMES_HOME", home, 1);
            setenv("SLERMES_HOME", home, 1);
            setenv("HOME", home, 1);
        }
    }

    if (strcmp(op, "profile_dir") == 0) {
        int status = 0;
        char *detail = NULL;
        char *dir = ws_chat_resolve_profile_dir(json_get_str(fx, "name", ""),
                                                &status, &detail);
        printf("{\"dir\":");
        if (dir) { putchar('"'); pjs(dir); putchar('"'); free(dir); }
        else fputs("null", stdout);
        printf(",\"status\":%d,\"detail\":", status);
        if (detail) { putchar('"'); pjs(detail); putchar('"'); free(detail); }
        else fputs("null", stdout);
        printf("}\n");
    } else if (strcmp(op, "resolve_sid") == 0) {
        char *sid = ws_chat_resolve_session_id(json_get_str(fx, "db", ""),
                                               json_get_str(fx, "sid", ""));
        printf("{\"id\":");
        if (sid) { putchar('"'); pjs(sid); putchar('"'); free(sid); }
        else fputs("null", stdout);
        printf("}\n");
    } else if (strcmp(op, "descendant") == 0) {
        json_t *r = ws_chat_session_latest_descendant(
            json_get_str(fx, "db", ""), json_get_str(fx, "sid", ""));
        char *d = json_dumps(r, 0);
        printf("%s\n", d);
        free(d);
        json_free(r);
    } else if (strcmp(op, "build_env") == 0) {
        json_t *env = ws_chat_build_env(json_object_get(fx, "base"),
                                        gs(fx, "resume"), gs(fx, "sidecar"),
                                        gs(fx, "profile_dir"), gs(fx, "asf"),
                                        gs(fx, "gateway"));
        char *d = json_dumps(env, 0);
        printf("%s\n", d);
        free(d);
        json_free(env);
    } else {
        return 2;
    }
    json_free(fx);
    return 0;
}
