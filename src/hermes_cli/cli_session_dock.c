/*
 * cli_session_dock.c - C home for upstream Hermes hermes_cli/cli_session_dock.py.
 * PoP: w15_clisession_7ffdfa_countdown @ hermes_cli/cli_session_dock.py:countdown
 * PoP: w15_clisession_7ffdfa__prompt_text @ hermes_cli/cli_session_dock.py:_prompt_text
 *
 * The two pure formatting helpers are ported. Manager state, the gateway queue
 * and the spinner-thread refresh bind live CLI objects and stay in Python.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_clisession_7ffdfa.h"

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *r = (char *)malloc(n + 1);
    if (r) memcpy(r, s, n + 1);
    return r;
}
static char *normalize_ws(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *r = (char *)malloc(n + 2);
    if (!r) return dupstr("");
    size_t k = 0;
    const char *p = s ? s : "";
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (k) r[k++] = ' ';
        while (*p && !isspace((unsigned char)*p)) r[k++] = *p++;
    }
    r[k] = '\0';
    return r;
}

struct json_t *w15_clisession_7ffdfa_countdown(double seconds) {
    double ceiled = ceil(seconds);
    long long left = (long long)ceiled;
    if (left < 0) left = 0;
    char b[64];
    if (left < 60) snprintf(b, sizeof(b), "%llds", left);
    else {
        long long total_min = (long long)ceil((double)left / 60.0);
        long long hours = total_min / 60, minutes = total_min % 60;
        if (hours) snprintf(b, sizeof(b), "%lldh%02lldm", hours, minutes);
        else snprintf(b, sizeof(b), "%lldm", minutes);
    }
    return json_string(b);
}

struct json_t *w15_clisession_7ffdfa_prompt_text(const struct json_t *item) {
    const char *text = "";
    if (item && item->type == JSON_STRING) {
        text = item->str_val;
    } else if (item && item->type == JSON_OBJECT) {
        json_t *d = json_obj_get(item, "display_text");
        if (d && d->type == JSON_STRING && d->str_val[0]) text = d->str_val;
        else {
            json_t *t = json_obj_get(item, "text");
            if (t && t->type == JSON_STRING && t->str_val[0]) text = t->str_val;
        }
    }
    char *n = normalize_ws(text);
    json_t *out = json_string(n);
    free(n);
    return out;
}
