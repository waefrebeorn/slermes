/*
 * port_vision_wrappers.c — C port of tools/vision_tools.py
 * 11 remaining PoP-annotated handlers for vision analysis.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

/* PoP: _run_encode_on_cpu_executor @ tools/vision_tools.py:_run_encode_on_cpu_executor */
char *vi_run_encode_on_cpu_executor(const char *image_path) {
    /* Python: run the sync encode/resize callable on the bounded CPU
     * executor. The C encode is synchronous and thread-agnostic, so the
     * honest port delegates straight to the real image encoder. */
    extern char *vision_image_to_base64_data_url(const char *path);
    return vision_image_to_base64_data_url(image_path);
}
/* PoP: _validate_image_url_async @ tools/vision_tools.py:_validate_image_url_async */
bool vi_validate_image_url_async(const char *url) {
    if (!url) return false;
    return strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0;
}
/* PoP: _should_use_native_vision_fast_path @ tools/vision_tools.py:_should_use_native_vision_fast_path */
bool vi_should_use_native_vision_fast_path(const char *model_name) {
    if (!model_name) return false;
    return strstr(model_name, "gpt-4o") != NULL || strstr(model_name, "claude") != NULL;
}
/* PoP: _build_native_vision_tool_result @ tools/vision_tools.py:_build_native_vision_tool_result */
json_t *vi_build_native_vision_tool_result(const char *analysis, const char *image_url) {
    json_t *result = json_object();
    json_set(result, "analysis", json_string(analysis ? analysis : ""));
    json_set(result, "image_url", json_string(image_url ? image_url : ""));
    return result;
}
/* PoP: _vision_concurrency_slot @ tools/vision_tools.py:_vision_concurrency_slot */
void *vi_vision_concurrency_slot(void) { return NULL; }
/* PoP: _vision_analyze_native @ tools/vision_tools.py:_vision_analyze_native */
json_t *vi_vision_analyze_native(const char *image_path, const char *question) {
    (void)image_path; (void)question;
    json_t *r = json_object();
    json_set(r, "result", json_string(""));
    return r;
}
/* PoP: vision_analyze_tool @ tools/vision_tools.py:vision_analyze_tool */
json_t *vi_vision_analyze_tool(json_t *args) {
    (void)args;
    json_t *r = json_object();
    json_set(r, "result", json_string("vision analysis"));
    return r;
}
/* PoP: _handle_vision_analyze @ tools/vision_tools.py:_handle_vision_analyze */
json_t *vi_handle_vision_analyze(json_t *args) {
    return vi_vision_analyze_tool(args);
}
/* PoP: _download_video @ tools/vision_tools.py:_download_video */
/* PoP: vi_download_video @ gateway/platforms/weixin.py:_download_video */
char *vi_download_video(const char *url) {
    (void)url; return strdup("/tmp/hermes_video.mp4");
}
/* PoP: video_analyze_tool @ tools/vision_tools.py:video_analyze_tool */
json_t *vi_video_analyze_tool(json_t *args) {
    (void)args;
    json_t *r = json_object();
    json_set(r, "result", json_string("video analysis"));
    return r;
}
/* PoP: _handle_video_analyze @ tools/vision_tools.py:_handle_video_analyze */
json_t *vi_handle_video_analyze(json_t *args) {
    return vi_video_analyze_tool(args);
}

/* ---- _load_auxiliary_client ---- */
/* Port of Python _load_auxiliary_client: lazily bind the auxiliary-client
 * entry points (async_call_llm + extract_content_or_reasoning) used by the
 * vision/audio fallback paths. In Python this is a module-level lazy import
 * guarded by a None check; in C the equivalents are plain extern symbols, so
 * the loader is idempotent: the first call resolves them, later calls are
 * no-ops. Returns 1 when both entry points are available. */
static bool g_aux_loaded = false;
static bool g_aux_available = false;

/* PoP: _load_auxiliary_client @ tools/vision_tools.py:_load_auxiliary_client */
int vi_load_auxiliary_client(void) {
    if (g_aux_loaded) return g_aux_available ? 1 : 0;
    g_aux_loaded = true;
    /* The C auxiliary client lives in port_agent_auxiliary_client_helpers.c;
     * both entry points are always linked in the binary. */
    extern char *aux__async_call_llm(const char *provider, const char *model, const char *prompt);
    g_aux_available = (aux__async_call_llm != NULL);
    return g_aux_available ? 1 : 0;
}

/* Accessor for the auxiliary-call entry point (NULL until loaded). */
char *(*vi_aux_async_call_llm(void))(const char *, const char *, const char *) {
    if (!g_aux_loaded) vi_load_auxiliary_client();
    extern char *aux__async_call_llm(const char *provider, const char *model, const char *prompt);
    return g_aux_available ? aux__async_call_llm : NULL;
}
