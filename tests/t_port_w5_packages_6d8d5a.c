/* t_port_w5_packages_6d8d5a.c — harness for pm/packages.py port.
 * Reads {"func":<name>, ...args...}, emits
 * {"func":<name>,"out":<value>, ...echoed input keys...}.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_packages_6d8d5a.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static void emit_str(const char *r) {
    json_t *j = json_string(r ? r : "");
    emit_key("out", j);
    json_free(j);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *ver = json_get_str(in, "version", "");
        const char *tgt = json_get_str(in, "target", "");
        json_t *out = NULL;
        int is_str = 0;
        char *r = NULL;
        if (!strcmp(func, "w5_packages_6d8d5a_dmgbuild_fetch_url")) {
            r = w5_packages_6d8d5a_dmgbuild_fetch_url(ver, tgt); is_str = 1;
        } else if (!strcmp(func, "w5_packages_6d8d5a_nodejs_fetch_url")) {
            r = w5_packages_6d8d5a_nodejs_fetch_url(ver, tgt); is_str = 1;
        } else if (!strcmp(func, "w5_packages_6d8d5a_git_fetch_url")) {
            r = w5_packages_6d8d5a_git_fetch_url(ver, tgt); is_str = 1;
        } else if (!strcmp(func, "w5_packages_6d8d5a_gh_fetch_url")) {
            r = w5_packages_6d8d5a_gh_fetch_url(ver, tgt); is_str = 1;
        } else if (!strcmp(func, "w5_packages_6d8d5a_uv_fetch_url")) {
            r = w5_packages_6d8d5a_uv_fetch_url(ver, tgt); is_str = 1;
        } else if (!strcmp(func, "w5_packages_6d8d5a_ripgrep_fetch_url")) {
            r = w5_packages_6d8d5a_ripgrep_fetch_url(ver, tgt); is_str = 1;
        } else if (!strcmp(func, "w5_packages_6d8d5a_chromium_store_entry")) {
            r = w5_packages_6d8d5a_chromium_store_entry(ver); is_str = 1;
        } else if (!strcmp(func, "w5_packages_6d8d5a_npm_env")) {
            out = w5_packages_6d8d5a_npm_env(json_get_str(in, "cache_dir", ""),
                                              json_obj_get(in, "base_env"));
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        if (is_str) {
            emit_str(r);
            free(r);
        } else {
            emit_key("out", out);
        }
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
