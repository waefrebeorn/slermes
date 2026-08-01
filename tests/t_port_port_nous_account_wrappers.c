/* AUTO-GENERATED integration oracle harness for port_nous_account_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_nous_account_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int nous_tool_gateway_entitled(const char *);
extern int nous_tool_gateway_entitled_for(const char *);
extern int nous_nous_portal_billing_url(const char *);
extern int nous_nous_portal_topup_url(const char *);
extern int nous_format_nous_portal_entitlement_message(const char *);
extern int nous_u_no_paid_access_message(const char *);
extern int nous_u_credit_detail(const char *);
extern int nous_reset_nous_portal_account_info_cache(const char *);
extern int nous_u_fresh_account_info(const char *);
extern int nous_u_info_from_inference_key_pool(const char *);
extern int nous_u_info_from_oauth_pool(const char *);
extern int nous_u_select_nous_pool_entry(const char *);
extern int nous_u_pool_entry_is_portal_oauth(const char *);
extern int nous_u_fetch_nous_account_info(const char *);
extern int nous_u_info_from_valid_jwt(const char *);
extern int nous_u_info_from_account_payload(const char *);
extern int nous_u_tool_access_from_value(const char *);
extern int nous_u_subscription_from_payload(const char *);
extern int nous_u_paid_service_access_from_payload(const char *);
extern int nous_u_error_info(const char *);
extern int nous_u_portal_base_url(const char *);
extern int nous_u_cache_key(const char *);
extern int nous_u_parse_iso_timestamp(const char *);
extern int nous_u_coerce_str(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_nous_tool_gateway_entitled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_tool_gateway_entitled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_tool_gateway_entitled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_tool_gateway_entitled_for(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_tool_gateway_entitled_for(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_tool_gateway_entitled_for"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_nous_portal_billing_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_nous_portal_billing_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_nous_portal_billing_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_nous_portal_topup_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_nous_portal_topup_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_nous_portal_topup_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_format_nous_portal_entitlement_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_format_nous_portal_entitlement_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_format_nous_portal_entitlement_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_no_paid_access_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_no_paid_access_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_no_paid_access_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_credit_detail(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_credit_detail(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_credit_detail"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_reset_nous_portal_account_info_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_reset_nous_portal_account_info_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_reset_nous_portal_account_info_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_fresh_account_info(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_fresh_account_info(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_fresh_account_info"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_info_from_inference_key_pool(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_info_from_inference_key_pool(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_info_from_inference_key_pool"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_info_from_oauth_pool(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_info_from_oauth_pool(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_info_from_oauth_pool"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_select_nous_pool_entry(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_select_nous_pool_entry(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_select_nous_pool_entry"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_pool_entry_is_portal_oauth(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_pool_entry_is_portal_oauth(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_pool_entry_is_portal_oauth"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_fetch_nous_account_info(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_fetch_nous_account_info(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_fetch_nous_account_info"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_info_from_valid_jwt(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_info_from_valid_jwt(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_info_from_valid_jwt"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_info_from_account_payload(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_info_from_account_payload(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_info_from_account_payload"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_tool_access_from_value(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_tool_access_from_value(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_tool_access_from_value"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_subscription_from_payload(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_subscription_from_payload(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_subscription_from_payload"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_paid_service_access_from_payload(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_paid_service_access_from_payload(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_paid_service_access_from_payload"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_error_info(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_error_info(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_error_info"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_portal_base_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_portal_base_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_portal_base_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_cache_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_cache_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_cache_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_parse_iso_timestamp(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_parse_iso_timestamp(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_parse_iso_timestamp"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nous_u_coerce_str(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nous_u_coerce_str(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nous_u_coerce_str"));
    json_set(o, "out", json_int(v)); return o;
}

int main(int argc, char **argv){
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    char *err = NULL; json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\n"); free(input); return 2; }
    int n = json_array_size(root);
    for (int i = 0; i < n; i++){
        json_t *c = json_get(root, i);
        const char *op = json_get_str(c, "op", "");
        json_t *o = NULL;
        if (strcmp(op, "nous_tool_gateway_entitled") == 0) o = emit_nous_tool_gateway_entitled(c);
        if (strcmp(op, "nous_tool_gateway_entitled_for") == 0) o = emit_nous_tool_gateway_entitled_for(c);
        if (strcmp(op, "nous_nous_portal_billing_url") == 0) o = emit_nous_nous_portal_billing_url(c);
        if (strcmp(op, "nous_nous_portal_topup_url") == 0) o = emit_nous_nous_portal_topup_url(c);
        if (strcmp(op, "nous_format_nous_portal_entitlement_message") == 0) o = emit_nous_format_nous_portal_entitlement_message(c);
        if (strcmp(op, "nous_u_no_paid_access_message") == 0) o = emit_nous_u_no_paid_access_message(c);
        if (strcmp(op, "nous_u_credit_detail") == 0) o = emit_nous_u_credit_detail(c);
        if (strcmp(op, "nous_reset_nous_portal_account_info_cache") == 0) o = emit_nous_reset_nous_portal_account_info_cache(c);
        if (strcmp(op, "nous_u_fresh_account_info") == 0) o = emit_nous_u_fresh_account_info(c);
        if (strcmp(op, "nous_u_info_from_inference_key_pool") == 0) o = emit_nous_u_info_from_inference_key_pool(c);
        if (strcmp(op, "nous_u_info_from_oauth_pool") == 0) o = emit_nous_u_info_from_oauth_pool(c);
        if (strcmp(op, "nous_u_select_nous_pool_entry") == 0) o = emit_nous_u_select_nous_pool_entry(c);
        if (strcmp(op, "nous_u_pool_entry_is_portal_oauth") == 0) o = emit_nous_u_pool_entry_is_portal_oauth(c);
        if (strcmp(op, "nous_u_fetch_nous_account_info") == 0) o = emit_nous_u_fetch_nous_account_info(c);
        if (strcmp(op, "nous_u_info_from_valid_jwt") == 0) o = emit_nous_u_info_from_valid_jwt(c);
        if (strcmp(op, "nous_u_info_from_account_payload") == 0) o = emit_nous_u_info_from_account_payload(c);
        if (strcmp(op, "nous_u_tool_access_from_value") == 0) o = emit_nous_u_tool_access_from_value(c);
        if (strcmp(op, "nous_u_subscription_from_payload") == 0) o = emit_nous_u_subscription_from_payload(c);
        if (strcmp(op, "nous_u_paid_service_access_from_payload") == 0) o = emit_nous_u_paid_service_access_from_payload(c);
        if (strcmp(op, "nous_u_error_info") == 0) o = emit_nous_u_error_info(c);
        if (strcmp(op, "nous_u_portal_base_url") == 0) o = emit_nous_u_portal_base_url(c);
        if (strcmp(op, "nous_u_cache_key") == 0) o = emit_nous_u_cache_key(c);
        if (strcmp(op, "nous_u_parse_iso_timestamp") == 0) o = emit_nous_u_parse_iso_timestamp(c);
        if (strcmp(op, "nous_u_coerce_str") == 0) o = emit_nous_u_coerce_str(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
