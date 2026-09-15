/* gate: providers_custom_provider_aliases — traced copy of
 * src/hermes_cli/ports_wrappers.c::hermes_cli_providers_custom_provider_aliases
 * (translation unit too heavy to link; body inlined verbatim, output rendered
 * as a SORTED '|'-joined alias set so set semantics are compared order-free).
 * LIVE twin: hermes_cli/providers.py::custom_provider_aliases.
 *
 * Input argv: "<display_name>\t<provider_key>".
 * Branch ladder (must match twin.py EXACTLY):
 *   B1: display raw non-empty   B2: display normalized starts "custom:"+suffix
 *   B3: key raw non-empty       B4: key normalized starts "custom:"+suffix
 *   VSTR(sorted aliases joined by '|')
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

static void decode_escapes(char *s) {
    char *o = s;
    for (char *p = s; *p; p++) {
        if (p[0] == '\\' && p[1] == 't') { *o++ = '\t'; p++; }
        else *o++ = *p;
    }
    *o = '\0';
}

/* verbatim port logic, collecting into a dedup'd list */
static char **build_aliases(const char *arg, size_t *out_n) {
    char display[512] = "";
    char pkey[512] = "";
    const char *tab = strchr(arg, '\t');
    if (tab) {
        size_t dn = (size_t)(tab - arg);
        if (dn > 511) dn = 511;
        memcpy(display, arg, dn);
        display[dn] = '\0';
        strncpy(pkey, tab + 1, 511);
        pkey[511] = '\0';
    } else {
        strncpy(display, arg, 511);
        display[511] = '\0';
    }

    char **aliases = NULL;
    size_t n = 0, cap = 0;

    for (int vi = 0; vi < 2; vi++) {
        const char *value = vi == 0 ? display : pkey;
        if (!value || !*value) continue;
        char raw[512];
        const char *s = value;
        while (*s == ' ' || *s == '\t') s++;
        size_t sl = strlen(s);
        while (sl > 0 && (s[sl-1] == ' ' || s[sl-1] == '\t')) sl--;
        for (size_t i = 0; i < sl && i < 511; i++)
            raw[i] = (char)tolower((unsigned char)s[i]);
        raw[sl < 512 ? sl : 511] = '\0';
        if (!*raw) continue;

        char norm[512];
        for (size_t i = 0; raw[i] && i < 511; i++)
            norm[i] = (raw[i] == ' ') ? '-' : raw[i];
        norm[strlen(raw)] = '\0';

        char slug[512];
        snprintf(slug, sizeof(slug), "custom:%s", norm);

        char *variants[] = { strdup(raw), strdup(norm), strdup(slug) };
        for (int k = 0; k < 3; k++) {
            bool found = false;
            for (size_t j = 0; j < n; j++)
                if (strcmp(aliases[j], variants[k]) == 0) { found = true; break; }
            if (!found) {
                if (n == cap) { cap = cap ? cap * 2 : 8; aliases = realloc(aliases, (cap + 1) * sizeof(char *)); }
                aliases[n++] = variants[k];
            } else free(variants[k]);
        }

        if (!strncmp(norm, "custom:", 7) && strlen(norm) > 7) {
            char suffix[512];
            strcpy(suffix, norm + 7);
            char alt[512];
            snprintf(alt, sizeof(alt), "custom:%s", norm);
            char *variants2[] = { strdup(suffix), strdup(alt) };
            for (int k = 0; k < 2; k++) {
                bool found = false;
                for (size_t j = 0; j < n; j++)
                    if (strcmp(aliases[j], variants2[k]) == 0) { found = true; break; }
                if (!found) {
                    if (n == cap) { cap = cap ? cap * 2 : 8; aliases = realloc(aliases, (cap + 1) * sizeof(char *)); }
                    aliases[n++] = variants2[k];
                } else free(variants2[k]);
            }
        }
    }
    *out_n = n;
    return aliases;
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void run_one(const char *input) {
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", input);
    decode_escapes(buf);

    /* trace the per-value ladder the port walks */
    const char *tab = strchr(buf, '\t');
    char v0[512] = "", v1[512] = "";
    if (tab) { memcpy(v0, buf, (size_t)(tab - buf)); strncpy(v1, tab + 1, 511); }
    else strncpy(v0, buf, 511);

    WCUP_BEGIN("custom_provider_aliases");
    WCUP_BRANCH(v0[0]);                                         /* B1 */
    WCUP_BRANCH(strncasecmp(v0, "custom:", 7) == 0 && strlen(v0) > 7); /* B2 */
    WCUP_BRANCH(v1[0]);                                         /* B3 */
    WCUP_BRANCH(strncasecmp(v1, "custom:", 7) == 0 && strlen(v1) > 7); /* B4 */

    size_t n = 0;
    char **aliases = build_aliases(buf, &n);
    qsort(aliases, n, sizeof(char *), cmp_str);
    char joined[4096] = "";
    size_t off = 0;
    for (size_t i = 0; i < n; i++)
        off += (size_t)snprintf(joined + off, sizeof joined - off, "%s%s",
                                i ? "|" : "", aliases[i]);
    WCUP_VALSTR(joined);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("custom_provider_aliases");

    for (size_t i = 0; i < n; i++) free(aliases[i]);
    free(aliases);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <fn> <display\\tkey...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
