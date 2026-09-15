/* gate: mcp_tool_resolve_client_cert — faithful traced copy of
 * src/tools/mcp_tool.c::mcp_tool_resolve_client_cert (that file pulls a wide
 * dependency web — base64/logger/registry headers — so it is not linked here;
 * libjson is linked for identical config-parse semantics). LIVE twin:
 * tools/mcp_tool.py::_resolve_client_cert.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: neither client_cert nor client_key -> none
 *   B2: client_cert is list/tuple form
 *     B3: client_key also set -> error (mutually exclusive)
 *     B4: len == 2
 *     B5: len == 3
 *       B6: passphrase is a string
 *   (string form)
 *   per _expand call, in evaluation order:
 *     E1: value is a non-blank string
 *     E2: expanded path isfile
 *   B7: string form with client_key set -> pair
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "hermes_json.h"
#include "watercup.h"

static bool wc_expand(wcup_t *wcp, json_t *val)
{
    bool ok_str = val && val->type == JSON_STRING && val->str_val &&
                  val->str_val[0] &&
                  strspn(val->str_val, " \t\r\n") != strlen(val->str_val);
    wcup_branch(wcp, !!(ok_str));                      /* E1 */
    if (!ok_str) return false;
    const char *p = val->str_val;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    char expanded[1024];
    if (p[0] == '~' && (p[1] == '\0' || p[1] == '/')) {
        const char *home = getenv("HOME");
        if (!home || !home[0]) home = "/";
        if (p[1] == '\0') snprintf(expanded, sizeof expanded, "%s", home);
        else snprintf(expanded, sizeof expanded, "%s%s", home, p + 1);
    } else {
        snprintf(expanded, sizeof expanded, "%s", p);
    }
    struct stat st;
    bool is_reg = stat(expanded, &st) == 0 && S_ISREG(st.st_mode);
    wcup_branch(wcp, !!(is_reg));                      /* E2 */
    return is_reg;
}

static void wc_resolve(json_t *cfg)
{
    WCUP_BEGIN("resolve_client_cert");
    json_t *raw_cert = cfg ? json_obj_get(cfg, "client_cert") : NULL;
    json_t *raw_key = cfg ? json_obj_get(cfg, "client_key") : NULL;
    bool cert_absent = !raw_cert || raw_cert->type == JSON_NULL;
    bool key_absent = !raw_key || raw_key->type == JSON_NULL;
    bool none = cert_absent && key_absent;
    WCUP_BRANCH(none);                        /* B1 */
    if (none) goto done;

    bool is_list = raw_cert && raw_cert->type == JSON_ARRAY;
    WCUP_BRANCH(is_list);                     /* B2 */
    if (is_list) {
        bool key_set = !key_absent;
        WCUP_BRANCH(!key_set);                /* B3 */
        if (key_set) goto done;
        size_t len = raw_cert->c.count;
        bool two = len == 2;
        WCUP_BRANCH(two);                     /* B4 */
        if (two) {
            if (!wc_expand(&_wc, json_get(raw_cert, 0))) goto done;
            wc_expand(&_wc, json_get(raw_cert, 1));
            goto done;
        }
        bool three = len == 3;
        WCUP_BRANCH(three);                   /* B5 */
        if (three) {
            json_t *pw = json_get(raw_cert, 2);
            bool pw_ok = pw && pw->type == JSON_STRING && pw->str_val;
            WCUP_BRANCH(pw_ok);               /* B6 */
            if (!pw_ok) goto done;
            if (!wc_expand(&_wc, json_get(raw_cert, 0))) goto done;
            wc_expand(&_wc, json_get(raw_cert, 1));
            goto done;
        }
        goto done;                            /* length error */
    }

    /* string form */
    if (!wc_expand(&_wc, raw_cert)) goto done;
    WCUP_BRANCH(!key_absent);                 /* B7 */
    if (!key_absent)
        wc_expand(&_wc, raw_key);
done:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("resolve_client_cert");
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s resolve_client_cert <config_json>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        json_t *cfg = json_parse(argv[i], NULL);
        wc_resolve(cfg);
        if (cfg) json_free(cfg);
    }
    return 0;
}
