/*
 * t_port_acp_client.c — Oracle harness for agent/acp_client.py's JSON-RPC /
 * session-model shaping (src/cli/port_acp_client.c). Emits inputs + C output as
 * JSONL so an oracle replays the LIVE Python on the identical values.
 * Families:
 *   result:       {family, case, id, result, out}      _jsonrpc_result
 *   error:        {family, case, id, code, message, out}  _jsonrpc_error
 *   ids:          {family, case, entries, key, out}    _enabled_id_list
 *   config_option:{family, case, session, out}         _model_config_option
 *   session_ids:  {family, case, session, out}         _session_model_ids
 *   legacy_ids:   {family, case, session, out}         _legacy_session_model_ids
 *   selection:    {family, case, session, model, ph, out}  _model_selection_request
 *   timeout:      {family, case, is_number, number, read, write, connect, pool,
 *                  timeout, out}                       _effective_timeout
 */
#include "port_acp_client.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
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

static void emit(const char *family, const char *case_name, const char *inputs,
                 const char *out_json)
{
    printf("{\"family\":\"%s\",\"case\":\"%s\",%s\"out\":%s}\n",
           family, case_name ? case_name : "", inputs,
           out_json ? out_json : "null");
}

static void emit_node(const char *family, const char *case_name,
                      const char *inputs, json_t *node)
{
    if (node) {
        char *s = json_serialize(node);
        emit(family, case_name, inputs, s ? s : "null");
        free(s);
        json_free(node);
    } else {
        emit(family, case_name, inputs, NULL);
    }
}

/* re-emit a raw json text with surrounding quotes stripped for the input field */
static char *strip_quotes(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    if (n >= 2 && s[0] == '"' && s[n - 1] == '"') {
        char *t = (char *)malloc(n - 1);
        memcpy(t, s + 1, n - 2);
        t[n - 2] = '\0';
        return t;
    }
    return strdup(s);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* result / error: C takes id as a JSON value; emit in decoded form too */
    {
        static const char *const ids[] = {"\"m1\"", "7", "null"};
        static const char *const res[] = {"{\"ok\":true}", "null", "\"text\""};
        static const char *const names[] = {"string_id", "number_id", "null_id"};
        for (int i = 0; i < 3; i++) {
            char *id_in = strip_quotes(ids[i]);
            printf("{\"family\":\"result\",\"case\":\"%s\",\"id\":%s,\"result\":%s,\"out\":",
                   names[i], ids[i], res[i]);
            json_t *r = acp_jsonrpc_result(ids[i], res[i]);
            if (r) { char *s = json_serialize(r); fputs(s ? s : "null", stdout); free(s); json_free(r); }
            else fputs("null", stdout);
            printf("}\n");
            free(id_in);
        }
    }
    {
        static const struct { const char *id; int code; const char *msg; const char *n; } cs[] = {
            {"\"m2\"", -32601, "Method not found", "basic"},
            {"\"m3\"", 1, "", "empty_message"},
        };
        for (int i = 0; i < 2; i++) {
            printf("{\"family\":\"error\",\"case\":\"%s\",\"id\":%s,\"code\":%d,\"message\":",
                   cs[i].n, cs[i].id, cs[i].code);
            jstr(cs[i].msg);
            printf(",\"out\":");
            json_t *r = acp_jsonrpc_error(cs[i].id, cs[i].code, cs[i].msg);
            if (r) { char *s = json_serialize(r); fputs(s ? s : "null", stdout); free(s); json_free(r); }
            else fputs("null", stdout);
            printf("}\n");
        }
    }

    /* ids family: entries JSON + key */
    {
        static const char *const entries[] = {
            "[]",
            "[{\"value\":\"a\"},{\"value\":\" b \"},{\"value\":\"a\"},{\"value\":\"\"},null,7,{\"value\":null},{\"other\":1}]",
            "{\"value\":\"x\"}",
            "null",
            "[{\"modelId\":\"m1\"},{\"modelId\":\"m2\"},{\"modelId\":\"m1\"}]",
            "[{\"value\":5},{\"value\":true}]",
        };
        static const char *const labels[] = {"ids_0", "ids_1", "ids_2", "ids_3", "ids_4", "ids_5"};
        static const char *const keys[] = {"value", "value", "value", "value", "modelId", "value"};
        for (int i = 0; i < 6; i++) {
            printf("{\"family\":\"ids\",\"case\":\"%s\",\"entries\":%s,\"key\":",
                   labels[i], entries[i]);
            jstr(keys[i]);
            printf(",\"out\":");
            json_t *r = acp_enabled_id_list(entries[i], keys[i]);
            if (r) { char *s = json_serialize(r); fputs(s ? s : "null", stdout); free(s); json_free(r); }
            else fputs("null", stdout);
            printf("}\n");
        }
    }

    /* config_option / session_ids / legacy_ids: session JSON */
    {
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
        for (int i = 0; i < 8; i++) {
            char n[32];
            snprintf(n, sizeof(n), "cfg_%d", i);
            printf("{\"family\":\"config_option\",\"case\":\"%s\",\"session\":%s,\"out\":",
                   n, sessions[i]);
            json_t *r = acp_model_config_option(sessions[i]);
            if (r) { char *s = json_serialize(r); fputs(s ? s : "null", stdout); free(s); json_free(r); }
            else fputs("null", stdout);
            printf("}\n");

            snprintf(n, sizeof(n), "sess_%d", i);
            printf("{\"family\":\"session_ids\",\"case\":\"%s\",\"session\":%s,\"out\":",
                   n, sessions[i]);
            r = acp_session_model_ids(sessions[i]);
            if (r) { char *s = json_serialize(r); fputs(s ? s : "null", stdout); free(s); json_free(r); }
            else fputs("null", stdout);
            printf("}\n");

            snprintf(n, sizeof(n), "legacy_%d", i);
            printf("{\"family\":\"legacy_ids\",\"case\":\"%s\",\"session\":%s,\"out\":",
                   n, sessions[i]);
            r = acp_legacy_session_model_ids(sessions[i]);
            if (r) { char *s = json_serialize(r); fputs(s ? s : "null", stdout); free(s); json_free(r); }
            else fputs("null", stdout);
            printf("}\n");
        }
    }

    /* selection: session + model + placeholder */
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
        for (int i = 0; i < 9; i++) {
            printf("{\"family\":\"selection\",\"case\":\"%s\",\"session\":%s,\"model\":",
                   cases[i].name, cases[i].session);
            jstr(cases[i].model);
            printf(",\"ph\":");
            jstr(cases[i].ph);
            printf(",\"out\":");
            json_t *r = acp_model_selection_request(cases[i].session, cases[i].model,
                                                    cases[i].ph);
            if (r) { char *s = json_serialize(r); fputs(s ? s : "null", stdout); free(s); json_free(r); }
            else fputs("null", stdout);
            printf("}\n");
        }
    }

    /* timeout: is_number + number OR an object with components */
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
        for (int i = 0; i < 7; i++) {
            acp_timeout_t *o = &cases[i].obj;
            printf("{\"family\":\"timeout\",\"case\":\"%s\",\"is_number\":%d,\"number\":%.17g,"
                   "\"has_read\":%d,\"read\":%.17g,\"has_write\":%d,\"write\":%.17g,"
                   "\"has_connect\":%d,\"connect\":%.17g,\"has_pool\":%d,\"pool\":%.17g,"
                   "\"has_timeout\":%d,\"timeout\":%.17g,\"out\":%.17g}\n",
                   cases[i].name, cases[i].is_number, cases[i].number,
                   o->has_read, o->read, o->has_write, o->write,
                   o->has_connect, o->connect, o->has_pool, o->pool,
                   o->has_timeout, o->timeout,
                   acp_effective_timeout(cases[i].is_number, cases[i].number, o));
        }
    }
    return 0;
}
