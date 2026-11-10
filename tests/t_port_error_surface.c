/*
 * t_port_error_surface.c — Oracle harness for agent/error_surface.py
 * (ported to src/agent/port_error_surface.c).
 */
#include "port_error_surface.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_json_str(const char *s)
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

static void emit_pred_int(const char *family, const char *name, int v,
                          const char *arg)
{
    printf("{\"family\":\"%s\",\"case\":\"%s\",\"arg\":", family, name);
    emit_json_str(arg);
    printf(",\"ret\":%d}\n", v ? 1 : 0);
}

static void emit_layer(const char *name, const char *reason,
                       const char *error, const char *provider)
{
    const char *layer = esc_result_layer(reason, error, provider);
    printf("{\"family\":\"layer\",\"case\":\"%s\",\"reason\":", name);
    emit_json_str(reason);
    printf(",\"error\":");
    emit_json_str(error);
    printf(",\"provider\":");
    emit_json_str(provider);
    printf(",\"layer\":");
    emit_json_str(layer);
    printf("}\n");
}

static void emit_result(const char *name, const char *doc)
{
    json_t *s = esc_build_from_result(doc, "", "", NULL, NULL, NULL);
    printf("{\"family\":\"result\",\"case\":\"%s\",\"doc\":", name);
    emit_json_str(doc);
    printf(",\"surface\":");
    if (!s) fputs("null", stdout);
    else {
        char *ser = json_serialize(s);
        fputs(ser ? ser : "null", stdout);
        free(ser);
        json_free(s);
    }
    printf("}\n");
}

int main(void)
{
    static const char *const providers[] = {
        NULL, "", "custom", " Custom ", "custom:my-endpoint", "ollama", "lmstudio",
        "vllm", "llama.cpp", "llamacpp", "local", "openai", "anthropic", "customary",
    };
    static const char *const messages[] = {
        NULL, "", "peer closed connection", "The SSE stream died", "MID-STREAM cut",
        "incomplete chunked read", "connection broken", "stream ended prematurely",
        "stream connection reset", "all good", "No space left on device",
        "database or disk is full", "ENOSPC", "disk full", "full disk",
        "not enough space", "diskfull", "SQLITE_FULL",
    };

    setvbuf(stdout, NULL, _IONBF, 0);

    for (size_t i = 0; i < sizeof(providers) / sizeof(providers[0]); i++)
        emit_pred_int("custom_endpoint", providers[i] ? providers[i] : "null",
                      esc_is_custom_endpoint(providers[i]), providers[i]);
    for (size_t i = 0; i < sizeof(messages) / sizeof(messages[0]); i++)
        emit_pred_int("stream_drop", messages[i] ? messages[i] : "null",
                      esc_looks_like_stream_drop(messages[i]), messages[i]);
    for (size_t i = 0; i < sizeof(messages) / sizeof(messages[0]); i++)
        emit_pred_int("disk_full", messages[i] ? messages[i] : "null",
                      esc_disk_full(messages[i]), messages[i]);

    emit_layer("auth", "auth", "", NULL);
    emit_layer("billing", "billing", "", NULL);
    emit_layer("loop_error", "loop_error", "", NULL);
    emit_layer("local_runtime", "local_runtime_unavailable", "", NULL);
    emit_layer("truncated", "truncated", "", NULL);
    emit_layer("timeout_plain", "timeout", "", NULL);
    emit_layer("timeout_custom", "timeout", "", "custom:box");
    emit_layer("timeout_ollama", "timeout", "", "ollama");
    emit_layer("ssl_plain", "ssl_cert_verification", "", "openai");
    emit_layer("ssl_custom", "ssl_cert_verification", "", "vllm");
    emit_layer("unknown_stream", "weird_reason", "peer closed connection", NULL);
    emit_layer("unknown_plain", "weird_reason", "boom", NULL);
    emit_layer("empty_reason_stream", "", "sse died", NULL);
    emit_layer("empty_reason_plain", "", "boom", NULL);

    emit_result("empty_object", "{}");
    emit_result("no_signal", "{\"failure_reason\":\"\",\"error\":\"\"}");
    emit_result("unknown_error", "{\"error\":\"boom\"}");
    emit_result("stream_error", "{\"error\":\"peer closed connection\"}");
    emit_result("disk_error", "{\"error\":\"No space left on device\"}");
    emit_result("disk_beats_reason", "{\"error\":\"ENOSPC\",\"failure_reason\":\"auth\"}");
    emit_result("auth_reason", "{\"failure_reason\":\"auth\"}");
    emit_result("auth_explicit_retryable",
                "{\"failure_reason\":\"auth\",\"failure_retryable\":true}");
    emit_result("timeout_reason", "{\"failure_reason\":\"timeout\"}");
    emit_result("padded_reason", "{\"failure_reason\":\"  context_overflow  \"}");
    emit_result("billing_flag", "{\"billing_block\":true}");
    emit_result("billing_reason", "{\"failure_reason\":\"billing\"}");
    emit_result("billing_window",
                "{\"billing_block\":true,\"usage_window_message\":\"window hit\","
                "\"failure_resets_at\":1700000000}");
    emit_result("billing_window_limit",
                "{\"failure_reason\":\"billing\",\"usage_window_message\":\"w\","
                "\"usage_limit\":{\"kind\":\"tokens\",\"max\":1000}}");
    emit_result("billing_window_empty",
                "{\"failure_reason\":\"billing_unverified\",\"usage_window_message\":\"\"}");
    emit_result("free_tier_retryable", "{\"free_tier\":{\"kind\":\"rate_limited\"}}");
    emit_result("free_tier_hard",
                "{\"free_tier\":{\"kind\":\"auth_required\",\"message\":\"sign in\"}}");
    emit_result("free_tier_empty_kind", "{\"free_tier\":{\"kind\":\"\"}}");
    emit_result("free_tier_not_object", "{\"free_tier\":\"nope\"}");
    emit_result("resets_at_with_reason",
                "{\"failure_reason\":\"rate_limited\",\"failure_resets_at\":1699999999}");
    emit_result("resets_at_bool",
                "{\"failure_reason\":\"rate_limited\",\"failure_resets_at\":true}");
    emit_result("not_an_object", "[]");
    emit_result("null_doc", NULL);
    return 0;
}
