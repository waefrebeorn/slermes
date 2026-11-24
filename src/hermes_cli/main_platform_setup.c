/*
 * main_platform_setup.c — C home for upstream Hermes hermes_cli/main_platform_setup.py.
 *
 * PoP: mainplatfo_yes_no @ hermes_cli/main_platform_setup.py:_yes_no
 * PoP: mainplatfo_whatsapp_mode @ hermes_cli/main_platform_setup.py:_whatsapp_choose_mode
 * PoP: mainplatfo_has_paired_identity @ hermes_cli/main_platform_setup.py:_whatsapp_has_paired_identity
 */
#include "noc_w18_mainplatfo_97fb7b.h"

#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (out) { if (n) memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

static char *py_strip_lower(const char *s) {
    static const char *WS = " \t\n\r\v\f";
    if (!s) return dupstr("");
    const char *a = s;
    while (*a && strchr(WS, *a)) a++;
    const char *b = a + strlen(a);
    while (b > a && strchr(WS, b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (out) {
        for (size_t i = 0; i < n; ++i) out[i] = (char)tolower((unsigned char)a[i]);
        out[n] = '\0';
    }
    return out;
}

int mainplatfo_yes_no(const char *response) {
    char *v = py_strip_lower(response);       /* NULL(EOF) -> "" -> no */
    int yes = v && (!strcmp(v, "y") || !strcmp(v, "yes"));
    free(v);
    return yes;
}

char *mainplatfo_whatsapp_mode(const char *current_mode, const char *choice) {
    if (current_mode && *current_mode) return dupstr(current_mode);
    if (!choice) return NULL;                  /* Ctrl-C/EOF => cancelled */
    char *v = py_strip_lower(choice);          /* Python strips; bot only on "1" */
    int bot = v && !strcmp(v, "1");
    free(v);
    return dupstr(bot ? "bot" : "self-chat");
}

int mainplatfo_has_paired_identity(const struct json_t *creds) {
    if (!creds || creds->type != JSON_OBJECT) return 0;
    json_t *me = json_obj_get(creds, "me");
    if (!me || me->type != JSON_OBJECT) return 0;
    json_t *id = json_obj_get(me, "id");
    return (id && id->type == JSON_STRING && id->str_val && *id->str_val) ? 1 : 0;
}
