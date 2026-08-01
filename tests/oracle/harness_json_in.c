/* JSON-in oracle harness: C fn takes a JSON string (const char*), returns scalar.
 * For each, parse argv[2] as the JSON input; C parses internally. We compare
 * against the Python twin fed json.loads(input). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool tts_tool_is_command_provider_config(const char *config);
extern int  tts_tool_get_command_tts_timeout(const char *config);
extern int  openrouter_model_is_free(const char *pricing_json);
extern int  cli_gateway_platforms_signal_rate_limit__is_signal_rate_limit_error(const char *err);
extern int  scale_to_zero_enabled(const char *environ_json);
extern int  scale_to_zero_messaging_is_relay_only_or_absent(const char *platforms_json);
extern int  scale_to_zero_messaging_is_relay_only_or_absent(const char *platforms_json);
extern json_t *redact_config_value(json_t *value, int depth);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <func> <json>\n", argv[0]); return 2; }
    const char *func = argv[1];
    const char *input = argv[2];
    if (strcmp(func, "is_command_provider_config") == 0) printf("%d\n", tts_tool_is_command_provider_config(input));
    else if (strcmp(func, "get_command_tts_timeout") == 0) printf("%.10g\n", (double)tts_tool_get_command_tts_timeout(input));
    else if (strcmp(func, "openrouter_model_is_free") == 0) printf("%d\n", openrouter_model_is_free(input));
    else if (strcmp(func, "is_signal_rate_limit_error") == 0) printf("%d\n", cli_gateway_platforms_signal_rate_limit__is_signal_rate_limit_error(input));
    else if (strcmp(func, "scale_to_zero_enabled") == 0) printf("%d\n", scale_to_zero_enabled(input));
    else if (strcmp(func, "messaging_is_relay_only_or_absent") == 0) printf("%d\n", scale_to_zero_messaging_is_relay_only_or_absent(input));
    else if (strcmp(func, "redact_config_value") == 0) {
        json_t *doc = json_parse(input, NULL);
        if (!doc) { fprintf(stderr, "parse fail\n"); return 3; }
        json_t *r = redact_config_value(doc, 0);
        char *s = json_serialize(r);
        printf("%s\n", s ? s : "null");
        free(s); json_free(r); json_free(doc);
    }
    else { fprintf(stderr, "unknown func %s\n", func); return 4; }
    return 0;
}
