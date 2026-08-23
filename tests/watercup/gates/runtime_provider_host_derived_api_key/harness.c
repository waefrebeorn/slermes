/* gate: runtime_provider_host_derived_api_key — traced copy of
 * src/hermes_cli/provider_registry.c::provider_host_derived_api_key
 * (LIVE twin: hermes_cli/runtime_provider.py::_host_derived_api_key).
 *
 * Rung ladder (must match twin.py EXACTLY):
 *   B1 hostname empty / B2 ':' in host / B3 host == localhost /
 *   label-split loop L ticks / B4 last label starts with a digit (IP) /
 *   api|www prefix-strip loop: L tick + B5 popped / B6 fewer than 2 labels
 *   remain / sanitize loop: L tick + B7 char -> '_' / V sanitized name /
 *   B8 empty or non-alpha first char / B9 OPENAI|OPENROUTER|OLLAMA exclusion
 *   / B10 env var set / V env value.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"
#include <json.h>

/* PoP of utils.base_url_hostname (provider_registry.c::provider_base_url_hostname) */
static char *host_of(const char *base_url)
{
    if (!base_url) return strdup("");
    const char *p = base_url;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return strdup("");
    const char *host = p;
    const char *scheme = strstr(p, "://");
    if (scheme) host = scheme + 3;
    const char *at = NULL;
    for (const char *q = host; *q && *q != '/' && *q != '?' && *q != '#'; q++)
        if (*q == '@') at = q;
    if (at) host = at + 1;
    size_t n = 0;
    for (const char *q = host; q[n] && q[n] != '/' && q[n] != '?' &&
                               q[n] != '#' && q[n] != ':'; ) n++;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)host[i]);
    out[n] = '\0';
    size_t len = strlen(out);
    while (len > 0 && out[len - 1] == '.') out[--len] = '\0';
    return out;
}

static void trace_key(wcup_t *wc, const char *base_url)
{
    char *host = host_of(base_url);
    wcup_branch(wc, !host || !*host);                          /* B1 */
    if (!host || !*host) { free(host); return; }
    int has_colon = strchr(host, ':') != NULL;
    wcup_branch(wc, has_colon);                                /* B2 */
    if (has_colon) { free(host); return; }
    int is_localhost = strcmp(host, "localhost") == 0;
    wcup_branch(wc, is_localhost);                             /* B3 */
    if (is_localhost) { free(host); return; }

    {
        char *labels[32];
        int nl = 0;
        char *tmp = strdup(host);
        for (char *tok = strtok(tmp, "."); tok && nl < 32; tok = strtok(NULL, "."))
            if (*tok) {
                wcup_loop(wc);
                labels[nl++] = tok;
            }
        int ip_like = nl > 0 && isdigit((unsigned char)labels[nl - 1][0]);
        wcup_branch(wc, ip_like);                              /* B4 */
        if (!ip_like) {
            int start = 0;
            while (start < nl &&
                   (strcmp(labels[start], "api") == 0 ||
                    strcmp(labels[start], "www") == 0)) {
                wcup_loop(wc);
                wcup_branch(wc, 1);                            /* B5 */
                start++;
            }
            wcup_branch(wc, nl - start < 2);               /* B6 */
            if (nl - start >= 2) {
                const char *vendor = labels[nl - 2];
                size_t vl = strlen(vendor);
                char san[64];
                size_t cap = sizeof(san) - 1;
                if (vl > cap) vl = cap;
                for (size_t i = 0; i < vl; i++) {
                    wcup_loop(wc);
                    char c = vendor[i];
                    int special = !isalnum((unsigned char)c);
                    wcup_branch(wc, special);                  /* B7 */
                    san[i] = special ? '_' : (char)toupper((unsigned char)c);
                }
                san[vl] = '\0';
                wcup_valstr(wc, san);
                size_t sl = strlen(san);
                wcup_branch(wc, !sl || !isalpha((unsigned char)san[0]));   /* B8 */
                int excluded = strcmp(san, "OPENAI") == 0 ||
                               strcmp(san, "OPENROUTER") == 0 ||
                               strcmp(san, "OLLAMA") == 0;
                wcup_branch(wc, excluded);                     /* B9 */
                if (sl && isalpha((unsigned char)san[0]) && !excluded) {
                    char envname[128];
                    snprintf(envname, sizeof(envname), "%s_API_KEY", san);
                    const char *v = getenv(envname);
                    wcup_branch(wc, v != NULL);                /* B10 */
                    if (v) {
                        while (*v && isspace((unsigned char)*v)) v++;
                        const char *e = v + strlen(v);
                        while (e > v && isspace((unsigned char)e[-1])) e--;
                        wcup_valstr(wc, v);
                    }
                }
            }
        }
        free(tmp);
    }
    free(host);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s host_derived_api_key <url>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("host_derived_api_key");
        trace_key(&_wc, argv[i]);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("host_derived_api_key");
    }
    return 0;
}
