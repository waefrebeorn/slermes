/*
 * t_port_prompt_builder.c -- oracle harness for port_prompt_builder.c
 */
#include "port_prompt_builder.h"

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

static void emit_strip(const char *label, const char *content)
{
    char *out = pb_strip_yaml_frontmatter(content);
    printf("{\"fn\":\"_strip_yaml_frontmatter\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(out);
    printf("}\n");
    free(out);
}

static void emit_marker(const char *label, const char *text)
{
    char *out = pb_format_steer_marker(text);
    printf("{\"fn\":\"format_steer_marker\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(out);
    printf("}\n");
    free(out);
}

static void emit_probe(const char *label, const char *output)
{
    char *out = pb_format_backend_probe(output);
    printf("{\"fn\":\"_format_backend_probe\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(out);
    printf("}\n");
    free(out);
}

static void emit_remote(const char *label, const char *backend, int attr)
{
    int v = pb_plugin_backend_is_remote(backend, attr);
    printf("{\"fn\":\"_plugin_backend_is_remote\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":%s}\n", v ? "true" : "false");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- _strip_yaml_frontmatter ---- */
    emit_strip("no-frontmatter", "# Title\nbody\n");
    emit_strip("simple",        "---\ntitle: x\n---\nbody\n");
    emit_strip("closing-only",  "---\nbody\n");
    emit_strip("bom",           "\xef\xbb\xbf---\ntitle: x\n---\nbody\n");
    emit_strip("bom-only",      "\xef\xbb\xbf# Title\n");
    emit_strip("empty-after",   "---\ntitle: x\n---\n");
    emit_strip("newlines-after","---\na: 1\n---\n\n\nbody\n");
    emit_strip("dash-in-body", "intro\n---\nbody\n");
    emit_strip("not-closed",    "---\nno closing fence\n");
    emit_strip("empty",         "");
    emit_strip("null",          NULL);
    emit_strip("four-dashes",   "----\nbody\n");

    /* ---- format_steer_marker ---- */
    emit_marker("simple", "please stop");
    emit_marker("multi",  "line one\nline two");
    emit_marker("empty",  "");

    /* ---- _format_backend_probe ---- */
    emit_probe("both",      "os=Linux\nkernel=5.15\n");
    emit_probe("os-only",   "os=Windows\n");
    emit_probe("kernel-only","kernel=6.1\n");
    emit_probe("unknown",   "os=unknown\nkernel=unknown\n");
    emit_probe("mixed",     "os=unknown\nkernel=6.1\n");
    emit_probe("spaces",    "  os = Linux  \n  kernel = 5.15  \n");
    emit_probe("later-wins","os=First\nos=Second\n");
    emit_probe("no-eq",     "no equals here\n");
    emit_probe("empty",     "");
    emit_probe("crlf",      "os=Linux\r\nkernel=5.15\r\n");
    emit_probe("other-keys","name=x\nversion=2\n");

    /* ---- _plugin_backend_is_remote ---- */
    emit_remote("local",     "local", 0);
    emit_remote("remote-set", "docker", 0);
    emit_remote("ssh",       "ssh", 0);
    emit_remote("remote-attr","custom", 1);
    emit_remote("unknown-local","custom", 0);
    emit_remote("empty",     "", 1);
    emit_remote("modal",     "modal", 0);

    return 0;
}
