/* gate: agent_bedrock_adapter_py___static_bedrock_context_length
 * Real port: longest-substring match over BEDROCK_CONTEXT_LENGTHS (-1 = None).
 * The table below is generated from the live python BEDROCK_CONTEXT_LENGTHS. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern long agent_bedrock_adapter_u_static_bedrock_context_length(const char *model, const char *table_json);
static const char *TABLE = "{\"ai21.jamba-1-5\": 256000, \"amazon.nova-2-lite\": 1000000, \"amazon.nova-lite\": 300000, \"amazon.nova-micro\": 128000, \"amazon.nova-pro\": 300000, \"anthropic.claude-3-5-haiku\": 200000, \"anthropic.claude-3-5-sonnet\": 200000, \"anthropic.claude-3-haiku\": 200000, \"anthropic.claude-3-opus\": 200000, \"anthropic.claude-3-sonnet\": 200000, \"anthropic.claude-fable\": 1000000, \"anthropic.claude-fable-5\": 1000000, \"anthropic.claude-haiku-4-5\": 200000, \"anthropic.claude-mythos\": 1000000, \"anthropic.claude-opus-4\": 200000, \"anthropic.claude-opus-4-6\": 1000000, \"anthropic.claude-opus-4-7\": 1000000, \"anthropic.claude-opus-4-8\": 1000000, \"anthropic.claude-opus-5\": 1000000, \"anthropic.claude-sonnet-4\": 200000, \"anthropic.claude-sonnet-4-5\": 200000, \"anthropic.claude-sonnet-4-6\": 1000000, \"anthropic.claude-sonnet-5\": 1000000, \"deepseek.v3\": 128000, \"google.gemma-4-26b\": 256000, \"google.gemma-4-31b\": 256000, \"meta.llama3-3-70b-instruct\": 128000, \"meta.llama4-maverick\": 128000, \"meta.llama4-scout\": 128000, \"minimax.minimax-m2.1\": 196000, \"minimax.minimax-m2.5\": 196000, \"mistral.devstral-2\": 256000, \"mistral.mistral-large\": 128000, \"mistral.mistral-large-3\": 256000, \"moonshot.kimi-k2-thinking\": 256000, \"moonshotai.kimi-k2-thinking\": 256000, \"moonshotai.kimi-k2.5\": 256000, \"moonshotai.kimi-k3\": 1000000, \"openai.gpt-5.4\": 1050000, \"openai.gpt-5.5\": 1050000, \"openai.gpt-5.6-luna\": 1050000, \"openai.gpt-5.6-sol\": 1050000, \"openai.gpt-5.6-terra\": 1050000, \"openai.gpt-6-astra\": 1050000, \"openai.gpt-6-luna\": 1050000, \"openai.gpt-6-sol\": 1050000, \"openai.gpt-6.1-sol\": 1000000, \"openai.gpt-6.1-terra\": 1050000, \"qwen.qwen3-coder\": 256000, \"xai.grok-4.6\": 500000, \"zai.glm-4.7\": 203000, \"zai.glm-5\": 200000, \"zai.glm-5.3\": 1000000}";
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <model>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("agent_bedrock_adapter_u_static_bedrock_context_length");
        fflush(stdout);
        long r = agent_bedrock_adapter_u_static_bedrock_context_length(argv[i], TABLE);
        WCUP_VALINT(r);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("agent_bedrock_adapter_u_static_bedrock_context_length");
    }
    return 0;
}
