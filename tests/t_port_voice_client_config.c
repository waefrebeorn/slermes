/*
 * t_port_voice_client_config.c — oracle harness for the voice client
 * config pure core. Fixture: JSON array of cases:
 *   {"op":"client_direct_value","value":...}
 *   {"op":"relay","reason":"..."}
 *   {"op":"stt_relay_reason","provider":...,"stt_enabled":...,
 *    "is_local":...,"in_builtin":...,"has_credentials":...}
 */
#include "voice_client_config.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 2;
    char *buf = NULL;
    size_t cap = 0, len = 0;
    for (;;) {
        if (len + 4096 > cap) { cap = cap ? cap * 2 : 8192; buf = realloc(buf, cap); }
        size_t r = fread(buf + len, 1, cap - len, f);
        if (!r) break;
        len += r;
    }
    fclose(f);
    if (!buf) return 2;

    char *jerr = NULL;
    json_t *cases = json_parse(buf, &jerr);
    free(buf);
    if (jerr) { free(jerr); return 2; }
    if (!cases || cases->type != JSON_ARRAY) { if (cases) json_free(cases); return 2; }

    for (size_t i = 0; i < json_len(cases); i++) {
        json_t *c = json_get(cases, i);
        const char *op = json_get_str(c, "op", "");
        if (strcmp(op, "client_direct_value") == 0) {
            bool ok = vcc_client_direct_value(json_obj_get(c, "value"));
            printf("{\"ok\":%s}\n", ok ? "true" : "false");
        } else if (strcmp(op, "relay") == 0) {
            json_t *r = vcc_relay(json_get_str(c, "reason", ""));
            char *txt = json_serialize(r);
            printf("%s\n", txt ? txt : "{}");
            free(txt);
            json_free(r);
        } else if (strcmp(op, "stt_relay_reason") == 0) {
            json_t *pv = json_obj_get(c, "provider");
            const char *provider = (pv && pv->type == JSON_STRING) ? pv->str_val : "";
            json_t *sv = json_obj_get(c, "stt_enabled");
            bool stt_enabled = (sv && sv->type == JSON_BOOL) ? sv->bool_val : true;
            json_t *lv = json_obj_get(c, "is_local");
            bool is_local = (lv && lv->type == JSON_BOOL) ? lv->bool_val : false;
            json_t *bv = json_obj_get(c, "in_builtin");
            bool in_builtin = (bv && bv->type == JSON_BOOL) ? bv->bool_val : true;
            json_t *cv = json_obj_get(c, "has_credentials");
            bool has_creds = (cv && cv->type == JSON_BOOL) ? cv->bool_val : true;
            const char *reason = vcc_stt_relay_reason(provider, stt_enabled,
                                                      is_local, in_builtin,
                                                      has_creds);
            if (reason) {
                json_t *s = json_string(reason);
                char *txt = json_serialize(s);
                printf("%s\n", txt ? txt : "null");
                free(txt);
                json_free(s);
            } else {
                printf("null\n");
            }
        }
    }
    json_free(cases);
    return 0;
}
