/*
 * port_access_policy.c — pure helpers (see header).
 * OPTIN_TRUTHY = frozenset({"true","1","yes"}).
 */

#include "port_access_policy.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int apm_is_truthy(const char *v)
{
    static const char *const TRUTHY[] = {"true", "1", "yes"};
    const char *s = v ? v : "";
    size_t len = strlen(s), b = 0, e, j = 0, i;
    char low[64];
    while (b < len && isspace((unsigned char)s[b])) b++;
    e = len;
    while (e > b && isspace((unsigned char)s[e - 1])) e--;
    if (e - b >= sizeof(low)) return 0;
    for (i = b; i < e; i++)
        low[j++] = (char)tolower((unsigned char)s[i]);
    low[j] = '\0';
    for (i = 0; i < 3; i++)
        if (strcmp(low, TRUTHY[i]) == 0) return 1;
    return 0;
}

int apm_open_dm_opted_in(const char *env0_value, const char *env1_value)
{
    return apm_is_truthy(env0_value) || apm_is_truthy(env1_value);
}

int apm_entry_matches(const char *entries_json, const char *target)
{
    json_t *arr = entries_json ? json_parse(entries_json, NULL) : NULL;
    int found = 0;
    if (!target || !target[0]) {
        if (arr) json_free(arr);
        return 0;
    }
    if (arr && arr->type == JSON_ARRAY) {
        size_t i;
        for (i = 0; i < arr->c.count; i++) {
            const json_t *it = arr->c.items[i];
            if (it && it->type == JSON_STRING && it->str_val &&
                strcmp(it->str_val, target) == 0) {
                found = 1;
                break;
            }
        }
    }
    if (arr) json_free(arr);
    return found;
}

/* strip (leading/trailing whitespace); returns a malloc'd copy if non-empty,
 * or NULL if empty after strip. */
static char *apm_stripped(const char *s)
{
    const char *b = s ? s : "";
    size_t len = strlen(b), b0 = 0, e;
    char *o;
    while (b0 < len && isspace((unsigned char)b[b0])) b0++;
    e = len;
    while (e > b0 && isspace((unsigned char)b[e - 1])) e--;
    if (b0 == e) return NULL;
    o = malloc(e - b0 + 1);
    if (o) { memcpy(o, b + b0, e - b0); o[e - b0] = '\0'; }
    return o;
}

int apm_is_dm_allowed(const char *dm_policy, const char *sender_id,
                      const char *allow_json, int optin)
{
    const char *p = dm_policy ? dm_policy : "";
    if (strcmp(p, "allowlist") == 0)
        return apm_entry_matches(allow_json, sender_id);
    return (strcmp(p, "open") == 0) && optin;
}

int apm_is_dm_intake_allowed(const char *dm_policy, const char *sender_id,
                             const char *allow_json, int optin)
{
    const char *p = dm_policy ? dm_policy : "";
    char *pr = apm_stripped(sender_id);
    int r;
    if (!pr) return 0; /* blank principal never admitted */
    if (strcmp(p, "pairing") == 0) { free(pr); return 1; }
    r = apm_is_dm_allowed(p, pr, allow_json, optin);
    free(pr);
    return r;
}

int apm_is_group_allowed(const char *group_policy, const char *chat_id,
                         const char *group_allow_json)
{
    const char *p = group_policy ? group_policy : "";
    if (strcmp(p, "allowlist") == 0)
        return apm_entry_matches(group_allow_json, chat_id);
    return strcmp(p, "open") == 0;
}
