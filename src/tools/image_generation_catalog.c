/*
 * image_generation_catalog.c — C home for tools/image_generation_catalog.py.
 * PoP: imgcatalog_default_model @ tools/image_generation_catalog.py:DEFAULT_MODEL
 * PoP: imgcatalog_default_aspect_ratio @ tools/image_generation_catalog.py:DEFAULT_ASPECT_RATIO
 * PoP: imgcatalog_valid_aspect_ratios @ tools/image_generation_catalog.py:VALID_ASPECT_RATIOS
 * PoP: imgcatalog_upscaler @ tools/image_generation_catalog.py:UPSCALER_MODEL
 * PoP: imgcatalog_model_field @ tools/image_generation_catalog.py:FAL_MODELS
 * PoP: imgcatalog_model_keys @ tools/image_generation_catalog.py:FAL_MODELS
 * PoP: imgcatalog_build_entry @ tools/image_generation_catalog.py:_model
 *
 * FAL_MODELS is embedded verbatim (sets rendered as sorted arrays) from the live
 * Python module; no stubs. Set-valued fields are arrays in the C representation.
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

static const char *const CATALOG_JSON =
    "{\"fal-ai/flux-2/klein/9b\":{\"display\":\"FLUX 2 Klein 9B\",\"speed\":\"<1s\",\"strengths\":\"Fast, crisp text\",\"price\":\"$0.006/MP\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\",\"portrait\":\"portrait_16_9\"},\"defaults\":{\"num_inference_steps\":4,\"output_format\":\"png\",\"enable_safety_checker\":false},\"supports\":[\"enable_safety_checker\",\"image_size\",\"num_inference_steps\",\"output_format\",\"prompt\",\"seed\"],\"upscale\":false,\"edit_endpoint\":\"fal-ai/flux-2/klein/9b/edit\",\"edit_supports\":[\"enable_safety_checker\",\"image_urls\",\"num_inference_steps\",\"output_format\",\"prompt\",\"seed\"],\"max_reference_images\":9},\"fal-ai/flux-2-pro\":{\"display\":\"FLUX 2 Pro\",\"speed\":\"~6s\",\"strengths\":\"Studio photorealism\",\"price\":\"$0.03/MP\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\",\"portrait\":\"portrait_16_9\"},\"defaults\":{\"num_inference_steps\":50,\"guidance_scale\":4.5,\"num_images\":1,\"output_format\":\"png\",\"enable_safety_checker\":false,\"safety_tolerance\":\"5\",\"sync_mode\":true},\"supports\":[\"enable_safety_checker\",\"guidance_scale\",\"image_size\",\"num_images\",\"num_inference_steps\",\"output_format\",\"prompt\",\"safety_tolerance\",\"seed\",\"sync_mode\"],\"upscale\":false,\"edit_endpoint\":\"fal-ai/flux-2-pro/edit\",\"edit_supports\":[\"enable_safety_checker\",\"guidance_scale\",\"image_urls\",\"num_images\",\"num_inference_steps\",\"output_format\",\"prompt\",\"safety_tolerance\",\"seed\",\"sync_mode\"],\"max_reference_images\":9},\"fal-ai/z-image/turbo\":{\"display\":\"Z-Image Turbo\",\"speed\":\"~2s\",\"strengths\":\"Bilingual EN/CN, 6B\",\"price\":\"$0.005/MP\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\",\"portrait\":\"portrait_16_9\"},\"defaults\":{\"num_inference_steps\":8,\"num_images\":1,\"output_format\":\"png\",\"enable_safety_checker\":false,\"enable_prompt_expansion\":false},\"supports\":[\"enable_prompt_expansion\",\"enable_safety_checker\",\"image_size\",\"num_images\",\"num_inference_steps\",\"output_format\",\"prompt\",\"seed\"],\"upscale\":false},\"fal-ai/nano-banana-pro\":{\"display\":\"Nano Banana Pro (Gemini 3 Pro Image)\",\"speed\":\"~8s\",\"strengths\":\"Gemini 3 Pro, reasoning depth, text rendering\",\"price\":\"$0.15/image (1K)\",\"size_style\":\"aspect_ratio\",\"sizes\":{\"landscape\":\"16:9\",\"square\":\"1:1\",\"portrait\":\"9:16\"},\"defaults\":{\"num_images\":1,\"output_format\":\"png\",\"safety_tolerance\":\"5\",\"resolution\":\"1K\"},\"supports\":[\"aspect_ratio\",\"enable_web_search\",\"limit_generations\",\"num_images\",\"output_format\",\"prompt\",\"resolution\",\"safety_tolerance\",\"seed\",\"sync_mode\"],\"upscale\":false,\"edit_endpoint\":\"fal-ai/nano-banana-pro/edit\",\"edit_supports\":[\"aspect_ratio\",\"enable_web_search\",\"image_urls\",\"limit_generations\",\"num_images\",\"output_format\",\"prompt\",\"resolution\",\"safety_tolerance\",\"seed\",\"sync_mode\"],\"max_reference_images\":2},\"fal-ai/nano-banana-2\":{\"display\":\"Nano Banana 2 (Gemini 3.1 Flash Image)\",\"speed\":\"~3s\",\"strengths\":\"Fast reasoning, multilingual text, infographics\",\"price\":\"Lower-cost Flash tier\",\"size_style\":\"aspect_ratio\",\"sizes\":{\"landscape\":\"16:9\",\"square\":\"1:1\",\"portrait\":\"9:16\"},\"defaults\":{\"num_images\":1,\"output_format\":\"png\",\"safety_tolerance\":\"4\",\"resolution\":\"1K\",\"limit_generations\":true},\"supports\":[\"aspect_ratio\",\"enable_web_search\",\"limit_generations\",\"num_images\",\"output_format\",\"prompt\",\"resolution\",\"safety_tolerance\",\"seed\",\"sync_mode\",\"system_prompt\",\"thinking_level\"],\"upscale\":false,\"edit_endpoint\":\"fal-ai/nano-banana-2/edit\",\"edit_supports\":[\"aspect_ratio\",\"enable_web_search\",\"image_urls\",\"limit_generations\",\"num_images\",\"output_format\",\"prompt\",\"resolution\",\"safety_tolerance\",\"seed\",\"sync_mode\",\"system_prompt\",\"thinking_level\"],\"max_reference_images\":14},\"fal-ai/gpt-image-1.5\":{\"display\":\"GPT Image 1.5\",\"speed\":\"~15s\",\"strengths\":\"Prompt adherence\",\"price\":\"$0.034/image\",\"size_style\":\"gpt_literal\",\"sizes\":{\"landscape\":\"1536x1024\",\"square\":\"1024x1024\",\"portrait\":\"1024x1536\"},\"defaults\":{\"quality\":\"medium\",\"num_images\":1,\"output_format\":\"png\"},\"supports\":[\"background\",\"image_size\",\"num_images\",\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],\"upscale\":false,\"edit_endpoint\":\"fal-ai/gpt-image-1.5/edit\",\"edit_supports\":[\"image_size\",\"image_urls\",\"num_images\",\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],\"max_reference_images\":16},\"fal-ai/gpt-image-2\":{\"display\":\"GPT Image 2\",\"speed\":\"~20s\",\"strengths\":\"SOTA text rendering + CJK, world-aware photorealism\",\"price\":\"$0.04–0.06/image\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_4_3\",\"square\":\"square_hd\",\"portrait\":\"portrait_4_3\"},\"defaults\":{\"quality\":\"medium\",\"num_images\":1,\"output_format\":\"png\"},\"supports\":[\"image_size\",\"num_images\",\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],\"upscale\":false,\"edit_endpoint\":\"openai/gpt-image-2/edit\",\"edit_supports\":[\"image_urls\",\"mask_image_url\",\"num_images\",\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],\"max_reference_images\":16},\"openai/gpt-image-2.5/flare/text-to-image\":{\"display\":\"GPT Image 2.5 Flare\",\"speed\":\"Fast\",\"strengths\":\"Everyday creation, natural lighting and textures\",\"price\":\"Token-based pricing\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_4_3\",\"square\":\"square_hd\",\"portrait\":\"portrait_4_3\"},\"defaults\":{\"quality\":\"medium\",\"num_images\":1,\"output_format\":\"png\"},\"supports\":[\"background\",\"image_size\",\"num_images\",\"output_compression\",\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],\"upscale\":false,\"edit_endpoint\":\"openai/gpt-image-2.5/flare/edit\",\"edit_supports\":[\"background\",\"image_size\",\"image_urls\",\"input_fidelity\",\"mask_url\",\"num_images\",\"output_compression\",\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],\"max_reference_images\":16},\"openai/gpt-image-2.5/sunburst/text-to-image\":{\"display\":\"GPT Image 2.5 Sunburst\",\"speed\":\"Slower\",\"strengths\":\"Precision editing, subject and composition consistency\",\"price\":\"Token-based pricing\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_4_3\",\"square\":\"square_hd\",\"portrait\":\"portrait_4_3\"},\"defaults\":{\"quality\":\"medium\",\"num_images\":1,\"output_format\":\"png\"},\"supports\":[\"background\",\"image_size\",\"num_images\",\"output_compression\",\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],\"upscale\":false,\"edit_endpoint\":\"openai/gpt-image-2.5/sunburst/edit\",\"edit_supports\":[\"background\",\"image_size\",\"image_urls\",\"input_fidelity\",\"mask_url\",\"num_images\",\"output_compression\",\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],\"max_reference_images\":16},\"openai/gpt-image-2.6/flare/text-to-image\":{\"display\":\"GPT Image 2.6 Flare\",\"speed\":\"Fast\",\"strengths\":\"Latest OpenAI image model, everyday creation and editing\",\"price\":\"Token-based pricing\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_4_3\",\"square\":\"square_hd\",\"portrait\":\"portrait_4_3\"},\"defaults\":{\"quality\":\"medium\",\"num_images\":1,\"output_format\":\"png\"},\"supports\":[\"background\",\"image_size\",\"num_images\",\"output_compression\",\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],\"upscale\":false,\"edit_endpoint\":\"openai/gpt-image-2.6/flare/edit\",\"edit_supports\":[\"background\",\"image_size\",\"image_urls\",\"mask_url\",\"num_images\",\"output_compression\",\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],\"max_reference_images\":16},\"openai/gpt-image-2.6/sunburst/text-to-image\":{\"display\":\"GPT Image 2.6 Sunburst\",\"speed\":\"Slower\",\"strengths\":\"Latest OpenAI image model, precision editing and consistency\",\"price\":\"Token-based pricing\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_4_3\",\"square\":\"square_hd\",\"portrait\":\"portrait_4_3\"},\"defaults\":{\"quality\":\"medium\",\"num_images\":1,\"output_format\":\"png\"},\"supports\":[\"background\",\"image_size\",\"num_images\",\"output_compression\",\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],\"upscale\":false,\"edit_endpoint\":\"openai/gpt-image-2.6/sunburst/edit\",\"edit_supports\":[\"background\",\"image_size\",\"image_urls\",\"mask_url\",\"num_images\",\"output_compression\",\"output_format\",\"prompt\",\"quality\",\"sync_mode\"],\"max_reference_images\":16},\"fal-ai/ideogram/v3\":{\"display\":\"Ideogram V3\",\"speed\":\"~5s\",\"strengths\":\"Best typography\",\"price\":\"$0.03-0.09/image\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\",\"portrait\":\"portrait_16_9\"},\"defaults\":{\"rendering_speed\":\"BALANCED\",\"expand_prompt\":true,\"style\":\"AUTO\"},\"supports\":[\"expand_prompt\",\"image_size\",\"prompt\",\"rendering_speed\",\"seed\",\"style\"],\"upscale\":false,\"edit_endpoint\":\"fal-ai/ideogram/v3/edit\",\"edit_supports\":[\"expand_prompt\",\"image_urls\",\"prompt\",\"rendering_speed\",\"seed\",\"style\"],\"max_reference_images\":1},\"fal-ai/recraft/v4/pro/text-to-image\":{\"display\":\"Recraft V4 Pro\",\"speed\":\"~8s\",\"strengths\":\"Design, brand systems, production-ready\",\"price\":\"$0.25/image\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\",\"portrait\":\"portrait_16_9\"},\"defaults\":{\"enable_safety_checker\":false},\"supports\":[\"background_color\",\"colors\",\"enable_safety_checker\",\"image_size\",\"prompt\"],\"upscale\":false},\"fal-ai/qwen-image\":{\"display\":\"Qwen Image\",\"speed\":\"~12s\",\"strengths\":\"LLM-based, complex text\",\"price\":\"$0.02/MP\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\",\"portrait\":\"portrait_16_9\"},\"defaults\":{\"num_inference_steps\":30,\"guidance_scale\":2.5,\"num_images\":1,\"output_format\":\"png\",\"acceleration\":\"regular\"},\"supports\":[\"acceleration\",\"guidance_scale\",\"image_size\",\"num_images\",\"num_inference_steps\",\"output_format\",\"prompt\",\"seed\",\"sync_mode\"],\"upscale\":false,\"edit_endpoint\":\"fal-ai/qwen-image-2/pro/edit\",\"edit_supports\":[\"acceleration\",\"guidance_scale\",\"image_urls\",\"num_images\",\"num_inference_steps\",\"output_format\",\"prompt\",\"seed\",\"sync_mode\"],\"max_reference_images\":3},\"fal-ai/krea/v2/medium/text-to-image\":{\"display\":\"Krea 2 Medium\",\"speed\":\"~15-25s\",\"strengths\":\"Illustration, anime, painting, expressive/artistic styles\",\"price\":\"$0.030 (text) / $0.035 (style refs)\",\"size_style\":\"aspect_ratio\",\"sizes\":{\"landscape\":\"16:9\",\"square\":\"1:1\",\"portrait\":\"9:16\"},\"defaults\":{\"creativity\":\"medium\"},\"supports\":[\"aspect_ratio\",\"creativity\",\"image_style_references\",\"prompt\",\"seed\"],\"upscale\":false},\"fal-ai/krea/v2/large/text-to-image\":{\"display\":\"Krea 2 Large\",\"speed\":\"~25-60s\",\"strengths\":\"Photorealism, raw textured looks (motion blur, grain, film)\",\"price\":\"$0.060 (text) / $0.065 (style refs)\",\"size_style\":\"aspect_ratio\",\"sizes\":{\"landscape\":\"16:9\",\"square\":\"1:1\",\"portrait\":\"9:16\"},\"defaults\":{\"creativity\":\"medium\"},\"supports\":[\"aspect_ratio\",\"creativity\",\"image_style_references\",\"prompt\",\"seed\"],\"upscale\":false},\"bytedance/seedream/v5/pro/text-to-image\":{\"display\":\"Seedream 5.0 Pro\",\"speed\":\"~10s\",\"strengths\":\"ByteDance flagship, dense layouts, native text in 14 languages\",\"price\":\"$0.0675/image (≤1536²)\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":{\"width\":2048,\"height\":1152},\"square\":{\"width\":1536,\"height\":1536},\"portrait\":{\"width\":1152,\"height\":2048}},\"defaults\":{\"num_images\":1,\"output_format\":\"png\",\"enable_safety_checker\":false},\"supports\":[\"enable_safety_checker\",\"image_size\",\"num_images\",\"output_format\",\"prompt\",\"sync_mode\"],\"upscale\":false,\"edit_endpoint\":\"bytedance/seedream/v5/pro/edit\",\"edit_supports\":[\"enable_safety_checker\",\"image_size\",\"image_urls\",\"num_images\",\"output_format\",\"prompt\",\"sync_mode\"],\"max_reference_images\":10},\"bytedance/seedream/v5/lite/text-to-image\":{\"display\":\"Seedream 5.0 Lite\",\"speed\":\"~5s\",\"strengths\":\"Fast/cheap Seedream tier, high-res output\",\"price\":\"$0.035/image\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\",\"portrait\":\"portrait_16_9\"},\"defaults\":{\"num_images\":1,\"enable_safety_checker\":false},\"supports\":[\"enable_safety_checker\",\"image_size\",\"max_images\",\"num_images\",\"prompt\",\"sync_mode\"],\"upscale\":false},\"ideogram/v4/instant\":{\"display\":\"Ideogram V4 (Instant)\",\"speed\":\"<1s\",\"strengths\":\"Latest Ideogram typography, posters/logos, instant\",\"price\":\"$0.0075/MP\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\",\"portrait\":\"portrait_16_9\"},\"defaults\":{\"expansion_model\":\"Medium\",\"output_format\":\"png\",\"enable_safety_checker\":false},\"supports\":[\"enable_safety_checker\",\"expansion_model\",\"image_size\",\"num_images\",\"output_format\",\"prompt\",\"seed\",\"sync_mode\"],\"upscale\":false},\"ideogram/v4/fast\":{\"display\":\"Ideogram V4 (Fast)\",\"speed\":\"~1s\",\"strengths\":\"Ideogram V4 quality tiers via rendering_speed\",\"price\":\"$0.005-0.018/MP\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\",\"portrait\":\"portrait_16_9\"},\"defaults\":{\"expansion_model\":\"Medium\",\"rendering_speed\":\"BALANCED\"},\"supports\":[\"expansion_model\",\"image_size\",\"num_images\",\"prompt\",\"rendering_speed\",\"seed\",\"sync_mode\"],\"upscale\":false},\"alibaba/qwen-image-3/text-to-image\":{\"display\":\"Qwen Image 3\",\"speed\":\"~8s\",\"strengths\":\"Complex CN/EN text rendering, prompt-guided resolution\",\"price\":\"$0.04 (1K) / $0.075 (2K) per image\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\",\"portrait\":\"portrait_16_9\"},\"defaults\":{\"num_images\":1,\"output_format\":\"png\",\"enable_prompt_expansion\":false,\"enable_safety_checker\":false},\"supports\":[\"enable_prompt_expansion\",\"enable_safety_checker\",\"image_size\",\"negative_prompt\",\"num_images\",\"output_format\",\"prompt\",\"seed\",\"sync_mode\"],\"upscale\":false,\"edit_endpoint\":\"alibaba/qwen-image-3/edit\",\"edit_supports\":[\"enable_prompt_expansion\",\"enable_safety_checker\",\"image_urls\",\"negative_prompt\",\"num_images\",\"output_format\",\"prompt\",\"seed\",\"sync_mode\"],\"max_reference_images\":3},\"microsoft/mai-image-2.5-pro\":{\"display\":\"MAI Image 2.5 Pro\",\"speed\":\"~10s\",\"strengths\":\"Microsoft flagship, hero imagery, precise typography\",\"price\":\"~$0.17/image\",\"size_style\":\"aspect_ratio\",\"sizes\":{\"landscape\":\"16:9\",\"square\":\"1:1\",\"portrait\":\"9:16\"},\"defaults\":{\"num_images\":1,\"output_format\":\"png\"},\"supports\":[\"aspect_ratio\",\"num_images\",\"output_format\",\"prompt\",\"sync_mode\"],\"upscale\":false},\"google/nano-banana-2-lite\":{\"display\":\"Nano Banana 2 Lite\",\"speed\":\"<2s\",\"strengths\":\"Gemini image family, sub-2s, 14 aspect ratios incl. extreme\",\"price\":\"~$0.04/image (1K fixed)\",\"size_style\":\"aspect_ratio\",\"sizes\":{\"landscape\":\"16:9\",\"square\":\"1:1\",\"portrait\":\"9:16\"},\"defaults\":{\"num_images\":1,\"output_format\":\"png\",\"safety_tolerance\":\"5\"},\"supports\":[\"aspect_ratio\",\"limit_generations\",\"num_images\",\"output_format\",\"prompt\",\"safety_tolerance\",\"seed\",\"sync_mode\",\"system_prompt\",\"thinking_level\"],\"upscale\":false,\"edit_endpoint\":\"google/nano-banana-2-lite/edit\",\"edit_supports\":[\"aspect_ratio\",\"image_urls\",\"num_images\",\"output_format\",\"prompt\",\"safety_tolerance\",\"seed\",\"sync_mode\",\"system_prompt\"],\"max_reference_images\":4},\"fal-ai/recraft/v4.1/text-to-image\":{\"display\":\"Recraft V4.1\",\"speed\":\"~8s\",\"strengths\":\"Design-first raster, brand systems, editorial\",\"price\":\"$0.035/image\",\"size_style\":\"image_size_preset\",\"sizes\":{\"landscape\":\"landscape_16_9\",\"square\":\"square_hd\",\"portrait\":\"portrait_16_9\"},\"defaults\":{\"enable_safety_checker\":false},\"supports\":[\"background_color\",\"colors\",\"enable_safety_checker\",\"image_size\",\"prompt\"],\"upscale\":false},\"xai/grok-imagine-image/v2.0/text-to-image\":{\"display\":\"Grok Imagine Image 2.0\",\"speed\":\"~5s\",\"strengths\":\"xAI. Design-grade typography/layout, instruction following\",\"price\":\"$0.06/image (1K medium)\",\"size_style\":\"aspect_ratio\",\"sizes\":{\"landscape\":\"16:9\",\"square\":\"1:1\",\"portrait\":\"9:16\"},\"defaults\":{\"num_images\":1,\"output_format\":\"png\",\"resolution\":\"1k\",\"quality\":\"medium\"},\"supports\":[\"aspect_ratio\",\"num_images\",\"output_format\",\"prompt\",\"quality\",\"resolution\",\"sync_mode\"],\"upscale\":false,\"edit_endpoint\":\"xai/grok-imagine-image/v2.0/edit\",\"edit_supports\":[\"image_urls\",\"num_images\",\"output_format\",\"prompt\",\"quality\",\"resolution\",\"sync_mode\"],\"max_reference_images\":3},\"fal-ai/kling-image/v3/text-to-image\":{\"display\":\"Kling Image v3\",\"speed\":\"~10s\",\"strengths\":\"Kuaishou. Realistic detail, cheap native 2K, wide AR set\",\"price\":\"$0.028/image\",\"size_style\":\"aspect_ratio\",\"sizes\":{\"landscape\":\"16:9\",\"square\":\"1:1\",\"portrait\":\"9:16\"},\"defaults\":{\"num_images\":1,\"output_format\":\"png\",\"resolution\":\"2K\"},\"supports\":[\"aspect_ratio\",\"negative_prompt\",\"num_images\",\"output_format\",\"prompt\",\"resolution\",\"sync_mode\"],\"upscale\":false,\"edit_endpoint\":\"fal-ai/kling-image/v3/image-to-image\",\"edit_supports\":[\"aspect_ratio\",\"image_url\",\"num_images\",\"output_format\",\"prompt\",\"resolution\",\"sync_mode\"],\"max_reference_images\":1,\"edit_image_param\":\"image_url\"},\"meta/muse-image/text-to-image\":{\"display\":\"Meta Muse Image\",\"speed\":\"~5s\",\"strengths\":\"Meta. Realism + typography at commodity price\",\"price\":\"$0.01/image\",\"size_style\":\"aspect_ratio\",\"sizes\":{\"landscape\":\"16:9\",\"square\":\"1:1\",\"portrait\":\"9:16\"},\"defaults\":{\"num_images\":1,\"output_format\":\"png\"},\"supports\":[\"aspect_ratio\",\"num_images\",\"output_format\",\"prompt\",\"sync_mode\"],\"upscale\":false,\"edit_endpoint\":\"meta/muse-image/edit\",\"edit_supports\":[\"image_urls\",\"num_images\",\"output_format\",\"prompt\",\"sync_mode\"],\"max_reference_images\":10}}";

static json_t *g_catalog = NULL;
static json_t *catalog(void) {
    if (!g_catalog) g_catalog = json_parse(CATALOG_JSON, NULL);
    return g_catalog;
}

char *imgcatalog_default_model(void) { return (char *)strdup("fal-ai/flux-2/klein/9b"); }
char *imgcatalog_default_aspect_ratio(void) { return (char *)strdup("landscape"); }

json_t *imgcatalog_valid_aspect_ratios(void) {
    json_t *a = json_array();
    json_append(a, json_string("landscape"));
    json_append(a, json_string("square"));
    json_append(a, json_string("portrait"));
    return a;
}

json_t *imgcatalog_upscaler(void) {
    json_t *o = json_object();
    json_set(o, "UPSCALER_MODEL", json_string("fal-ai/clarity-upscaler"));
    json_set(o, "UPSCALER_FACTOR", json_number(2));
    json_set(o, "UPSCALER_SAFETY_CHECKER", json_bool(false));
    json_set(o, "UPSCALER_DEFAULT_PROMPT", json_string("masterpiece, best quality, highres"));
    json_set(o, "UPSCALER_NEGATIVE_PROMPT", json_string("(worst quality, low quality, normal quality:2)"));
    /* Floats are decimal STRINGS: libjson serializes doubles with %.17g, which
     * would turn 0.35 into 0.34999999999999998 and break exact differential parity. */
    json_set(o, "UPSCALER_CREATIVITY", json_string("0.35"));
    json_set(o, "UPSCALER_RESEMBLANCE", json_string("0.6"));
    json_set(o, "UPSCALER_GUIDANCE_SCALE", json_number(4));
    json_set(o, "UPSCALER_NUM_INFERENCE_STEPS", json_number(18));
    return o;
}

json_t *imgcatalog_model_field(const char *key, const char *field) {
    json_t *root = catalog();
    if (!root || !key || !field) return json_null();
    json_t *entry = json_obj_get(root, key);
    if (!entry || entry->type != JSON_OBJECT) return json_null();
    json_t *v = json_obj_get(entry, field);
    return v ? json_copy(v) : json_null();
}

json_t *imgcatalog_model_keys(void) {
    json_t *a = json_array();
    json_t *root = catalog();
    if (!root || root->type != JSON_OBJECT) return a;
    for (size_t i = 0; i < json_object_size(root); i++) {
        const char *k = json_object_get_key_at(root, i);
        if (k) json_append(a, json_string(k));
    }
    return a;
}

static int cmp_json_str(const void *pa, const void *pb) {
    json_t *const *a = (json_t *const *)pa;
    json_t *const *b = (json_t *const *)pb;
    const char *sa = (*a && (*a)->type == JSON_STRING) ? (*a)->str_val : "";
    const char *sb = (*b && (*b)->type == JSON_STRING) ? (*b)->str_val : "";
    return strcmp(sa, sb);
}

/* Python set -> C JSON array, sorted to match sorted(set) in the oracle. */
static json_t *sorted_string_set(const json_t *arr) {
    json_t *out = json_array();
    if (!arr || arr->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING) json_append(out, json_copy(v));
    }
    if (out->c.count > 1) qsort(out->c.items, out->c.count, sizeof(json_t *), cmp_json_str);
    return out;
}

static json_t *default_sizes(const char *style) {
    json_t *o = json_object();
    if (!strcmp(style, "image_size_preset")) {
        json_set(o, "landscape", json_string("landscape_16_9"));
        json_set(o, "square", json_string("square_hd"));
        json_set(o, "portrait", json_string("portrait_16_9"));
    } else if (!strcmp(style, "aspect_ratio")) {
        json_set(o, "landscape", json_string("16:9"));
        json_set(o, "square", json_string("1:1"));
        json_set(o, "portrait", json_string("9:16"));
    }
    return o;
}

json_t *imgcatalog_build_entry(const json_t *p) {
    json_t *entry = json_object();
    if (!p || p->type != JSON_OBJECT) return entry;
    json_set(entry, "display", json_string(json_get_str(p, "display", "")));
    json_set(entry, "speed", json_string(json_get_str(p, "speed", "")));
    json_set(entry, "strengths", json_string(json_get_str(p, "strengths", "")));
    json_set(entry, "price", json_string(json_get_str(p, "price", "")));
    const char *style = json_get_str(p, "style", "image_size_preset");
    json_set(entry, "size_style", json_string(style));
    json_t *sizes = json_obj_get(p, "sizes");
    json_set(entry, "sizes", (sizes && sizes->type == JSON_OBJECT) ? json_copy(sizes) : default_sizes(style));
    json_t *defs = json_obj_get(p, "defaults");
    json_set(entry, "defaults", (defs && defs->type == JSON_OBJECT) ? json_copy(defs) : json_object());
    json_set(entry, "supports", sorted_string_set(json_obj_get(p, "supports")));
    json_set(entry, "upscale", json_bool(false));
    const char *edit_endpoint = json_get_str(p, "edit_endpoint", "");
    if (edit_endpoint && *edit_endpoint) {
        json_set(entry, "edit_endpoint", json_string(edit_endpoint));
        json_t *es = json_obj_get(p, "edit_supports");
        json_set(entry, "edit_supports", (es && es->type == JSON_ARRAY) ? sorted_string_set(es) : json_null());
        const char *mri = json_get_str(p, "max_reference_images", "");
        if (mri && *mri) json_set(entry, "max_reference_images", json_number((double)strtoll(mri, NULL, 10)));
        else json_set(entry, "max_reference_images", json_null());
        const char *eip = json_get_str(p, "edit_image_param", "");
        if (eip && *eip) json_set(entry, "edit_image_param", json_string(eip));
    }
    return entry;
}
