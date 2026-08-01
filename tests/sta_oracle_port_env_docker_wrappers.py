"""AUTO-GENERATED integration oracle for port_env_docker_wrappers (gen_integration_oracle.py)."""
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
MODS['tools.environments.docker'] = _load('tools/environments/docker.py')

DISPATCH = {
    'envd_u_normalize_forward_env_names': ('tools.environments.docker', '_normalize_forward_env_names'),
    'envd_u_normalize_env_dict': ('tools.environments.docker', '_normalize_env_dict'),
    'envd_u_load_hermes_env_vars': ('tools.environments.docker', '_load_hermes_env_vars'),
    'envd_u_sanitize_label_value': ('tools.environments.docker', '_sanitize_label_value'),
    'envd_u_get_active_profile_name': ('tools.environments.docker', '_get_active_profile_name'),
    'envd_reap_orphan_containers': ('tools.environments.docker', 'reap_orphan_containers'),
    'envd_u_container_finished_at': ('tools.environments.docker', '_container_finished_at'),
    'envd_find_docker': ('tools.environments.docker', 'find_docker'),
    'envd_u_egress_proxy_args_for_docker': ('tools.environments.docker', '_egress_proxy_args_for_docker'),
    'envd_u_egress_reuse_fingerprint': ('tools.environments.docker', '_egress_reuse_fingerprint'),
    'envd_u_egress_enforce_on_docker': ('tools.environments.docker', '_egress_enforce_on_docker'),
    'envd_u_critical_egress_env_names': ('tools.environments.docker', '_critical_egress_env_names'),
    'envd_u_extra_args_egress_collisions': ('tools.environments.docker', '_extra_args_egress_collisions'),
    'envd_u_build_security_args': ('tools.environments.docker', '_build_security_args'),
    'envd_u_image_uses_init_entrypoint': ('tools.environments.docker', '_image_uses_init_entrypoint'),
    'envd_u_resolve_host_user_spec': ('tools.environments.docker', '_resolve_host_user_spec'),
    'envd_u_cgroup_limits_available': ('tools.environments.docker', '_cgroup_limits_available'),
    'envd_u_ensure_docker_available': ('tools.environments.docker', '_ensure_docker_available'),
    'envd_u_build_init_env_args': ('tools.environments.docker', '_build_init_env_args'),
    'envd_u_is_container_gone': ('tools.environments.docker', '_is_container_gone'),
    'envd_u_recreate_container': ('tools.environments.docker', '_recreate_container'),
    'envd_u_storage_opt_supported': ('tools.environments.docker', '_storage_opt_supported'),
    'envd_u_container_network_mode': ('tools.environments.docker', '_container_network_mode'),
    'envd_u_find_reusable_container': ('tools.environments.docker', '_find_reusable_container'),
    'envd_wait_for_cleanup': ('tools.environments.docker', 'wait_for_cleanup'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_env_docker_wrappers.py <cases.json>\n"); return 2
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
