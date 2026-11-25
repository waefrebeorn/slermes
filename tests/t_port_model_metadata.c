/* t_port_model_metadata.c -- oracle harness for the pure slice of
 * agent/model_metadata.py. Emits one JSON line per case. */
#include "port_model_metadata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cs = 0;

static void jstr(const char *s)
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

static void str_case(const char *fn, const char *in_json, char *out)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":", ++cs, fn, in_json);
    if (out)
        jstr(out);
    else
        printf("null");
    printf("}\n");
    fflush(stdout);
    free(out);
}

static void bool_case(const char *fn, const char *in_json, int out)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":%s}\n",
           ++cs, fn, in_json, out ? "true" : "false");
    fflush(stdout);
}

static void in1(const char *key, const char *val, char *buf, size_t cap)
{
    snprintf(buf, cap, "{\"%s\":", key);
    snprintf(buf + strlen(buf), cap - strlen(buf), "\"%s\"}", val);
}

static void run(const char *fn, const char *key, const char *val)
{
    char in[512];
    in1(key, val, in, sizeof(in));
    if (!strcmp(fn, "normalize_base_url"))
        str_case(fn, in, mm_normalize_base_url(val));
    else if (!strcmp(fn, "localhost_ipv4"))
        str_case(fn, in, mm_localhost_to_ipv4(val));
    else if (!strcmp(fn, "server_root"))
        str_case(fn, in, mm_server_root(val));
    else if (!strcmp(fn, "lmstudio_root"))
        str_case(fn, in, mm_lmstudio_server_root(val));
    else if (!strcmp(fn, "grok_effort"))
        bool_case(fn, in, mm_grok_supports_reasoning_effort(val));
    else if (!strcmp(fn, "openai_rejects"))
        bool_case(fn, in, mm_openai_model_rejects_reasoning(val));
    else if (!strcmp(fn, "grok_46"))
        bool_case(fn, in, mm_is_grok_46_family(val));
    else if (!strcmp(fn, "fast_mode"))
        bool_case(fn, in, mm_is_anthropic_fast_mode_model(val));
    else if (!strcmp(fn, "local_endpoint"))
        bool_case(fn, in, mm_is_local_endpoint(val));
    else if (!strcmp(fn, "bare_codex_slug"))
        str_case(fn, in, mm_bare_codex_slug(val));
    else if (!strcmp(fn, "codex_variant_base"))
        str_case(fn, in, mm_codex_variant_base(val));
    else if (!strcmp(fn, "suggests_minimax"))
        bool_case(fn, in, mm_model_name_suggests_minimax(val));
    else if (!strcmp(fn, "stale_32k"))
        bool_case(fn, in, mm_model_name_suggests_stale_32k_underreport(val));
}

static void key_case(const char *key, const char *model_lower)
{
    char in[512];
    snprintf(in, sizeof(in), "{\"key\":\"%s\",\"model_lower\":\"%s\"}",
             key, model_lower);
    bool_case("catalog_key", in, mm_catalog_key_matches(key, model_lower));
}

static void longest_case(const char *table_json, const char *model_lower)
{
    json_t *t = json_parse(table_json, NULL);
    char keybuf[256];
    json_t *hit = mm_longest_key_match(t, model_lower, keybuf, sizeof(keybuf));
    printf("{\"case\":%d,\"fn\":\"longest_key\",\"in\":{\"table\":%s,"
           "\"model_lower\":\"%s\"},\"out\":",
           ++cs, table_json, model_lower);
    if (hit)
        printf("[");
    if (hit) {
        jstr(keybuf);
        printf(",");
        printf("%s", json_serialize(hit));
    }
    if (hit)
        printf("]");
    else
        printf("null");
    printf("}\n");
    fflush(stdout);
    json_free(t);
}

/* ---- batch 2 emitters ---------------------------------------------------- */

static void out_int_case(const char *fn, const char *in_json, int found,
                         long long v)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":", ++cs, fn, in_json);
    if (found)
        printf("%lld", v);
    else
        printf("null");
    printf("}\n");
    fflush(stdout);
}

/* Serializes out (or null) and frees it. */
static void out_json_case(const char *fn, const char *in_json, json_t *out)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":", ++cs, fn, in_json);
    if (out) {
        char *s = json_serialize(out);
        printf("%s", s ? s : "null");
        free(s);
    } else {
        printf("null");
    }
    printf("}\n");
    fflush(stdout);
    json_free(out);
}

static void payload_case(const char *fn, const char *payload_json)
{
    char in[1024];
    snprintf(in, sizeof(in), "{\"payload\":%s}", payload_json);
    json_t *p = json_parse(payload_json, NULL);
    long long v = 0;
    int found = strcmp(fn, "flat_ctx") == 0
                    ? mm_extract_flat_context_length(p, &v)
                    : mm_context_length_from_model_payload(p, &v);
    out_int_case(fn, in, found, v);
    json_free(p);
}

static void rates_case(const char *pricing_json, const char *unit_json)
{
    char in[1024];
    snprintf(in, sizeof(in), "{\"pricing\":%s,\"unit\":%s}", pricing_json,
             unit_json);
    json_t *p = json_parse(pricing_json, NULL);
    json_t *u = json_parse(unit_json, NULL);
    out_json_case("token_rates", in, mm_normalize_token_rates(p, u));
    json_free(u);
}

static void posint_case(const char *value_json)
{
    char in[512];
    snprintf(in, sizeof(in), "{\"value\":%s}", value_json);
    json_t *v = json_parse(value_json, NULL);
    long long out = 0;
    int found = mm_positive_int(v, &out);
    out_int_case("positive_int", in, found, out);
    json_free(v);
}

static void phrase_case(const char *text, const char *groups_json)
{
    char in[1024];
    snprintf(in, sizeof(in), "{\"text\":\"%s\",\"groups\":%s}", text,
             groups_json);
    json_t *g = json_parse(groups_json, NULL);
    bool_case("phrase_group", in, mm_any_phrase_group(text, g));
    json_free(g);
}

static void split_case(const char *err)
{
    char in[1024];
    snprintf(in, sizeof(in), "{\"error_lower\":\"%s\"}", err);
    long long v = 0;
    /* Call BEFORE passing v: sibling printf-style arguments have unspecified
     * evaluation order — reading v in the same call list read the stale 0. */
    int found = mm_completion_split_budget(err, &v);
    out_int_case("split_budget", in, found, v);
}

static void sglang_case(const char *err)
{
    char in[1024];
    snprintf(in, sizeof(in), "{\"error_lower\":\"%s\"}", err);
    long long w = 0, i = 0;
    int found = mm_sglang_window_and_input(err, &w, &i);
    printf("{\"case\":%d,\"fn\":\"sglang\",\"in\":%s,\"out\":", ++cs, in);
    if (found)
        printf("[%lld,%lld]", w, i);
    else
        printf("null");
    printf("}\n");
    fflush(stdout);
}

static void codex_route_case(const char *provider, const char *base_url,
                             const char *cp_json)
{
    char in[2048];
    snprintf(in, sizeof(in),
             "{\"provider\":\"%s\",\"base_url\":\"%s\",\"custom_providers\":%s}",
             provider, base_url, cp_json);
    json_t *cp = json_parse(cp_json, NULL);
    bool_case("codex_route", in, mm_is_codex_route(provider, base_url, cp));
    json_free(cp);
}

static void strip_or_case(const char *model, const char *base_url,
                          const char *provider)
{
    char in[768];
    snprintf(in, sizeof(in),
             "{\"model\":\"%s\",\"base_url\":\"%s\",\"provider\":\"%s\"}",
             model, base_url, provider);
    str_case("strip_or_variant", in,
             mm_strip_openrouter_routing_variant(model, base_url, provider));
}

static void bedrock_case(const char *base_url, const char *provider)
{
    char in[512];
    snprintf(in, sizeof(in), "{\"base_url\":\"%s\",\"provider\":\"%s\"}",
             base_url, provider);
    bool_case("bedrock_ctx", in, mm_is_bedrock_context(base_url, provider));
}

static void codex_key_case(const char *keys_json, const char *slug)
{
    char in[768];
    snprintf(in, sizeof(in), "{\"keys\":%s,\"slug\":\"%s\"}", keys_json, slug);
    json_t *k = json_parse(keys_json, NULL);
    str_case("codex_catalog_key", in, mm_codex_catalog_key(k, slug));
    json_free(k);
}

static void parse_url_case(const char *base_url, const char *scheme)
{
    char in[512];
    snprintf(in, sizeof(in), "{\"base_url\":\"%s\",\"scheme\":\"%s\"}",
             base_url, scheme);
    mm_parsed_url u;
    int ok = mm_parse_base_url(base_url, scheme, &u);
    printf("{\"case\":%d,\"fn\":\"parse_base_url\",\"in\":%s,\"out\":", ++cs,
           in);
    if (!ok) {
        printf("null}\n");
        fflush(stdout);
        return;
    }
    printf("{\"scheme\":");
    jstr(u.scheme);
    printf(",\"netloc\":");
    jstr(u.netloc);
    printf(",\"path\":");
    jstr(u.path);
    printf(",\"params\":");
    jstr(u.params);
    printf(",\"query\":");
    jstr(u.query);
    printf(",\"fragment\":");
    jstr(u.fragment);
    printf(",\"hostname\":");
    if (u.hostname[0])
        jstr(u.hostname);
    else
        printf("null");
    printf(",\"port\":");
    if (u.port >= 0)
        printf("%ld", u.port);
    else
        printf("null");
    printf(",\"port_invalid\":%s", u.port_invalid ? "true" : "false");
    printf(",\"username\":");
    if (!u.username_none)
        jstr(u.username);
    else
        printf("null");
    printf(",\"password\":");
    if (!u.password_none)
        jstr(u.password);
    else
        printf("null");
    printf("}\n");
    fflush(stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);

    /* _normalize_base_url */
    run("normalize_base_url", "base_url", "");
    run("normalize_base_url", "base_url", "   ");
    run("normalize_base_url", "base_url", "http://x");
    run("normalize_base_url", "base_url", "  http://x/  ");
    run("normalize_base_url", "base_url", "http://x///");
    run("normalize_base_url", "base_url", "http://x/y/");

    /* grok_supports_reasoning_effort */
    run("grok_effort", "model", "");
    run("grok_effort", "model", "grok-3-mini");
    run("grok_effort", "model", "x-ai/grok-4.6-2025");
    run("grok_effort", "model", " openrouter/grok-4.3-latest ");
    run("grok_effort", "model", "grok-4");
    run("grok_effort", "model", "grok-4.20-multi-agent-x");
    run("grok_effort", "model", "GROK-4.5");
    run("grok_effort", "model", "llama-3");

    /* openai_model_rejects_reasoning */
    run("openai_rejects", "model", "");
    run("openai_rejects", "model", "gpt-3.5-turbo");
    run("openai_rejects", "model", "openai/gpt-4");
    run("openai_rejects", "model", "gpt-4-turbo");
    run("openai_rejects", "model", "gpt-4o");
    run("openai_rejects", "model", "gpt-40");
    run("openai_rejects", "model", "gpt-4.1");
    run("openai_rejects", "model", "chatgpt-4o-latest");
    run("openai_rejects", "model", "ft:gpt-4:org::id");
    run("openai_rejects", "model", "ft:gpt-5:org::id");
    run("openai_rejects", "model", "gpt-5");
    run("openai_rejects", "model", "GPT-4");

    /* is_grok_46_family */
    run("grok_46", "model", "");
    run("grok_46", "model", "grok-4.6");
    run("grok_46", "model", "grok-4.6-latest");
    run("grok_46", "model", "x-ai/grok-4.6");
    run("grok_46", "model", "grok_4.6");
    run("grok_46", "model", "grok-4.60");
    run("grok_46", "model", "grok-4.5");
    run("grok_46", "model", "GROK-4.6");

    /* is_anthropic_fast_mode_model */
    run("fast_mode", "model", "");
    run("fast_mode", "model", "claude-opus-4-8");
    run("fast_mode", "model", "claude-opus-5");
    run("fast_mode", "model", "claude-opus-5-5");
    run("fast_mode", "model", "anthropic/claude-opus-5.5");
    run("fast_mode", "model", "claude-opus-4-8-20260601");
    run("fast_mode", "model", "claude-opus-4-8:beta");
    run("fast_mode", "model", "claude-opus-4-7");
    run("fast_mode", "model", "claude-opus-4-6");
    run("fast_mode", "model", "claude-sonnet-4-5");

    /* _catalog_key_matches */
    key_case("glm", "z-ai/glm-5-3");
    key_case("glm-5.3", "z-ai/glm-5-3");
    key_case("glm-5.3", "z-ai/glm-53");
    key_case("gpt-5", "openai/gpt-5.4-mini");
    key_case("gpt-5.4-mini", "openai/gpt-5.4-mini");
    key_case("claude", "anthropic/claude-opus-4-6");
    key_case("opus", "claude-opus-4-6");
    key_case("solar-mini", "solar-mini4");
    key_case("solar-mini", "solar-mini-pro");
    key_case("solar-pro-", "solar-pro-3");
    key_case("solar-pro-", "solar-pro-3a");
    key_case("solar-pro-", "solar-pro-31");
    key_case("zzz", "abc");

    /* _longest_key_match */
    longest_case("{\"gpt-5\":1,\"gpt-5.4-mini\":2}", "openai/gpt-5.4-mini");
    longest_case("{\"gpt-5.4-mini\":2,\"gpt-5\":1}", "openai/gpt-5.4-mini");
    longest_case("{\"a\":1,\"bb\":2,\"bb\":3}", "xxbbxx");
    longest_case("{\"a\":1}", "zzz");
    longest_case("{}", "zzz");
    longest_case("{\"gpt-4\":10,\"gpt-4o\":20}", "openai/gpt-4o-mini");

    /* is_local_endpoint */
    run("local_endpoint", "base_url", "");
    run("local_endpoint", "base_url", "http://localhost:11434");
    run("local_endpoint", "base_url", "http://127.0.0.1:8000/v1");
    run("local_endpoint", "base_url", "http://[::1]:8000");
    run("local_endpoint", "base_url", "http://0.0.0.0:8000");
    run("local_endpoint", "base_url", "http://ollama:11434");
    run("local_endpoint", "base_url", "http://printer.local:631");
    run("local_endpoint", "base_url", "http://host.docker.internal:8080");
    run("local_endpoint", "base_url", "http://10.0.0.5:8080");
    run("local_endpoint", "base_url", "http://172.16.0.1:8080");
    run("local_endpoint", "base_url", "http://172.32.0.1:8080");
    run("local_endpoint", "base_url", "http://192.168.1.1:8080");
    run("local_endpoint", "base_url", "http://100.64.0.1:8080");
    run("local_endpoint", "base_url", "http://100.128.0.1:8080");
    run("local_endpoint", "base_url", "http://8.8.8.8:53");
    run("local_endpoint", "base_url", "https://api.openai.com/v1");
    run("local_endpoint", "base_url", "http://[fd00::1]/v1");
    run("local_endpoint", "base_url", "http://[fe80::1]/v1");
    run("local_endpoint", "base_url", "http://[2001:4860:4860::8888]/v1");

    /* _localhost_to_ipv4 */
    run("localhost_ipv4", "url", "http://localhost:11434");
    run("localhost_ipv4", "url", "https://localhost/v1");
    run("localhost_ipv4", "url", "http://localhost");
    run("localhost_ipv4", "url", "http://localhostness:80");
    run("localhost_ipv4", "url", "http://x?upstream=http://localhost");
    run("localhost_ipv4", "url", "HTTP://localhost:8080");
    run("localhost_ipv4", "url", "http://127.0.0.1:80");

    /* _server_root / _lmstudio_server_root */
    run("server_root", "base_url", "http://localhost:11434/v1/");
    run("server_root", "base_url", "http://localhost:11434");
    run("server_root", "base_url", "http://127.0.0.1:8000/v1");
    run("lmstudio_root", "base_url", "http://localhost:1234/v1");
    run("lmstudio_root", "base_url", "http://localhost:1234/api/v1");
    run("lmstudio_root", "base_url", "http://localhost:1234/api");
    run("lmstudio_root", "base_url", "http://localhost:1234");
    run("lmstudio_root", "base_url", "http://localhost:1234/");

    /* ---- batch 2: pure gap ports ---- */

    /* _strip_openrouter_routing_variant */
    strip_or_case("x-ai/grok-4.6:nitro", "https://openrouter.ai/api/v1", "");
    strip_or_case("x-ai/grok-4.6:nitro", "", "openrouter");
    strip_or_case("x-ai/grok-4.6:nitro", "", " OpenRouter ");
    strip_or_case("x-ai/grok-4.6:nitro", "http://localhost:11434", "");
    strip_or_case("x-ai/grok-4:free", "https://openrouter.ai/api/v1", "");
    strip_or_case("model:NITRO", "https://openrouter.ai/api/v1", "");
    strip_or_case(":nitro", "https://openrouter.ai/api/v1", "");
    strip_or_case("", "https://openrouter.ai/api/v1", "openrouter");

    /* _is_codex_route */
    codex_route_case("openai-codex", "", "[]");
    codex_route_case(" OpenAI-Codex ", "", "[]");
    codex_route_case("openai", "http://127.0.0.1:8080/v1",
                     "[{\"base_url\":\"http://127.0.0.1:8080/v1\","
                     "\"api_mode\":\"codex_responses\"}]");
    codex_route_case("", "http://127.0.0.1:8080/v1/",
                     "[{\"base_url\":\"http://127.0.0.1:8080/v1\","
                     "\"api_mode\":\"responses\"}]");
    codex_route_case("", "http://127.0.0.1:8080/v1",
                     "[{\"base_url\":\"http://127.0.0.1:8080/v1\","
                     "\"api_mode\":\"openai\"}]");
    codex_route_case("", "http://127.0.0.1:8080/v1",
                     "[{\"base_url\":\"http://127.0.0.1:8080/v1\","
                     "\"transport\":\"codex_responses\"}]");
    codex_route_case("", "http://127.0.0.1:8080/v1", "[]");
    codex_route_case("", "",
                     "[{\"base_url\":\"http://x/v1\","
                     "\"api_mode\":\"codex_responses\"}]");
    codex_route_case("custom", "HTTP://LOCALHOST:8080/v1",
                     "[{\"base_url\":\"http://localhost:8080/v1\","
                     "\"api_mode\":\"  \",\"transport\":\"openai-responses\"}]");

    /* _extract_flat_context_length */
    payload_case("flat_ctx", "{\"context_length\":131072}");
    payload_case("flat_ctx", "{\"context_window\":\"204800\"}");
    payload_case("flat_ctx", "{\"max_tokens\":65536}");
    payload_case("flat_ctx", "{\"context_length\":512}");
    payload_case("flat_ctx", "{\"context_length\":true,\"n_ctx\":4096}");
    payload_case("flat_ctx", "{\"context_length\":\"1,048,576\"}");
    payload_case("flat_ctx", "{\"nested\":{\"context_length\":999999}}");
    payload_case("flat_ctx", "{\"max_model_len\":8192.9}");

    /* _context_length_from_model_payload */
    payload_case("model_payload_ctx",
                 "{\"max_input_tokens\":1000000,\"max_tokens\":128000}");
    payload_case("model_payload_ctx", "{\"max_tokens\":65536}");
    payload_case("model_payload_ctx", "{\"max_tokens\":0}");
    payload_case("model_payload_ctx", "{\"max_tokens\":\"500\"}");
    payload_case("model_payload_ctx", "{\"max_tokens\":true}");
    payload_case("model_payload_ctx", "{\"max_tokens\":128000.7}");
    payload_case("model_payload_ctx", "{}");
    payload_case("model_payload_ctx", "{\"context_length\":2048,\"max_tokens\":999}");

    /* _normalize_token_rates */
    rates_case("{\"prompt\":\"3\",\"completion\":\"15\"}", "\"per_1m_tokens\"");
    rates_case("{\"prompt\":0.0000005,\"completion\":0.0000015}", "null");
    rates_case("{\"prompt\":0.003,\"completion\":0.015}", "null");
    rates_case("{\"prompt\":\"2.5\",\"completion\":\"10\",\"request\":\"0.01\"}",
               "\"per_1k_tokens\"");
    rates_case("{\"prompt\":\"0.000001\",\"cache_read\":\"0.0000001\"}",
               "\" Per_Million_Tokens \"");
    rates_case("{\"prompt\":\"abc\",\"completion\":4}", "\"per_1m_tokens\"");
    rates_case("{\"prompt\":3,\"completion\":15}", "\"per_token\"");

    /* _completion_split_budget */
    split_case("maximum context length is 131072 tokens "
               "(60000 in the messages, 65536 in the completion)");
    split_case("maximum context length is 4096 tokens "
               "(771 in your prompt; 4000 for the completion)");
    split_case("maximum context length is 4096 tokens "
               "(4096 in the messages, 100 in the completion)");
    split_case("maximum context length is 4096 tokens "
               "(5000 in the messages, 100 in the completion)");
    split_case("prompt is too long: 5000 tokens");
    split_case("maximum context length is 8192 tokens "
               "(1000 tokens in the prompt, 2000 tokens for the completion)");
    split_case("(100 in the messages, 200 in the completion)");

    /* _sglang_window_and_input */
    sglang_case("maximum context length of 131072 tokens. you requested a "
                "total of 132528 tokens: 66992 tokens from the input messages "
                "and 65536 tokens for the completion");
    sglang_case("maximum context length of 8192 token. "
                "8000 tokens from the input messages");
    sglang_case("maximum context length of 8192 tokens");
    sglang_case("66992 tokens from the input messages");
    sglang_case("maximum context length of 8192 tokens and "
                "100tokens from the input messages");

    /* _any_phrase_group */
    phrase_case("range of max_tokens should be [1, 65536]",
                "[[\"range of max_tokens should be\"]]");
    phrase_case("maximum context length is 8192 (100 in the output)",
                "[[\"in the output\",\"maximum context length\"]]");
    phrase_case("maximum context length is 8192",
                "[[\"in the output\",\"maximum context length\"]]");
    phrase_case("anything", "[]");
    phrase_case("anything", "[[]]");
    phrase_case("limited to 16384 for glm", "[[\"nope\"],[\"limited to\"]]");

    /* _model_name_suggests_minimax */
    run("suggests_minimax", "model", "minimax-m3");
    run("suggests_minimax", "model", "MiniMaxAI/MiniMax-M3");
    run("suggests_minimax", "model", "minimax");
    run("suggests_minimax", "model", " minimax");
    run("suggests_minimax", "model", "mini-max");
    run("suggests_minimax", "model", "ab/minimax-m2");

    /* _model_name_suggests_stale_32k_underreport */
    run("stale_32k", "model", "kimi-k3");
    run("stale_32k", "model", "moonshotai/kimi-k3");
    run("stale_32k", "model", "minimax-m3");
    run("stale_32k", "model", "gpt-5");
    run("stale_32k", "model", "KIMI-K2");
    run("stale_32k", "model", " kimi");

    /* _positive_int */
    posint_case("4096");
    posint_case("0");
    posint_case("-5");
    posint_case("2.9");
    posint_case("0.5");
    posint_case("true");
    posint_case("false");
    posint_case("\"4096\"");
    posint_case("null");

    /* _bare_codex_slug */
    run("bare_codex_slug", "model", "openai/gpt-5.6-sol-900k");
    run("bare_codex_slug", "model", "  GPT-5.6-Sol  ");
    run("bare_codex_slug", "model", "");
    run("bare_codex_slug", "model", "gpt-5.4");
    run("bare_codex_slug", "model", "a/b/gpt-6-sol");

    /* _codex_variant_base */
    run("codex_variant_base", "model", "gpt-5.6-sol-900k");
    run("codex_variant_base", "model", "openai/GPT-5.6-SOL-900K");
    run("codex_variant_base", "model", "gpt-5.5-900k");
    run("codex_variant_base", "model", "gpt-5.6-sol");
    run("codex_variant_base", "model", "gpt-5.6-sol-2026-07-09-900k");
    run("codex_variant_base", "model", "gpt-5.6-sol-900k-900k");
    run("codex_variant_base", "model", "");

    /* _is_bedrock_context */
    bedrock_case("", "bedrock");
    bedrock_case("https://bedrock-runtime.us-east-1.amazonaws.com", "");
    bedrock_case("bedrock-runtime.eu-west-1.amazonaws.com", "");
    bedrock_case("https://bedrock-runtime.us-east-1.amazonaws.com.evil.com", "");
    bedrock_case("https://api.openai.com/v1", "bedrock");
    bedrock_case("https://evil.com/bedrock-runtime.amazonaws.com", "");
    bedrock_case("", "");
    bedrock_case("HTTPS://BEDROCK-RUNTIME.US-EAST-1.AMAZONAWS.COM.", "");

    /* _parse_base_url */
    parse_url_case("https://api.openai.com/v1", "http");
    parse_url_case("localhost:11434", "http");
    parse_url_case("", "http");
    parse_url_case("  http://x/  ", "http");
    parse_url_case("http://user:***@host:8080/p;params?q=1#frag", "http");
    parse_url_case("https://[::1]:8443/v1", "http");
    parse_url_case("host/path", "https");
    parse_url_case("http://host:99999", "http");

    /* _codex_catalog_key */
    codex_key_case("{\"gpt-5.6-sol\":272000}", "gpt-5.6-sol");
    codex_key_case("{\"GPT-5.6-Sol\":272000}", "gpt-5.6-sol");
    codex_key_case("{\"gpt-5.4\":1}", "gpt-5.6-sol");
    codex_key_case("[\"GPT-5.6-Sol\",\"gpt-5.4\"]", "gpt-5.6-sol");
    codex_key_case("{}", "x");
    codex_key_case("{\"a\":1,\"A\":2}", "a");

    return 0;
}
