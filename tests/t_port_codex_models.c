/*
 * t_port_codex_models.c -- oracle harness for port_codex_models.c
 */
#include "port_codex_models.h"

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

static void emit(const char *fn, const char *label, const char *const *ids)
{
    int n = 0;
    const char **out = strcmp(fn, "_dedupe") == 0
                     ? cm_dedupe(ids, &n)
                     : cm_add_forward_compat(ids, &n);
    printf("{\"fn\":");
    jstr(fn);
    printf(",\"case\":");
    jstr(label);
    printf(",\"out\":[");
    for (int i = 0; i < n; i++) { if (i) putchar(','); jstr(out[i]); }
    printf("]}\n");
    free(out);
}

int main(void)
{
    static const char *ids_empty[] = { NULL };
    static const char *ids_one[] = { "gpt-5.5", NULL };
    static const char *ids_dupes[] = { "gpt-5.5", "gpt-5.4", "gpt-5.5", "gpt-5.4", NULL };
    static const char *ids_chain[] = { "gpt-5.4", "gpt-5.4-mini", NULL };
    static const char *ids_spark[] = { "gpt-5.4", NULL };
    static const char *ids_already[] = { "gpt-6-sol", "gpt-5.5", NULL };

    /* ---- _dedupe ---- */
    emit("_dedupe", "empty", ids_empty);
    emit("_dedupe", "one", ids_one);
    emit("_dedupe", "dupes", ids_dupes);

    /* ---- _add_forward_compat_models ---- */
    emit("_add_forward_compat_models", "empty", ids_empty);
    emit("_add_forward_compat_models", "one-template", ids_one);
    emit("_add_forward_compat_models", "chain", ids_chain);
    emit("_add_forward_compat_models", "spark", ids_spark);
    emit("_add_forward_compat_models", "already-present", ids_already);

    return 0;
}
