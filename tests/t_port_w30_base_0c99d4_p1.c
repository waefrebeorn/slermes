/* JSON-lines harness for the w30_base_0c99d4_p1 gate (base environment env /
 * capability / derivation core).  One request object per stdin line; one
 * response line carrying "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_base_0c99d4_p1.h"

typedef json_t *(*w_fn)(const json_t *);

static const struct { const char *name; w_fn fn; } W_TABLE[] = {
    { "sandbox_dir",          w30_base_0c99d4_sandbox_dir },
    { "uses_host_filesystem", w30_base_0c99d4_uses_host_filesystem },
    { "activity_due",         w30_base_0c99d4_activity_due },
    { "env_name_valid",       w30_base_0c99d4_env_name_valid },
    { "snapshot_exclusions",  w30_base_0c99d4_snapshot_exclusions },
    { "sudo_nopasswd",        w30_base_0c99d4_sudo_nopasswd },
    { "probe_nonlogin",       w30_base_0c99d4_probe_nonlogin },
    { "init_paths",           w30_base_0c99d4_init_paths },
    { "session_markers",      w30_base_0c99d4_session_markers },
    { "task_snapshot_path",   w30_base_0c99d4_task_snapshot_path },
    { "update_cwd",           w30_base_0c99d4_update_cwd },
    { "fetch_realpath",       w30_base_0c99d4_fetch_realpath },
    { "usable_cwd",           w30_base_0c99d4_usable_cwd },
    { "temp_dir",             w30_base_0c99d4_temp_dir },
    { "script_kwargs",        w30_base_0c99d4_script_kwargs },
};

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        size_t i, t;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in || in->type != JSON_OBJECT) { if (in) json_free(in); continue; }
        func = json_get_str(in, "func", "");
        out = NULL;
        for (t = 0; t < sizeof(W_TABLE) / sizeof(W_TABLE[0]); t++) {
            if (!strcmp(func, W_TABLE[t].name)) { out = W_TABLE[t].fn(in); break; }
        }
        if (!out) out = json_string("UNKNOWN_FUNC");
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (i = 0; i < json_object_size(in); i++) {
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
