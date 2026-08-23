/* gate: no_proxy_entry_matches — traced copy of
 * src/gateway/platforms/base.c::no_proxy_entry_matches
 * (LIVE twin: gateway/platforms/base.py::_no_proxy_entry_matches).
 *
 * argv input per cup: "<entry>|<host>|<port>" (port -1 = None).
 *
 * Branch ladder (twin mirrors EXACTLY):
 *   B1: empty token      -> false
 *   B2: token == "*"     -> true
 *   B3: port mismatch    -> false
 *   B4: token has port, host port is None -> false
 *   (B5 empty token_host is unreachable after strip in both sides; kept)
 *   B6: token startswith "*."  -> suffix endswith
 *   B7: token startswith "."   -> eq or endswith
 *   else exact / ".host" endswith
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

static int ends_with(const char *s, const char *suf) {
    size_t ls = strlen(s), lf = strlen(suf);
    if (lf > ls) return 0;
    return strcmp(s + ls - lf, suf) == 0;
}

static void lower_into(const char *src, char *dst, size_t cap) {
    size_t x = 0;
    for (; src[x] && x < cap - 1; x++) dst[x] = (char)tolower((unsigned char)src[x]);
    dst[x] = '\0';
}

static int no_proxy_entry_matches(const char *entry, const char *host, int port) {
    WCUP_BEGIN("no_proxy_entry_matches");
    if (!entry || !host) { WCUP_BRANCH(1); goto done_false; }
    WCUP_BRANCH(0);
    char tok[256];
    size_t i = 0;
    while (entry[i] == ' ' || entry[i] == '\t') i++;
    size_t j = strlen(entry);
    while (j > i && (entry[j-1]==' '||entry[j-1]=='\t')) j--;
    size_t k = 0;
    for (; i < j && k < sizeof(tok)-1; i++) tok[k++] = (char)tolower((unsigned char)entry[i]);
    tok[k] = '\0';
    if (!tok[0]) { WCUP_BRANCH(1); goto done_false; }
    WCUP_BRANCH(0);
    if (strcmp(tok, "*") == 0) { WCUP_BRANCH(1); goto done_true; }
    WCUP_BRANCH(0);

    /* split host:port */
    char th[256]; snprintf(th, sizeof(th), "%s", tok);
    char *colon = strrchr(th, ':');
    int tok_port = -1;
    if (colon && strchr(colon, '.') == NULL) { tok_port = atoi(colon+1); *colon = '\0'; }
    if (tok_port != -1 && port != -1 && tok_port != port) { WCUP_BRANCH(1); goto done_false; }
    WCUP_BRANCH(0);
    if (tok_port != -1 && port == -1) { WCUP_BRANCH(1); goto done_false; }
    WCUP_BRANCH(0);

    {
        char lhost[256], lh[256];
        lower_into(th, lhost, sizeof lhost);
        lower_into(host, lh, sizeof lh);

        int result;
        if (strncmp(lhost, "*.", 2) == 0) {
            WCUP_BRANCH(1);
            const char *suffix = lhost + 1; /* ".domain" */
            result = ends_with(lh, suffix);
        } else {
            WCUP_BRANCH(0);
            if (lhost[0] == '.') {
                WCUP_BRANCH(1);
                result = (strcmp(lh, lhost+1) == 0) || ends_with(lh, lhost);
            } else {
                WCUP_BRANCH(0);
                char dotted[260];
                snprintf(dotted, sizeof dotted, ".%s", lhost);
                result = (strcmp(lh, lhost) == 0) || ends_with(lh, dotted);
            }
        }
        char vbuf[16];
        snprintf(vbuf, sizeof vbuf, "%d", result);
        WCUP_VALSTR(vbuf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("no_proxy_entry_matches");
        return result;
    }

done_false:
    WCUP_VALSTR("0");
    { char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup); }
    WCUP_END("no_proxy_entry_matches");
    return 0;
done_true:
    WCUP_VALSTR("1");
    { char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup); }
    WCUP_END("no_proxy_entry_matches");
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s no_proxy_entry_matches <entry|host|port>...\n", argv[0]); return 2; }
    for (int a = 2; a < argc; a++) {
        char arg[1024];
        snprintf(arg, sizeof arg, "%s", argv[a]);
        char *sp1 = strchr(arg, '|');
        char *sp2 = sp1 ? strchr(sp1 + 1, '|') : NULL;
        if (!sp1 || !sp2) { fprintf(stderr, "bad input %s\n", argv[a]); return 2; }
        *sp1 = '\0'; *sp2 = '\0';
        no_proxy_entry_matches(arg, sp1 + 1, atoi(sp2 + 1));
    }
    return 0;
}
