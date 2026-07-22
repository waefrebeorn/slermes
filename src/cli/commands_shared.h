/*
 * commands_shared.h - symbols shared between commands.c (dispatch
 * facade) and the cli_cmd_<category>.c handler modules.
 */
#ifndef SLERMES_COMMANDS_SHARED_H
#define SLERMES_COMMANDS_SHARED_H

#include "hermes_core_types.h"

/* System + POSIX headers the extracted cli_cmd_* handlers need (moved out of
 * commands.c, which pulled these in late). Centralised here so every handler
 * module that includes commands_shared.h compiles identically to the old facade. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/utsname.h>

/* Type headers for handlers that reference these structs (were transitively
 * available in commands.c via its late local includes). */
#include "hermes_secrets.h"
#include "hermes_gateway.h"
#include "hermes_skills_hub.h"
#include "mcp.h"
#include "hermes_auth.h"
#include "hermes_curator.h"
#include "skill_usage.h"
#include "usage_pricing.h"
#include "hermes_insights.h"
#include "provider.h"
#include "provider_metadata.h"

#define CMD_COMPRESS_DEFAULT_KEEP_LAST 2
#define CMD_COMPRESS_MAX_KEEP_LAST 100

/* Shared CLI state. Defined (non-static) in commands.c facade; declared
 * extern here so the extracted cli_cmd_<cat>.c handler modules can read/write
 * it. Mirrors the g_busy_mode precedent. */
extern int g_busy_mode;
extern int g_yolo_mode;
extern int g_verbose;
extern char g_current_skin[64];
extern int g_fast_mode;
extern char g_queued_prompt[4096];
extern char g_home_channel[256];
extern char g_indicator_style[32];
extern int g_statusbar_on;
extern int g_footer_on;
extern int g_voice_mode;
/* MCP server registry (defined non-static in src/tools/mcp_tool.c). */
extern int g_server_count;
extern mcp_server_t *g_servers[];

/* Config category table type + canonical array (defined non-static in
 * cli_cmd_config.c; the facade's /config path also reads it). */
typedef struct {
    const char *name;
    const char *desc;
    const char *prefix;
    int key_count;
} cfg_category_t;

extern const cfg_category_t CFG_CATEGORIES[];
extern bool show_config_section(const hermes_config_t *cfg, const char *section);
extern bool config_set_key(hermes_config_t *cfg, const char *args);

/* Shared config-display helpers (defined non-static in commands.c facade;
 * used by both the facade and cli_cmd_config.c). */
void show_cfg_val(const char *key, const char *type, const char *val);
void show_cfg_val_int(const char *key, int val);
void show_cfg_val_bool(const char *key, bool val);
void show_cfg_val_float(const char *key, float val);
void show_section_model(const hermes_config_t *cfg);
void show_section_display(const hermes_config_t *cfg);
void show_section_agent(const hermes_config_t *cfg);
void show_section_tools(const hermes_config_t *cfg);
void show_section_auxiliary(const hermes_config_t *cfg);
void show_section_tts(const hermes_config_t *cfg);
void show_section_stt(const hermes_config_t *cfg);
void show_section_voice(const hermes_config_t *cfg);
void show_section_delegation(const hermes_config_t *cfg);
void show_section_browser(const hermes_config_t *cfg);
void show_section_memory(const hermes_config_t *cfg);
void show_section_compression(const hermes_config_t *cfg);
void show_section_cron(const hermes_config_t *cfg);
void show_section_notification(const hermes_config_t *cfg);
void show_section_security(const hermes_config_t *cfg);
void show_section_sessions(const hermes_config_t *cfg);
void show_section_plugin(const hermes_config_t *cfg);
void show_section_mcp(const hermes_config_t *cfg);

extern const command_def_t COMMANDS[];

/* Shared message/handoff helpers (defined non-static in commands.c facade;
 * used by cli_cmd_session.c and cli_cmd_system.c). */
void print_messages(const agent_state_t *state, size_t start, size_t count,
                    const char *filter_role, bool show_full);
void handoff_write_request(const char *handoff_id, const char *platform,
                           const char *session_id, const char *requester);

/* Command dispatch API (defined non-static in commands.c facade). */
extern const command_def_t *commands_get_all(void);
extern const command_def_t *commands_resolve(const char *input);
extern int commands_count(void);

typedef struct {
    char *id;
    char *platform;
    char *session_id;
    char *requester;
    char *status;
} handoff_entry_t;

typedef struct {
    void **items;
    int count;
    int capacity;
} list_t;

#endif /* SLERMES_COMMANDS_SHARED_H */
