/*
 * main_container.c — C home for upstream Hermes hermes_cli/main_container.py.
 *
 * PoP: w12_maincontai_9649d2_exec_plan @ hermes_cli/main_container.py:exec_in_container
 *
 * Deterministic seam: subprocess/shutil/os/sys side effects are injected as explicit
 * fixture arguments (env values, tty flag, probe return codes) so the command-building
 * and error-message semantics can be compared against the live Python function.
 */
#include "noc_w12_maincontai_9649d2.h"

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *env_get(const json_t *env, const char *key, const char *def) {
    if (!env || env->type != JSON_OBJECT) return def;
    json_t *v = json_obj_get((json_t *)env, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}

static int env_truthy(const json_t *env, const char *key) {
    const char *v = env_get(env, key, "");
    return v && *v ? 1 : 0;
}

/* Python ' '.join(cli_args). Caller frees. */
static char *join_cli(const json_t *args) {
    size_t total = 0;
    if (args && args->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(args); ++i) {
            json_t *v = json_get(args, i);
            total += (v && v->type == JSON_STRING && v->str_val) ? strlen(v->str_val) : 0;
        }
        total += json_len(args); /* spaces */
    }
    char *out = (char *)malloc(total + 1);
    if (!out) return strdup("");
    size_t n = 0;
    out[0] = '\0';
    if (args && args->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(args); ++i) {
            if (i) { out[n++] = ' '; out[n] = '\0'; }
            json_t *v = json_get(args, i);
            const char *s = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
            size_t l = strlen(s);
            memcpy(out + n, s, l);
            n += l;
            out[n] = '\0';
        }
    }
    return out;
}

static void append_str(json_t *arr, const char *s) {
    json_append(arr, json_string(s ? s : ""));
}

static json_t *error_result(const char *msg) {
    json_t *out = json_object();
    json_set(out, "kind", json_string("error"));
    json_set(out, "message", json_string(msg));
    return out;
}

struct json_t *w12_maincontai_9649d2_exec_plan(const char *backend,
                                                const char *container_name,
                                                const char *exec_user,
                                                const char *hermes_bin,
                                                const struct json_t *cli_args,
                                                const struct json_t *env,
                                                int stdin_isatty,
                                                int probe1_rc,
                                                int probe2_rc) {
    const char *be = backend ? backend : "";
    const char *name = container_name ? container_name : "";
    const char *user = exec_user ? exec_user : "";
    const char *bin = hermes_bin ? hermes_bin : "";
    const char *runtime = env_get(env, "runtime", "");
    const char *sudo_path = env_get(env, "sudo_path", "");
    char buf[16384];
    char *joined = join_cli(cli_args);

    if (!runtime || !*runtime) {
        snprintf(buf, sizeof(buf),
            "Error: %s not found on PATH. Cannot route to container.\n", be);
        free(joined);
        return error_result(buf);
    }

    int via_sudo = 0;
    if (probe1_rc != 0) {
        if (!sudo_path || !*sudo_path) {
            snprintf(buf, sizeof(buf),
                "Error: container '%s' not found via %s.\n"
                "The container may be running under root. Try: sudo hermes %s\n",
                name, be, joined);
            free(joined);
            return error_result(buf);
        }
        via_sudo = 1;
        if (probe2_rc != 0) {
            const char *who = env_get(env, "USER", "your-user");
            snprintf(buf, sizeof(buf),
                "Error: container '%s' not found via %s.\n"
                "\n"
                "The container is likely running as root. Your user cannot see it\n"
                "because %s uses per-user namespaces. Grant passwordless\n"
                "sudo for %s \xE2\x80\x94 the -n (non-interactive) flag is required\n"
                "because a password prompt would hang or break piped commands.\n"
                "\n"
                "On NixOS:\n"
                "\n"
                "  security.sudo.extraRules = [{\n"
                "    users = [ \"%s\" ];\n"
                "    commands = [{ command = \"%s\"; options = [ \"NOPASSWD\" ]; }];\n"
                "  }];\n"
                "\n"
                "Or run: sudo hermes %s\n",
                name, be, be, be, who, runtime, joined);
            free(joined);
            return error_result(buf);
        }
    }

    json_t *argv = json_array();
    if (via_sudo) {
        append_str(argv, sudo_path);
        append_str(argv, "-n");
        append_str(argv, runtime);
    } else {
        append_str(argv, runtime);
    }
    append_str(argv, "exec");
    append_str(argv, stdin_isatty ? "-it" : "-i");
    append_str(argv, "-u");
    append_str(argv, user);

    static const char *vars[4] = { "TERM", "COLORTERM", "LANG", "LC_ALL" };
    for (int i = 0; i < 4; ++i) {
        if (env_truthy(env, vars[i])) {
            const char *val = env_get(env, vars[i], "");
            append_str(argv, "-e");
            char pair[1024];
            snprintf(pair, sizeof(pair), "%s=%s", vars[i], val);
            append_str(argv, pair);
        }
    }
    append_str(argv, name);
    append_str(argv, bin);
    if (cli_args && cli_args->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(cli_args); ++i) {
            json_t *v = json_get(cli_args, i);
            append_str(argv, (v && v->type == JSON_STRING) ? v->str_val : "");
        }
    }
    free(joined);

    json_t *out = json_object();
    json_set(out, "kind", json_string("exec"));
    json_set(out, "argv", argv);
    return out;
}
