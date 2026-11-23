#ifndef NOC_W6_SESSION_5810AF_H
#define NOC_W6_SESSION_5810AF_H

#include "hermes_json.h"

/* Public C API for the pure helpers of acp_adapter/session.py
 * (gate w6_session_5810af). acpsess_ prefix avoids collisions with the many
 * other session_* symbols in this repo. */
struct json_t *acpsess_build_session_title(const struct json_t *title,
                                           const struct json_t *preview,
                                           const char *cwd);
struct json_t *acpsess_format_updated_at(const struct json_t *value);
struct json_t *acpsess_updated_at_sort_key(const struct json_t *value);
struct json_t *acpsess_parse_model_config(const struct json_t *mc);
struct json_t *acpsess_expand_acp_enabled_toolsets(const struct json_t *toolsets,
                                                   const struct json_t *mcp_names);
struct json_t *acpsess_session_info(const char *sid, const char *cwd,
                                    const struct json_t *model, double history_len,
                                    const struct json_t *title, const struct json_t *preview,
                                    const struct json_t *updated_at);
struct json_t *acpsess_first_user_preview(const struct json_t *history,
                                          const char *default_val);

#endif /* NOC_W6_SESSION_5810AF_H */
