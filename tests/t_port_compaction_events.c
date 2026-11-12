/* t_port_compaction_events.c — oracle harness for agent/compaction_events.py.
 * Emits one JSON line per case with a "fn" discriminator. */
#include "port_compaction_events.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *P(const char *s)
{
    return json_parse(s, NULL);
}

static void jstr_escape(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') {
            putchar('\\');
            putchar(ch);
        } else if (ch < 0x20) {
            printf("\\u%04x", ch);
        } else {
            putchar(ch);
        }
    }
    putchar('"');
}

/* "out" is emitted as a JSON STRING so a bare scalar (number/null) survives. */
static void emit(int cs, const char *fn, const char *in, json_t *out)
{
    char *s = out ? json_serialize(out) : NULL;
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":", cs, fn, in);
    jstr_escape(s ? s : "null");
    printf("}\n");
    fflush(stdout);
    free(s);
    json_free(out);
}

static void emit_closed(int cs, const char *in, const char *allowed,
                        const char *dflt, json_t *out)
{
    char *s = out ? json_serialize(out) : NULL;
    printf("{\"case\":%d,\"fn\":\"closed\",\"in\":%s,\"allowed\":%s,\"dflt\":",
           cs, in, allowed);
    jstr_escape(dflt ? dflt : "");
    printf(",\"out\":");
    jstr_escape(s ? s : "null");
    printf("}\n");
    fflush(stdout);
    free(s);
    json_free(out);
}

static void emit_attempt(int cs, const char *record_json, const char *cc_json,
                         json_t *out)
{
    char *s = out ? json_serialize(out) : NULL;
    printf("{\"case\":%d,\"fn\":\"attempt_payload\",\"in\":{\"record\":%s,\"cc\":%s},\"out\":",
           cs, record_json, cc_json);
    jstr_escape(s ? s : "null");
    printf("}\n");
    fflush(stdout);
    free(s);
    json_free(out);
}

static void emit_micro(int cs, const char *record_json, json_t *out)
{
    char *s = out ? json_serialize(out) : NULL;
    printf("{\"case\":%d,\"fn\":\"micro_payload\",\"in\":{\"record\":%s},\"out\":",
           cs, record_json);
    jstr_escape(s ? s : "null");
    printf("}\n");
    fflush(stdout);
    free(s);
    json_free(out);
}

static void emit_str(int cs, const char *fn, const char *in, const char *out)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":\"%s\"}\n",
           cs, fn, in, out);
    fflush(stdout);
}

static const char *const KINDS_S[] = {
    "summarize", "micro_summarize", "prune_tool_results", "provider_native"};
static const char *const TCV_S[] = {
    "auto", "manual", "overflow", "provider", "unknown"};

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* _closed */
    emit_closed(1, "\"summarize\"",
                "[\"summarize\",\"micro_summarize\",\"prune_tool_results\",\"provider_native\"]",
                "other", ce_closed(P("\"summarize\""), KINDS_S, 4, "other"));
    emit_closed(2, "\"bogus\"",
                "[\"summarize\",\"micro_summarize\",\"prune_tool_results\",\"provider_native\"]",
                "other", ce_closed(P("\"bogus\""), KINDS_S, 4, "other"));
    emit_closed(3, "null",
                "[\"summarize\",\"micro_summarize\",\"prune_tool_results\",\"provider_native\"]",
                "other", ce_closed(P("null"), KINDS_S, 4, "other"));
    emit_closed(4, "\"\"",
                "[\"summarize\",\"micro_summarize\",\"prune_tool_results\",\"provider_native\"]",
                "other", ce_closed(P("\"\""), KINDS_S, 4, "other"));
    emit_closed(5, "5",
                "[\"summarize\",\"micro_summarize\",\"prune_tool_results\",\"provider_native\"]",
                "other", ce_closed(P("5"), KINDS_S, 4, "other"));
    emit_closed(6, "\"auto\"",
                "[\"auto\",\"manual\",\"overflow\",\"provider\",\"unknown\"]",
                NULL, ce_closed(P("\"auto\""), TCV_S, 5, NULL));
    emit_closed(7, "\"manual\"",
                "[\"auto\",\"manual\",\"overflow\",\"provider\",\"unknown\"]",
                NULL, ce_closed(P("\"manual\""), TCV_S, 5, NULL));

    /* _int */
    emit(8, "int", "5", ce_int(P("5")));
    emit(9, "int", "true", ce_int(P("true")));
    emit(10, "int", "\"5\"", ce_int(P("\"5\"")));
    emit(11, "int", "null", ce_int(P("null")));
    emit(12, "int", "-3", ce_int(P("-3")));

    /* failure_class */
    emit(13, "failure_class", "\"exception:ValueError\"",
         ce_failure_class(P("\"exception:ValueError\"")));
    emit(14, "failure_class", "\"rollback:Foo\"",
         ce_failure_class(P("\"rollback:Foo\"")));
    emit(15, "failure_class", "\"none\"", ce_failure_class(P("\"none\"")));
    emit(16, "failure_class", "\"\"", ce_failure_class(P("\"\"")));
    emit(17, "failure_class", "null", ce_failure_class(P("null")));
    emit(18, "failure_class", "\"bogus\"", ce_failure_class(P("\"bogus\"")));
    emit(19, "failure_class", "\"summary_network_failure\"",
         ce_failure_class(P("\"summary_network_failure\"")));
    emit(20, "failure_class", "\"blocked:cooldown\"",
         ce_failure_class(P("\"blocked:cooldown\"")));

    /* event_name */
    emit_str(21, "event_name", "{\"outcome\":\"committed\",\"scope\":\"history\"}",
             ce_event_name(P("{\"outcome\":\"committed\",\"scope\":\"history\"}")));
    emit_str(22, "event_name", "{\"outcome\":\"committed\",\"scope\":\"provider\"}",
             ce_event_name(P("{\"outcome\":\"committed\",\"scope\":\"provider\"}")));
    emit_str(23, "event_name", "{\"outcome\":\"failed\",\"scope\":\"history\"}",
             ce_event_name(P("{\"outcome\":\"failed\",\"scope\":\"history\"}")));
    emit_str(24, "event_name", "{\"outcome\":\"committed\",\"scope\":\"other\"}",
             ce_event_name(P("{\"outcome\":\"committed\",\"scope\":\"other\"}")));

    /* _payload */
    emit(25, "payload", "{}", ce_payload(P("{}")));
    emit(26, "payload",
         "{\"kind\":\"summarize\",\"scope\":\"history\",\"method\":\"llm_summary\","
         "\"trigger\":\"manual\",\"outcome\":\"committed\",\"attempt_id\":\"a1\","
         "\"session_id\":\"s1\",\"tokens_before\":10,\"tokens_after\":4}",
         ce_payload(P("{\"kind\":\"summarize\",\"scope\":\"history\","
                      "\"method\":\"llm_summary\",\"trigger\":\"manual\","
                      "\"outcome\":\"committed\",\"attempt_id\":\"a1\","
                      "\"session_id\":\"s1\",\"tokens_before\":10,"
                      "\"tokens_after\":4}")));
    emit(27, "payload", "{\"kind\":\"bogus\",\"trigger\":\"bogus\"}",
         ce_payload(P("{\"kind\":\"bogus\",\"trigger\":\"bogus\"}")));
    emit(28, "payload",
         "{\"trigger\":\"overflow\",\"trigger_class\":\"manual\",\"attempt_id\":\"a2\"}",
         ce_payload(P("{\"trigger\":\"overflow\",\"trigger_class\":\"manual\","
                      "\"attempt_id\":\"a2\"}")));
    emit(29, "payload",
         "{\"scope\":\"history\",\"outcome\":\"committed\",\"attempt_id\":\"a3\"}",
         ce_payload(P("{\"scope\":\"history\",\"outcome\":\"committed\","
                      "\"attempt_id\":\"a3\"}")));
    emit(30, "payload",
         "{\"scope\":\"provider\",\"outcome\":\"committed\",\"attempt_id\":\"a4\"}",
         ce_payload(P("{\"scope\":\"provider\",\"outcome\":\"committed\","
                      "\"attempt_id\":\"a4\"}")));

    /* _attempt_outcome */
    emit(31, "attempt_outcome", "{\"cs\":\"committed\",\"failure\":\"none\",\"hr\":false}",
         ce_attempt_outcome(P("\"committed\""), P("\"none\""), P("false")));
    emit(32, "attempt_outcome", "{\"cs\":\"aborted\",\"failure\":\"snapshot_stale\",\"hr\":false}",
         ce_attempt_outcome(P("\"aborted\""), P("\"snapshot_stale\""), P("false")));
    emit(33, "attempt_outcome", "{\"cs\":\"aborted\",\"failure\":\"would_grow\",\"hr\":false}",
         ce_attempt_outcome(P("\"aborted\""), P("\"would_grow\""), P("false")));
    emit(34, "attempt_outcome", "{\"cs\":\"aborted\",\"failure\":\"other\",\"hr\":false}",
         ce_attempt_outcome(P("\"aborted\""), P("\"other\""), P("false")));
    emit(35, "attempt_outcome", "{\"cs\":\"weird\",\"failure\":\"none\",\"hr\":false}",
         ce_attempt_outcome(P("\"weird\""), P("\"none\""), P("false")));
    emit(36, "attempt_outcome", "{\"cs\":\"failed\",\"failure\":\"none\",\"hr\":true}",
         ce_attempt_outcome(P("\"failed\""), P("\"none\""), P("true")));

    /* _attempt_trigger */
    emit(37, "attempt_trigger", "\"auto\"",
         ce_attempt_trigger(P("\"auto\"")));
    emit(38, "attempt_trigger", "\"manual\"",
         ce_attempt_trigger(P("\"manual\"")));
    emit(39, "attempt_trigger", "null", ce_attempt_trigger(P("null")));

    /* attempt_payload */
    emit_attempt(40,
                 "{\"route\":\"codex_app_server\",\"failure_class\":\"exception:ValueError\","
                 "\"split_status\":\"in_place_committed\",\"commit_status\":\"committed\","
                 "\"history_rewritten\":true,\"trigger_source\":\"overflow\","
                 "\"overflow_reason\":\"context_overflow\",\"attempt_id\":\"a40\","
                 "\"session_id\":\"s40\",\"token_count_method\":\"estimate_rough\","
                 "\"aux_provider\":\"p\",\"aux_model\":\"m\",\"has_focus_topic\":true,"
                 "\"tokens_before\":100,\"tokens_after\":40,\"main_context_limit\":200000,"
                 "\"effective_threshold\":150000,\"total_duration_ms\":1200,"
                 "\"summary_generation_ms\":800,\"commit_ms\":100,\"tool_results_pruned\":3}",
                 "7",
                 ce_attempt_payload(P("{\"route\":\"codex_app_server\","
                                      "\"failure_class\":\"exception:ValueError\","
                                      "\"split_status\":\"in_place_committed\","
                                      "\"commit_status\":\"committed\","
                                      "\"history_rewritten\":true,"
                                      "\"trigger_source\":\"overflow\","
                                      "\"overflow_reason\":\"context_overflow\","
                                      "\"attempt_id\":\"a40\",\"session_id\":\"s40\","
                                      "\"token_count_method\":\"estimate_rough\","
                                      "\"aux_provider\":\"p\",\"aux_model\":\"m\","
                                      "\"has_focus_topic\":true,\"tokens_before\":100,"
                                      "\"tokens_after\":40,\"main_context_limit\":200000,"
                                      "\"effective_threshold\":150000,"
                                      "\"total_duration_ms\":1200,"
                                      "\"summary_generation_ms\":800,\"commit_ms\":100,"
                                      "\"tool_results_pruned\":3}"),
                                    P("7")));
    emit_attempt(41,
                 "{\"route\":\"llm\",\"failure_class\":\"summary_network_failure\","
                 "\"split_status\":\"rotated_committed\",\"commit_status\":\"aborted\","
                 "\"history_rewritten\":false,\"trigger_source\":\"auto\","
                 "\"attempt_id\":\"a41\",\"session_id\":\"s41\",\"tokens_before\":50,"
                 "\"tokens_after\":20,\"messages_before\":9,\"messages_after\":4,"
                 "\"items_dropped\":5,\"commit_ms\":42}",
                 "null",
                 ce_attempt_payload(P("{\"route\":\"llm\","
                                      "\"failure_class\":\"summary_network_failure\","
                                      "\"split_status\":\"rotated_committed\","
                                      "\"commit_status\":\"aborted\","
                                      "\"history_rewritten\":false,"
                                      "\"trigger_source\":\"auto\",\"attempt_id\":\"a41\","
                                      "\"session_id\":\"s41\",\"tokens_before\":50,"
                                      "\"tokens_after\":20,\"messages_before\":9,"
                                      "\"messages_after\":4,\"items_dropped\":5,"
                                      "\"commit_ms\":42}"),
                                    P("null")));
    emit_attempt(42, "{}", "null",
                 ce_attempt_payload(P("{}"), P("null")));

    /* micro_payload */
    emit_micro(43,
               "{\"outcome\":\"absorbed\",\"session_id\":\"s43\",\"tokens_before\":100,"
               "\"tokens_after\":40,\"messages_before\":8,\"messages_after\":3,"
               "\"duration_ms\":55,\"threshold_tokens\":150000,\"context_limit\":200000,"
               "\"aux_model\":\"m\"}",
               ce_micro_payload(P("{\"outcome\":\"absorbed\",\"session_id\":\"s43\","
                                  "\"tokens_before\":100,\"tokens_after\":40,"
                                  "\"messages_before\":8,\"messages_after\":3,"
                                  "\"duration_ms\":55,\"threshold_tokens\":150000,"
                                  "\"context_limit\":200000,\"aux_model\":\"m\"}")));
    emit_micro(44,
               "{\"outcome\":\"stale_generation\",\"session_id\":\"s44\","
               "\"tokens_before\":10,\"tokens_after\":9}",
               ce_micro_payload(P("{\"outcome\":\"stale_generation\","
                                  "\"session_id\":\"s44\",\"tokens_before\":10,"
                                  "\"tokens_after\":9}")));
    emit_micro(45, "{\"outcome\":\"defrag\"}",
               ce_micro_payload(P("{\"outcome\":\"defrag\"}")));
    emit_micro(46, "{\"outcome\":\"weird\"}",
               ce_micro_payload(P("{\"outcome\":\"weird\"}")));

    /* prune_payload */
    emit(47, "prune_payload",
         "{\"session_id\":\"s47\",\"tb\":100,\"ta\":40,\"m\":3,\"tr\":2}",
         ce_prune_payload("s47", 100, 40, 3, 2));

    /* provider_native_payload */
    emit(48, "provider_native_payload",
         "{\"session_id\":\"s48\",\"cc\":3}",
         ce_provider_native_payload("s48", P("3")));
    emit(49, "provider_native_payload",
         "{\"session_id\":\"s49\",\"cc\":null}",
         ce_provider_native_payload("s49", P("null")));
    return 0;
}
