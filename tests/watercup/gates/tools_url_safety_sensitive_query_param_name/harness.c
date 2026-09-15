/* gate: tools_url_safety_sensitive_query_param_name — traced copy of
 * src/tools/url_safety.c::sensitive_query_param_name (the REAL port logic,
 * also linked via extra_src and called for the result value).
 * LIVE twin: tools/url_safety.py::sensitive_query_param_name.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: no '?' in url -> NULL
 *   B2: scheme http/https before ':' -> else NULL
 *   B3: query non-empty -> else NULL
 *   LOOP over '&'-split pairs:
 *     B4: value non-empty
 *     B5: decoded lowercased key in sensitive set -> return ORIGINAL key
 *   B6: exhausted -> NULL
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "watercup.h"

char *sensitive_query_param_name(const char *url); /* real port (extra_src) */

static const char *const QP[] = {
    "access_token", "api_key", "apikey", "auth_token", "authorization",
    "awsaccesskeyid", "client_secret", "credential", "credentials", "jwt",
    "password", "passwd", "secret", "session_id", "signature", "token",
    "x_amz_security_token", "x_amz_signature", "x-amz-security-token",
    "x-amz-signature", NULL
};

static void urldecode(char *s) {
    char *o = s;
    for (char *p = s; *p; ) {
        if (*p == '%' && p[1] && p[2] &&
            isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
            int hi = (p[1] >= '0' && p[1] <= '9') ? p[1]-'0'
                    : (tolower((unsigned char)p[1]) - 'a' + 10);
            int lo = (p[2] >= '0' && p[2] <= '9') ? p[2]-'0'
                    : (tolower((unsigned char)p[2]) - 'a' + 10);
            *o++ = (char)(hi*16 + lo);
            p += 3;
        } else {
            *o++ = *p++;
        }
    }
    *o = '\0';
}

static void trace_one(const char *url) {
    WCUP_BEGIN("sensitive_query_param_name");
    char result[512]; result[0] = '\0';
    if (!url || !*url) { WCUP_BRANCH(0); goto done; }      /* B1a empty */
    WCUP_BRANCH(1);
    {
        const char *q = strchr(url, '?');
        if (!q) { WCUP_BRANCH(0); goto done; }             /* B1 no '?' */
        WCUP_BRANCH(1);
        char scheme[16];
        const char *col = strchr(url, ':');
        int scheme_ok = 0;
        if (col && col <= q) {                             /* B2 */
            WCUP_BRANCH(1);
            size_t sl = (size_t)(col - url);
            if (sl > 0 && sl < sizeof(scheme)) {
                memcpy(scheme, url, sl); scheme[sl] = '\0';
                for (char *s = scheme; *s; s++)
                    *s = (char)tolower((unsigned char)*s);
                scheme_ok = (strcmp(scheme, "http") == 0 ||
                             strcmp(scheme, "https") == 0);
            }
        } else {
            WCUP_BRANCH(0);
        }
        if (!scheme_ok) goto done;
        const char *query = q + 1;
        if (!*query) { WCUP_BRANCH(0); goto done; }        /* B3 */
        WCUP_BRANCH(1);
        {
            char *qbuf = strdup(query);
            char *save = NULL;
            int found = 0;
            for (char *pair = strtok_r(qbuf, "&", &save); pair;
                 pair = strtok_r(NULL, "&", &save)) {
                WCUP_LOOP_TICK();
                char *eq = strchr(pair, '=');
                char *key = pair;
                char *val = "";
                if (eq) { *eq = '\0'; val = eq + 1; WCUP_BRANCH(1); }
                else WCUP_BRANCH(0);
                if (val && *val) {                         /* B4 */
                    WCUP_BRANCH(1);
                    char kdec[512];
                    snprintf(kdec, sizeof kdec, "%s", key);
                    urldecode(kdec);
                    for (char *k = kdec; *k; k++)
                        *k = (char)tolower((unsigned char)*k);
                    int hit = 0;
                    for (int i = 0; QP[i]; i++)
                        if (strcmp(kdec, QP[i]) == 0) { hit = 1; break; }
                    if (hit) {                             /* B5 */
                        WCUP_BRANCH(1);
                        snprintf(result, sizeof result, "%s", key);
                        found = 1;
                        break;
                    }
                    WCUP_BRANCH(0);
                } else {
                    WCUP_BRANCH(0);
                }
            }
            free(qbuf);
            if (!found) WCUP_BRANCH(0);                    /* B6 exhausted */
            else WCUP_BRANCH(1);
        }
    }
done:
    WCUP_VALSTR(result);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("sensitive_query_param_name");
    }
    /* sanity: the REAL port must agree with the traced copy's verdict */
    {
        char *real = sensitive_query_param_name(url);
        if ((real && !result[0]) || (!real && result[0]) ||
            (real && result[0] && strcmp(real, result) != 0)) {
            fprintf(stderr, "traced copy diverges from real port on '%s'\n", url);
            exit(3);
        }
        free(real);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sensitive_query_param_name <url>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
