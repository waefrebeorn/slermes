/*
 * registry_init_all.c - run every module's tool registration once.
 *
 * 90 translation units each expose a registry_init_<module>() that calls registry_register() for
 * the tools they port, and nothing called any of them: `slermes tools` listed a single entry
 * (transcribe) because that is the only registration that happened eagerly elsewhere. The Python
 * side registers its whole toolset before the CLI starts; this is that step, and it is generated
 * from the set of initializers that actually exist so a new module is one line away.
 *
 * Order matters only where a later registration overrides an earlier one; registry_register()
 * keeps the first definition per name, so the alphabetical order here is stable and predictable.
 */
#include <stdbool.h>

bool registry_alias(const char *alias, const char *target);

void registry_init_ansi_strip(void);
void registry_init_approval(void);
void registry_init_browser(void);
void registry_init_clarify(void);
void registry_init_close_terminal(void);
void registry_init_computer_use(void);
void registry_init_cron_cmd(void);
void registry_init_cronjob(void);
void registry_init_curator_backup(void);
void registry_init_delegate(void);
void registry_init_desktop_ui(void);
void registry_init_project_tools(void);
void registry_init_xai_video_tools(void);
void registry_init_setup_beat(void);
void registry_init_discord(void);
void registry_init_env_probe(void);
void registry_init_exec_code(void);
void registry_init_feishu_tools(void);
void registry_init_file(void);
void registry_init_file_batch(void);
void registry_init_file_merge(void);
void registry_init_file_watch(void);
void registry_init_homeassistant(void);
void registry_init_image_gen(void);
void registry_init_kanban(void);
void registry_init_mcp(void);
void registry_init_memory(void);
void registry_init_mixture_of_agents(void);
void registry_init_patch(void);
void registry_init_path_security(void);
void registry_init_process(void);
void registry_init_send_message(void);
void registry_init_session_crud(void);
void registry_init_session_search(void);
void registry_init_skill_view(void);
void registry_init_skills(void);
void registry_init_skills_guard(void);
void registry_init_terminal(void);
void registry_init_todo(void);
void registry_init_transcribe(void);
void registry_init_transcribe_impl(void);
void registry_init_tts(void);
void registry_init_video_analyze(void);
void registry_init_video_gen(void);
void registry_init_vision(void);
void registry_init_voice(void);
void registry_init_web(void);
void registry_init_x_search(void);
void registry_init_xai_http(void);
void registry_init_yuanbao_tools(void);

/* Registration is idempotent: registry_register() ignores a name that already exists, so calling
 * this from more than one entry point (CLI, gateway, cron) cannot duplicate tools. */
void hermes_tools_register_all(void)
{
    static bool done = false;
    if (done) return;
    done = true;


    registry_init_ansi_strip();
    registry_init_approval();
    registry_init_browser();
    registry_init_clarify();
    registry_init_close_terminal();
    registry_init_computer_use();
    registry_init_cron_cmd();
    registry_init_cronjob();
    registry_init_curator_backup();
    registry_init_delegate();
    registry_init_desktop_ui();
    registry_init_project_tools();
    registry_init_xai_video_tools();
    registry_init_setup_beat();
    registry_init_discord();
    registry_init_env_probe();
    registry_init_exec_code();
    registry_init_feishu_tools();
    registry_init_file();
    registry_init_file_batch();
    registry_init_file_merge();
    registry_init_file_watch();
    registry_init_homeassistant();
    registry_init_image_gen();
    registry_init_kanban();
    registry_init_mcp();
    registry_init_memory();
    registry_init_mixture_of_agents();
    registry_init_patch();
    registry_init_path_security();
    registry_init_process();
    registry_init_send_message();
    registry_init_session_crud();
    registry_init_session_search();
    registry_init_skill_view();
    registry_init_skills();
    registry_init_skills_guard();
    registry_init_terminal();
    registry_init_todo();
    registry_init_transcribe();
    registry_init_transcribe_impl();
    registry_init_tts();
    registry_init_video_analyze();
    registry_init_video_gen();
    registry_init_vision();
    registry_init_voice();
    registry_init_web();
    registry_init_x_search();
    registry_init_xai_http();
    registry_init_yuanbao_tools();
    /* The Python product exposes these tools under these names (toolsets.BUILTIN_TOOL_NAMES).
     * Without the alias a model asking for "todo_list" gets "unknown tool" even though the
     * implementation is registered as "todo". */
    static const struct { const char *alias, *target; } CANONICAL_ALIASES[] = {
        { "todo_list",      "todo"     },
        { "process_manage", "process"  },
        { "cronjob_manage", "cronjob"  },
    };
    for (size_t i = 0; i < sizeof CANONICAL_ALIASES / sizeof CANONICAL_ALIASES[0]; i++)
        registry_alias(CANONICAL_ALIASES[i].alias, CANONICAL_ALIASES[i].target);
}
