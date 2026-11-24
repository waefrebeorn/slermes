/*
 * gateway_env.c -- C11 port of plugins/platforms/sms/gateway_env.py.
 *
 * The module is data plus two tiny behaviours: the EnvCred activation gate and
 * the `_api_key` tail that stores the Twilio auth token on the platform config.
 * Both are real here; the env value and the token are injected so no fixture
 * has to mutate the process environment.
 *
 * PoP: w22_gatewayenv_8633ba_enable_credentials @ plugins/platforms/sms/gateway_env.py:ENV_ENABLE_CREDENTIALS
 * PoP: w22_gatewayenv_8633ba_overrides @ plugins/platforms/sms/gateway_env.py:ENV_OVERRIDES
 * PoP: w22_gatewayenv_8633ba_authz @ plugins/platforms/sms/gateway_env.py:AUTHZ
 * PoP: w22_gatewayenv_8633ba_authz_get @ plugins/platforms/sms/gateway_env.py:AUTHZ
 * PoP: w22_gatewayenv_8633ba_cred_active @ plugins/platforms/sms/gateway_env.py:ENV_OVERRIDES
 * PoP: w22_gatewayenv_8633ba_api_key @ plugins/platforms/sms/gateway_env.py:_api_key
 */
#include "hermes_json.h"
#include "noc_w22_gatewayenv_8633ba.h"

#include <stddef.h>
#include <string.h>

static const char *const SMS_PLATFORM = "sms";
static const char *const SMS_SID_ENV = "TWILIO_ACCOUNT_SID";
static const char *const SMS_TOKEN_ENV = "TWILIO_AUTH_TOKEN";
static const char *const SMS_HOME_ENV = "SMS_HOME_CHANNEL";

json_t *w22_gatewayenv_8633ba_enable_credentials(void) {
    json_t *out = json_array();
    json_append(out, json_string(SMS_SID_ENV));
    return out;
}

static json_t *ge_strings(const char *const *values) {
    json_t *out = json_array();
    size_t i;
    for (i = 0; values[i]; i++) json_append(out, json_string(values[i]));
    return out;
}

json_t *w22_gatewayenv_8633ba_overrides(void) {
    static const char *const CREDS[] = {SMS_SID_ENV, NULL};
    static const char *const EMPTY[] = {NULL};
    json_t *out = json_array();
    json_t *cred = json_object();
    json_t *home = json_object();

    json_set(cred, "kind", json_string("cred"));
    json_set(cred, "platform", json_string(SMS_PLATFORM));
    json_set(cred, "creds", ge_strings(CREDS));
    json_set(cred, "token", json_null());
    json_set(cred, "fixed", ge_strings(EMPTY));
    json_set(cred, "optional", ge_strings(EMPTY));
    json_set(cred, "optional_stripped", ge_strings(EMPTY));
    json_set(cred, "warn_missing", json_null());
    json_set(cred, "then", json_string("_api_key"));
    json_set(cred, "home", json_null());
    json_set(cred, "home_strip", json_bool(0));
    json_append(out, cred);

    json_set(home, "kind", json_string("home"));
    json_set(home, "platform", json_string(SMS_PLATFORM));
    json_set(home, "env_base", json_string(SMS_HOME_ENV));
    json_set(home, "strip", json_bool(0));
    json_append(out, home);
    return out;
}

json_t *w22_gatewayenv_8633ba_authz(void) {
    json_t *out = json_object();
    json_set(out, "allowed_users_env", json_string("SMS_ALLOWED_USERS"));
    json_set(out, "allow_all_env", json_string("SMS_ALLOW_ALL_USERS"));
    json_set(out, "allowlist_suppresses_pairing", json_bool(1));
    return out;
}

json_t *w22_gatewayenv_8633ba_authz_get(const json_t *key) {
    const char *name;
    if (!key || key->type != JSON_STRING || !key->str_val) return json_null();
    name = key->str_val;
    if (!strcmp(name, "allowed_users_env")) return json_string("SMS_ALLOWED_USERS");
    if (!strcmp(name, "allow_all_env")) return json_string("SMS_ALLOW_ALL_USERS");
    if (!strcmp(name, "allowlist_suppresses_pairing")) return json_bool(1);
    return json_null();
}

int w22_gatewayenv_8633ba_cred_active(const char *account_sid) {
    /* env_first(group) is the value or "", and all(...) is a truthiness test. */
    return (account_sid && account_sid[0]) ? 1 : 0;
}

json_t *w22_gatewayenv_8633ba_api_key(const char *token) {
    /* _getenv_str never returns None: an unset variable reads "". */
    return json_string(token ? token : "");
}
