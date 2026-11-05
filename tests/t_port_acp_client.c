/*
 * t_port_acp_client.c — Oracle harness for agent/acp_client.py's JSON-RPC /
 * session-model shaping (src/cli/port_acp_client.c).
 */
#include "port_acp_client.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit(const char *family, const char *case_name, json_t *node)
{
    printf("{\"family\":\"%s\",\"case\":\"%s\",\"out\":", family,
           case_name ? case_name : "");
    if (!node) {
        fputs("null", stdout);
    } else {
        char *s = json_serialize(node);
        fputs(s ? s : "null", stdout);
        free(s);
        json_free(node);
    }
    printf("}\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    emit("result", "string_id", acp_jsonrpc_result("\"m1\"", "{\"ok\":true}"));
    emit("result", "number_id", acp_jsonrpc_result("7", "null"));
    emit("result", "null_id", acp_jsonrpc_result(NULL, "\"text\""));
    emit("error", "basic", acp_jsonrpc_error("\"m2\"", -32601, "Method not found"));
    emit("error", "empty_message", acp_jsonrpc_error("\"m3\"", 1, ""));

    static const char *const entries[] = {
        "[]",
        "[{\"value\":\"a\"},{\"value\":\" b \"},{\"value\":\"a\"},{\"value\":\"\"},null,7,{\"value\":null},{\"other\":1}]",
        "{\"value\":\"x\"}",
        "null",
        "[{\"modelId\":\"m1\"},{\"modelId\":\"m2\"},{\"modelId\":\"m1\"}]",
        "[{\"value\":5},{\"value\":true}]",
    };
    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        char name[32];
        snprintf(name, sizeof(name), "ids_%zu", i);
        const char *key = (i == 4) ? "modelId" : "value";
        emit("ids", name, acp_enabled_id_list(entries[i], key));
    }

    static const char *const sessions[] = {
        "{}",
        "{\"note\":\"no session keys\"}",
        "{\"configOptions\":[]}",
        "{\"configOptions\":[{\"id\":\"model\",\"options\":[{\"value\":\"m-a\"},{\"value\":\"m-b\"}]}]}",
        "{\"configOptions\":[{\"category\":\"model\",\"id\":\"cfg1\",\"options\":[{\"value\":\"m-a\"}]},{\"id\":\"model\",\"options\":[{\"value\":\"zzz\"}]}]}",
        "{\"configOptions\":[{\"category\":\"other\",\"id\":\"notmodel\"}]}",
        "{\"models\":{\"availableModels\":[{\"modelId\":\"legacy-1\"},{\"modelId\":\"legacy-1\"},{\"modelId\":\"legacy-2\"}]}}",
        "{\"configOptions\":[{\"id\":\"model\",\"options\":\"nope\"}],\"models\":{\"availableModels\":[{\"modelId\":\"fallback\"}]}}",
    };
    for (size_t i = 0; i < sizeof(sessions) / sizeof(sessions[0]); i++) {
        char name[32];
        snprintf(name, sizeof(name), "cfg_%zu", i);
        emit("config_option", name, acp_model_config_option(sessions[i]));
        char ids[32];
        snprintf(ids, sizeof(ids), "sess_%zu", i);
        emit("session_ids", ids, acp_session_model_ids(sessions[i]));
        char leg[32];
        snprintf(leg, sizeof(leg), "legacy_%zu", i);
        emit("legacy_ids", leg, acp_legacy_session_model_ids(sessions[i]));
    }

    {
        struct { const char *session, *model, *ph, *name; } cases[] = {
            { "{\"configOptions\":[{\"id\":\"model\",\"options\":[{\"value\":\"m-a\"}]}],\"sessionId\":\"s1\"}", "m-a", "", "v1_ok" },
            { "{\"configOptions\":[{\"id\":\"model\",\"options\":[{\"value\":\"m-a\"}]}],\"sessionId\":\"s1\"}", "m-b", "", "v1_unknown" },
            { "{\"configOptions\":[{\"category\":\"model\",\"id\":\"cfg9\",\"options\":[{\"value\":\"m-a\"}]}],\"sessionId\":\" s1 \"}", "m-a", "", "v1_custom_config_id" },
            { "{\"models\":{\"availableModels\":[{\"modelId\":\"legacy-1\"}]},\"sessionId\":\"s2\"}", "legacy-1", "", "legacy_ok" },
            { "{\"models\":{\"availableModels\":[{\"modelId\":\"legacy-1\"}]},\"sessionId\":\"s2\"}", "legacy-9", "", "legacy_unknown" },
            { "{\"sessionId\":\"s3\"}", "anything", "", "no_list_at_all" },
            { "{\"sessionId\":\"\"}", "m-a", "", "empty_session" },
            { "{\"sessionId\":\"s4\",\"models\":{\"availableModels\":[{\"modelId\":\"m-a\"}]}}", "m-a", "m-a", "placeholder_id" },
            { "{\"sessionId\":\"s5\"}", "  m-a  ", "", "trimmed_model" },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            emit("selection", cases[i].name,
                 acp_model_selection_request(cases[i].session, cases[i].model,
                                             cases[i].ph));
        }
    }

    {
        struct { int is_number; double number; acp_timeout_t obj; const char *name; } cases[] = {
            { 1, 30, {0}, "number_int" },
            { 1, 30.5, {0}, "number_float" },
            { 0, 0, {0}, "empty_object" },
            { 0, 0, { .has_read = 1, .read = 10, .has_write = 1, .write = 20 }, "read_write" },
            { 0, 0, { .has_connect = 1, .connect = 5, .has_pool = 1, .pool = 7.5 }, "connect_pool" },
            { 0, 0, { .has_timeout = 1, .timeout = 1.25 }, "timeout_only" },
            { 0, 0, {0}, "no_components" },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            printf("{\"family\":\"timeout\",\"case\":\"%s\",\"out\":%.17g}\n",
                   cases[i].name,
                   acp_effective_timeout(cases[i].is_number, cases[i].number,
                                         &cases[i].obj));
        }
    }
    return 0;
}
