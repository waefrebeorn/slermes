"""AUTO-GENERATED integration oracle for port_cli_commands_mixin_wrappers (gen_integration_oracle.py)."""
import os, sys, json, importlib.util

MODS = {}
def _load(rel):
    _repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if _repo not in sys.path: sys.path.insert(0, _repo)
    for base in sys.path:
        cand = os.path.join(base, rel)
        try:
            spec = importlib.util.spec_from_file_location('live_' + rel.replace('/', '_').replace('.', '_'), cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    return None
MODS['hermes_cli.cli_commands_mixin'] = _load('hermes_cli/cli_commands_mixin.py')

DISPATCH = {
    'ccm_handle_rollback_command': ('hermes_cli.cli_commands_mixin', '_handle_rollback_command'),
    'ccm_handle_snapshot_command': ('hermes_cli.cli_commands_mixin', '_handle_snapshot_command'),
    'ccm_handle_stop_command': ('hermes_cli.cli_commands_mixin', '_handle_stop_command'),
    'ccm_handle_agents_command': ('hermes_cli.cli_commands_mixin', '_handle_agents_command'),
    'ccm_handle_journey_command': ('hermes_cli.cli_commands_mixin', '_handle_journey_command'),
    'ccm_handle_paste_command': ('hermes_cli.cli_commands_mixin', '_handle_paste_command'),
    'ccm_handle_copy_command': ('hermes_cli.cli_commands_mixin', '_handle_copy_command'),
    'ccm_handle_image_command': ('hermes_cli.cli_commands_mixin', '_handle_image_command'),
    'ccm_handle_tools_command': ('hermes_cli.cli_commands_mixin', '_handle_tools_command'),
    'ccm_handle_profile_command': ('hermes_cli.cli_commands_mixin', '_handle_profile_command'),
    'ccm_handle_handoff_command': ('hermes_cli.cli_commands_mixin', '_handle_handoff_command'),
    'ccm_handle_resume_command': ('hermes_cli.cli_commands_mixin', '_handle_resume_command'),
    'ccm_handle_sessions_command': ('hermes_cli.cli_commands_mixin', '_handle_sessions_command'),
    'ccm_handle_branch_command': ('hermes_cli.cli_commands_mixin', '_handle_branch_command'),
    'ccm_handle_personality_command': ('hermes_cli.cli_commands_mixin', '_handle_personality_command'),
    'ccm_handle_pet_command': ('hermes_cli.cli_commands_mixin', '_handle_pet_command'),
    'ccm_handle_hatch_command': ('hermes_cli.cli_commands_mixin', '_handle_hatch_command'),
    'ccm_handle_cron_command': ('hermes_cli.cli_commands_mixin', '_handle_cron_command'),
    'ccm_handle_suggestions_command': ('hermes_cli.cli_commands_mixin', '_handle_suggestions_command'),
    'ccm_handle_blueprint_command': ('hermes_cli.cli_commands_mixin', '_handle_blueprint_command'),
    'ccm_handle_curator_command': ('hermes_cli.cli_commands_mixin', '_handle_curator_command'),
    'ccm_handle_kanban_command': ('hermes_cli.cli_commands_mixin', '_handle_kanban_command'),
    'ccm_handle_skills_command': ('hermes_cli.cli_commands_mixin', '_handle_skills_command'),
    'ccm_handle_learn_command': ('hermes_cli.cli_commands_mixin', '_handle_learn_command'),
    'ccm_handle_memory_command': ('hermes_cli.cli_commands_mixin', '_handle_memory_command'),
    'ccm_handle_background_command': ('hermes_cli.cli_commands_mixin', '_handle_background_command'),
    'ccm_handle_bundles_command': ('hermes_cli.cli_commands_mixin', '_handle_bundles_command'),
    'ccm_handle_browser_command': ('hermes_cli.cli_commands_mixin', '_handle_browser_command'),
    'ccm_handle_goal_command': ('hermes_cli.cli_commands_mixin', '_handle_goal_command'),
    'ccm_handle_goal_draft': ('hermes_cli.cli_commands_mixin', '_handle_goal_draft'),
    'ccm_handle_subgoal_command': ('hermes_cli.cli_commands_mixin', '_handle_subgoal_command'),
    'ccm_handle_skin_command': ('hermes_cli.cli_commands_mixin', '_handle_skin_command'),
    'ccm_handle_prompt_compose_command': ('hermes_cli.cli_commands_mixin', '_handle_prompt_compose_command'),
    'ccm_handle_footer_command': ('hermes_cli.cli_commands_mixin', '_handle_footer_command'),
    'ccm_handle_timestamps_command': ('hermes_cli.cli_commands_mixin', '_handle_timestamps_command'),
    'ccm_handle_reasoning_command': ('hermes_cli.cli_commands_mixin', '_handle_reasoning_command'),
    'ccm_handle_busy_command': ('hermes_cli.cli_commands_mixin', '_handle_busy_command'),
    'ccm_handle_fast_command': ('hermes_cli.cli_commands_mixin', '_handle_fast_command'),
    'ccm_handle_debug_command': ('hermes_cli.cli_commands_mixin', '_handle_debug_command'),
    'ccm_handle_update_command': ('hermes_cli.cli_commands_mixin', '_handle_update_command'),
    'ccm_handle_voice_command': ('hermes_cli.cli_commands_mixin', '_handle_voice_command'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_cli_commands_mixin_wrappers.py <cases.json>\n"); return 2
    with open(sys.argv[1], 'r', encoding='utf-8') as f: cases = json.load(f)
    for c in cases:
        op = c.get('op'); value = c.get('value', '')
        d = DISPATCH.get(op)
        if not d: sys.stdout.write(json.dumps({'fn':op}, separators=(',',':')) + '\n'); continue
        pymod, pyfn = d
        mod = MODS.get(pymod)
        try:
            out = getattr(mod, pyfn)(value) if mod else None
        except Exception as e:
            out = 'PYERR:' + str(e)
        if isinstance(out, bool): out = bool(out)
        elif isinstance(out, (int, float)) and not isinstance(out, bool): out = int(out)
        elif out is None: out = ''
        else: out = str(out)
        sys.stdout.write(json.dumps({'fn':op,'out':out}, ensure_ascii=True, separators=(',',':')) + '\n')
    return 0

if __name__ == '__main__':
    sys.exit(main())
