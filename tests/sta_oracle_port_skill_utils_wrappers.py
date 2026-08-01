"""AUTO-GENERATED integration oracle for port_skill_utils_wrappers (gen_integration_oracle.py)."""
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
MODS['agent.skill_utils'] = _load('agent/skill_utils.py')

DISPATCH = {
    'sku_is_excluded_skill_path': ('agent.skill_utils', 'is_excluded_skill_path'),
    'sku_yaml_load': ('agent.skill_utils', 'yaml_load'),
    'sku_parse_frontmatter': ('agent.skill_utils', 'parse_frontmatter'),
    'sku_skill_matches_platform_list': ('agent.skill_utils', 'skill_matches_platform_list'),
    'sku_skill_matches_platform': ('agent.skill_utils', 'skill_matches_platform'),
    'sku_u_detect_environment': ('agent.skill_utils', '_detect_environment'),
    'sku_skill_matches_environment': ('agent.skill_utils', 'skill_matches_environment'),
    'sku_get_disabled_skill_names': ('agent.skill_utils', 'get_disabled_skill_names'),
    'sku_u_normalize_string_set': ('agent.skill_utils', '_normalize_string_set'),
    'sku_u_external_dirs_cache_clear': ('agent.skill_utils', '_external_dirs_cache_clear'),
    'sku_get_external_skills_dirs': ('agent.skill_utils', 'get_external_skills_dirs'),
    'sku_get_all_skills_dirs': ('agent.skill_utils', 'get_all_skills_dirs'),
    'sku_normalize_skill_lookup_name': ('agent.skill_utils', 'normalize_skill_lookup_name'),
    'sku_extract_skill_conditions': ('agent.skill_utils', 'extract_skill_conditions'),
    'sku_extract_skill_config_vars': ('agent.skill_utils', 'extract_skill_config_vars'),
    'sku_discover_all_skill_config_vars': ('agent.skill_utils', 'discover_all_skill_config_vars'),
    'sku_u_resolve_dotpath': ('agent.skill_utils', '_resolve_dotpath'),
    'sku_resolve_skill_config_values': ('agent.skill_utils', 'resolve_skill_config_values'),
    'sku_u_normalize_skill_description': ('agent.skill_utils', '_normalize_skill_description'),
    'sku_extract_skill_description': ('agent.skill_utils', 'extract_skill_description'),
    'sku_is_skill_description_truncated_for_prompt': ('agent.skill_utils', 'is_skill_description_truncated_for_prompt'),
    'sku_iter_skill_index_files': ('agent.skill_utils', 'iter_skill_index_files'),
    'sku_parse_qualified_name': ('agent.skill_utils', 'parse_qualified_name'),
    'sku_is_valid_namespace': ('agent.skill_utils', 'is_valid_namespace'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_skill_utils_wrappers.py <cases.json>\n"); return 2
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
