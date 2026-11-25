/* JSON-lines harness for the w30_runtime_3588a2_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_runtime_3588a2_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const json_t *in, const char *func) {
    if (!strcmp(func, "state_dir"))              return w30_rt_p1_state_dir(in);
    if (!strcmp(func, "is_supported_host"))      return w30_rt_p1_is_supported_host(in);
    if (!strcmp(func, "missing_binaries"))       return w30_rt_p1_missing_binaries(in);
    if (!strcmp(func, "package_manager"))        return w30_rt_p1_package_manager(in);
    if (!strcmp(func, "is_root"))                return w30_rt_p1_is_root(in);
    if (!strcmp(func, "installable"))            return w30_rt_p1_installable(in);
    if (!strcmp(func, "install_command"))        return w30_rt_p1_install_command(in);
    if (!strcmp(func, "launcher_identity"))      return w30_rt_p1_launcher_identity(in);
    if (!strcmp(func, "launcher_pid"))           return w30_rt_p1_launcher_pid(in);
    if (!strcmp(func, "recorded_launcher_pid"))  return w30_rt_p1_recorded_launcher_pid(in);
    if (!strcmp(func, "x_lock_pid"))             return w30_rt_p1_x_lock_pid(in);
    if (!strcmp(func, "x_socket_bound"))         return w30_rt_p1_x_socket_bound(in);
    if (!strcmp(func, "display_in_use"))         return w30_rt_p1_display_in_use(in);
    if (!strcmp(func, "pick_display"))           return w30_rt_p1_pick_display(in);
    if (!strcmp(func, "published_env"))          return w30_rt_p1_published_env(in);
    if (!strcmp(func, "rfb_socket_path"))        return w30_rt_p1_rfb_socket_path(in);
    if (!strcmp(func, "is_running"))             return w30_rt_p1_is_running(in);
    if (!strcmp(func, "host_published_env"))     return w30_rt_p1_host_published_env(in);
    if (!strcmp(func, "desktop_env"))            return w30_rt_p1_desktop_env(in);
    if (!strcmp(func, "geometry"))               return w30_rt_p1_geometry(in);
    return json_string("UNKNOWN_FUNC");
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        out = dispatch(in, func);
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
