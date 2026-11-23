#ifndef NOC_W15_WEBBUILDL_3B4D8F_H
#define NOC_W15_WEBBUILDL_3B4D8F_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_webbuildl_3b4d8f_light_build_requested @ hermes_cli/web_build_limits.py:light_build_requested */
int w15_webbuildl_3b4d8f_light_build_requested(const struct json_t *env);

/* PoP: w15_webbuildl_3b4d8f_bounded_threads @ hermes_cli/web_build_limits.py:_bounded_threads
 * available_cores is the injected seam for _available_cores(). */
int w15_webbuildl_3b4d8f_bounded_threads(int available_cores);

/* PoP: w15_webbuildl_3b4d8f_bounded_heap_mb @ hermes_cli/web_build_limits.py:_bounded_heap_mb
 * cgroup_present/cgroup_mb is the injected seam for _cgroup_memory_limit_mb(). */
int w15_webbuildl_3b4d8f_bounded_heap_mb(int cgroup_present, long long cgroup_mb);

/* PoP: w15_webbuildl_3b4d8f_web_build_limits @ hermes_cli/web_build_limits.py:web_build_limits
 * Returns a new env object; caller frees. */
struct json_t *w15_webbuildl_3b4d8f_web_build_limits(const struct json_t *env,
                                                    int cgroup_present, long long cgroup_mb,
                                                    int available_cores);

/* PoP: w15_webbuildl_3b4d8f_apply_web_build_limits @ hermes_cli/web_build_limits.py:apply_web_build_limits */
struct json_t *w15_webbuildl_3b4d8f_apply_web_build_limits(const struct json_t *env,
                                                          int cgroup_present, long long cgroup_mb,
                                                          int available_cores);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_WEBBUILDL_3B4D8F_H */
