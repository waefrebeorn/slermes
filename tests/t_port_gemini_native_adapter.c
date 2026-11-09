/*
 * t_port_gemini_native_adapter.c -- oracle harness for
 * port_gemini_native_adapter.c
 */
#include "port_gemini_native_adapter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else putchar(*p);
    }
    putchar('"');
}

static void emit_bare(const char *label, const char *model)
{
    char *out = gna_bare_model_id(model);
    printf("{\"fn\":\"bare_gemini_model_id\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(out);
    printf("}\n");
    free(out);
}

static void emit_version(const char *label, const char *model)
{
    int maj = 0, min = 0;
    gna_version(model, &maj, &min);
    printf("{\"fn\":\"_gemini_version\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":[%d,%d]}\n", maj, min);
}

static void emit_bool(const char *fn, const char *label, int v)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":%s}\n", v ? "true" : "false");
}

static void emit_normalize(const char *label, const char *base_url)
{
    char *out = gna_normalize_base_url(base_url);
    printf("{\"fn\":\"normalize_gemini_base_url\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(out);
    printf("}\n");
    free(out);
}

static void emit_coerce(const char *label, const char *const *items)
{
    char *out = gna_coerce_content_to_text(items);
    printf("{\"fn\":\"_coerce_content_to_text\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(out);
    printf("}\n");
    free(out);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- bare_gemini_model_id ---- */
    emit_bare("plain",     "gemini-3-pro");
    emit_bare("google",    "google/gemini-3-pro");
    emit_bare("gemini",    "gemini/gemini-3-pro");
    emit_bare("upper",     "GOOGLE/gemini-3-pro");
    emit_bare("padded",    "  gemini-3-pro  ");
    emit_bare("empty",     "");
    emit_bare("null",      NULL);
    emit_bare("prefix-only", "google/");

    /* ---- _gemini_version ---- */
    emit_version("v4-argon",  "gemini-4-argon");
    emit_version("v3-8",      "google/gemini-3.8-flash");
    emit_version("v3",        "gemini-3-pro");
    emit_version("latest",    "gemini-flash-latest");
    emit_version("exp",       "gemini-exp-1206");
    emit_version("non-gemini","gpt-4o");
    emit_version("empty",     "");
    emit_version("v2-5",      "gemini-2.5-pro");

    /* ---- version gates ---- */
    emit_bool("gemini_requires_tool_call_ids", "v3",    gna_requires_tool_call_ids("gemini-3-pro"));
    emit_bool("gemini_requires_tool_call_ids", "v2",    gna_requires_tool_call_ids("gemini-2.5-pro"));
    emit_bool("gemini_requires_tool_call_ids", "v4",    gna_requires_tool_call_ids("gemini-4-argon"));
    emit_bool("gemini_requires_tool_call_ids", "other", gna_requires_tool_call_ids("gpt-4o"));

    emit_bool("gemini_uses_thinking_level", "v3",    gna_uses_thinking_level("gemini-3-pro"));
    emit_bool("gemini_uses_thinking_level", "latest",gna_uses_thinking_level("gemini-flash-latest"));
    emit_bool("gemini_uses_thinking_level", "v2",    gna_uses_thinking_level("gemini-2.5-pro"));
    emit_bool("gemini_uses_thinking_level", "v4",    gna_uses_thinking_level("gemini-4-argon"));

    emit_bool("gemini_sampling_deprecated", "v3-6",  gna_sampling_deprecated("gemini-3.6-pro"));
    emit_bool("gemini_sampling_deprecated", "v3-5",  gna_sampling_deprecated("gemini-3.5-pro"));
    emit_bool("gemini_sampling_deprecated", "v4",    gna_sampling_deprecated("gemini-4-argon"));
    emit_bool("gemini_sampling_deprecated", "v2",    gna_sampling_deprecated("gemini-2.5-pro"));

    /* ---- base URL classification ---- */
    emit_bool("is_vertex_express_base_url", "express",  gna_is_vertex_express("https://aiplatform.googleapis.com/v1beta"));
    emit_bool("is_vertex_express_base_url", "oauth",    gna_is_vertex_express("https://aiplatform.googleapis.com/endpoints/openapi"));
    emit_bool("is_vertex_express_base_url", "generative", gna_is_vertex_express("https://generativelanguage.googleapis.com/v1beta"));
    emit_bool("is_vertex_express_base_url", "empty",    gna_is_vertex_express(""));

    emit_bool("is_native_gemini_base_url", "generative", gna_is_native_base_url("https://generativelanguage.googleapis.com/v1beta"));
    emit_bool("is_native_gemini_base_url", "openai",     gna_is_native_base_url("https://generativelanguage.googleapis.com/openai"));
    emit_bool("is_native_gemini_base_url", "express",    gna_is_native_base_url("https://aiplatform.googleapis.com/v1beta"));
    emit_bool("is_native_gemini_base_url", "proxy",      gna_is_native_base_url("http://localhost:4000/gemini"));

    emit_bool("gemini_accepts_parameters_json_schema", "v1beta", gna_accepts_parameters_schema("https://generativelanguage.googleapis.com/v1beta"));
    emit_bool("gemini_accepts_parameters_json_schema", "v1",     gna_accepts_parameters_schema("https://generativelanguage.googleapis.com/v1"));
    emit_bool("gemini_accepts_parameters_json_schema", "v1alpha",gna_accepts_parameters_schema("https://generativelanguage.googleapis.com/v1alpha"));
    emit_bool("gemini_accepts_parameters_json_schema", "other",  gna_accepts_parameters_schema("http://localhost:4000/gemini"));

    /* ---- normalize_gemini_base_url ---- */
    emit_normalize("default",   "");
    emit_normalize("generative","https://generativelanguage.googleapis.com");
    emit_normalize("has-v1beta","https://generativelanguage.googleapis.com/v1beta");
    emit_normalize("openai",    "https://generativelanguage.googleapis.com/openai");
    emit_normalize("proxy",     "http://localhost:4000/gemini");
    emit_normalize("slashes",   "https://generativelanguage.googleapis.com///");
    emit_normalize("express",   "https://aiplatform.googleapis.com");
    emit_normalize("express-v", "https://aiplatform.googleapis.com/v1beta1");
    emit_normalize("express-p", "https://aiplatform.googleapis.com/projects/p/locations/l");
    emit_normalize("publishers","https://aiplatform.googleapis.com/v1beta1/publishers/google");
    emit_normalize("extra-path","https://generativelanguage.googleapis.com/v1beta/extra");

    /* ---- error classification ---- */
    emit_bool("is_free_tier_quota_error", "hit",   gna_is_free_tier_quota_error("429 free_tier quota exceeded"));
    emit_bool("is_free_tier_quota_error", "upper", gna_is_free_tier_quota_error("FREE_TIER exhausted"));
    emit_bool("is_free_tier_quota_error", "miss",  gna_is_free_tier_quota_error("quota exceeded"));
    emit_bool("is_free_tier_quota_error", "empty", gna_is_free_tier_quota_error(""));
    emit_bool("is_free_tier_quota_error", "null",  gna_is_free_tier_quota_error(NULL));

    emit_bool("is_standard_key_auth_error", "v401", gna_is_standard_key_error(401, "expected OAuth 2 access token", "", ""));
    emit_bool("is_standard_key_auth_error", "v401r", gna_is_standard_key_error(401, "nope", "ACCESS_TOKEN_TYPE_UNSUPPORTED", ""));
    emit_bool("is_standard_key_auth_error", "v400", gna_is_standard_key_error(400, "API key not valid", "", "AIzaSyX"));
    emit_bool("is_standard_key_auth_error", "v400-not-aiza", gna_is_standard_key_error(400, "API key not valid", "", "AQ.foo"));
    emit_bool("is_standard_key_auth_error", "v403", gna_is_standard_key_error(403, "API key not valid", "", "AIzaSyX"));

    /* ---- _coerce_content_to_text ---- */
    static const char *t1[] = { "hello", "world", NULL };
    static const char *t2[] = { "only", NULL };
    static const char *t3[] = { NULL };
    emit_coerce("two", t1);
    emit_coerce("one", t2);
    emit_coerce("empty", t3);

    return 0;
}
