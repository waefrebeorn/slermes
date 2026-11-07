/*
 * port_micro_env.c — micro pure helpers (see header).
 */

#include "port_micro_env.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* PoP: tep_strip_env_keys @ agent/terminal_env_provider.py:strip_env_keys */
char *tep_strip_env_keys(void)
{
    return strdup("[]");
}

static char *micro_snprintf_dup(const char *fmt, const char *arg)
{
    size_t need = strlen(fmt) + strlen(arg) + 1;
    char *o = malloc(need);
    if (!o) return NULL;
    snprintf(o, need, fmt, arg);
    return o;
}

/* PoP: tep_env_description @ agent/terminal_env_provider.py:env_description */
char *tep_env_description(const char *display_name)
{
    return micro_snprintf_dup("a %s environment (likely Linux)",
                              display_name ? display_name : "");
}

/* PoP: apm_allow_all_env_names @ gateway/platforms/access_policy_mixin.py:_allow_all_env_names */
char *apm_allow_all_env_names(const char *prefix)
{
    const char *p = prefix ? prefix : "";
    size_t need = 128 + strlen(p);
    char *o = malloc(need);
    if (!o) return NULL;
    snprintf(o, need, "[\"GATEWAY_ALLOW_ALL_USERS\",\"%s_ALLOW_ALL_USERS\"]", p);
    return o;
}

/* PoP: ht_normalized @ gateway/host_topology.py:_normalized */
char *ht_normalized(const char *name)
{
    const char *s = name ? name : "";
    size_t len = strlen(s), b;
    size_t j = 0, i, end;
    char *out;
    if (s[0] == '\0')
        return strdup(""); /* Python: `if not name` */
    b = 0;
    while (b < len && isspace((unsigned char)s[b])) b++;
    end = len;
    while (end > b && isspace((unsigned char)s[end - 1])) end--;
    if (b == end)
        return strdup(""); /* Python normalize raises ValueError here; C can't */
    /* casefold == "default" (ASCII) -> "default" */
    if (len >= end - b && strncasecmp(s + b, "default", end - b) == 0 &&
        (size_t)(end - b) == strlen("default"))
        return strdup("default");
    out = malloc(end - b + 1);
    if (!out) return NULL;
    for (i = b, j = 0; i < end; i++)
        out[j++] = (char)tolower((unsigned char)s[i]);
    out[j] = '\0';
    return out;
}

/* ---- HostGatewayTopology.serves ---- */
/* PoP: hgt_serves @ gateway/host_topology.py:HostGatewayTopology.serves */

int hgt_serves(const char *profiles_json, const char *profile_name)
{
    json_t *arr = profiles_json ? json_parse(profiles_json, NULL) : NULL;
    char *wanted = ht_normalized(profile_name);
    int served = 0;
    if (!wanted || wanted[0] == '\0') { free(wanted); if (arr) json_free(arr); return 0; }
    if (arr && arr->type == JSON_ARRAY) {
        size_t i;
        for (i = 0; i < arr->c.count && !served; i++) {
            const json_t *it = arr->c.items[i];
            if (it && it->type == JSON_STRING) {
                char *np = ht_normalized(it->str_val);
                if (np && strcmp(np, wanted) == 0) served = 1;
                free(np);
            }
        }
    }
    if (arr) json_free(arr);
    free(wanted);
    return served;
}
