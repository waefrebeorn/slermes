/* Oracle harness: credits free-tier detection vs LIVE Python.
 * Links the REAL port objects (credits_tracker.o, models_gaps.o,
 * models_helpers.o, port_anon_auth.o) — not an inline copy — so the gate
 * tests exactly what the binary links. Each record optionally seeds the
 * pricing cache first (models_cache_catalog takes ownership of the parsed
 * catalog), then calls the port and emits the verdict as JSON. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "models_gaps.h"
#include "libjson/json.h"

/* Declared in src/agent/credits_tracker.c (no header exports it yet). */
extern int cli_agent_credits_tracker_is_free_tier_model(const char *model,
                                                        const char *base_url);

static const char *js(const char *s) {
    static char b[4][4096]; static int t = 0; t = (t + 1) & 3;
    char *q = b[t]; *q++ = '"';
    if (s) for (const char *p = s; *p && (q - b[t]) < 4000; p++) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = (char)c; }
        else if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
        else if (c == '\t') { *q++ = '\\'; *q++ = 't'; }
        else if (c < ' ') {
            *q++ = '\\'; *q++ = 'u'; *q++ = '0'; *q++ = '0';
            *q++ = (char)((c > 15 ? '1' : '0'));
            *q++ = (char)(c % 16 < 10 ? c % 16 + '0' : c % 16 - 10 + 'a');
        }
        else *q++ = (char)c;
    }
    *q++ = '"'; *q = 0; return b[t];
}

static void emit(const char *model, const char *base_url,
                 const char *seed_key, const char *seed_catalog) {
    if (seed_key) {
        json_t *cat = json_parse(seed_catalog, NULL);
        models_cache_catalog(seed_key, cat);
    }
    int r = cli_agent_credits_tracker_is_free_tier_model(model, base_url) ? 1 : 0;
    printf("{\"func\":\"credits_is_free_tier_model\",\"model\":%s,\"base_url\":%s,",
           js(model), js(base_url));
    if (seed_key)
        printf("\"seed_key\":%s,\"seed_catalog\":%s,", js(seed_key), seed_catalog);
    else
        printf("\"seed_key\":null,\"seed_catalog\":null,");
    printf("\"ret\":%d}\n", r);
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    /* 1-4: suffix / stealth / no-base / empty-model rules (no cache). */
    emit("nvidia/nemotron-3-ultra:free", "", NULL, NULL);
    emit("stealth/preview-x", "", NULL, NULL);
    emit("gpt-4o", "", NULL, NULL);
    emit("", "https://api.example.com", NULL, NULL);
    /* 5: welcome host serves only the free tier. */
    emit("gpt-4o", "https://welcome-api.nousresearch.com/v1", NULL, NULL);
    /* 6-8: seeded catalog — $0 row free, priced row billable, absent row
     * billable. Prices are STRINGS, exactly as /v1/models serves them. */
    emit("m1", "https://api.example.com/v1", "https://api.example.com",
         "{\"m1\":{\"prompt\":\"0\",\"completion\":\"0\"},"
         "\"m3\":{\"prompt\":\"0.5\",\"completion\":\"0\"}}");
    emit("m3", "https://api.example.com", NULL, NULL);
    emit("absent", "https://api.example.com", NULL, NULL);
    /* 9: a different host has no cached catalog and no suffix. */
    emit("m1", "https://other.example.com", NULL, NULL);
    /* 10: suffix outranks the cache (live Python checks suffix first). */
    emit("paid:free", "https://api.example.com", NULL, NULL);
    /* 11: empty cached catalog falls through to the suffix rule. */
    emit("x", "https://empty.example.com", "https://empty.example.com", "{}");
    /* 12-15: re-seed the SAME key with a merged catalog (cache replace keeps
     * m1/m3), then exercise subscription billing, numeric zeros, missing
     * fields (default "1"), and unparseable prices (fail open to billable). */
    emit("sub-model", "https://api.example.com", "https://api.example.com",
         "{\"m1\":{\"prompt\":\"0\",\"completion\":\"0\"},"
         "\"m3\":{\"prompt\":\"0.5\",\"completion\":\"0\"},"
         "\"sub-model\":{\"prompt\":\"9\",\"completion\":\"9\",\"billing_mode\":\"subscription\"},"
         "\"num-zero\":{\"prompt\":0,\"completion\":0},"
         "\"half-priced\":{\"prompt\":\"0\"},"
         "\"junk-priced\":{\"prompt\":\"abc\",\"completion\":\"0\"}}");
    emit("num-zero", "https://api.example.com", NULL, NULL);
    emit("half-priced", "https://api.example.com", NULL, NULL);
    emit("junk-priced", "https://api.example.com", NULL, NULL);
    /* 16: /v1 suffix + trailing slashes normalize to the cached root. */
    emit("m1", "https://api.example.com/v1///", NULL, NULL);
    return 0;
}
