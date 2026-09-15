/* gate: message_sanitization_repair_tool_call_arguments — traced copy of
 * src/agent/message_sanitization_ports.c::msz_repair_tool_call_arguments
 * (the REAL ported file is linked via extra_src; libjson supplies the same
 * parse oracle json.loads uses). LIVE twin:
 * agent/message_sanitization.py::_repair_tool_call_arguments.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: empty/whitespace -> "{}"
 *   B2: literal "None" -> "{}"
 *   B3: strict parse (pass 0) ok -> reserialise
 *   LOOP trim (bounded 50):
 *     B4: parses -> break
 *     B5: trailing excess '}' chopped
 *     B6: trailing excess ']' chopped
 *   B7: final parse ok -> return repaired
 *   B8: escape pass changed + parses -> return escaped
 *   B9: fallback "{}"
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "watercup.h"

static bool wc_parses(const char *s)
{
    if (!s) return false;
    json_t *j = json_parse(s, NULL);
    if (!j) return false;
    json_free(j);
    return true;
}

static char *wc_repair(const char *args_json)
{
    WCUP_BEGIN("repair_tool_call_arguments");
    char *raw_stripped;
    if (!args_json) {
        raw_stripped = strdup("");
    } else {
        const char *b = args_json;
        while (*b && isspace((unsigned char)*b)) b++;
        const char *e = args_json + strlen(args_json);
        while (e > b && isspace((unsigned char)e[-1])) e--;
        raw_stripped = strndup(b, (size_t)(e - b));
    }
    bool empty = !raw_stripped[0];
    WCUP_BRANCH(empty);                       /* B1 */
    if (empty) { free(raw_stripped); goto finalize; }
    bool is_none = strcmp(raw_stripped, "None") == 0;
    WCUP_BRANCH(is_none);                     /* B2 */
    if (is_none) { free(raw_stripped); goto finalize; }

    {
        json_t *parsed = json_parse(raw_stripped, NULL);
        bool ok = parsed != NULL;
        WCUP_BRANCH(ok);                      /* B3 */
        if (ok) {
            char *out = json_serialize(parsed);
            json_free(parsed);
            free(raw_stripped);
            goto finalize;
        }
    }

    char *fixed = strdup(raw_stripped);
    {   /* strip trailing commas before } or ] */
        size_t n = strlen(fixed), w = 0;
        for (size_t i = 0; i < n; ) {
            if (fixed[i] == ',') {
                size_t k = i + 1;
                while (k < n && isspace((unsigned char)fixed[k])) k++;
                if (k < n && (fixed[k] == '}' || fixed[k] == ']')) { i++; continue; }
            }
            fixed[w++] = fixed[i++];
        }
        fixed[w] = '\0';
    }
    {   /* close unclosed structures */
        int open_curly = 0, open_bracket = 0;
        bool in_str = false;
        for (const char *p = fixed; *p; p++) {
            if (*p == '"' && !(p > fixed && p[-1] == '\\')) in_str = !in_str;
            if (!in_str) {
                if (*p == '{') open_curly++;
                else if (*p == '}') open_curly--;
                else if (*p == '[') open_bracket++;
                else if (*p == ']') open_bracket--;
            }
        }
        size_t fl = strlen(fixed);
        if (open_curly > 0 || open_bracket > 0) {
            fixed = realloc(fixed, fl + (size_t)(open_curly > 0 ? open_curly : 0) +
                                    (size_t)(open_bracket > 0 ? open_bracket : 0) + 1);
            for (int i = 0; i < open_curly; i++) fixed[fl++] = '}';
            for (int i = 0; i < open_bracket; i++) fixed[fl++] = ']';
            fixed[fl] = '\0';
        }
    }
    for (int _ = 0; _ < 50; _++) {            /* trim loop */
        WCUP_LOOP_TICK();
        bool parses = wc_parses(fixed);
        WCUP_BRANCH(parses);                  /* B4 */
        if (parses) break;
        size_t fl = strlen(fixed);
        long c_curly = 0, o_curly = 0, c_brack = 0, o_brack = 0;
        for (const char *p = fixed; *p; p++) {
            if (*p == '{') o_curly++;
            else if (*p == '}') c_curly++;
            else if (*p == '[') o_brack++;
            else if (*p == ']') c_brack++;
        }
        bool chop_curly = fl > 0 && fixed[fl - 1] == '}' && c_curly > o_curly;
        WCUP_BRANCH(chop_curly);              /* B5 */
        if (chop_curly) { fixed[fl - 1] = '\0'; continue; }
        bool chop_brack = fl > 0 && fixed[fl - 1] == ']' && c_brack > o_brack;
        WCUP_BRANCH(chop_brack);              /* B6 */
        if (chop_brack) { fixed[fl - 1] = '\0'; continue; }
        break;
    }

    {
        bool ok = wc_parses(fixed);
        WCUP_BRANCH(ok);                      /* B7 */
        if (ok) {
            free(raw_stripped);
            free(fixed);
            goto finalize;
        }
    }

    {   /* escape pass */
        size_t cap = strlen(fixed) * 6 + 8;
        char *escaped = malloc(cap);
        if (escaped) {
            size_t w = 0;
            bool in_str = false;
            for (size_t i = 0; fixed[i]; ) {
                char ch = fixed[i];
                if (in_str) {
                    unsigned char c = (unsigned char)ch;
                    if (ch == '\\' && fixed[i + 1]) {
                        escaped[w++] = ch; escaped[w++] = fixed[i + 1];
                        i += 2;
                        continue;
                    }
                    if (ch == '"') { in_str = false; escaped[w++] = ch; i++; continue; }
                    if (c < 0x20) {
                        w += (size_t)sprintf(escaped + w, "\\u%04x", c);
                        i++;
                        continue;
                    }
                    escaped[w++] = ch; i++;
                    continue;
                }
                if (ch == '"') in_str = true;
                escaped[w++] = ch; i++;
            }
            escaped[w] = '\0';
            bool use = strcmp(escaped, fixed) != 0 && wc_parses(escaped);
            WCUP_BRANCH(use);                 /* B8 */
            if (use) {
                free(fixed);
                free(raw_stripped);
                free(escaped);
                goto finalize;
            }
            free(escaped);
        } else {
            WCUP_BRANCH(false);               /* B8 (alloc fail: skip) */
        }
    }

    WCUP_BRANCH(true);                        /* B9 fallback */
    free(fixed);
    free(raw_stripped);

finalize:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("repair_tool_call_arguments");
        return strdup("{}");
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s repair_tool_call_arguments <raw_args>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++)
        wc_repair(argv[i]);
    return 0;
}
