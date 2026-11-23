/*
 * profile_cmd.c — C home for upstream Hermes hermes_cli/profile_cmd.py.
 * PoP: w8_profilecmd_e127ec_confirm @ hermes_cli/profile_cmd.py:_confirm
 * PoP: w8_profilecmd_e127ec_is_active @ hermes_cli/profile_cmd.py:_is_active
 * PoP: w8_profilecmd_e127ec_action_keys @ hermes_cli/profile_cmd.py:PROFILE_ACTIONS
 * PoP: w8_profilecmd_e127ec_has_action @ hermes_cli/profile_cmd.py:cmd_profile
 */
#include "noc_w8_profilecmd_e127ec.h"
#include "hermes_json.h"
#include <ctype.h>
#include <string.h>

static const char *const ACTS[] = {
    "alias", "create", "delete", "describe", "export", "import", "info",
    "install", "list", "migrate-identity", "purge-identity", "rename",
    "show", "update", "use"
};
#define N_ACTS ((int)(sizeof(ACTS) / sizeof(ACTS[0])))

int w8_profilecmd_e127ec_confirm(const char *answer) {
    if (!answer) return 0;
    while (*answer && isspace((unsigned char)*answer)) answer++;
    size_t n = strlen(answer);
    while (n && isspace((unsigned char)answer[n - 1])) n--;
    if (n >= 64) return 0;
    char buf[64];
    for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)answer[i]);
    buf[n] = '\0';
    return (!strcmp(buf, "y") || !strcmp(buf, "yes")) ? 1 : 0;
}

int w8_profilecmd_e127ec_is_active(const char *name, int is_default, const char *active) {
    if (!name) name = "";
    if (!active) active = "";
    return (!strcmp(name, active) || (!strcmp(active, "default") && is_default)) ? 1 : 0;
}

struct json_t *w8_profilecmd_e127ec_action_keys(void) {
    json_t *out = json_array();
    for (int i = 0; i < N_ACTS; i++) json_append(out, json_string(ACTS[i]));
    return out;
}

int w8_profilecmd_e127ec_has_action(const char *action) {
    if (!action) return 0;
    for (int i = 0; i < N_ACTS; i++) if (!strcmp(ACTS[i], action)) return 1;
    return 0;
}
