/* JSON-lines harness for the w28_modelswitc_6dc907_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_modelswitc_6dc907_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "catalog_stale")) {
            out = w28ms_p1_catalog_stale(json_obj_get(in, "entry"), json_obj_get(in, "model_ids"));
        } else if (!strcmp(func, "any_env")) {
            out = w28ms_p1_any_env(json_obj_get(in, "env_vars"), json_obj_get(in, "env"));
        } else if (!strcmp(func, "skip")) {
            out = w28ms_p1_skip(json_obj_get(in, "seen"), json_obj_get(in, "excluded"),
                                json_obj_get(in, "keys"));
        } else if (!strcmp(func, "azure_entra_configured")) {
            out = w28ms_p1_azure_entra_configured(json_obj_get(in, "model_cfg"),
                                                  json_obj_get(in, "env"));
        } else if (!strcmp(func, "fast_aws_signal")) {
            out = w28ms_p1_fast_aws_signal(json_obj_get(in, "env"));
        } else if (!strcmp(func, "aws_creds_for_listing")) {
            out = w28ms_p1_aws_creds_for_listing(json_obj_get(in, "slug"),
                                                 json_obj_get(in, "current_provider"),
                                                 json_obj_get(in, "env"),
                                                 json_obj_get(in, "sdk_creds"));
        } else if (!strcmp(func, "is_aws_sdk")) {
            out = w28ms_p1_is_aws_sdk(json_obj_get(in, "pconfig"));
        } else if (!strcmp(func, "first_curated")) {
            out = w28ms_p1_first_curated(json_obj_get(in, "curated"), json_obj_get(in, "keys"));
        } else if (!strcmp(func, "free_tier_nous_row")) {
            out = w28ms_p1_free_tier_nous_row(json_obj_get(in, "row"), json_obj_get(in, "flags"));
        } else if (!strcmp(func, "cap_models")) {
            out = w28ms_p1_cap_models(json_obj_get(in, "model_ids"),
                                      json_obj_get(in, "max_models"), json_obj_get(in, "slug"));
        } else if (!strcmp(func, "split_chain_entries")) {
            out = w28ms_p1_split_chain_entries(json_obj_get(in, "models"));
        } else if (!strcmp(func, "extend_unique")) {
            out = w28ms_p1_extend_unique(json_obj_get(in, "target"), json_obj_get(in, "items"));
        } else if (!strcmp(func, "norm_url")) {
            out = w28ms_p1_norm_url(json_obj_get(in, "url"));
        } else if (!strcmp(func, "entry_base_url")) {
            out = w28ms_p1_entry_base_url(json_obj_get(in, "entry"), json_obj_get(in, "keys"));
        } else if (!strcmp(func, "entry_api_mode")) {
            out = w28ms_p1_entry_api_mode(json_obj_get(in, "entry"));
        } else if (!strcmp(func, "entry_credentials")) {
            out = w28ms_p1_entry_credentials(json_obj_get(in, "entry"),
                                             json_obj_get(in, "key_env_keys"),
                                             json_obj_get(in, "key_cmd_identity"));
        } else if (!strcmp(func, "discover_flag")) {
            out = w28ms_p1_discover_flag(json_obj_get(in, "entry"));
        } else if (!strcmp(func, "display_prefix")) {
            out = w28ms_p1_display_prefix(json_obj_get(in, "name"));
        } else if (!strcmp(func, "group_display_name")) {
            out = w28ms_p1_group_display_name(json_obj_get(in, "name"));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
