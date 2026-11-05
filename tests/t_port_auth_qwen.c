/*
 * t_port_auth_qwen.c — Oracle harness for the file/env half of
 * hermes_cli/auth_qwen.py (ported to src/cli/port_auth_qwen.c).
 */
#include "port_auth_qwen.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_json_str(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    {
        char *p = qwen_auth_path("/home/u");
        printf("{\"family\":\"path\",\"out\":");
        emit_json_str(p);
        printf("}\n");
        free(p);
    }

    {
        static const struct { const char *env, *name; } cases[] = {
            { NULL, "null" }, { "", "empty" }, { "   ", "spaces" },
            { "https://x.test/v1/", "trailing_slash" },
            { " https://x.test/v1// ", "trim_and_slashes" },
            { "https://x.test", "plain" },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            char *u = qwen_base_url(cases[i].env);
            printf("{\"family\":\"base_url\",\"case\":\"%s\",\"env\":", cases[i].name);
            emit_json_str(cases[i].env);
            printf(",\"out\":");
            emit_json_str(u);
            printf("}\n");
            free(u);
        }
    }

    {
        /* (expiry, skew, now) -> expiring? */
        struct { const char *expiry; int skew; double now; const char *name; } cases[] = {
            { "1000000", 120, 760.0, "exact_boundary" },   /* (760+120)*1000 == 880000 */
            { "880000", 120, 760.0, "equal" },
            { "880001", 120, 760.0, "just_after" },
            { "1000", 120, 760.0, "long_past" },
            { "abc", 120, 760.0, "not_a_number" },
            { "", 120, 760.0, "empty_string" },
            { "9999999999999", -50, 760.0, "negative_skew" },
            { "1e12", 0, 760.0, "scientific" },
            { "  900000  ", 0, 760.0, "padded" },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            int r = qwen_token_is_expiring(cases[i].expiry, cases[i].skew, cases[i].now);
            printf("{\"family\":\"expiring\",\"case\":\"%s\",\"expiry\":", cases[i].name);
            emit_json_str(cases[i].expiry);
            printf(",\"skew\":%d,\"now\":%.17g,\"ret\":%d}\n",
                   cases[i].skew, cases[i].now, r);
        }
    }

    /* read: BOM tolerated, invalid JSON, non-object, missing file */
    {
        const char *files[4] = {
            ".testbin/qwen_bom.json", ".testbin/qwen_bad.json",
            ".testbin/qwen_array.json", ".testbin/qwen_missing.json",
        };
        FILE *f = fopen(files[0], "wb");
        if (f) {
            const char *bom_doc = "\xef\xbb\xbf{\"access_token\":\"tok\",\"expiry_date\":123}";
            fwrite(bom_doc, 1, strlen(bom_doc), f);
            fclose(f);
        }
        f = fopen(files[1], "wb");
        if (f) { fputs("{not json", f); fclose(f); }
        f = fopen(files[2], "wb");
        if (f) { fputs("[1,2,3]", f); fclose(f); }
        remove(files[3]);

        static const char *names[4] = { "bom", "bad_json", "not_object", "missing" };
        for (int i = 0; i < 4; i++) {
            char *err = NULL;
            json_t *doc = qwen_read_tokens(files[i], &err);
            printf("{\"family\":\"read\",\"case\":\"%s\",\"ok\":%d,\"fields\":",
                   names[i], doc != NULL);
            if (doc) {
                char *s = json_serialize(doc);
                fputs(s ? s : "null", stdout);
                free(s);
                json_free(doc);
            } else {
                fputs("null", stdout);
            }
            printf(",\"err_class\":");
            /* compare the stable prefix, the path detail varies by directory */
            const char *k = err ? err : "";
            if (strstr(k, "not found")) emit_json_str("not_found");
            else if (strstr(k, "Failed to read")) emit_json_str("read_failed");
            else if (strstr(k, "Invalid Qwen CLI credentials")) emit_json_str("invalid");
            else emit_json_str("none");
            printf("}\n");
            free(err);
        }
    }

    /* save: round trip through read */
    {
        const char *path = ".testbin/qwen_saved/creds.json";
        int ok = qwen_save_tokens(path, "{\"access_token\":\"a\",\"expiry_date\":42}");
        char *err = NULL;
        json_t *doc = qwen_read_tokens(path, &err);
        printf("{\"family\":\"save\",\"case\":\"round_trip\",\"ok\":%d,\"read_ok\":%d,"
               "\"access_token\":", ok, doc != NULL);
        emit_json_str(doc ? json_get_str(doc, "access_token", "") : NULL);
        printf("}\n");
        if (doc) json_free(doc);
        free(err);
    }
    return 0;
}
