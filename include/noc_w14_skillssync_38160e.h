#ifndef NOC_W14_SKILLSSYNC_38160E_H
#define NOC_W14_SKILLSSYNC_38160E_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_skillssync_38160e_safe_rel_install_path @ tools/skills_sync_optional.py:_safe_rel_install_path */
/* Returns a JSON string on success, or {"error":"ValueError"} on rejection. */
struct json_t *w14_skillssync_38160e_safe_rel_install_path(const char *path,
                                                           const char *base);

/* PoP: w14_skillssync_38160e_hub_lock_entries @ tools/skills_sync_optional.py:_hub_lock_entries */
struct json_t *w14_skillssync_38160e_hub_lock_entries(const struct json_t *data);

/* PoP: w14_skillssync_38160e_read_hub_install_paths @ tools/skills_sync_optional.py:_read_hub_install_paths */
/* Returns a sorted JSON array of strings (Python set -> sorted output). */
struct json_t *w14_skillssync_38160e_read_hub_install_paths(const struct json_t *data);

/* PoP: w14_skillssync_38160e_is_runtime_cache @ tools/skills_sync_optional.py:_is_runtime_cache */
/* is_dir/py_exists are the injected Path.is_dir() and .with_suffix(".py").is_file() results. */
int w14_skillssync_38160e_is_runtime_cache(const char *path, const char *skill_dir,
                                           int is_dir, int py_exists);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SKILLSSYNC_38160E_H */
