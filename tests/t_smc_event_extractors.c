/* t_smc_event_extractors.c — verify the shared_metrics_contract extractors
 * against a Python oracle encoding the SAME event-extraction rules. */
#define _POSIX_C_SOURCE 200809L
#include "port_hermes_cli_shared_metrics_contract.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } \
    else { printf("  ok: %s\n", msg); } \
} while (0)

/* metrics-v2 metadata envelope used by the shared counters */
#define META_V2 "\"metadata\":{\"hermes.metrics.schema_version\":\"hermes.metrics.event.v2\",\"hermes.relay.runtime_instance\":\"inst1\"}"

int main(void) {
    /* task end event (valid) */
    const char *task_end =
        "{\"kind\":\"scope\",\"category\":\"function\",\"name\":\"hermes.task_run\","
        "\"scope_category\":\"end\"," META_V2 ","
        "\"category_profile\":null,"
        "\"data\":{\"duration_bucket\":\"lt_1s\",\"end_reason\":\"completed\",\"entrypoint\":\"interactive\","
        "\"execution_surface\":\"cli\",\"model_call_count_bucket\":\"0\","
        "\"outcome\":\"success\",\"retry_count_bucket\":\"0\",\"termination\":\"none\","
        "\"tool_call_count_bucket\":\"0\"}}";
    char *tc = smc_task_counter(task_end);
    CHECK(tc && tc[0] == '{', "task_counter returns JSON object");
    CHECK(strstr(tc, "\"outcome\":\"success\"") != NULL, "task_counter end outcome");
    free(tc);

    /* invalid event (bad schema key) -> None */
    const char *bad = "{\"kind\":\"scope\",\"metadata\":{\"hermes.metrics.schema_version\":\"wrong\"}}";
    char *rc = smc_task_counter(bad);
    CHECK(strcmp(rc, "None") == 0, "task_counter rejects bad schema");
    free(rc);

    /* task start event */
    const char *start =
        "{\"kind\":\"scope\",\"category\":\"function\",\"name\":\"hermes.task_run\","
        "\"scope_category\":\"start\"," META_V2 ",\"category_profile\":null,"
        "\"data\":{\"entrypoint\":\"cli\",\"execution_surface\":\"cli\"}}";
    char *rs = smc_task_counter(start);
    CHECK(strstr(rs, "\"entrypoint\":\"cli\"") != NULL, "task_start entrypoint");
    CHECK(strstr(rs, "\"execution_surface\":\"cli\"") != NULL, "task_start surface");
    free(rs);

    /* primary model_call scope end event */
    const char *mcall =
        "{\"kind\":\"scope\",\"category\":\"llm\",\"name\":\"hermes.model_call\","
        "\"scope_category\":\"end\"," META_V2 ","
        "\"category_profile\":{\"model_name\":\"unknown\"},"
        "\"data\":{\"model\":\"gpt-4o\",\"provider\":\"openai\"}}";
    char *md = smc_model_call_dimensions(mcall);
    CHECK(strstr(md, "\"model\":\"gpt-4o\"") != NULL, "model_call_dimensions extracts model");
    CHECK(strstr(md, "\"provider\":\"openai\"") != NULL, "model_call_dimensions extracts provider");
    free(md);

    /* auxiliary projection: event name=hermes.logical_llm_call with relay envelope */
    const char *aux =
        "{\"kind\":\"scope\",\"category\":\"function\",\"name\":\"hermes.logical_llm_call\","
        "\"scope_category\":\"end\",\"category_profile\":null,"
        "\"metadata\":{\"hermes.relay.schema_version\":\"hermes.relay.runtime.v1\","
        "\"hermes.relay.runtime_instance\":\"i\",\"hermes.call_role\":\"auxiliary:z\"},"
        "\"data\":{\"model\":\"gpt-4o\",\"outcome\":\"success\",\"provider\":\"openai\"}}";
    char *ad = smc_auxiliary_model_call_dimensions(aux);
    CHECK(strstr(ad, "\"model\":\"gpt-4o\"") != NULL, "auxiliary extracts model");
    CHECK(strstr(ad, "\"provider\":\"openai\"") != NULL, "auxiliary extracts provider");
    CHECK(strstr(ad, "\"outcome\"") == NULL, "auxiliary omits outcome (model_call_fields)");
    free(ad);

    /* tool_call_dimensions */
    const char *tc_ev =
        "{\"kind\":\"scope\",\"category\":\"tool\",\"name\":\"hermes.tool_call\","
        "\"scope_category\":\"end\"," META_V2 ",\"category_profile\":{},"
        "\"data\":{\"approval_outcome\":\"not_required\",\"latency_bucket\":\"1s_to_2s\","
        "\"outcome\":\"success\",\"retry_count_bucket\":\"0\",\"tool_category\":\"web\"}}";
    char *tcd = smc_tool_call_dimensions(tc_ev);
    CHECK(tcd && tcd[0] == '{', "tool_call_dimensions returns JSON");
    CHECK(strstr(tcd, "\"tool_category\":\"web\"") != NULL, "tool_call category web");
    free(tcd);

    /* tool_approval_counter */
    const char *ta =
        "{\"kind\":\"mark\",\"name\":\"hermes.tool_approval\"," META_V2 ","
        "\"category\":null,\"scope_category\":null,\"category_profile\":null,"
        "\"data\":{\"attribution\":\"tool_call\",\"outcome\":\"approved\"}}";
    char *tac = smc_tool_approval_counter(ta);
    CHECK(strcmp(tac, "{\"attribution\":\"tool_call\",\"outcome\":\"approved\"}") == 0,
          "tool_approval_counter extracts dims");
    free(tac);

    /* skill_lifecycle counter */
    const char *slc =
        "{\"kind\":\"mark\",\"name\":\"hermes.skill.lifecycle\"," META_V2 ","
        "\"category\":null,\"scope_category\":null,\"category_profile\":null,"
        "\"data\":{\"action\":\"installed\",\"provenance\":\"external\"}}";
    char *sl = smc_skill_counter(slc);
    CHECK(strcmp(sl, "{\"action\":\"installed\",\"provenance\":\"external\"}") == 0,
          "skill_lifecycle_counter extracts dims");
    free(sl);

    /* skill_load counter */
    const char *slc2 =
        "{\"kind\":\"mark\",\"name\":\"hermes.skill.load\"," META_V2 ","
        "\"category\":null,\"scope_category\":null,\"category_profile\":null,"
        "\"data\":{\"post_patch_state\":\"no_new_patch\",\"provenance\":\"local\","
        "\"reuse_state\":\"reused\",\"use_count_bucket\":\"3_to_5\"}}";
    char *sl2 = smc_skill_counter(slc2);
    CHECK(strcmp(sl2, "{\"post_patch_state\":\"no_new_patch\",\"provenance\":\"local\",\"reuse_state\":\"reused\",\"use_count_bucket\":\"3_to_5\"}") == 0,
          "skill_load_counter extracts dims");
    free(sl2);

    /* client_active_counter requires data == {} */
    const char *ca_ok =
        "{\"kind\":\"mark\",\"name\":\"hermes.client.active\"," META_V2 ","
        "\"category\":null,\"scope_category\":null,\"category_profile\":null,\"data\":{}}";
    char *car = smc_client_active_counter(ca_ok);
    CHECK(strcmp(car, "{}") == 0, "client_active_counter returns {} on empty data");
    free(car);
    const char *ca_bad =
        "{\"kind\":\"mark\",\"name\":\"hermes.client.active\"," META_V2 ","
        "\"category\":null,\"scope_category\":null,\"category_profile\":null,"
        "\"data\":{\"entrypoint\":\"cli\"}}";
    char *cab = smc_client_active_counter(ca_bad);
    CHECK(strcmp(cab, "None") == 0, "client_active_counter rejects non-empty data");
    free(cab);

    /* scalar helpers */
    CHECK(strcmp(smc_skill_provenance("External "), "external") == 0, "skill_provenance normal");
    CHECK(strcmp(smc_skill_provenance("weird"), "unknown") == 0, "skill_provenance unknown");
    CHECK(strcmp(smc_tool_category("MCP-Something"), "mcp") == 0, "tool_category mcp prefix");
    CHECK(strcmp(smc_tool_category("x_search"), "web") == 0, "tool_category web alias");
    CHECK(strcmp(smc_tool_category("browser"), "browser") == 0, "tool_category known");
    CHECK(strcmp(smc_tool_outcome("OK"), "success") == 0, "tool_outcome ok->success");
    CHECK(strcmp(smc_tool_outcome("error"), "failed") == 0, "tool_outcome error->failed");
    CHECK(strcmp(smc_tool_approval_outcome("approve"), "approved") == 0, "tool_approval_outcome");
    CHECK(strcmp(smc_client_os_family("Darwin"), "macos") == 0, "client_os_family darwin");
    CHECK(strcmp(smc_client_architecture("x86_64"), "x86_64") == 0, "client_architecture x86_64");
    CHECK(strcmp(smc_client_install_method("pip"), "pip") == 0, "client_install_method pip");
    CHECK(strcmp(smc_client_install_method("nix"), "unknown") == 0, "client_install_method nix->unknown");

    json_t *nb = json_number(42);
    CHECK(smc_non_negative_number(nb) == 42.0, "non_negative_number int");
    free(nb);
    json_t *nb2 = json_number(-5);
    CHECK(isnan(smc_non_negative_number(nb2)), "non_negative_number rejects negative");
    free(nb2);

    char *mi = smc_metric_identifier("GPT-4o", 256);
    CHECK(strcmp(mi, "gpt-4o") == 0, "metric_identifier normalizes case");
    free(mi);
    char *mi2 = smc_metric_identifier("bad space", 256);
    CHECK(strcmp(mi2, "unknown") == 0, "metric_identifier rejects space");
    free(mi2);
    char *mi3 = smc_metric_identifier("waytoolongidentifierthatexceedsthelimit", 10);
    CHECK(strcmp(mi3, "unknown") == 0, "metric_identifier rejects >maxlen");
    free(mi3);

    /* client_resource + is_valid */
    char *cr = smc_client_resource("0.1.6", "Linux", "x86_64", "pip");
    CHECK(strstr(cr, "\"os_family\":\"linux\"") != NULL, "client_resource os_family");
    CHECK(smc_client_resource_is_valid(cr), "client_resource_is_valid passes");
    free(cr);
    CHECK(!smc_client_resource_is_valid("{\"a\":1}"), "client_resource_is_valid rejects");

    /* tool latency / retry buckets */
    json_t *d = json_number(150);
    CHECK(strcmp(smc_tool_latency_bucket(d, NULL), "100ms_to_250ms") == 0, "tool_latency_bucket");
    free(d);
    json_t *r = json_number(3);
    CHECK(strcmp(smc_tool_retry_bucket(r), "3_to_5") == 0, "tool_retry_bucket");
    free(r);
    json_t *rb = json_bool(1);
    CHECK(strcmp(smc_tool_retry_bucket(rb), "unknown") == 0, "tool_retry_bucket rejects bool");
    free(rb);

    /* task_counter rejects missing fields */
    const char *tcbad =
        "{\"kind\":\"scope\",\"category\":\"function\",\"name\":\"hermes.task_run\","
        "\"scope_category\":\"end\"," META_V2 ",\"category_profile\":null,"
        "\"data\":{\"entrypoint\":\"cli\"}}";
    char *tcr = smc_task_counter(tcbad);
    CHECK(strcmp(tcr, "None") == 0, "task_counter rejects missing fields");
    free(tcr);

    if (fails == 0) printf("ALL PASS\n");
    else printf("%d FAILED\n", fails);
    return fails;
}
