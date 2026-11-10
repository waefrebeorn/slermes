/*
 * t_port_inventory.c -- oracle harness for port_inventory.c
 *
 * Emits one JSON object per line. The input is echoed from C (never retyped in
 * the oracle) so the two sides can never drift on a literal.
 */
#include "port_inventory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else if (*p == '\n') fputs("\\n", stdout);
        else if (*p == '\r') fputs("\\r", stdout);
        else if (*p == '\t') fputs("\\t", stdout);
        else putchar(*p);
    }
    putchar('"');
}

static void emit_slug_case(const char *label, const char *json)
{
    json_t *row = json_parse(json, NULL);
    char *s = inv_slug(row);
    printf("{\"fn\":\"_slug\",\"case\":");
    jstr(label);
    printf(",\"in\":");
    jstr(json);
    printf(",\"out\":");
    jstr(s);
    printf("}\n");
    free(s);
    json_free(row);
}

static void emit_iso_case(double epoch)
{
    char *s = inv_iso_from_epoch(epoch);
    printf("{\"fn\":\"_iso_from_epoch\",\"in\":%.17g,\"out\":", epoch);
    jstr(s);
    printf("}\n");
    free(s);
}

int main(void)
{
    /* ---- _slug: missing / null / number / bool / blank / padded / cased ---- */
    emit_slug_case("missing",   "{\"name\":\"x\"}");
    emit_slug_case("null",      "{\"slug\":null}");
    emit_slug_case("number",    "{\"slug\":2070}");
    emit_slug_case("bool",      "{\"slug\":true}");
    emit_slug_case("empty",     "{\"slug\":\"\"}");
    emit_slug_case("padded",    "{\"slug\":\"  OpenRouter  \"}");
    emit_slug_case("cased",     "{\"slug\":\"GPT-5\"}");
    emit_slug_case("object",    "{\"slug\":{\"a\":1}}");

    /* ---- _row: fixed shape ---- */
    {
        json_t *r = inv_row("openrouter", "OpenRouter", 1);
        char *d = json_dumps(r, 0);
        printf("{\"fn\":\"_row\",\"case\":\"current\",\"out\":%s}\n", d);
        free(d); json_free(r);
    }
    {
        json_t *r = inv_row("  x ", "X", 0);
        char *d = json_dumps(r, 0);
        printf("{\"fn\":\"_row\",\"case\":\"plain\",\"out\":%s}\n", d);
        free(d); json_free(r);
    }

    /* ---- _without_slug: filter a row array ---- */
    {
        const char *rows = "[{\"slug\":\"a\"},{\"slug\":\" b \"},{\"slug\":\"c\"},{\"slug\":\"a\"}]";
        json_t *arr = json_parse(rows, NULL);
        json_t *kept = inv_without_slug(arr, "a");
        char *d = json_dumps(kept, 0);
        printf("{\"fn\":\"_without_slug\",\"case\":\"a\",\"out\":%s}\n", d);
        free(d); json_free(kept); json_free(arr);
    }
    {
        const char *rows = "[{\"slug\":\"a\"},{\"slug\":\"b\"}]";
        json_t *arr = json_parse(rows, NULL);
        json_t *kept = inv_without_slug(arr, "zz");
        char *d = json_dumps(kept, 0);
        printf("{\"fn\":\"_without_slug\",\"case\":\"none-match\",\"out\":%s}\n", d);
        free(d); json_free(kept); json_free(arr);
    }
    {
        json_t *kept = inv_without_slug(NULL, "a");
        char *d = json_dumps(kept, 0);
        printf("{\"fn\":\"_without_slug\",\"case\":\"null-rows\",\"out\":%s}\n", d);
        free(d); json_free(kept);
    }

    /* ---- _iso_from_epoch ---- */
    emit_iso_case(0.0);              /* 1970-01-01T00:00:00+00:00 */
    emit_iso_case(1.5);              /* .5 s -> microseconds */
    emit_iso_case(1762700096.0);     /* whole second */
    emit_iso_case(1762700096.123456);/* microseconds */
    emit_iso_case(-1.0);             /* pre-epoch, whole second */

    /* ---- with_overrides: TRUTHY only ---- */
    {
        const char *ctx = "{\"current_provider\":\"nous\",\"current_model\":\"m1\","
                          "\"current_base_url\":\"http://x\"}";
        json_t *c = json_parse(ctx, NULL);
        json_t *o = inv_with_overrides(c, "", "m2", NULL);
        char *d = o ? json_dumps(o, 0) : NULL;
        printf("{\"fn\":\"with_overrides\",\"case\":\"empty-provider-kept\","
               "\"in\":%s,\"applied\":%s}\n", ctx, d ? d : "null");
        free(d); if (o) json_free(o); json_free(c);
    }
    {
        const char *ctx = "{\"current_provider\":\"nous\",\"current_model\":\"m1\"}";
        json_t *c = json_parse(ctx, NULL);
        json_t *o = inv_with_overrides(c, "x", "y", "z");
        char *d = o ? json_dumps(o, 0) : NULL;
        printf("{\"fn\":\"with_overrides\",\"case\":\"all-set\","
               "\"in\":%s,\"applied\":%s}\n", ctx, d ? d : "null");
        free(d); if (o) json_free(o); json_free(c);
    }
    {
        const char *ctx = "{\"current_provider\":\"nous\"}";
        json_t *c = json_parse(ctx, NULL);
        json_t *o = inv_with_overrides(c, "", "", "");
        printf("{\"fn\":\"with_overrides\",\"case\":\"all-empty\","
               "\"applied\":%s}\n", o ? "non-null" : "null");
        if (o) json_free(o); json_free(c);
    }

    return 0;
}
