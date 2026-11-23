#ifndef NOC_W11_SCHEDULERW_1873FB_H
#define NOC_W11_SCHEDULERW_1873FB_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_schedulerw_1873fb_pin_pythonpath @ cron/scheduler_worker_env.py:pin_hermes_tree_on_pythonpath
 * Faithful port of pin_hermes_tree_on_pythonpath. Returns a NEW env object;
 * inputs are not mutated. `installed_purelib` and `dependency` (NULL == None)
 * are explicit seams for sysconfig's purelib and the committed dependency
 * site-packages, injected by the fixture instead of touching the process env.
 * PATH separator is ";" (Windows os.pathsep). */
struct json_t *w11_schedulerw_1873fb_pin_pythonpath(const struct json_t *env,
                                                     const char *root,
                                                     const char *installed_purelib,
                                                     const char *dependency);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W11_SCHEDULERW_1873FB_H */
