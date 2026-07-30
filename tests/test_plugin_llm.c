/*
 * tests/test_plugin_llm.c — Unit tests for plugin LLM facade
 *
 * Tests the standalone parts: trust policy resolution, override checking,
 * message building. Links against plugin_llm.o for actual implementation.
 */
#include "plugin_llm.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Stubs for external LLM functions that plugin_llm.c references
 * but our unit tests don't exercise at runtime. */
#include "hermes_agent.h"
llm_response_t *llm_chat_completion(llm_config_t *cfg,
                                     const message_t **messages,
                                     size_t message_count,
                                     json_t *tools_json) {
    (void)cfg; (void)messages; (void)message_count; (void)tools_json;
    return NULL;
}
void llm_response_free(llm_response_t *resp) { (void)resp; }

static int pass = 0, fail = 0;

#define TEST(name) do { \
    printf("  %s ... ", #name); \
    if (test_##name()) { \
        printf("PASS\n"); pass++; \
    } else { \
        printf("FAIL\n"); fail++; \
    } \
} while(0)

#define CHECK(cond, fmt, ...) do { \
    if (!(cond)) { \
        printf("    FAIL at %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        return 0; \
    } \
} while(0)

/* Default trust policy (fully restrictive) */
static int test_default_policy(void) {
    plugin_llm_trust_policy_t p = plugin_llm_resolve_trust_policy("test-plugin", NULL);
    CHECK(strcmp(p.plugin_id, "test-plugin") == 0, "plugin_id mismatch");
    CHECK(!p.allow_provider_override, "provider override should be false");
    CHECK(!p.allow_model_override, "model override should be false");
    CHECK(!p.allow_agent_id_override, "agent id override should be false");
    CHECK(!p.allow_profile_override, "profile override should be false");
    return 1;
}

/* Permissive policy with all overrides allowed */
static int test_permissive_policy(void) {
    json_t *config = json_object();
    json_set(config, "allow_provider_override", json_bool(true));
    json_set(config, "allow_model_override", json_bool(true));
    json_set(config, "allow_agent_id_override", json_bool(true));
    json_set(config, "allow_profile_override", json_bool(true));

    plugin_llm_trust_policy_t p = plugin_llm_resolve_trust_policy("perm-plugin", config);
    CHECK(p.allow_provider_override, "provider override should be true");
    CHECK(p.allow_model_override, "model override should be true");
    CHECK(p.allow_agent_id_override, "agent id override should be true");
    CHECK(p.allow_profile_override, "profile override should be true");

    json_free(config);
    return 1;
}

/* Allowed providers list */
static int test_allowed_providers(void) {
    json_t *config = json_object();
    json_set(config, "allow_provider_override", json_bool(true));
    json_t *providers = json_array();
    json_append(providers, json_string("openai"));
    json_append(providers, json_string("anthropic"));
    json_set(config, "allowed_providers", providers);

    plugin_llm_trust_policy_t p = plugin_llm_resolve_trust_policy("list-plugin", config);
    CHECK(p.allow_provider_override, "provider override should be true");
    CHECK(strstr(p.allowed_providers, "openai") != NULL, "openai not in list");
    CHECK(strstr(p.allowed_providers, "anthropic") != NULL, "anthropic not in list");
    CHECK(!p.allow_any_provider, "allow_any should be false");

    json_free(config);
    return 1;
}

/* Wildcard providers */
static int test_wildcard_providers(void) {
    json_t *config = json_object();
    json_set(config, "allow_provider_override", json_bool(true));
    json_t *providers = json_array();
    json_append(providers, json_string("*"));
    json_set(config, "allowed_providers", providers);

    plugin_llm_trust_policy_t p = plugin_llm_resolve_trust_policy("wild-plugin", config);
    CHECK(p.allow_any_provider, "allow_any should be true");

    json_free(config);
    return 1;
}

/* Override checking — denied */
static int test_override_denied(void) {
    plugin_llm_trust_policy_t p = plugin_llm_resolve_trust_policy("restrict-plugin", NULL);
    const char *provider = "openai";
    const char *model = NULL;
    const char *agent_id = NULL;
    const char *profile = NULL;
    char err[512];

    int rc = plugin_llm_check_overrides(&p, &provider, &model, &agent_id, &profile,
                                         err, sizeof(err));
    CHECK(rc != 0, "should return error for denied provider override");
    CHECK(strlen(err) > 0, "error message should be non-empty");

    return 1;
}

/* Override checking — allowed */
static int test_override_allowed(void) {
    json_t *config = json_object();
    json_set(config, "allow_provider_override", json_bool(true));
    json_t *providers = json_array();
    json_append(providers, json_string("openai"));
    json_set(config, "allowed_providers", providers);

    plugin_llm_trust_policy_t p = plugin_llm_resolve_trust_policy("ok-plugin", config);
    const char *provider = "openai";
    const char *model = NULL;
    const char *agent_id = NULL;
    const char *profile = NULL;
    char err[512];

    int rc = plugin_llm_check_overrides(&p, &provider, &model, &agent_id, &profile,
                                         err, sizeof(err));
    CHECK(rc == 0, "should succeed for allowed provider");

    json_free(config);
    return 1;
}

/* Model override denied (not in allowed list) */
static int test_override_model_denied(void) {
    json_t *config = json_object();
    json_set(config, "allow_model_override", json_bool(true));
    json_t *models = json_array();
    json_append(models, json_string("gpt-4o"));
    json_set(config, "allowed_models", models);

    plugin_llm_trust_policy_t p = plugin_llm_resolve_trust_policy("model-plugin", config);
    const char *provider = NULL;
    const char *model = "claude-3-sonnet";
    const char *agent_id = NULL;
    const char *profile = NULL;
    char err[512];

    int rc = plugin_llm_check_overrides(&p, &provider, &model, &agent_id, &profile,
                                         err, sizeof(err));
    CHECK(rc != 0, "should return error for denied model override");

    json_free(config);
    return 1;
}

/* Build structured messages with instructions only */
static int test_build_msg_instructions(void) {
    json_t *msgs = plugin_llm_build_structured_messages(
        "Summarize this", NULL, 0, false, NULL, NULL, NULL);
    CHECK(msgs != NULL, "messages should not be NULL");
    CHECK(json_len(msgs) == 1, "should have 1 message (no system)");

    json_t *user = json_get(msgs, 0);
    CHECK(user != NULL, "first message should exist");
    json_t *role = json_obj_get(user, "role");
    CHECK(role != NULL && role->type == JSON_STRING, "role should be a string");
    CHECK(strcmp(role->str_val, "user") == 0, "role should be 'user'");

    json_free(msgs);
    return 1;
}

/* Build with system prompt */
static int test_build_msg_system(void) {
    json_t *msgs = plugin_llm_build_structured_messages(
        "Do something", NULL, 0, false, NULL, NULL, "You are helpful");
    CHECK(msgs != NULL, "messages should not be NULL");
    CHECK(json_len(msgs) == 2, "should have 2 messages (system + user)");

    json_t *sys = json_get(msgs, 0);
    json_t *role = json_obj_get(sys, "role");
    CHECK(role != NULL && role->type == JSON_STRING, "sys role should be a string");
    CHECK(strcmp(role->str_val, "system") == 0, "sys role should be 'system'");

    json_free(msgs);
    return 1;
}

/* Build with JSON mode */
static int test_build_msg_json(void) {
    json_t *msgs = plugin_llm_build_structured_messages(
        "Output JSON", NULL, 0, true, NULL, NULL, NULL);
    CHECK(msgs != NULL, "messages should not be NULL");
    CHECK(json_len(msgs) == 2, "should have 2 messages (system + user) with json mode");

    json_free(msgs);
    return 1;
}

/* Free NULL safety */
static int test_free_null(void) {
    plugin_llm_result_free(NULL);
    plugin_llm_structured_result_free(NULL);
    plugin_llm_input_free(NULL);
    return 1;
}

/* Build messages with image input */
static int test_build_msg_image(void) {
    plugin_llm_input_t inputs[1];
    memset(&inputs[0], 0, sizeof(inputs[0]));
    inputs[0].type = PLUGIN_LLM_INPUT_TYPE_IMAGE;
    inputs[0].url = strdup("https://example.com/img.png");
    inputs[0].mime_type = strdup("image/png");

    json_t *msgs = plugin_llm_build_structured_messages(
        "Describe this image", inputs, 1, false, NULL, NULL, NULL);
    CHECK(msgs != NULL, "messages should not be NULL");
    CHECK(json_len(msgs) == 1, "should have 1 message");

    json_t *user = json_get(msgs, 0);
    json_t *content = json_obj_get(user, "content");
    CHECK(content != NULL && content->type == JSON_ARRAY, "content should be array");
    CHECK(json_len(content) == 2, "should have 2 parts (text + image)");

    plugin_llm_input_free(&inputs[0]);
    json_free(msgs);
    return 1;
}

/* Schema name in build messages */
static int test_build_msg_schema_name(void) {
    json_t *msgs = plugin_llm_build_structured_messages(
        "Output JSON", NULL, 0, false, NULL, "MySchema", NULL);
    CHECK(msgs != NULL, "messages should not be NULL");

    json_t *user = json_get(msgs, 0);
    json_t *content = json_obj_get(user, "content");
    CHECK(content != NULL && content->type == JSON_ARRAY, "content should be array");
    CHECK(json_len(content) >= 1, "should have at least 1 part");

    json_free(msgs);
    return 1;
}

/* Usage extraction from NULL */
static int test_usage_null(void) {
    plugin_llm_usage_t u = plugin_llm_extract_usage(NULL);
    CHECK(u.input_tokens == 0, "input tokens should be 0");
    CHECK(u.output_tokens == 0, "output tokens should be 0");
    CHECK(u.total_tokens == 0, "total tokens should be 0");
    return 1;
}

/* Main */
int main(void) {
    printf("=== Plugin LLM Tests ===\n");

    TEST(default_policy);
    TEST(permissive_policy);
    TEST(allowed_providers);
    TEST(wildcard_providers);
    TEST(override_denied);
    TEST(override_allowed);
    TEST(override_model_denied);
    TEST(build_msg_instructions);
    TEST(build_msg_system);
    TEST(build_msg_json);
    TEST(build_msg_image);
    TEST(build_msg_schema_name);
    TEST(usage_null);
    TEST(free_null);

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
