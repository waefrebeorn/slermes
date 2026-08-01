/* AUTO-GENERATED integration oracle harness for port_cli_commands_mixin_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_cli_commands_mixin_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int ccm_handle_rollback_command(const char *);
extern int ccm_handle_snapshot_command(const char *);
extern int ccm_handle_stop_command(const char *);
extern int ccm_handle_agents_command(const char *);
extern int ccm_handle_journey_command(const char *);
extern int ccm_handle_paste_command(const char *);
extern int ccm_handle_copy_command(const char *);
extern int ccm_handle_image_command(const char *);
extern int ccm_handle_tools_command(const char *);
extern int ccm_handle_profile_command(const char *);
extern int ccm_handle_handoff_command(const char *);
extern int ccm_handle_resume_command(const char *);
extern int ccm_handle_sessions_command(const char *);
extern int ccm_handle_branch_command(const char *);
extern int ccm_handle_personality_command(const char *);
extern int ccm_handle_pet_command(const char *);
extern int ccm_handle_hatch_command(const char *);
extern int ccm_handle_cron_command(const char *);
extern int ccm_handle_suggestions_command(const char *);
extern int ccm_handle_blueprint_command(const char *);
extern int ccm_handle_curator_command(const char *);
extern int ccm_handle_kanban_command(const char *);
extern int ccm_handle_skills_command(const char *);
extern int ccm_handle_learn_command(const char *);
extern int ccm_handle_memory_command(const char *);
extern int ccm_handle_background_command(const char *);
extern int ccm_handle_bundles_command(const char *);
extern int ccm_handle_browser_command(const char *);
extern int ccm_handle_goal_command(const char *);
extern int ccm_handle_goal_draft(const char *);
extern int ccm_handle_subgoal_command(const char *);
extern int ccm_handle_skin_command(const char *);
extern int ccm_handle_prompt_compose_command(const char *);
extern int ccm_handle_footer_command(const char *);
extern int ccm_handle_timestamps_command(const char *);
extern int ccm_handle_reasoning_command(const char *);
extern int ccm_handle_busy_command(const char *);
extern int ccm_handle_fast_command(const char *);
extern int ccm_handle_debug_command(const char *);
extern int ccm_handle_update_command(const char *);
extern int ccm_handle_voice_command(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_ccm_handle_rollback_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_rollback_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_rollback_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_snapshot_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_snapshot_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_snapshot_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_stop_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_stop_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_stop_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_agents_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_agents_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_agents_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_journey_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_journey_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_journey_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_paste_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_paste_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_paste_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_copy_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_copy_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_copy_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_image_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_image_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_image_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_tools_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_tools_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_tools_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_profile_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_profile_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_profile_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_handoff_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_handoff_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_handoff_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_resume_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_resume_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_resume_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_sessions_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_sessions_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_sessions_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_branch_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_branch_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_branch_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_personality_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_personality_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_personality_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_pet_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_pet_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_pet_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_hatch_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_hatch_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_hatch_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_cron_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_cron_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_cron_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_suggestions_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_suggestions_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_suggestions_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_blueprint_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_blueprint_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_blueprint_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_curator_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_curator_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_curator_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_kanban_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_kanban_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_kanban_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_skills_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_skills_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_skills_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_learn_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_learn_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_learn_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_memory_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_memory_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_memory_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_background_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_background_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_background_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_bundles_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_bundles_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_bundles_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_browser_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_browser_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_browser_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_goal_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_goal_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_goal_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_goal_draft(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_goal_draft(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_goal_draft"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_subgoal_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_subgoal_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_subgoal_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_skin_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_skin_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_skin_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_prompt_compose_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_prompt_compose_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_prompt_compose_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_footer_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_footer_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_footer_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_timestamps_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_timestamps_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_timestamps_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_reasoning_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_reasoning_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_reasoning_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_busy_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_busy_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_busy_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_fast_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_fast_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_fast_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_debug_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_debug_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_debug_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_update_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_update_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_update_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ccm_handle_voice_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ccm_handle_voice_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ccm_handle_voice_command"));
    json_set(o, "out", json_int(v)); return o;
}

int main(int argc, char **argv){
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    char *err = NULL; json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\n"); free(input); return 2; }
    int n = json_array_size(root);
    for (int i = 0; i < n; i++){
        json_t *c = json_get(root, i);
        const char *op = json_get_str(c, "op", "");
        json_t *o = NULL;
        if (strcmp(op, "ccm_handle_rollback_command") == 0) o = emit_ccm_handle_rollback_command(c);
        if (strcmp(op, "ccm_handle_snapshot_command") == 0) o = emit_ccm_handle_snapshot_command(c);
        if (strcmp(op, "ccm_handle_stop_command") == 0) o = emit_ccm_handle_stop_command(c);
        if (strcmp(op, "ccm_handle_agents_command") == 0) o = emit_ccm_handle_agents_command(c);
        if (strcmp(op, "ccm_handle_journey_command") == 0) o = emit_ccm_handle_journey_command(c);
        if (strcmp(op, "ccm_handle_paste_command") == 0) o = emit_ccm_handle_paste_command(c);
        if (strcmp(op, "ccm_handle_copy_command") == 0) o = emit_ccm_handle_copy_command(c);
        if (strcmp(op, "ccm_handle_image_command") == 0) o = emit_ccm_handle_image_command(c);
        if (strcmp(op, "ccm_handle_tools_command") == 0) o = emit_ccm_handle_tools_command(c);
        if (strcmp(op, "ccm_handle_profile_command") == 0) o = emit_ccm_handle_profile_command(c);
        if (strcmp(op, "ccm_handle_handoff_command") == 0) o = emit_ccm_handle_handoff_command(c);
        if (strcmp(op, "ccm_handle_resume_command") == 0) o = emit_ccm_handle_resume_command(c);
        if (strcmp(op, "ccm_handle_sessions_command") == 0) o = emit_ccm_handle_sessions_command(c);
        if (strcmp(op, "ccm_handle_branch_command") == 0) o = emit_ccm_handle_branch_command(c);
        if (strcmp(op, "ccm_handle_personality_command") == 0) o = emit_ccm_handle_personality_command(c);
        if (strcmp(op, "ccm_handle_pet_command") == 0) o = emit_ccm_handle_pet_command(c);
        if (strcmp(op, "ccm_handle_hatch_command") == 0) o = emit_ccm_handle_hatch_command(c);
        if (strcmp(op, "ccm_handle_cron_command") == 0) o = emit_ccm_handle_cron_command(c);
        if (strcmp(op, "ccm_handle_suggestions_command") == 0) o = emit_ccm_handle_suggestions_command(c);
        if (strcmp(op, "ccm_handle_blueprint_command") == 0) o = emit_ccm_handle_blueprint_command(c);
        if (strcmp(op, "ccm_handle_curator_command") == 0) o = emit_ccm_handle_curator_command(c);
        if (strcmp(op, "ccm_handle_kanban_command") == 0) o = emit_ccm_handle_kanban_command(c);
        if (strcmp(op, "ccm_handle_skills_command") == 0) o = emit_ccm_handle_skills_command(c);
        if (strcmp(op, "ccm_handle_learn_command") == 0) o = emit_ccm_handle_learn_command(c);
        if (strcmp(op, "ccm_handle_memory_command") == 0) o = emit_ccm_handle_memory_command(c);
        if (strcmp(op, "ccm_handle_background_command") == 0) o = emit_ccm_handle_background_command(c);
        if (strcmp(op, "ccm_handle_bundles_command") == 0) o = emit_ccm_handle_bundles_command(c);
        if (strcmp(op, "ccm_handle_browser_command") == 0) o = emit_ccm_handle_browser_command(c);
        if (strcmp(op, "ccm_handle_goal_command") == 0) o = emit_ccm_handle_goal_command(c);
        if (strcmp(op, "ccm_handle_goal_draft") == 0) o = emit_ccm_handle_goal_draft(c);
        if (strcmp(op, "ccm_handle_subgoal_command") == 0) o = emit_ccm_handle_subgoal_command(c);
        if (strcmp(op, "ccm_handle_skin_command") == 0) o = emit_ccm_handle_skin_command(c);
        if (strcmp(op, "ccm_handle_prompt_compose_command") == 0) o = emit_ccm_handle_prompt_compose_command(c);
        if (strcmp(op, "ccm_handle_footer_command") == 0) o = emit_ccm_handle_footer_command(c);
        if (strcmp(op, "ccm_handle_timestamps_command") == 0) o = emit_ccm_handle_timestamps_command(c);
        if (strcmp(op, "ccm_handle_reasoning_command") == 0) o = emit_ccm_handle_reasoning_command(c);
        if (strcmp(op, "ccm_handle_busy_command") == 0) o = emit_ccm_handle_busy_command(c);
        if (strcmp(op, "ccm_handle_fast_command") == 0) o = emit_ccm_handle_fast_command(c);
        if (strcmp(op, "ccm_handle_debug_command") == 0) o = emit_ccm_handle_debug_command(c);
        if (strcmp(op, "ccm_handle_update_command") == 0) o = emit_ccm_handle_update_command(c);
        if (strcmp(op, "ccm_handle_voice_command") == 0) o = emit_ccm_handle_voice_command(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
