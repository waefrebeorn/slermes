/* Scalar-string oracle harness: C fn takes a single const char* (a value/header/
 * url/string), returns int/bool. Compare against the Python twin fed the same
 * string. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern int  is_openai_fast_model(const char *model_id);
extern int  is_anthropic_fast_model(const char *model_id);
extern int  model_supports_fast_mode(const char *model_id);
extern int  is_github_models_base_url(const char *base_url);
extern bool _is_loopback_hostname(const char *hostname);
extern bool gw_is_control_interrupt_message(const char *message);
extern bool gw_is_auto_continue_noise(const char *content);
extern int  cli_gateway_response_filters_is_intentional_silence_response(const char *response);
extern int  cli_gateway_response_filters_is_partial_silence_marker(const char *text);
extern int  config_coerce_ssl_verify(const char *value);
extern int  config_coerce_config_version(const char *value);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <func> <str>\n", argv[0]); return 2; }
    const char *func = argv[1];
    const char *a = argv[2];
    #define B(x) printf("%d\n", (x)?1:0)
    if (strcmp(func,"is_openai_fast_model")==0) printf("%d\n", is_openai_fast_model(a));
    else if (strcmp(func,"is_anthropic_fast_model")==0) printf("%d\n", is_anthropic_fast_model(a));
    else if (strcmp(func,"model_supports_fast_mode")==0) printf("%d\n", model_supports_fast_mode(a));
    else if (strcmp(func,"is_github_models_base_url")==0) printf("%d\n", is_github_models_base_url(a));
    else if (strcmp(func,"is_loopback_hostname")==0) B(_is_loopback_hostname(a));
    else if (strcmp(func,"is_control_interrupt_message")==0) B(gw_is_control_interrupt_message(a));
    else if (strcmp(func,"is_auto_continue_noise")==0) B(gw_is_auto_continue_noise(a));
    else if (strcmp(func,"is_intentional_silence_response")==0) printf("%d\n", cli_gateway_response_filters_is_intentional_silence_response(a));
    else if (strcmp(func,"is_partial_silence_marker")==0) printf("%d\n", cli_gateway_response_filters_is_partial_silence_marker(a));
    else if (strcmp(func,"coerce_ssl_verify")==0) printf("%d\n", config_coerce_ssl_verify(a));
    else if (strcmp(func,"coerce_config_version")==0) printf("%d\n", config_coerce_config_version(a));
    else { fprintf(stderr, "unknown func %s\n", func); return 4; }
    #undef B
    return 0;
}
