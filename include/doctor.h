#ifndef HERMES_DOCTOR_H
#define HERMES_DOCTOR_H

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *doctor_termux_browser_setup_steps(int node_installed);
json_t *doctor_termux_install_all_fallback_notes(void);
int doctor_has_provider_env_config(const char *content);
const char *doctor_tool_availability_detail(const char *toolset, const char *kanban_task_env);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_DOCTOR_H */
