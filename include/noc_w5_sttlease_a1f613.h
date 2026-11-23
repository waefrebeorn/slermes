#ifndef NOC_W5_STTLEASE_A1F613_H
#define NOC_W5_STTLEASE_A1F613_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_sttlease_a1f613_warm_stt_provider @ tools/stt_lease.py:warm_stt_provider
 * Explicit seams replace every side effect (config provider resolution,
 * faster-whisper availability/lazy install, model normalisation/cache state,
 * load result and elapsed time). Returns a NEW json object; caller frees.
 *
 * provider_arg/config_provider may be NULL. has_fw/lazy_install_ok/hot_before/
 * load_ok are booleans (0/1). load_error is the exception text on a failed
 * load, or NULL for the defensive "load returned None" case. */
struct json_t *w5_sttlease_a1f613_warm_stt_provider_ex(
        const char *provider_arg, const char *config_provider,
        int has_fw, int lazy_install_ok, const char *model_name,
        int hot_before, int load_ok, const char *load_error, long long elapsed_ms);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W5_STTLEASE_A1F613_H */
