/* t_port_models_pricing.c — oracle harness for the pure pricing helpers.
 * Emits one JSON line per case with a "fn" discriminator. */
#include "port_models_pricing.h"

#include <stdio.h>
#include <string.h>

static void jstr(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        if (*s == '"' || *s == '\\') {
            putchar('\\');
            putchar(*s);
        } else {
            putchar(*s);
        }
    }
    putchar('"');
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* _price_float(raw, positive) */
    static const char *const pf_raw[] = {
        "1.5", "0", "-1", "abc", "", " 2.5 ", "1e3", "nan", "inf", "0.0", "0.0001", "-0.5",
    };
    static const int pf_pos[] = {1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0};
    size_t npf = sizeof(pf_raw) / sizeof(pf_raw[0]);
    for (size_t i = 0; i < npf; i++) {
        double out = 0;
        int ok = mp_price_float(pf_raw[i], pf_pos[i], &out);
        printf("{\"case\":%zu,\"fn\":\"price_float\",\"raw\":", i);
        jstr(pf_raw[i]);
        printf(",\"positive\":%d,\"out\":", pf_pos[i]);
        if (!ok) {
            printf("null");
        } else if (out != out) {
            printf("null");           /* NaN: reported as None by the oracle rule */
        } else {
            printf("%.17g", out);
        }
        printf("}\n");
    }

    /* _sale_pct(current, original) */
    static const char *const sp[][2] = {
        {"0.5", "1.0"}, {"1.0", "1.0"}, {"2.0", "1.0"}, {"abc", "1.0"},
        {"0.75", "1.0"}, {"0.005", "1.0"}, {"0", "1.0"}, {"1.0", "0"},
        {"0.333", "1.0"}, {"0.99", "1.0"},
    };
    size_t nsp = sizeof(sp) / sizeof(sp[0]);
    for (size_t i = 0; i < nsp; i++) {
        int out = mp_sale_pct(sp[i][0], sp[i][1]);
        printf("{\"case\":%zu,\"fn\":\"sale_pct\",\"current\":", i);
        jstr(sp[i][0]);
        printf(",\"original\":");
        jstr(sp[i][1]);
        printf(",\"out\":%d}\n", out);
    }

    /* _format_price_per_mtok(per_token_str) */
    static const char *const fp[] = {
        "0.0000015", "0", "abc", "0.000000001", "0.000001", "0.00001",
        "0.0001", "0.001", "0.01", "1", "1.5", "", " 0.0000025 ",
    };
    size_t nfp = sizeof(fp) / sizeof(fp[0]);
    for (size_t i = 0; i < nfp; i++) {
        char out[64];
        mp_format_price_per_mtok(fp[i], out, sizeof(out));
        printf("{\"case\":%zu,\"fn\":\"format_price_per_mtok\",\"in\":", i);
        jstr(fp[i]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
    }

    /* _strip_v1(url) */
    static const char *const sv[] = {
        "https://x/v1", "https://x/v1/", "https://x", "https://x/api/v1",
        "", "/v1", "v1",
    };
    size_t nsv = sizeof(sv) / sizeof(sv[0]);
    for (size_t i = 0; i < nsv; i++) {
        char out[128];
        mp_strip_v1(sv[i], out, sizeof(out));
        printf("{\"case\":%zu,\"fn\":\"strip_v1\",\"in\":", i);
        jstr(sv[i]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
    }
    return 0;
}
