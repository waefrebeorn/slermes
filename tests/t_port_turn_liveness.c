/*
 * t_port_turn_liveness.c — Oracle harness for the PURE half of
 * agent/turn_liveness.py (ported to src/agent/port_turn_liveness.c).
 */
#include "port_turn_liveness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_json_str(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

static void emit_settings(const char *name, const char *config_json)
{
    int has_timeout = -1;
    double timeout = 0.0, poll = 0.0;
    int warnings = turn_liveness_resolve_settings(config_json, &has_timeout,
                                                  &timeout, &poll);
    printf("{\"case\":\"%s\",\"config\":", name);
    emit_json_str(config_json);
    printf(",\"has_timeout\":%d,\"timeout\":%.17g,\"poll\":%.17g,\"warnings\":%d}\n",
           has_timeout, timeout, poll, warnings);
}

static void emit_msg(const char *name, const char *key, const char *raw,
                     double def)
{
    char *msg = turn_liveness_invalid_value_message(key, raw, def);
    printf("{\"case\":\"%s\",\"key\":", name);
    emit_json_str(key);
    printf(",\"raw\":");
    emit_json_str(raw);
    printf(",\"def\":%.17g,\"msg\":", def);
    emit_json_str(msg);
    printf("}\n");
    free(msg);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    emit_settings("null_config", NULL);
    emit_settings("empty_object", "{}");
    emit_settings("no_agent", "{\"other\":1}");
    emit_settings("empty_agent", "{\"agent\":{}}");
    emit_settings("empty_section", "{\"agent\":{\"turn_liveness\":{}}}");
    emit_settings("explicit", "{\"agent\":{\"turn_liveness\":{\"timeout_s\":120,\"poll_s\":5}}}");
    emit_settings("string_numbers", "{\"agent\":{\"turn_liveness\":{\"timeout_s\":\"120\",\"poll_s\":\"5\"}}}");
    emit_settings("scientific", "{\"agent\":{\"turn_liveness\":{\"timeout_s\":\"1e3\",\"poll_s\":\"0.25\"}}}");
    emit_settings("timeout_zero", "{\"agent\":{\"turn_liveness\":{\"timeout_s\":0}}}");
    emit_settings("timeout_negative", "{\"agent\":{\"turn_liveness\":{\"timeout_s\":-30}}}");
    emit_settings("poll_zero", "{\"agent\":{\"turn_liveness\":{\"poll_s\":0}}}");
    emit_settings("poll_negative", "{\"agent\":{\"turn_liveness\":{\"poll_s\":-2}}}");
    emit_settings("poll_typo", "{\"agent\":{\"turn_liveness\":{\"poll_s\":\"abc\"}}}");
    emit_settings("timeout_typo", "{\"agent\":{\"turn_liveness\":{\"timeout_s\":\"abc\"}}}");
    emit_settings("timeout_nan", "{\"agent\":{\"turn_liveness\":{\"timeout_s\":\"nan\"}}}");
    emit_settings("timeout_inf", "{\"agent\":{\"turn_liveness\":{\"timeout_s\":\"inf\"}}}");
    emit_settings("poll_inf", "{\"agent\":{\"turn_liveness\":{\"poll_s\":\"-Infinity\"}}}");
    emit_settings("section_is_string", "{\"agent\":{\"turn_liveness\":\"nonsense\"}}");
    emit_settings("section_is_number", "{\"agent\":{\"turn_liveness\":7}}");
    emit_settings("below_min_poll", "{\"agent\":{\"turn_liveness\":{\"poll_s\":0.005}}}");
    emit_settings("fractional", "{\"agent\":{\"turn_liveness\":{\"timeout_s\":600.5,\"poll_s\":12.25}}}");

    emit_msg("msg_typo", "agent.turn_liveness.poll_s", "abc", 15.0);
    emit_msg("msg_section", "agent.turn_liveness", "nonsense", 600.0);
    emit_msg("msg_quote", "agent.turn_liveness.poll_s", "it's", 15.0);
    emit_msg("msg_no_raw", "agent.turn_liveness.poll_s", NULL, 15.0);
    return 0;
}
