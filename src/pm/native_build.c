/*
 * native_build.c — C home for upstream Hermes pm/native_build.py.
 *
 * PoP: w5_nativebuild_671b76_prepare_validate @ pm/native_build.py:prepare_windows_environment
 * PoP: w5_nativebuild_671b76_source_build_environment @ pm/native_build.py:source_build_environment
 *
 * Filesystem/subprocess/provider reads are injected as explicit seams from
 * the fixture; the deterministic validation + merge logic is ported verbatim.
 * Exported symbols are w5_nativebuild_671b76_ prefixed.
 */
#include "noc_w5_nativebuild_671b76.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

/* if not isinstance(prepared, dict) or any(not isinstance(k,str) or
 * not isinstance(v,str) for k,v in prepared.items()): raise ValueError */
struct json_t *w5_nativebuild_671b76_prepare_validate(const struct json_t *prepared, int *status) {
    if (status) *status = 0;
    if (!prepared || prepared->type != JSON_OBJECT) { if (status) *status = 2; return NULL; }
    for (size_t i = 0; i < json_object_size(prepared); ++i) {
        const char *k = json_object_get_key_at(prepared, i);
        json_t *v = json_object_get_at(prepared, i);
        if (!k || !v || v->type != JSON_STRING) { if (status) *status = 2; return NULL; }
    }
    return json_copy(prepared);
}

struct json_t *w5_nativebuild_671b76_source_build_environment_ex(const char *target,
        int provider_present, const struct json_t *prepared, const struct json_t *bridged,
        int *status) {
    if (status) *status = 1;
    /* if current_target() != "win32-arm64" or not provider file: return None */
    if (!target || strcmp(target, "win32-arm64") || !provider_present) return NULL;
    int st = 0;
    json_t *out = w5_nativebuild_671b76_prepare_validate(prepared, &st);
    if (!out) { if (status) *status = 2; return NULL; }
    /* prepared.update(bridged_index_settings(...)) */
    if (bridged && bridged->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(bridged); ++i) {
            const char *k = json_object_get_key_at(bridged, i);
            if (k) json_set(out, k, json_copy(json_object_get_at(bridged, i)));
        }
    }
    if (status) *status = 0;
    return out;
}

