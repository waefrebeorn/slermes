/*
 * image_generation_managed.c - C11 port of upstream
 * tools/image_generation_managed.py (gate w23_imagegener_6a7ba9).
 *
 * PoP: w23_imagegener_6a7ba9_backend_for_model @ tools/image_generation_managed.py:managed_backend_for_model
 * PoP: w23_imagegener_6a7ba9_route @ tools/image_generation_managed.py:managed_route
 * PoP: w23_imagegener_6a7ba9_portal_duplicate_of @ tools/image_generation_managed.py:_PORTAL_DUPLICATE_OF
 * PoP: w23_imagegener_6a7ba9_is_fal_krea_entry @ tools/image_generation_managed.py:_FAL_KREA_PREFIX
 *
 * The two id catalogs are in-tree static data (tools/image_generation_catalog.py
 * FAL_MODELS keys and plugins/image_gen/krea KREA_MODEL_IDS), so membership is
 * decided here; every other input is the raw stored config value.
 */
#include <stdlib.h>
#include <string.h>
#include "noc_w23_imagegener_6a7ba9.h"

#define IGM_FAL "fal"
#define IGM_KREA "krea"
#define IGM_PORTAL "portal"
#define IGM_NOUS "nous"
#define IGM_FAL_KREA_PREFIX "fal-ai/krea/"

/* FAL_MODELS keys (27). */
static const char *const IGM_FAL_MODELS[] = {
    "alibaba/qwen-image-3/text-to-image",
    "bytedance/seedream/v5/lite/text-to-image",
    "bytedance/seedream/v5/pro/text-to-image",
    "fal-ai/flux-2-pro",
    "fal-ai/flux-2/klein/9b",
    "fal-ai/gpt-image-1.5",
    "fal-ai/gpt-image-2",
    "fal-ai/ideogram/v3",
    "fal-ai/kling-image/v3/text-to-image",
    "fal-ai/krea/v2/large/text-to-image",
    "fal-ai/krea/v2/medium/text-to-image",
    "fal-ai/nano-banana-2",
    "fal-ai/nano-banana-pro",
    "fal-ai/qwen-image",
    "fal-ai/recraft/v4.1/text-to-image",
    "fal-ai/recraft/v4/pro/text-to-image",
    "fal-ai/z-image/turbo",
    "google/nano-banana-2-lite",
    "ideogram/v4/fast",
    "ideogram/v4/instant",
    "meta/muse-image/text-to-image",
    "microsoft/mai-image-2.5-pro",
    "openai/gpt-image-2.5/flare/text-to-image",
    "openai/gpt-image-2.5/sunburst/text-to-image",
    "openai/gpt-image-2.6/flare/text-to-image",
    "openai/gpt-image-2.6/sunburst/text-to-image",
    "xai/grok-imagine-image/v2.0/text-to-image",
};

/* plugins/image_gen/krea KREA_MODEL_IDS (4). */
static const char *const IGM_KREA_MODELS[] = {
    "krea-2-large",
    "krea-2-medium",
    "krea-2-medium-turbo",
    "krea-3",
};

/* _PORTAL_DUPLICATE_OF: only cross-id duplicates live here. */
static const struct {
    const char *portal_id;
    const char *native_id;
} IGM_DUPLICATES[] = {
    {"openai/gpt-image-2", "fal-ai/gpt-image-2"},
    {"google/gemini-3-pro-image", "fal-ai/nano-banana-pro"},
    {"google/gemini-3.1-flash-image", "fal-ai/nano-banana-2"},
    {"google/gemini-3.1-flash-lite-image", "google/nano-banana-2-lite"},
    {"krea/krea-2-medium", "krea-2-medium"},
    {"krea/krea-2-medium-turbo", "krea-2-medium-turbo"},
    {"krea/krea-2-large", "krea-2-large"},
    {"krea/krea-3", "krea-3"},
};

#define IGM_COUNT(a) (sizeof(a) / sizeof((a)[0]))

static int igm_member(const char *text, const char *const *set, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        if (strcmp(text, set[i]) == 0) return 1;
    return 0;
}

static char *igm_trim_copy(const char *s)
{
    const char *start = s;
    const char *end;
    size_t n;
    char *out;
    if (!s) return NULL;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r' ||
           *start == '\f' || *start == '\v')
        start++;
    end = start + strlen(start);
    while (end > start) {
        char ch = end[-1];
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v') end--;
        else break;
    }
    n = (size_t)(end - start);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, start, n);
    out[n] = '\0';
    return out;
}

/* str.strip() if the value is a non-blank string, else NULL (Python's None). */
static char *igm_stripped(const json_t *v)
{
    char *text;
    if (!v || v->type != JSON_STRING || !v->str_val) return NULL;
    text = igm_trim_copy(v->str_val);
    if (!text) return NULL;
    if (!text[0]) { free(text); return NULL; }
    return text;
}

const char *w23_imagegener_6a7ba9_backend_for_model(const json_t *model_id)
{
    char *candidate = igm_stripped(model_id);
    const char *backend;
    if (!candidate) return IGM_FAL;
    if (igm_member(candidate, IGM_FAL_MODELS, IGM_COUNT(IGM_FAL_MODELS))) backend = IGM_FAL;
    else if (igm_member(candidate, IGM_KREA_MODELS, IGM_COUNT(IGM_KREA_MODELS))) backend = IGM_KREA;
    else backend = IGM_PORTAL;
    free(candidate);
    return backend;
}

const char *w23_imagegener_6a7ba9_route(const json_t *provider, const json_t *model_id)
{
    char *provider_text = igm_stripped(provider);
    char *model_text = igm_stripped(model_id);
    const char *backend;
    int nous_pick;
    json_t *model_node = NULL;

    nous_pick = provider_text && strcmp(provider_text, IGM_NOUS) == 0;
    /* a blank or non-string provider reads as unset, which is not a nous pick */
    if (provider_text && !nous_pick) {
        free(provider_text);
        free(model_text);
        return NULL;
    }
    if (model_text) {
        model_node = json_string(model_text);
        backend = w23_imagegener_6a7ba9_backend_for_model(model_node);
        json_free(model_node);
    } else {
        backend = w23_imagegener_6a7ba9_backend_for_model(NULL);
    }
    if (backend == IGM_PORTAL && !nous_pick) backend = IGM_FAL;
    free(provider_text);
    free(model_text);
    return backend;
}

const char *w23_imagegener_6a7ba9_portal_duplicate_of(const json_t *model_id)
{
    size_t i;
    if (!model_id || model_id->type != JSON_STRING || !model_id->str_val) return NULL;
    for (i = 0; i < IGM_COUNT(IGM_DUPLICATES); i++)
        if (strcmp(model_id->str_val, IGM_DUPLICATES[i].portal_id) == 0)
            return IGM_DUPLICATES[i].native_id;
    return NULL;
}

int w23_imagegener_6a7ba9_is_fal_krea_entry(const json_t *model_id)
{
    size_t plen = strlen(IGM_FAL_KREA_PREFIX);
    if (!model_id || model_id->type != JSON_STRING || !model_id->str_val) return 0;
    return strncmp(model_id->str_val, IGM_FAL_KREA_PREFIX, plen) == 0 ? 1 : 0;
}
