#ifndef SLERMES_PORT_SKILLS_SYNC_H
#define SLERMES_PORT_SKILLS_SYNC_H

#include <stdbool.h>
#include <stddef.h>

typedef struct json_t json_t;
typedef struct port_skills_sync_state port_skills_sync_state_t;

/* Lifecycle */
port_skills_sync_state_t *port_skills_sync_state_init(void);
void port_skills_sync_state_cleanup(port_skills_sync_state_t *state);

/* Public API */
bool is_tracked_user_modification(const char *origin_hash, const char *user_hash);
char *read_for_diff(const char *path);
json_t *diff_bundled_skill(const char *name);
json_t *list_user_modified_bundled_skills(void);
char *get_bundled_dir(void);
char *get_optional_dir(void);
json_t *build_external_skill_index(void);
json_t *read_manifest(void);
void write_manifest(json_t *manifest);
char *read_skill_name(const char *skill_md_path, const char *fallback);
json_t *read_suppressed_names(void);
json_t *discover_bundled_skills(const char *bundled_dir);
char *compute_relative_dest(const char *skill_dir, const char *bundled_dir);
char *safe_rel_install_path(const char *path, const char *base);

#endif /* SLERMES_PORT_SKILLS_SYNC_H */
