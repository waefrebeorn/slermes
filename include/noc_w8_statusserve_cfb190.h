#ifndef NOC_W8_STATUSERVE_CFB190_H
#define NOC_W8_STATUSERVE_CFB190_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Faithful C11 port of the pure selection/projection helpers in
 * gateway/status_served.py. */

/* PoP: w8_statusserve_cfb190_local_liveness_pid @ gateway/status_served.py:local_liveness_pid */
struct json_t *w8_statusserve_cfb190_local_liveness_pid(const struct json_t *liveness);

/* PoP: w8_statusserve_cfb190_served_profile_runtime @ gateway/status_served.py:served_profile_runtime
 * Seam values (own_running_pid, served, profile_name, platforms) mirror the
 * gateway.status collaborators resolved at call time. */
struct json_t *w8_statusserve_cfb190_served_profile_runtime(const struct json_t *home,
        const struct json_t *own_runtime, const struct json_t *gateway_pid,
        const struct json_t *seams);

/* PoP: w8_statusserve_cfb190_served_profile_mirror_platforms @ gateway/status_served.py:served_profile_mirror_platforms */
struct json_t *w8_statusserve_cfb190_served_profile_mirror_platforms(
        const struct json_t *runtime, const char *profile);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_STATUSERVE_CFB190_H */
