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

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

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

    return 0;
}
