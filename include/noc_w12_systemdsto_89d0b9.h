#ifndef NOC_W12_SYSTEMDSTO_89D0B9_H
#define NOC_W12_SYSTEMDSTO_89D0B9_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_systemdsto_89d0b9_resolve_pid @ gateway/systemd_stop_mark.py:_resolve_target_pid
 * argv[1] when present/non-empty else systemd's MAINPID; Python int(str(raw).strip())
 * semantics; returns JSON null when unknown or <= 0. `mainpid_value` is the env seam. */
struct json_t *w12_systemdsto_89d0b9_resolve_pid(const struct json_t *argv,
                                                 const char *mainpid_value);

/* PoP: w12_systemdsto_89d0b9_main @ gateway/systemd_stop_mark.py:main
 * Always returns 0 (best effort); the marker write is the caller's side effect. */
struct json_t *w12_systemdsto_89d0b9_main(const struct json_t *argv,
                                          const char *mainpid_value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_SYSTEMDSTO_89D0B9_H */
