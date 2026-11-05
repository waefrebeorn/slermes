/* t_port_outbound_webhooks.c — oracle harness for agent/outbound_webhooks.py
 * pure delivery-shape helpers: _serialize_payload and _build_delivery. */
#include "port_outbound_webhooks.h"

#include <stdio.h>
#include <stdlib.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    static const char *const evs[]  = {"session_start", "pre_tool_call", "post_tool_call"};
    static const char *const prs[]  = {"work", "default", "prof2"};
    static const char *const dids[] = {"a1b2", "x9", "abc123"};
    static const char *const tss[]  = {"2026-11-05T02:00:00", "2026-11-05T02:01:00",
                                       "2026-11-05T02:02:00"};
    static const char *const exts[] = {"{}",
                                       "{\"tool_name\":\"bash\",\"detail\":\"run\"}",
                                       "{\"exit\":0,\"reason\":{\"code\":\"ok\"},\"mode\":null}"};
    for (int i = 0; i < 3; i++) {
        char *out = ow_serialize_payload(evs[i], prs[i], dids[i], tss[i], exts[i]);
        printf("{\"family\":\"ser\",\"case\":%d,\"event\":", i);
        jstr(evs[i]);
        printf(",\"prof\":");
        jstr(prs[i]);
        printf(",\"did\":");
        jstr(dids[i]);
        printf(",\"ts\":");
        jstr(tss[i]);
        printf(",\"extra\":%s,\"out\":", exts[i]);
        jstr(out);
        printf("}\n");
        free(out);
    }

    /* _build_delivery: signed and unsigned. */
    static const char *const de_list[][7] = {
        {"session_end", "https://hooks.example.com/x", "target-A", "",
         "20", "{\"hook_event_name\":\"session_end\"}", "id-1"},
        {"pre_tool_call", "http://insecure.local:9999", "t2", "topsecret",
         "5", "{\"tool_name\":\"bash\"}", "id-2"},
        {"session_start", "https://x.io", "", "s3cr3t!", "30",
         "{\"a\":1,\"b\":[1,2]}", "id-3"},
    };
    for (int i = 0; i < 3; i++) {
        char *out = ow_build_delivery_json(de_list[i][0], de_list[i][1],
                                           de_list[i][2], de_list[i][3],
                                           atoi(de_list[i][4]), de_list[i][5],
                                           de_list[i][6]);
        printf("{\"family\":\"del\",\"case\":%d,\"event\":", i);
        jstr(de_list[i][0]);
        printf(",\"url\":");
        jstr(de_list[i][1]);
        printf(",\"label\":");
        jstr(de_list[i][2]);
        printf(",\"secret\":");
        jstr(de_list[i][3]);
        printf(",\"timeout\":%s,\"body\":", de_list[i][4]);
        jstr(de_list[i][5]);
        printf(",\"did\":");
        jstr(de_list[i][6]);
        printf(",\"out\":%s}\n", out ? out : "null");
        free(out);
    }

    /* _parse_single_target: validate one hooks.outbound entry. */
    static const char *const PARS[][7] = {
        /* raw, valid, tscoped, env, def */
        {"{\"url\":\" https://x.io/hook \",\"events\":[\"pre_tool_call\",\"on_session_end\"],\"name\":\"alias\",\"matcher\":\"bash\",\"secret\":\"s\",\"timeout\":\"30\"}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "", "10"},
        {"{\"url\":\"https://h\",\"events\":[\"pre_tool_call\",\"bogus\"]}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "", "10"},
        {"{\"url\":\"https://h\",\"events\":[\"bogus\"]}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "", "10"},
        {"{\"url\":\"https://h\",\"events\":[\"on_session_end\"],\"matcher\":\"..\"}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "", "10"},
        {"{\"url\":\"https://h\",\"events\":[\"pre_tool_call\"],\"matcher\":5}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "", "10"},
        {"{\"url\":\"https://h\",\"events\":[\"on_session_end\"],\"timeout\":150}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "", "10"},
        {"{\"url\":\"https://h\",\"events\":[\"on_session_end\"],\"timeout\":-5}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "", "10"},
        {"{\"url\":\"https://h\",\"events\":[\"on_session_end\"],\"timeout\":12.7}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "", "10"},
        {"{\"url\":\"https://h\",\"events\":[\"on_session_end\"],\"timeout\":\"abc\"}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "", "10"},
        {"{\"url\":\"https://h\",\"events\":[\"pre_tool_call\"],\"secret_env\":\"OW_SECRET\",\"timeout\":\"5\"}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "ENVVALUE", "10"},
        {"{\"url\":\"https://h\",\"events\":[\"pre_tool_call\"],\"secret_env\":\"UNSET_VAR\"}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "", "10"},
        {"{\"url\":\"https://h\",\"events\":[\"pre_tool_call\"],\"secret_env\":\"  \",\"secret\":\"inline\"}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "", "10"},
        {"{\"events\":[\"on_session_end\"]}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "", "10"},
        {"{\"url\":\"ftp://x\",\"events\":[\"on_session_end\"]}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "", "10"},
        {"{\"url\":\"   \",\"events\":[\"on_session_end\"]}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "", "10"},
        {"{\"url\":\"https://h\",\"events\":[]}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "", "10"},
        {"{\"url\":\"https://h\",\"events\":\"on_session_end\"}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "", "10"},
        {"null", "[\"pre_tool_call\"]", "[\"pre_tool_call\"]", "", "10"},
        {"{\"url\":\"https://h\",\"events\":[\"on_session_end\"],\"name\":5,\"matcher\":\"mm\"}",
         "[\"pre_tool_call\",\"on_session_end\",\"session_start\"]",
         "[\"pre_tool_call\",\"post_tool_call\"]", "", "10"},
    };
    for (size_t i = 0; i < sizeof(PARS) / sizeof(PARS[0]); i++) {
        char *out = ow_parse_single_target(PARS[i][0], PARS[i][1], PARS[i][2],
                                           PARS[i][3], atoi(PARS[i][4]));
        printf("{\"family\":\"par\",\"case\":%zu,\"raw\":%s,\"valid\":%s,"
               "\"tscoped\":%s,\"env\":", i, PARS[i][0], PARS[i][1], PARS[i][2]);
        jstr(PARS[i][3]);
        printf(",\"def\":%s,\"out\":%s}\n", PARS[i][4], out ? out : "null");
        free(out);
    }
    return 0;
}
