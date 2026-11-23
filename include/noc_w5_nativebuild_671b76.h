#ifndef NOC_W5_NATIVEBUILD_671B76_H
#define NOC_W5_NATIVEBUILD_671B76_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_nativebuild_671b76_prepare_validate @ pm/native_build.py:prepare_windows_environment
 * Ports the provider-output validation. On success returns a NEW json object;
 * on failure returns NULL and sets *status = 2 (Python ValueError). */
struct json_t *w5_nativebuild_671b76_prepare_validate(const struct json_t *prepared, int *status);

/* PoP: w5_nativebuild_671b76_source_build_environment @ pm/native_build.py:source_build_environment
 * Explicit seams: `target` = current_target(), `provider_present` = provider
 * file probe, `prepared` = provider environment, `bridged` = bridged settings.
 * *status: 0 = dict returned, 1 = None, 2 = ValueError. Caller frees result. */
struct json_t *w5_nativebuild_671b76_source_build_environment_ex(const char *target,
        int provider_present, const struct json_t *prepared, const struct json_t *bridged,
        int *status);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W5_NATIVEBUILD_671B76_H */
