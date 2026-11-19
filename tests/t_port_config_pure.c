/* t_port_config_pure.c -- harness for the config.py pure-gap closure batch.
 *
 * Emits one JSON line per case: {"func":<c_name>,"in":{...},"out":<value>}
 * (plus "errtype"/"err" when the C port reports a Python exception).
 * oracle_config_pure.py replays "in" against the LIVE hermes_cli.config defs.
 * Includes the port .c by bare name (runner puts src/hermes_cli on -I).
 */
#include "config_py_pure.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct cp_case { const char *func; const char *in; };

static const struct cp_case CASES[] = {
    /* _split_key_path */
    { "config_py_split_key_path", "{\"key\":\"a.b.c\"}" },
    { "config_py_split_key_path", "{\"key\":\"\"}" },
    { "config_py_split_key_path", "{\"key\":\"a.\"}" },
    { "config_py_split_key_path", "{\"key\":\"qwen3\\\\.5-397b-wafer.x\"}" },
    { "config_py_split_key_path", "{\"key\":\"a\\\\b.c\"}" },
    { "config_py_split_key_path", "{\"key\":\".a\"}" },
    /* _greedy_literal_match */
    { "config_py_greedy_literal_match", "{\"container\":{\"models\":{\"grok-4.6\":{\"ctx\":1}}},\"parts\":[\"models\",\"grok-4.6\",\"ctx\"]}" },
    { "config_py_greedy_literal_match", "{\"container\":{\"grok-4.6\":{\"ctx\":1}},\"parts\":[\"grok-4.6\",\"ctx\"]}" },
    { "config_py_greedy_literal_match", "{\"container\":{\"a.b.c\":1},\"parts\":[\"a\",\"b\",\"c\"]}" },
    { "config_py_greedy_literal_match", "{\"container\":{\"a\":1},\"parts\":[\"x\",\"y\"]}" },
    { "config_py_greedy_literal_match", "{\"container\":{\"a\":1},\"parts\":[]}" },
    { "config_py_greedy_literal_match", "{\"container\":[1,2],\"parts\":[\"0\"]}" },
    /* _phantom_sibling */
    { "config_py_phantom_sibling", "{\"container\":{\"grok-4.5\":1},\"part\":\"grok-4\"}" },
    { "config_py_phantom_sibling", "{\"container\":{\"grok-4\":1},\"part\":\"grok-4\"}" },
    { "config_py_phantom_sibling", "{\"container\":{\"a.b\":1,\"a.c\":2},\"part\":\"a\"}" },
    { "config_py_phantom_sibling", "{\"container\":{},\"part\":\"a\"}" },
    { "config_py_phantom_sibling", "{\"container\":[1],\"part\":\"0\"}" },
    /* _locate_nested */
    { "config_py_locate_nested", "{\"config\":{\"a\":{\"b\":1}},\"parts\":[\"a\",\"b\"]}" },
    { "config_py_locate_nested", "{\"config\":{\"models\":{\"grok-4.6\":{\"ctx\":7}}},\"parts\":[\"models\",\"grok-4.6\",\"ctx\"]}" },
    { "config_py_locate_nested", "{\"config\":{\"a\":[10,20]},\"parts\":[\"a\",\"1\"]}" },
    { "config_py_locate_nested", "{\"config\":{\"a\":[10,20]},\"parts\":[\"a\",\"-1\"]}" },
    { "config_py_locate_nested", "{\"config\":{\"a\":[10,20]},\"parts\":[\"a\",\"x\"]}" },
    { "config_py_locate_nested", "{\"config\":{\"a\":1},\"parts\":[\"a\",\"b\"]}" },
    { "config_py_locate_nested", "{\"config\":{},\"parts\":[]}" },
    /* _validate_entry_list */
    { "config_py_validate_entry_list", "{\"entries\":[{\"name\":\"p\",\"base_url\":\"u\"}],\"label\":\"custom_providers\",\"fields\":[[\"name\",\"hint-n\"],[\"base_url\",\"hint-b\"]],\"severity\":\"warning\",\"message\":\"custom_providers[{i}] is not a dict (got {type})\",\"hint\":\"Each entry should have at minimum: name, base_url\"}" },
    { "config_py_validate_entry_list", "{\"entries\":[{\"name\":\"p\"}],\"label\":\"custom_providers\",\"fields\":[[\"name\",\"hint-n\"],[\"base_url\",\"hint-b\"]],\"severity\":\"warning\",\"message\":\"m[{i}] {type}\",\"hint\":\"h\"}" },
    { "config_py_validate_entry_list", "{\"entries\":[\"x\",5,null,true],\"label\":\"L\",\"fields\":[],\"severity\":\"error\",\"message\":\"L[{i}] is not a dict (got {type})\",\"hint\":\"hint\"}" },
    { "config_py_validate_entry_list", "{\"entries\":[{\"name\":\"\",\"base_url\":0}],\"label\":\"L\",\"fields\":[[\"name\",\"hn\"],[\"base_url\",\"hb\"]],\"severity\":\"warning\",\"message\":\"m\",\"hint\":\"h\"}" },
    { "config_py_validate_entry_list", "{\"entries\":[],\"label\":\"L\",\"fields\":[],\"severity\":\"warning\",\"message\":\"m\",\"hint\":\"h\"}" },
    /* _validate_custom_providers */
    { "config_py_validate_custom_providers", "{\"cp\":null}" },
    { "config_py_validate_custom_providers", "{\"cp\":{\"base_url\":\"u\",\"api_key\":\"k\",\"other\":1}}" },
    { "config_py_validate_custom_providers", "{\"cp\":[{\"name\":\"p\",\"base_url\":\"u\"},{\"name\":\"q\"}]}" },
    { "config_py_validate_custom_providers", "{\"cp\":\"text\"}" },
    { "config_py_validate_custom_providers", "{\"cp\":[]}" },
    { "config_py_validate_custom_providers", "{\"cp\":[5]}" },
    /* _validate_fallback_model */
    { "config_py_validate_fallback_model", "{\"fb\":null}" },
    { "config_py_validate_fallback_model", "{\"fb\":{\"provider\":\"openrouter\",\"model\":\"m\"}}" },
    { "config_py_validate_fallback_model", "{\"fb\":{\"provider\":\"openrouter\"}}" },
    { "config_py_validate_fallback_model", "{\"fb\":{}}" },
    { "config_py_validate_fallback_model", "{\"fb\":[{\"provider\":\"p\",\"model\":\"m\"},\"x\"]}" },
    { "config_py_validate_fallback_model", "{\"fb\":\"s\"}" },
    /* _strip_dotted_keys */
    { "config_py_strip_dotted_keys", "{\"cfg\":{\"a\":{\"b\":1,\"c\":2},\"d\":3},\"dotted_keys\":[\"a.b\",\"d\",\"x.y\"]}" },
    { "config_py_strip_dotted_keys", "{\"cfg\":{\"a.b\":1},\"dotted_keys\":[\"a.b\"]}" },
    { "config_py_strip_dotted_keys", "{\"cfg\":{\"a\":{\"b\":1}},\"dotted_keys\":[\"a.b\",\"a.b\"]}" },
    { "config_py_strip_dotted_keys", "{\"cfg\":{},\"dotted_keys\":[\"a\"]}" },
    { "config_py_strip_dotted_keys", "{\"cfg\":{\"a\":null},\"dotted_keys\":[\"a\"]}" },
    /* _is_non_env_secret_ref */
    { "config_py_is_non_env_secret_ref", "{\"ref\":\"bitwarden:FOO\"}" },
    { "config_py_is_non_env_secret_ref", "{\"ref\":\"env:FOO\"}" },
    { "config_py_is_non_env_secret_ref", "{\"ref\":\"Env:FOO\"}" },
    { "config_py_is_non_env_secret_ref", "{\"ref\":\"FOO\"}" },
    { "config_py_is_non_env_secret_ref", "{\"ref\":\":FOO\"}" },
    { "config_py_is_non_env_secret_ref", "{\"ref\":\"vault-a_b0:x\"}" },
    /* _canonicalize_config */
    { "config_py_canonicalize_config", "{\"config\":{\"max_turns\":5}}" },
    { "config_py_canonicalize_config", "{\"config\":{}}" },
    { "config_py_canonicalize_config", "{\"config\":{\"agent\":null}}" },
    { "config_py_canonicalize_config", "{\"config\":{\"model\":\"m\",\"provider\":\"p\"}}" },
    { "config_py_canonicalize_config", "{\"config\":{\"model\":{\"name\":\"n\",\"provider\":2}}}" },
    { "config_py_canonicalize_config", "{\"config\":{\"model\":{\"default\":{\"model\":\"m\",\"provider\":\"p\"}}}}" },
    { "config_py_canonicalize_config", "{\"config\":{\"agent\":\"x\"}}" },
    /* _ensure_dict */
    { "config_py_ensure_dict", "{\"parent\":{},\"key\":\"platforms\"}" },
    { "config_py_ensure_dict", "{\"parent\":{\"a\":{\"b\":1}},\"key\":\"a\"}" },
    { "config_py_ensure_dict", "{\"parent\":{\"a\":5},\"key\":\"a\"}" },
    { "config_py_ensure_dict", "{\"parent\":{\"a\":null},\"key\":\"a\"}" },
    /* _commented_sections_for_save */
    { "config_py_commented_sections_for_save", "{\"normalized\":{}}" },
    { "config_py_commented_sections_for_save", "{\"normalized\":{\"security\":{\"redact_secrets\":false},\"fallback_model\":{\"provider\":\"p\",\"model\":\"m\"}}}" },
    { "config_py_commented_sections_for_save", "{\"normalized\":{\"security\":{\"redact_secrets\":null}}}" },
    { "config_py_commented_sections_for_save", "{\"normalized\":{\"fallback_model\":[{\"provider\":\"p\",\"model\":\"m\"}]}}" },
    { "config_py_commented_sections_for_save", "{\"normalized\":{\"security\":\"x\"}}" },
    { "config_py_commented_sections_for_save", "{\"normalized\":{\"security\":{},\"fallback_model\":null}}" },
    /* _is_secret_config_key */
    { "config_py_is_secret_config_key", "{\"key\":\"api_key\"}" },
    { "config_py_is_secret_config_key", "{\"key\":\"mcp_servers.x.headers.X-API-Key\"}" },
    { "config_py_is_secret_config_key", "{\"key\":\"token_count\"}" },
    { "config_py_is_secret_config_key", "{\"key\":\"TERMINAL_SSH_HOST\"}" },
    { "config_py_is_secret_config_key", "{\"key\":\"OPENAI_API_KEY\"}" },
    { "config_py_is_secret_config_key", "{\"key\":\"model.default\"}" },
    { "config_py_is_secret_config_key", "{\"key\":\"FAL_KEY\"}" },
    /* _looks_structured_value */
    { "config_py_looks_structured_value", "{\"value\":\"[1,2]\"}" },
    { "config_py_looks_structured_value", "{\"value\":\" -5\"}" },
    { "config_py_looks_structured_value", "{\"value\":\"a: b\\nc: d\"}" },
    { "config_py_looks_structured_value", "{\"value\":\"key:\\nother\"}" },
    { "config_py_looks_structured_value", "{\"value\":\"- item\\nx\"}" },
    { "config_py_looks_structured_value", "{\"value\":\"plain text\"}" },
    { "config_py_looks_structured_value", "{\"value\":\"# comment: x\\ny: z\"}" },
    { "config_py_looks_structured_value", "{\"value\":\"no newline: yes\"}" },
    /* _expected_container_type */
    { "config_py_expected_container_type", "{\"key\":\"custom_providers\",\"user_config\":{}}" },
    { "config_py_expected_container_type", "{\"key\":\"model.aliases\",\"user_config\":{}}" },
    { "config_py_expected_container_type", "{\"key\":\"agent.max_turns\",\"user_config\":{}}" },
    { "config_py_expected_container_type", "{\"key\":\"providers\",\"user_config\":{}}" },
    { "config_py_expected_container_type", "{\"key\":\"display\",\"user_config\":{\"display\":{\"x\":1}}}" },
    { "config_py_expected_container_type", "{\"key\":\"terminal.backend\",\"user_config\":{\"terminal\":{\"backend\":\"local\"}}}" },
    { "config_py_expected_container_type", "{\"key\":\"plugins.enabled\",\"user_config\":{}}" },
    { "config_py_expected_container_type", "{\"key\":\"custom_providers\",\"user_config\":{\"custom_providers\":\"x\"}}" },
    /* _refuse_container_type_mismatch */
    { "config_py_refuse_container_type_mismatch", "{\"key\":\"agent.disabled_toolsets\",\"value\":\"foo\",\"user_config\":{},\"force\":false}" },
    { "config_py_refuse_container_type_mismatch", "{\"key\":\"custom_providers\",\"value\":\"x\",\"user_config\":{},\"force\":false}" },
    { "config_py_refuse_container_type_mismatch", "{\"key\":\"custom_providers\",\"value\":[{\"name\":\"p\"}],\"user_config\":{},\"force\":false}" },
    { "config_py_refuse_container_type_mismatch", "{\"key\":\"providers\",\"value\":\"x\",\"user_config\":{},\"force\":true}" },
    { "config_py_refuse_container_type_mismatch", "{\"key\":\"providers\",\"value\":\"x\",\"user_config\":{},\"force\":false}" },
    { "config_py_refuse_container_type_mismatch", "{\"key\":\"agent.max_turns\",\"value\":\"5\",\"user_config\":{},\"force\":false}" },
    { "config_py_refuse_container_type_mismatch", "{\"key\":\"plugins.enabled\",\"value\":5,\"user_config\":{},\"force\":false}" },
    /* install-method shims */
    { "config_py_is_uv_tool_install", "{}" },
    { "config_py_is_uv_tool_install", "{\"note\":\"second\"}" },
    { "config_py_is_uv_tool_install", "{\"note\":\"third\"}" },
    { "config_py_is_unsupported_install_method", "{\"method\":\"pip\"}" },
    { "config_py_is_unsupported_install_method", "{\"method\":\"docker\"}" },
    { "config_py_is_unsupported_install_method", "{\"method\":\"\"}" },
    { "config_py_format_unsupported_install_warning", "{\"method\":\"pip\"}" },
    { "config_py_format_unsupported_install_warning", "{\"method\":\"nix\"}" },
    { "config_py_format_unsupported_install_warning", "{\"method\":\"\"}" },
    /* _env_var_policy_name */
    { "config_py_env_var_policy_name", "{\"key\":\"Path\",\"is_windows\":true}" },
    { "config_py_env_var_policy_name", "{\"key\":\"Path\",\"is_windows\":false}" },
    { "config_py_env_var_policy_name", "{\"key\":\"Path\",\"is_windows\":null}" },
    { "config_py_env_var_policy_name", "{\"key\":\"\",\"is_windows\":true}" },
    /* validate_env_var_name_for_write */
    { "config_py_validate_env_var_name_for_write", "{\"key\":\"MY_VAR\",\"is_windows\":null,\"launch_denylist\":[]}" },
    { "config_py_validate_env_var_name_for_write", "{\"key\":\"9BAD\",\"is_windows\":null,\"launch_denylist\":[]}" },
    { "config_py_validate_env_var_name_for_write", "{\"key\":\"PATH\",\"is_windows\":null,\"launch_denylist\":[]}" },
    { "config_py_validate_env_var_name_for_write", "{\"key\":\"path\",\"is_windows\":null,\"launch_denylist\":[]}" },
    { "config_py_validate_env_var_name_for_write", "{\"key\":\"LD_FOO\",\"is_windows\":null,\"launch_denylist\":[]}" },
    { "config_py_validate_env_var_name_for_write", "{\"key\":\"GIT_CONFIG_KEY_17\",\"is_windows\":null,\"launch_denylist\":[]}" },
    { "config_py_validate_env_var_name_for_write", "{\"key\":\"MY_LAUNCH_VAR\",\"is_windows\":null,\"launch_denylist\":[\"MY_LAUNCH_VAR\"]}" },
    { "config_py_validate_env_var_name_for_write", "{\"key\":\"HERMES_MANAGED_DIR\",\"is_windows\":null,\"launch_denylist\":[]}" },
    { "config_py_validate_env_var_name_for_write", "{\"key\":\"FOO\\n\",\"is_windows\":null,\"launch_denylist\":[]}" },
    { "config_py_validate_env_var_name_for_write", "{\"key\":\"BAD NAME\",\"is_windows\":null,\"launch_denylist\":[]}" },
    { "config_py_validate_env_var_name_for_write", "{\"key\":\"it's\",\"is_windows\":null,\"launch_denylist\":[]}" },
    /* _omitted_config_paths */
    { "config_py_omitted_config_paths", "{\"existing\":{\"a\":1,\"b\":2},\"proposed\":{\"a\":1}}" },
    { "config_py_omitted_config_paths", "{\"existing\":{\"a\":{\"x\":1,\"y\":2}},\"proposed\":{\"a\":{\"x\":1}}}" },
    { "config_py_omitted_config_paths", "{\"existing\":{\"a\":{\"x\":1}},\"proposed\":{\"a\":5}}" },
    { "config_py_omitted_config_paths", "{\"existing\":{\"a\":{}},\"proposed\":{\"a\":5}}" },
    { "config_py_omitted_config_paths", "{\"existing\":{\"a\":{\"x\":1}},\"proposed\":{}}" },
    { "config_py_omitted_config_paths", "{\"existing\":{},\"proposed\":{}}" },
    /* _install_method_stamp */
    { "config_py_install_method_stamp", "{\"text\":\"docker\"}" },
    { "config_py_install_method_stamp", "{\"text\":\"  GIT \\n\"}" },
    { "config_py_install_method_stamp", "{\"text\":\"\\ufeffnix\"}" },
    { "config_py_install_method_stamp", "{\"text\":\"pip\"}" },
    { "config_py_install_method_stamp", "{\"text\":null}" },
    { "config_py_install_method_stamp", "{\"text\":\"Home-Manager\"}" },
};

/* Serialize a _locate_nested result: parents as [container, key] hops (key is
 * an int for array containers), container, leaf key (int for array leaves). */
static json_t *cp_loc_to_json(const cfg_loc_t *loc) {
    json_t *o = json_object();
    json_t *par = json_array();
    for (int i = 0; i < loc->n_parents; i++) {
        json_t *hop = json_array();
        json_append(hop, json_copy(loc->parents[i]));
        if (loc->parents[i]->type == JSON_ARRAY)
            json_append(hop, json_number((double)strtoll(loc->parent_keys[i], NULL, 10)));
        else
            json_append(hop, json_string(loc->parent_keys[i]));
        json_append(par, hop);
    }
    json_set(o, "parents", par);
    json_set(o, "container", json_copy(loc->container));
    if (loc->is_index)
        json_set(o, "key", json_number((double)strtoll(loc->key, NULL, 10)));
    else
        json_set(o, "key", json_string(loc->key));
    return o;
}

static int cp_is_windows_arg(const json_t *in) {
    json_t *w = json_obj_get(in, "is_windows");
    if (!w || w->type == JSON_NULL) return -1;
    if (w->type == JSON_BOOL) return w->bool_val ? 1 : 0;
    if (w->type == JSON_NUMBER) return w->num_val != 0.0 ? 1 : 0;
    return -1;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    for (size_t i = 0; i < sizeof CASES / sizeof CASES[0]; i++) {
        const char *fn = CASES[i].func;
        json_t *in = json_parse(CASES[i].in, NULL);
        if (!in) { fprintf(stderr, "bad case JSON: %s\n", CASES[i].in); return 2; }
        json_t *work = json_copy(in);
        json_t *rec = json_object();
        json_set(rec, "func", json_string(fn));
        json_set(rec, "in", in);            /* rec owns the pristine input */
        json_t *out = NULL;
        char *err = NULL;
        const char *errtype = NULL;

        if (strcmp(fn, "config_py_split_key_path") == 0) {
            out = config_py_split_key_path(json_get_str(work, "key", ""));
        } else if (strcmp(fn, "config_py_greedy_literal_match") == 0) {
            char buf[2048];
            int n = config_py_greedy_literal_match(json_obj_get(work, "container"),
                                                   json_obj_get(work, "parts"), buf, sizeof buf);
            if (n > 0) {
                out = json_array();
                json_append(out, json_string(buf));
                json_append(out, json_int(n));
            }
        } else if (strcmp(fn, "config_py_phantom_sibling") == 0) {
            char buf[2048];
            if (config_py_phantom_sibling(json_obj_get(work, "container"),
                                          json_get_str(work, "part", ""), buf, sizeof buf))
                out = json_string(buf);
        } else if (strcmp(fn, "config_py_locate_nested") == 0) {
            cfg_loc_t loc;
            if (config_py_locate_nested(json_obj_get(work, "config"),
                                        json_obj_get(work, "parts"), &loc)) {
                out = cp_loc_to_json(&loc);
                free(loc.key);
                free(loc.parents);
                free(loc.parent_keys);
            }
        } else if (strcmp(fn, "config_py_validate_entry_list") == 0) {
            out = json_array();
            config_py_validate_entry_list(out, json_obj_get(work, "entries"),
                                          json_get_str(work, "label", ""),
                                          json_obj_get(work, "fields"),
                                          json_get_str(work, "severity", ""),
                                          json_get_str(work, "message", ""),
                                          json_get_str(work, "hint", ""));
        } else if (strcmp(fn, "config_py_validate_custom_providers") == 0) {
            out = json_array();
            config_py_validate_custom_providers(out, json_obj_get(work, "cp"));
        } else if (strcmp(fn, "config_py_validate_fallback_model") == 0) {
            out = json_array();
            config_py_validate_fallback_model(out, json_obj_get(work, "fb"));
        } else if (strcmp(fn, "config_py_strip_dotted_keys") == 0) {
            json_t *stripped = NULL;
            config_py_strip_dotted_keys(json_obj_get(work, "cfg"),
                                        json_obj_get(work, "dotted_keys"), &stripped);
            out = json_object();
            json_set(out, "cfg", json_copy(json_obj_get(work, "cfg")));
            json_set(out, "stripped", stripped);
        } else if (strcmp(fn, "config_py_is_non_env_secret_ref") == 0) {
            out = json_bool(config_py_is_non_env_secret_ref(json_get_str(work, "ref", "")));
        } else if (strcmp(fn, "config_py_canonicalize_config") == 0) {
            out = config_py_canonicalize_config(json_obj_get(work, "config"), &err);
            if (err) errtype = "Exception";   /* oracle compares the "Type: msg" text */
        } else if (strcmp(fn, "config_py_ensure_dict") == 0) {
            json_t *parent = json_obj_get(work, "parent");
            json_t *child = config_py_ensure_dict(parent, json_get_str(work, "key", ""));
            out = json_object();
            json_set(out, "parent", json_copy(parent));
            json_set(out, "child", child ? json_copy(child) : json_null());
        } else if (strcmp(fn, "config_py_commented_sections_for_save") == 0) {
            char *text = config_py_commented_sections_for_save(json_obj_get(work, "normalized"), &err);
            if (err) errtype = "AttributeError";
            else if (text) { out = json_string(text); free(text); }
        } else if (strcmp(fn, "config_py_is_secret_config_key") == 0) {
            out = json_bool(config_py_is_secret_config_key(json_get_str(work, "key", "")));
        } else if (strcmp(fn, "config_py_looks_structured_value") == 0) {
            out = json_bool(config_py_looks_structured_value(json_get_str(work, "value", "")));
        } else if (strcmp(fn, "config_py_expected_container_type") == 0) {
            const char *t = config_py_expected_container_type(json_get_str(work, "key", ""),
                                                              json_obj_get(work, "user_config"));
            if (t) out = json_string(t);
        } else if (strcmp(fn, "config_py_refuse_container_type_mismatch") == 0) {
            json_t *force = json_obj_get(work, "force");
            out = config_py_refuse_container_type_mismatch(
                json_get_str(work, "key", ""), json_obj_get(work, "value"),
                json_obj_get(work, "user_config"),
                (force && force->type == JSON_BOOL && force->bool_val) ? 1 : 0, &err);
            if (err) errtype = "SystemExit";
        } else if (strcmp(fn, "config_py_is_uv_tool_install") == 0) {
            out = json_bool(config_py_is_uv_tool_install());
        } else if (strcmp(fn, "config_py_is_unsupported_install_method") == 0) {
            out = json_bool(config_py_is_unsupported_install_method(json_get_str(work, "method", "")));
        } else if (strcmp(fn, "config_py_format_unsupported_install_warning") == 0) {
            char *w = config_py_format_unsupported_install_warning(json_get_str(work, "method", ""));
            out = json_string(w ? w : "");
            free(w);
        } else if (strcmp(fn, "config_py_env_var_policy_name") == 0) {
            char buf[1024];
            config_py_env_var_policy_name(json_get_str(work, "key", ""),
                                          cp_is_windows_arg(work), buf, sizeof buf);
            out = json_string(buf);
        } else if (strcmp(fn, "config_py_validate_env_var_name_for_write") == 0) {
            char ebuf[2048];
            int rc = config_py_validate_env_var_name_for_write(
                json_get_str(work, "key", ""), cp_is_windows_arg(work),
                json_obj_get(work, "launch_denylist"), ebuf, sizeof ebuf);
            if (rc != CFG_ENV_OK) { err = strdup(ebuf); errtype = "ValueError"; }
        } else if (strcmp(fn, "config_py_omitted_config_paths") == 0) {
            out = config_py_omitted_config_paths(json_obj_get(work, "existing"),
                                                 json_obj_get(work, "proposed"));
        } else if (strcmp(fn, "config_py_install_method_stamp") == 0) {
            json_t *text = json_obj_get(work, "text");
            const char *m = config_py_install_method_stamp(
                (text && text->type == JSON_STRING) ? text->str_val : NULL);
            if (m) out = json_string(m);
        } else {
            fprintf(stderr, "unknown func %s\n", fn);
            return 2;
        }

        if (err) {
            json_set(rec, "out", json_null());
            json_set(rec, "errtype", json_string(errtype));
            json_set(rec, "err", json_string(err));
            free(err);
        } else {
            json_set(rec, "out", out ? out : json_null());
        }
        char *ser = json_serialize(rec);
        printf("%s\n", ser);
        free(ser);
        json_free(rec);
        json_free(work);
    }
    return 0;
}
