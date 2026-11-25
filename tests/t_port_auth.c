/* t_port_port_auth.c -- harness for the hermes_cli/auth.py port. Emits JSONL. */
#include "port_auth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const DEFAULT_ACTUAL = "https://api.actual.inc/v1";

static void jstr(const char *s)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else putchar(*p);
        }
    }
    putchar('"');
}

static const char *const VALUES[] = {
    "", " ", "  x  ", "sk-xxxxxxxx", "sk-or-v1-abc", "sk-abc", "ghp_xxx",
    "hf_xxx", "your_api_key_here", "your_key_here", "changeme", "placeholder",
    "dummy", "null", "none", "example", "***", "*", "xxxxxxxx", "xxxx-xxxx",
    "xxxx xxxx", "x", "ab", "abc", "abcd", "sk-", "sk-y", "sk-yx",
    "sk-or-", "OPENAI_API_KEY", "ghp_", "hf_", "sk-XXXX", "xxh", "axxxx",
};

static const char *const URLS[] = {
    "", "http://127.0.0.1:8080/v1", "http://127.0.0.1:8080", "http://localhost",
    "http://localhost/", "http://LOCALHOST./", "https://api.actual.inc",
    "https://api.actual.inc/", "https://api.actual.inc/v1",
    "https://api.actual.inc/v1/", "http://[::1]:8080/v1", "http://[::1]",
    "http://0.0.0.0:8080", "http://example.com/v1", "not a url",
    "localhost:8080/x", "//127.0.0.1/x", "http://user:pw@127.0.0.1/x",
    "  https://api.actual.inc  ", "ftp://127.0.0.1/v1", "http://127.0.0.1./v1",
};

int main(void)
{
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);

    for (size_t i = 0; i < sizeof(VALUES) / sizeof(VALUES[0]); i++) {
        printf("{\"fn\":\"nonempty\",\"arg\":");
        jstr(VALUES[i]);
        printf(",\"out\":%d}\n", st_nonempty_str(VALUES[i]));

        printf("{\"fn\":\"placeholder\",\"arg\":");
        jstr(VALUES[i]);
        printf(",\"out\":%d}\n", st_is_placeholder_shape(VALUES[i]));

        int mins[] = {1, 4, 8};
        for (size_t k = 0; k < 3; k++) {
            printf("{\"fn\":\"usable\",\"arg\":");
            jstr(VALUES[i]);
            printf(",\"min\":%d,\"out\":%d}\n", mins[k],
                   st_has_usable_secret(VALUES[i], mins[k]));
        }

        printf("{\"fn\":\"openrouter\",\"arg\":");
        jstr(VALUES[i]);
        printf(",\"out\":%d}\n", st_looks_like_openrouter_key(VALUES[i]));

        const char *provs[] = {"openrouter", "openai", ""};
        for (size_t k = 0; k < 3; k++) {
            printf("{\"fn\":\"prefix\",\"prov\":");
            jstr(provs[k]);
            printf(",\"arg\":");
            jstr(VALUES[i]);
            printf(",\"out\":%d}\n",
                   st_matches_key_prefix(provs[k], VALUES[i]));
        }

        for (size_t k = 0; k < 3; k++) {
            char out[2048];
            int warned = 0;
            int ok = st_usable_declared_secret(provs[k], VALUES[i], ".env",
                                               out, sizeof(out), &warned);
            printf("{\"fn\":\"declared\",\"prov\":");
            jstr(provs[k]);
            printf(",\"arg\":");
            jstr(VALUES[i]);
            printf(",\"ok\":%d,\"warned\":%d,\"out\":", ok, warned);
            jstr(out);
            fputs("}\n", stdout);
        fflush(stdout);
        }
    }

    for (size_t i = 0; i < sizeof(URLS) / sizeof(URLS[0]); i++) {
        printf("{\"fn\":\"local\",\"arg\":");
        jstr(URLS[i]);
        printf(",\"out\":%d}\n", st_is_actual_local_base_url(URLS[i]));

        char out[2048];
        st_normalize_actual_base_url(URLS[i], DEFAULT_ACTUAL, out, sizeof(out));
        printf("{\"fn\":\"normalize\",\"arg\":");
        jstr(URLS[i]);
        printf(",\"out\":");
        jstr(out);
        fputs("}\n", stdout);
        fflush(stdout);
    }
    return 0;
}
