/* t_cch_ports.c -- oracle harness for the four helpers added to
 * src/agent/chat_completion_helpers.c. Emits one JSON line per case. */
#include "port_chat_completion_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cs = 0;

static void payload_case(const char *v, int image_cost)
{
    json_t *j = json_parse(v, NULL);
    int r = agent_cch_payload_chars(j, image_cost);
    printf("{\"case\":%d,\"fn\":\"payload_chars\",\"in\":{\"value\":%s,"
           "\"image_cost\":%d},\"out\":%d}\n", ++cs, v, image_cost, r);
    fflush(stdout);
    json_free(j);
}

static void image_case(const char *v, int image_cost)
{
    json_t *j = json_parse(v, NULL);
    int r = agent_cch_image_part_chars(j, image_cost);
    printf("{\"case\":%d,\"fn\":\"image_part_chars\",\"in\":{\"part\":%s,"
           "\"image_cost\":%d},\"out\":%d}\n", ++cs, v, image_cost, r);
    fflush(stdout);
    json_free(j);
}

static void scale_case(double base, int est)
{
    double r = agent_cch_scale_stale_timeout_for_context(base, est);
    printf("{\"case\":%d,\"fn\":\"scale_stale\",\"in\":{\"base\":%.17g,"
           "\"est_tokens\":%d},\"out\":%.17g}\n", ++cs, base, est, r);
    fflush(stdout);
}

static void bound_case(double stale, int est, double hard)
{
    double r = agent_cch_bound_codex_stale_timeout(stale, est, hard);
    printf("{\"case\":%d,\"fn\":\"bound_stale\",\"in\":{\"stale_timeout\":%.17g,"
           "\"est_tokens\":%d,\"hard\":%.17g},\"out\":%.17g}\n",
           ++cs, stale, est, hard, r);
    fflush(stdout);
}

static void bedrock_case(const char *id)
{
    double r = agent_cch_bedrock_reasoning_stale_floor(id);
    printf("{\"case\":%d,\"fn\":\"bedrock_floor\",\"in\":{\"model_id\":", ++cs);
    {
        putchar('"');
        for (const char *p = id; *p; p++) {
            unsigned char ch = (unsigned char)*p;
            if (ch == '"' || ch == '\\') {
                putchar('\\');
                putchar(ch);
            } else {
                putchar(ch);
            }
        }
        putchar('"');
    }
    printf("},\"out\":%.17g}\n", r);
    fflush(stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    const int C = 7;                        /* current_image_token_cost stub */

    /* _payload_chars */
    payload_case("null", C);
    payload_case("\"hello\"", C);
    payload_case("\"\"", C);
    payload_case("5", C);
    payload_case("-2.5", C);
    payload_case("true", C);
    payload_case("false", C);
    payload_case("[]", C);
    payload_case("[1,2,3]", C);
    payload_case("[\"a\"]", C);
    payload_case("{}", C);
    payload_case("{\"a\":1}", C);
    payload_case("{\"a\":\"bb\",\"c\":null}", C);
    payload_case("{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:...\"}}", C);
    payload_case("{\"type\":\"input_image\",\"image\":\"x\"}", C);
    payload_case("{\"type\":\"image\",\"source\":{}}", C);
    payload_case("{\"type\":\"image\",\"file_id\":\"f\"}", C);
    payload_case("{\"type\":\"image\"}", C);
    payload_case("{\"type\":{\"nested\":1},\"image_url\":{}}", C);
    payload_case("{\"type\":\"text\",\"text\":\"abc\"}", C);
    payload_case("{\"type\":\"image_url\",\"text\":\"abc\"}", C);
    payload_case("[[1],[2]]", C);
    payload_case("{\"a\":{\"b\":{\"c\":1}}}", C);
    payload_case("[{\"type\":\"image_url\",\"image_url\":{}}]", C);

    /* _image_part_chars */
    image_case("{}", C);
    image_case("{\"text\":\"abcd\"}", C);
    image_case("{\"text\":\"\"}", C);
    image_case("{\"text\":5}", C);
    /* a null part makes Python raise AttributeError -- not a contract */
    image_case("{\"type\":\"image_url\"}", C);

    /* _scale_stale_timeout_for_context */
    scale_case(60.0, 0);
    scale_case(60.0, 10000);
    scale_case(60.0, 10001);
    scale_case(60.0, 50000);
    scale_case(60.0, 50001);
    scale_case(60.0, 100000);
    scale_case(60.0, 100001);
    scale_case(400.0, 200000);
    scale_case(250.0, 60000);

    /* _bound_openai_codex_stale_timeout */
    bound_case(60.0, 0, 1500.0);
    bound_case(60.0, 10001, 1500.0);
    bound_case(60.0, 60000, 1500.0);
    bound_case(60.0, 200000, 1500.0);
    bound_case(2000.0, 200000, 1500.0);
    bound_case(2000.0, 200000, 0.0);
    bound_case(2000.0, 200000, -1.0);
    bound_case(1200.0, 200000, 1500.0);
    bound_case(300.0, 60000, 1500.0);

    /* _bedrock_reasoning_stale_floor */
    bedrock_case("");
    bedrock_case("us.anthropic.claude-opus-4-6-v1:0");
    bedrock_case("global.anthropic.claude-sonnet-4-5-v1:0");
    bedrock_case("eu.anthropic.claude-sonnet-4.6-v1:0");
    bedrock_case("apac.amazon.nova-pro-v1:0");
    bedrock_case("us.meta.llama4-maverick-17b-instruct-v1:0");
    bedrock_case("deepseek-r1-v1:0");
    bedrock_case("us.deepseek.deepseek-r1-v1:0");
    bedrock_case("anthropic.claude-opus-4-8");
    bedrock_case("claude-opus-4-6");
    bedrock_case("qwen3-235b-a22b");
    bedrock_case("some-other-qwen3");
    bedrock_case("llama-4-70b-o1-preview");
    bedrock_case("o3-mini");
    bedrock_case("o3");
    bedrock_case("openrouter/qwen3");
    bedrock_case("  US.Anthropic.Claude-Opus-4-6-v1:0  ");
    bedrock_case("unknown-model-v1:0");
    bedrock_case("af.grok-4-fast-reasoning");

    return 0;
}
