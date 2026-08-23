/* gate: clarify_tool_flatten_choice — traced copy of the REAL C port
 * src/tools/clarify_tool.c::clarify_flatten_choice
 * (PoP: tools/clarify_tool.py:_flatten_choice).
 * LIVE twin: tools/clarify_tool.py::_flatten_choice.
 * Input: one JSON-encoded choice value per argv element.
 *
 * Ladder (twin.py mirrors EXACTLY):
 *   B1: NULL input -> ""
 *   B2: JSON parse failed / no node -> ""
 *   B3: string -> strip(copy)
 *   B4: object -> LOOP 4 canonical keys (label/description/text/title),
 *       B5 usable-string hit breaks; B6 nothing found -> ""
 *   B7: array -> LOOP elements, B8 usable string joins with " "
 *   else scalar: B9 number ("%g") else ""
 *   final strip; result valstr'd.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

static void str_strip_inplace(char *s) {
    char *a = s, *b = s + strlen(s);
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)*(b - 1))) b--;
    if (a != s) memmove(s, a, b - a);
    s[b - a] = '\0';
}

/* minimal JSON scanner: returns malloc'd unquoted string for a top-level
 * JSON string, or the raw text for objects/arrays; NULL on garbage. */
static char *slurp(const char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (!*s) return NULL;
    if (*s == '"') {
        char *out = malloc(strlen(s) + 1);
        size_t o = 0;
        s++;
        while (*s && *s != '"') {
            if (*s == '\\' && s[1]) {
                s++;
                switch (*s) {
                case 'n': out[o++] = '\n'; break;
                case 't': out[o++] = '\t'; break;
                default: out[o++] = *s; break;
                }
                s++;
            } else out[o++] = *s++;
        }
        out[o] = '\0';
        return out;
    }
    return strdup(s);
}

/* extract the value of `key` from a flat top-level object text */
static int obj_get_string(const char *obj, const char *key, char *out, size_t cap) {
    char needle[128];
    snprintf(needle, sizeof needle, "\"%s\"", key);
    const char *p = strstr(obj, needle);
    if (!p) return 0;
    p = strchr(p + strlen(needle), ':');
    if (!p) return 0;
    p++;
    while (*p == ' ') p++;
    if (*p != '"') return 0;
    p++;
    size_t o = 0;
    while (*p && *p != '"' && o + 1 < cap) out[o++] = *p++;
    out[o] = '\0';
    return 1;
}

static int split_top_array(const char *arr, char elems[][256], int max) {
    const char *p = strchr(arr, '[');
    if (!p) return 0;
    p++;
    int n = 0;
    while (*p && n < max) {
        while (*p == ' ' || *p == ',') p++;
        if (*p == ']') break;
        if (*p == '"') {
            p++;
            size_t o = 0;
            while (*p && *p != '"' && o + 1 < 256) elems[n][o++] = *p++;
            elems[n][o] = '\0';
            if (*p == '"') p++;
            n++;
        } else if (*p) {
            /* non-string element: unusable in the traced subset */
            snprintf(elems[n], 256, "%c", *p);
            n++;
            p++;
        } else break;
    }
    return n;
}

#define IS_STR(s) ((s)[0] != '\0')

static void run_one(const char *choice_json) {
    WCUP_BEGIN("flatten_choice");
    char out[1024]; out[0] = '\0';
    if (!choice_json) {                              /* B1 */
        WCUP_BRANCH(1);
        out[0] = '\0';
        goto done;
    }
    WCUP_BRANCH(0);
    {
        char *text = slurp(choice_json);
        /* classify: string / object / array / other by first char */
        const char *q = choice_json;
        while (*q == ' ') q++;
        char kind = *q == '"' ? 's' : (*q == '{' ? 'o' : (*q == '[' ? 'a' : '?'));
        if (!text || kind == '?') {                  /* B2 */
            WCUP_BRANCH(1);
            out[0] = '\0';
            if (text) free(text);
            goto done;
        }
        WCUP_BRANCH(0);
        if (kind == 's') {                           /* B3 */
            WCUP_BRANCH(1);
            snprintf(out, sizeof out, "%s", text);
        } else {
            WCUP_BRANCH(0);
            if (kind == 'o') {                       /* B4 */
                WCUP_BRANCH(1);
                static const char *const KEYS[] = {"label", "description", "text", "title"};
                int found = 0;
                for (int i = 0; i < 4; i++) {        /* LOOP keys */
                    WCUP_LOOP_TICK();
                    char v[512];
                    if (obj_get_string(text, KEYS[i], v, sizeof v) &&
                        IS_STR(v)) {                 /* B5 */
                        WCUP_BRANCH(1);
                        snprintf(out, sizeof out, "%s", v);
                        found = 1;
                        break;
                    }
                    WCUP_BRANCH(0);
                }
                if (!found) {                        /* B6 */
                    WCUP_BRANCH(1);
                    out[0] = '\0';
                } else WCUP_BRANCH(0);
            } else {                                 /* B7 (array) */
                WCUP_BRANCH(1);
                char elems[16][256];
                int n = split_top_array(text, elems, 16);
                size_t len = 0;
                for (int i = 0; i < n; i++) {        /* LOOP elems */
                    WCUP_LOOP_TICK();
                    if (IS_STR(elems[i])) {          /* B8 */
                        WCUP_BRANCH(1);
                        if (len) out[len++] = ' ';
                        size_t add = strlen(elems[i]);
                        if (len + add < sizeof out) {
                            memcpy(out + len, elems[i], add);
                            len += add;
                            out[len] = '\0';
                        }
                    } else WCUP_BRANCH(0);
                }
            }
        }
        free(text);
    }
done:
    str_strip_inplace(out);
    WCUP_VALSTR(out);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("flatten_choice");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s flatten_choice <json-choice>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
