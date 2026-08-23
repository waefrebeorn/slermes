/* gate: models_normalize_opencode_base_url — traced C copy of
 * src/hermes_cli/models_py_helpers.c::models_normalize_opencode_base_url
 * (PoP of LIVE hermes_cli/models.py::normalize_opencode_base_url).
 * argv input format: "<provider_id>|<api_mode>|<base_url>"
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

static char *normalize_opencode_base_url(const char *provider_id,
                                         const char *api_mode,
                                         const char *base_url) {
    WCUP_BEGIN("normalize_opencode_base_url");
    if (!base_url) base_url = "";
    char *url = strdup(base_url);
    /* strip leading ws */
    char *p = url; while (*p == ' ' || *p == '\t') p++;
    memmove(url, p, strlen(p) + 1);
    /* rstrip '/' and trailing ws */
    size_t L = strlen(url);
    while (L > 0 && (url[L-1] == '/' || url[L-1] == ' ' || url[L-1] == '\t'))
        url[--L] = '\0';
    WCUP_VALSTR(url);

    char *result;
    if (url[0] == '\0') { WCUP_BRANCH(1); result = url; goto out; }
    WCUP_BRANCH(0);

    /* provider family check (C port: opencode-zen | opencode-go) */
    {
        const char *pid = provider_id ? provider_id : "";
        int family = (strcmp(pid, "opencode-zen") == 0 || strcmp(pid, "opencode-go") == 0);
        if (!family) { WCUP_BRANCH(1); result = url; goto out; }
        WCUP_BRANCH(0);
    }

    if (api_mode && strcmp(api_mode, "anthropic_messages") == 0) {
        WCUP_BRANCH(1);
        size_t n = strlen(url);
        if (n >= 3 && strcmp(url + n - 3, "/v1") == 0) url[n - 3] = '\0';
        result = url;
        goto out;
    }
    WCUP_BRANCH(0);

    {
        size_t n = strlen(url);
        if (n >= 3 && strcmp(url + n - 3, "/v1") == 0) {
            WCUP_BRANCH(1); result = url; goto out;
        }
        WCUP_BRANCH(0);
        /* host parse */
        char host[1024];
        const char *scheme = strstr(url, "://");
        const char *start = scheme ? scheme + 3 : url;
        const char *slash = strchr(start, '/');
        size_t hlen = slash ? (size_t)(slash - start) : strlen(start);
        if (hlen >= sizeof host) hlen = sizeof host - 1;
        memcpy(host, start, hlen); host[hlen] = '\0';
        for (char *q = host; *q; q++) *q = (char)tolower((unsigned char)*q);
        WCUP_VALSTR(host);
        if (strcmp(host, "opencode.ai") == 0 ||
            (hlen > 11 && strcmp(host + hlen - 11, ".opencode.ai") == 0)) {
            WCUP_BRANCH(1);
            char *out2 = (char *)malloc(strlen(url) + 4);
            sprintf(out2, "%s/v1", url);
            free(url);
            result = out2;
            goto out;
        }
        WCUP_BRANCH(0);
        result = url;
    }
out:
    WCUP_VALSTR(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("normalize_opencode_base_url");
    return result;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s normalize_opencode_base_url <prov|mode|url>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *arg = argv[i];
        char *m = strchr(arg, '|');
        char *u = m ? strchr(m + 1, '|') : NULL;
        if (!m || !u) { fprintf(stderr, "bad input: %s\n", arg); return 2; }
        *m = '\0'; *u = '\0';
        const char *url = (strcmp(u + 1, "null") == 0) ? NULL : u + 1;
        free(normalize_opencode_base_url(arg, m + 1, url));
    }
    return 0;
}
