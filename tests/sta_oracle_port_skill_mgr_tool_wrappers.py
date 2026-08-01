"""AUTO-GENERATED integration oracle for port_skill_mgr_tool_wrappers (gen_integration_oracle.py)."""
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
MODS['tools.skill_manager_tool'] = _load('tools/skill_manager_tool.py')

DISPATCH = {
    'smt_mark_background_review_skill_read': ('tools.skill_manager_tool', 'mark_background_review_skill_read'),
    'smt_u_background_review_has_read': ('tools.skill_manager_tool', '_background_review_has_read'),
    'smt_u_reset_background_review_read_marks': ('tools.skill_manager_tool', '_reset_background_review_read_marks'),
    'smt_u_guard_agent_created_enabled': ('tools.skill_manager_tool', '_guard_agent_created_enabled'),
    'smt_u_security_scan_skill': ('tools.skill_manager_tool', '_security_scan_skill'),
    'smt_u_pinned_guard': ('tools.skill_manager_tool', '_pinned_guard'),
    'smt_u_background_review_write_guard': ('tools.skill_manager_tool', '_background_review_write_guard'),
    'smt_u_background_review_read_before_write_guard': ('tools.skill_manager_tool', '_background_review_read_before_write_guard'),
    'smt_u_background_review_preflight': ('tools.skill_manager_tool', '_background_review_preflight'),
    'smt_u_curator_consolidation_delete_guard': ('tools.skill_manager_tool', '_curator_consolidation_delete_guard'),
    'smt_u_validate_category': ('tools.skill_manager_tool', '_validate_category'),
    'smt_u_validate_frontmatter': ('tools.skill_manager_tool', '_validate_frontmatter'),
    'smt_u_resolve_skill_dir': ('tools.skill_manager_tool', '_resolve_skill_dir'),
    'smt_u_find_skill_in_other_profiles': ('tools.skill_manager_tool', '_find_skill_in_other_profiles'),
    'smt_u_skill_not_found_error': ('tools.skill_manager_tool', '_skill_not_found_error'),
    'smt_u_atomic_write_text': ('tools.skill_manager_tool', '_atomic_write_text'),
    'smt_u_add_description_prompt_preview': ('tools.skill_manager_tool', '_add_description_prompt_preview'),
    'smt_u_create_skill': ('tools.skill_manager_tool', '_create_skill'),
    'smt_u_edit_skill': ('tools.skill_manager_tool', '_edit_skill'),
    'smt_u_patch_skill': ('tools.skill_manager_tool', '_patch_skill'),
    'smt_u_delete_skill': ('tools.skill_manager_tool', '_delete_skill'),
    'smt_u_remove_file': ('tools.skill_manager_tool', '_remove_file'),
    'smt_u_apply_skill_write_gate': ('tools.skill_manager_tool', '_apply_skill_write_gate'),
    'smt_apply_skill_pending': ('tools.skill_manager_tool', 'apply_skill_pending'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_skill_mgr_tool_wrappers.py <cases.json>\n"); return 2
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
