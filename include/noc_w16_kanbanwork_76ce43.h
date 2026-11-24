#ifndef NOC_W16_KANBANWORK_76CE43_H
#define NOC_W16_KANBANWORK_76CE43_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_kanbanwork_76ce43_rotation_config @ hermes_cli/kanban_worker_log.py:worker_log_rotation_config */
struct json_t *w16_kanbanwork_76ce43_rotation_config(const struct json_t *kanban_cfg);
/* PoP: w16_kanbanwork_76ce43_rotated_log_path @ hermes_cli/kanban_worker_log.py:_rotated_log_path */
struct json_t *w16_kanbanwork_76ce43_rotated_log_path(const char *log_path, long long generation);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_KANBANWORK_76CE43_H */
