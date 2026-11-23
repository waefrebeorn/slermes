/*
 * setup_terminal.c — C home for upstream Hermes hermes_cli/setup_terminal.py.
 * PoP: w12_setuptermi_48d5ea_report_binary @ hermes_cli/setup_terminal.py:_report_binary
 * PoP: w12_setuptermi_48d5ea_existing_secret_keeps @ hermes_cli/setup_terminal.py:_existing_secret_keeps
 * PoP: w12_setuptermi_48d5ea_prompt_secret_env @ hermes_cli/setup_terminal.py:_prompt_secret_env
 *
 * The wizard's decision helpers are pure once the _setup interactions are
 * injected; the harness feeds those injected values from each fixture line.
 */
#include "hermes_json.h"
#include "noc_w12_setuptermi_48d5ea.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *call2(const char *kind, const char *text)
{
    json_t *o = json_object();
    json_set(o, "kind", json_string(kind));
    json_set(o, "text", json_string(text));
    return o;
}

struct json_t *w12_setuptermi_48d5ea_report_binary(const char *found,
                                                   const char *missing,
                                                   const char *install_hint,
                                                   const char *found_prefix)
{
    json_t *arr = json_array();
    char buf[2048];

    if (!found_prefix) found_prefix = "";
    if (found && *found) {
        snprintf(buf, sizeof(buf), "%s%s", found_prefix, found);
        json_append(arr, call2("info", buf));
    } else {
        json_append(arr, call2("warning", missing ? missing : ""));
        json_append(arr, call2("info", install_hint ? install_hint : ""));
    }
    return arr;
}

struct json_t *w12_setuptermi_48d5ea_existing_secret_keeps(const char *env_value,
                                                           const char *label,
                                                           const char *question,
                                                           int prompt_answer)
{
    json_t *out = json_object();
    (void)question;  /* only forwarded to the injected prompt_yes_no */

    if (!env_value || !*env_value) {
        json_set(out, "keeps", json_bool(false));
        json_set(out, "info", json_null());
        return out;
    }
    {
        char buf[1024];
        snprintf(buf, sizeof(buf), "  %s: already configured", label ? label : "");
        json_set(out, "keeps", json_bool(prompt_answer ? false : true));
        json_set(out, "info", json_string(buf));
    }
    return out;
}

struct json_t *w12_setuptermi_48d5ea_prompt_secret_env(const char *value,
                                                       const char *label,
                                                       const char *env_var,
                                                       const char *confirm_msg)
{
    json_t *arr = json_array();
    (void)label;     /* only forwarded to the injected prompt() */

    if (value && *value) {
        json_t *o = json_object();
        json_set(o, "call", json_string("save_env_value"));
        json_set(o, "env_var", json_string(env_var ? env_var : ""));
        json_set(o, "value", json_string(value));
        json_append(arr, o);
        if (confirm_msg && *confirm_msg) {
            o = json_object();
            json_set(o, "call", json_string("print_success"));
            json_set(o, "text", json_string(confirm_msg));
            json_append(arr, o);
        }
    }
    return arr;
}
