/*
 * t_port_methods_tools.c -- oracle harness for port_methods_tools.c
 */
#include "port_methods_tools.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else putchar(*p);
    }
    putchar('"');
}

static void emit_str(const char *fn, const char *label, const char *v)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(v);
    printf("}\n");
}

static void emit_err(const char *fn, const char *label, const char *out)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(out);
    printf("}\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    char buf[MT_ERR_MAX];

    /* ---- _str_arg ---- */
    static const char *a1[] = { "name", "  hello  ", NULL };
    static const char *a2[] = { "name", "", NULL };
    static const char *a3[] = { "name", NULL, NULL };
    static const char *a4[] = { "other", "x", NULL };
    static const char *a5[] = { NULL };
    char *s;
    s = mt_str_arg(a1, "name"); emit_str("_str_arg", "plain",   s); free(s);
    s = mt_str_arg(a2, "name"); emit_str("_str_arg", "empty",   s); free(s);
    s = mt_str_arg(a3, "name"); emit_str("_str_arg", "null",    s); free(s);
    s = mt_str_arg(a4, "name"); emit_str("_str_arg", "missing", s); free(s);
    s = mt_str_arg(a5, "name"); emit_str("_str_arg", "no-map",  s); free(s);

    /* ---- busy_message ---- */
    s = mt_busy_message("/model", 0);
    emit_str("busy_message", "replying", s); free(s);
    s = mt_busy_message("/compress", 1);
    emit_str("busy_message", "compressing", s); free(s);
    s = mt_busy_message("model", 0);
    emit_str("busy_message", "no-slash", s); free(s);
    s = mt_busy_message("///loop", 0);
    emit_str("busy_message", "many-slashes", s); free(s);

    /* ---- _err ---- */
    mt_err_json(buf, sizeof(buf), "7", 4001, "no active session", NULL);
    emit_err("_err", "no-data", buf);
    mt_err_json(buf, sizeof(buf), "7", 4064, "profile 'x' not found", "extra");
    emit_err("_err", "with-data", buf);
    mt_err_json(buf, sizeof(buf), NULL, 4001, "quote\" and \\ slash", NULL);
    emit_err("_err", "escapes", buf);

    /* ---- _mcp_plugin_write_error ---- */
    static const char *pl1[] = { "github", "github-mcp", NULL };
    static const char *pl2[] = { "other", "thing", NULL };
    static const char *pl3[] = { NULL };
    if (mt_plugin_error("3", "github", pl1, buf, sizeof(buf)))
        emit_err("_mcp_plugin_write_error", "provided", buf);
    else
        emit_err("_mcp_plugin_write_error", "provided", "");
    if (mt_plugin_error("3", "github", pl2, buf, sizeof(buf)))
        emit_err("_mcp_plugin_write_error", "not-provided", buf);
    else
        emit_err("_mcp_plugin_write_error", "not-provided", "");
    if (mt_plugin_error("3", "github", pl3, buf, sizeof(buf)))
        emit_err("_mcp_plugin_write_error", "no-plugins", buf);
    else
        emit_err("_mcp_plugin_write_error", "no-plugins", "");

    /* ---- _busy_error ---- */
    static const char *se1[] = { "running", "1", NULL };
    static const char *se2[] = { "running", "1", "_manual_compress_active", "1", NULL };
    static const char *se3[] = { "running", "0", NULL };
    static const char *se4[] = { NULL };
    if (mt_busy_error("5", se1, "/model", buf, sizeof(buf)))
        emit_err("_busy_error", "running", buf);
    else
        emit_err("_busy_error", "running", "");
    if (mt_busy_error("5", se2, "/compress", buf, sizeof(buf)))
        emit_err("_busy_error", "compressing", buf);
    else
        emit_err("_busy_error", "compressing", "");
    if (mt_busy_error("5", se3, "/model", buf, sizeof(buf)))
        emit_err("_busy_error", "idle", buf);
    else
        emit_err("_busy_error", "idle", "");
    if (mt_busy_error("5", se4, "/model", buf, sizeof(buf)))
        emit_err("_busy_error", "no-session", buf);
    else
        emit_err("_busy_error", "no-session", "");

    return 0;
}
