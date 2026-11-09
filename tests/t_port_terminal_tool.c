/*
 * t_port_terminal_tool.c -- oracle harness for port_terminal_tool.c
 */
#include "port_terminal_tool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_bool(const char *fn, const char *label, int v)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":%s}\n", v ? "true" : "false");
}

static void emit_str(const char *fn, const char *label, const char *v)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":");
    if (!v) fputs("null", stdout);
    else {
        putchar('"');
        for (const char *q = v; *q; q++) {
            if (*q == '"' || *q == '\\') { putchar('\\'); putchar(*q); }
            else putchar(*q);
        }
        putchar('"');
    }
    printf("}\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- _docker_volume_uses_host_path ---- */
    emit_bool("_docker_volume_uses_host_path", "abs",       tt_volume_uses_host_path("/Users/me:/data"));
    emit_bool("_docker_volume_uses_host_path", "tilde",     tt_volume_uses_host_path("~/x:/data"));
    emit_bool("_docker_volume_uses_host_path", "dot",       tt_volume_uses_host_path("./x:/data"));
    emit_bool("_docker_volume_uses_host_path", "dotdot",    tt_volume_uses_host_path("../x:/data"));
    emit_bool("_docker_volume_uses_host_path", "drive",     tt_volume_uses_host_path("C:\\x:/data"));
    emit_bool("_docker_volume_uses_host_path", "drive-fwd", tt_volume_uses_host_path("C:/x:/data"));
    emit_bool("_docker_volume_uses_host_path", "named",     tt_volume_uses_host_path("mydata:/data"));
    emit_bool("_docker_volume_uses_host_path", "padded",    tt_volume_uses_host_path("  /abs:/x  "));
    emit_bool("_docker_volume_uses_host_path", "empty",     tt_volume_uses_host_path("   "));
    emit_bool("_docker_volume_uses_host_path", "null",      tt_volume_uses_host_path(NULL));
    emit_bool("_docker_volume_uses_host_path", "short-drive", tt_volume_uses_host_path("C:"));

    /* ---- _docker_has_host_access ---- */
    static const char *vols_abs[] = { "/Users/me:/data", NULL };
    static const char *vols_named[] = { "mydata:/data", NULL };
    static const char *cfg1[] = { "env_type", "docker", "docker_volumes", (const char *)vols_abs, NULL };
    static const char *cfg2[] = { "env_type", "docker", "docker_volumes", (const char *)vols_named, NULL };
    static const char *cfg3[] = { "env_type", "docker", "host_cwd", "/Users/me",
                                  "docker_mount_cwd_to_workspace", "true", NULL };
    static const char *cfg4[] = { "env_type", "local", "docker_volumes", (const char *)vols_abs, NULL };
    static const char *cfg5[] = { "env_type", "docker", NULL };
    emit_bool("_docker_has_host_access", "abs-vol",   tt_has_host_access(cfg1));
    emit_bool("_docker_has_host_access", "named-vol", tt_has_host_access(cfg2));
    emit_bool("_docker_has_host_access", "host-cwd",  tt_has_host_access(cfg3));
    emit_bool("_docker_has_host_access", "not-docker",tt_has_host_access(cfg4));
    emit_bool("_docker_has_host_access", "no-vols",   tt_has_host_access(cfg5));

    /* ---- _is_isolation_override_key ---- */
    emit_bool("_is_isolation_override_key", "env_type",  tt_is_isolation_key("env_type"));
    emit_bool("_is_isolation_override_key", "docker",    tt_is_isolation_key("docker_image"));
    emit_bool("_is_isolation_override_key", "plugin",    tt_is_isolation_key("mybackend_image"));
    emit_bool("_is_isolation_override_key", "other",     tt_is_isolation_key("docker_image_x"));
    emit_bool("_is_isolation_override_key", "empty",     tt_is_isolation_key(""));
    emit_bool("_is_isolation_override_key", "null",      tt_is_isolation_key(NULL));

    /* ---- _qualify_task_key ---- */
    char *q1 = tt_qualify_task_key("sess-1", "work");
    emit_str("_qualify_task_key", "scoped", q1); free(q1);
    char *q2 = tt_qualify_task_key("sess-1", "");
    emit_str("_qualify_task_key", "unscoped", q2); free(q2);
    char *q3 = tt_qualify_task_key("sess-1", NULL);
    emit_str("_qualify_task_key", "null-scope", q3); free(q3);

    /* ---- _resolve_container_alias ---- */
    static const char *a1[] = { "child", "parent", "parent", "root", NULL };
    static const char *a2[] = { "a", "b", "b", "a", NULL };
    static const char *a3[] = { NULL };
    char *r1 = tt_resolve_container_alias("child", a1);
    emit_str("_resolve_container_alias", "chain", r1); free(r1);
    char *r2 = tt_resolve_container_alias("a", a2);
    emit_str("_resolve_container_alias", "cycle", r2); free(r2);
    char *r3 = tt_resolve_container_alias("unknown", a1);
    emit_str("_resolve_container_alias", "miss", r3); free(r3);
    char *r4 = tt_resolve_container_alias("root", a1);
    emit_str("_resolve_container_alias", "end", r4); free(r4);
    char *r5 = tt_resolve_container_alias("solo", a3);
    emit_str("_resolve_container_alias", "empty-table", r5); free(r5);

    return 0;
}
