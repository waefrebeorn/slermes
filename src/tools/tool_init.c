/*
 * tool_init.c — Tool initialization for Hermes C.
 * Auto-discovery: calls all registry_init_*() functions.
 *
 * Port of Python tool registration system.
 */

#include "hermes_core_types.h"
#include "registry.h"
#include "memory_store.h"  /* memory_tool_set_gate + memory_write_gate_decision_t (live memory wiring) */

/* P168: File sandbox init */
void sandbox_init(void);

/* O14: Sandbox escape detection init */
void sandbox_escape_init(void);

/* Tool init declarations (defined in each tool's .c file) */
void registry_init_terminal(void);
void registry_init_file(void);
void registry_init_web(void);
void registry_init_skills(void);
void registry_init_patch(void);
void registry_init_exec_code(void);
void registry_init_clarify(void);
void registry_init_memory(void);
void registry_init_todo(void);
void registry_init_process(void);
void registry_init_send_message(void);
void registry_init_cronjob(void);
void registry_init_skill_view(void);
void registry_init_xai_http(void);
void registry_init_path_security(void);
void registry_init_ansi_strip(void);
void registry_init_session_search(void);
void registry_init_session_crud(void);
void registry_init_tts(void);
void registry_init_vision(void);
void registry_init_delegate(void);
void registry_init_x_search(void);
void registry_init_browser(void);
void registry_init_approval(void);
void registry_init_voice(void);
void registry_init_transcribe(void);
void registry_init_image_gen(void);
void registry_init_video_gen(void);
void registry_init_homeassistant(void);
void registry_init_kanban(void);
void registry_init_computer_use(void);
void registry_init_discord(void);
void registry_init_mcp(void);
void registry_init_file_batch(void);

void registry_init_file_watch(void);
void registry_init_feishu_tools(void);
void registry_init_file_merge(void);
void registry_init_mixture_of_agents(void);
void registry_init_video_analyze(void);
void registry_init_yuanbao_tools(void);
void registry_init_env_probe(void);
void registry_init_skills_guard(void);
void registry_init_curator_backup(void);
void registry_init_close_terminal(void);

/* ---- write-approval gate -> live memory tool (wiring layer) -------------
 * This belongs here (not in either port module) so neither port couples to
 * the other: tool_init.c links both port_memory_tool.o and
 * port_tools_write_approval.o. Faithful to tools/memory_tool.py:_apply_write_gate:
 * gate OFF -> writes flow freely (allow); gate ON -> stage (the core binary
 * has no interactive prompt channel, mirroring Python's None -> stage). */
extern int  cli_tools_write_approval_write_approval_enabled(const char *subsystem);
extern int  cli_tools_write_approval_evaluate_gate(const char *subsystem, const char *action, const char *detail);
extern json_node_t *cli_tools_write_approval_stage_write(const char *subsystem, const json_node_t *payload, const char *summary, const char *origin);

static memory_write_gate_decision_t wa_memory_gate_adapter(const char *target, const char *detail) {
    memory_write_gate_decision_t d = {0};
    int need = cli_tools_write_approval_evaluate_gate("memory", "memory_write", NULL);
    if (need == 0) { d.allow = 1; return d; }

    json_node_t *payload = json_new_object();
    json_object_set(payload, "action", json_string("memory_write"));
    json_object_set(payload, "target", json_string(target ? target : "memory"));
    json_object_set(payload, "detail", json_string(detail ? detail : ""));
    json_node_t *rec = cli_tools_write_approval_stage_write("memory", payload,
                                                            detail ? detail : "memory write", "memory_tool");
    json_free(payload);
    if (rec) {
        const char *id = json_object_get_string(rec, "id", "");
        d.staged = 1;
        d.pending_id = strdup(id ? id : "");
        d.message = strdup(detail ? detail : "Staged for approval.");
        json_free(rec);
    } else {
        d.allow = 1;  /* staging failed -> fail open rather than drop the write */
    }
    return d;
}

void cli_tools_write_approval_attach_memory_gate(void) {
    memory_tool_set_gate(wa_memory_gate_adapter);
}

/* Register all tools */
void tools_init_all(void) {
    /* P168: Initialize file sandbox before any tool registration */
    sandbox_init();

    /* O14: Initialize sandbox escape detection */
    sandbox_escape_init();

    registry_init_terminal();
    registry_init_close_terminal();
    registry_init_file();
    registry_init_web();
    registry_init_skills();
    registry_init_patch();
    registry_init_exec_code();
    registry_init_clarify();
    registry_init_memory();
    /* Attach the write-approval gate to the live memory tool (fail-open if the
     * gate module can't load — mirrors Python's lazy-import gate). */
    cli_tools_write_approval_attach_memory_gate();
    registry_init_todo();
    registry_init_process();
    registry_init_send_message();
    registry_init_cronjob();
    registry_init_skill_view();
    registry_init_xai_http();
    registry_init_path_security();
    registry_init_ansi_strip();
    registry_init_session_search();
    registry_init_session_crud();
    registry_init_tts();
    registry_init_vision();
    registry_init_delegate();
    registry_init_x_search();
    registry_init_browser();
    registry_init_approval();
    registry_init_voice();
    registry_init_transcribe();
    registry_init_image_gen();
    registry_init_video_gen();
    registry_init_homeassistant();
    registry_init_kanban();
    registry_init_computer_use();
    registry_init_discord();
    registry_init_mcp();

    /* F15: Batch file ops */
    registry_init_file_batch();

    /* F34: File watch (inotify) */
    registry_init_file_watch();

    /* D22: Feishu doc/drive tools */
    registry_init_feishu_tools();

    /* F33: File merge tool */
    registry_init_file_merge();

    /* N02: Mixture of Agents tool */
    registry_init_mixture_of_agents();

    /* P04: Video analysis tool */
    registry_init_video_analyze();

    /* M05-M07: Yuanbao platform tools */
    registry_init_yuanbao_tools();

    /* System capability probe */
    registry_init_env_probe();

    /* Skills security guard */
    registry_init_skills_guard();

    /* Curator backup / snapshot / rollback */
    registry_init_curator_backup();

    /* P150: Assign toolsets for enabled/disabled filtering */
    registry_set_toolset("browser_navigate", "browser");
    registry_set_toolset("browser_snapshot", "browser");
    registry_set_toolset("browser_back", "browser");
    registry_set_toolset("browser_forward", "browser");
    registry_set_toolset("browser_click", "browser");
    registry_set_toolset("browser_type", "browser");
    registry_set_toolset("browser_scroll", "browser");
    registry_set_toolset("browser_get_images", "browser");
    registry_set_toolset("browser_press", "browser");
    registry_set_toolset("browser_vision", "browser");
    registry_set_toolset("browser_console", "browser");
    registry_set_toolset("browser_dialog", "browser");
    registry_set_toolset("browser_cdp", "browser");

    registry_set_toolset("kanban_show", "kanban");
    registry_set_toolset("kanban_list", "kanban");
    registry_set_toolset("kanban_complete", "kanban");
    registry_set_toolset("kanban_block", "kanban");
    registry_set_toolset("kanban_heartbeat", "kanban");
    registry_set_toolset("kanban_comment", "kanban");
    registry_set_toolset("kanban_create", "kanban");
    registry_set_toolset("kanban_link", "kanban");
    registry_set_toolset("kanban_unblock", "kanban");

    registry_set_toolset("ha_list_entities", "homeassistant");
    registry_set_toolset("ha_get_state", "homeassistant");
    registry_set_toolset("ha_list_services", "homeassistant");
    registry_set_toolset("ha_call_service", "homeassistant");

    registry_set_toolset("voice_listen", "voice");
    registry_set_toolset("voice_speak", "voice");

    registry_set_toolset("image_generate", "image_gen");
    registry_set_toolset("video_generate", "video_gen");
    registry_set_toolset("video_analyze", "vision");
    registry_set_toolset("cronjob", "cron");
    registry_set_toolset("cron_cmd", "cron");
    registry_set_toolset("memory", "memory");
    registry_set_toolset("close_terminal", "terminal");
    registry_set_toolset("delegate_task", "delegate");
    registry_set_toolset("send_message", "send_message");
    registry_set_toolset("computer_use", "computer_use");
}

/* delegate_task — the full delegation lifecycle (spawn child, pipe messages,
 * collect results) is registered here. The handler spawns a child agent process,
 * connects via pipes, and returns the collected result.
 * Until the full spawn/process pipeline is wired, we register with a handler
 * that returns a "delegation not available in this build" message. */
static json_t *delegate_handler(const json_t *args, void *ctx) {
    (void)args; (void)ctx;
    json_t *resp = json_new_object();
    json_object_set(resp, "error", json_string(
        "delegate_task: agent delegation requires full subprocess spawning "
        "which is not available in this build. Use execute_code for "
        "parallel computation instead."));
    return resp;
}

void registry_init_delegate(void) {
    registry_register("delegate_task",
        "Delegate a task to a subagent. Spawns a child agent to work on an "
        "independent subtask. The subagent runs in its own context with its "
        "own tool set. Use for parallel work streams or when you need to "
        "isolate a complex subtask. Returns the subagent's final result.",
        "{\"type\":\"object\","
        "\"properties\":{"
          "\"prompt\":{\"type\":\"string\",\"description\":\"The task instruction for the subagent\"},"
          "\"workdir\":{\"type\":\"string\",\"description\":\"Working directory for the subagent\"}"
        "},"
        "\"required\":[\"prompt\"]"
        "}", delegate_handler);
}

/* transcribe — audio transcription via Whisper STT. */
void registry_init_transcribe(void) {
    /* The real registration lives in transcribe.c */
    extern void registry_init_transcribe_impl(void);
    registry_init_transcribe_impl();
}
