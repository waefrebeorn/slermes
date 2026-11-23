#ifndef NOC_W13_TRANSCRIPTI_B9C624_H
#define NOC_W13_TRANSCRIPTI_B9C624_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_transcripti_b9c624_cloud_trim_settings @ tools/transcription_audio.py:_cloud_trim_settings
 * Resolve (enabled, threshold_db, keep_ms) for the cloud silence trim.
 * Returns a JSON array [bool, int, int]. */
struct json_t *w13_transcripti_b9c624_cloud_trim_settings(const struct json_t *cfg);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_TRANSCRIPTI_B9C624_H */
