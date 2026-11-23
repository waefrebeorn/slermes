#!/usr/bin/env python3
"""Differential oracle for tools/file_tools_paths.py (gate w9_filetoolsp_5a4ce8)."""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m

agent = _mod("agent"); agent.__path__ = [Q + "/agent"]
_mod("agent.runtime_self_protection", split_entry=lambda p: (p.rsplit("/", 1)[0] if "/" in p else "", p.rsplit("/", 1)[-1]))
tools = _mod("tools"); tools.__path__ = [Q + "/tools"]
_mod("tools.terminal_tool_config", _is_container_backend=lambda t: False,
     plugin_sandbox_home=lambda e: None, coerce_ssh_remote_cwd=lambda t, k: t)
envs = _mod("tools.environments"); envs.__path__ = [Q + "/tools/environments"]
_mod("tools.environments.sandbox_scope", hosted_scope=lambda: None)
HOME = [""]
_mod("hermes_constants", get_subprocess_home=lambda: HOME[0])

NAME = "w9_filetoolsp_5a4ce8_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/tools/file_tools_paths.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "normalize_posix":
        return str(live._normalize_without_host_deref(r.get("path", "")))
    if f == "ssh_escapes_home":
        return live._ssh_path_escapes_home(r.get("path", ""))
    if f == "sentinel":
        return str(r.get("value") or "").strip().lower() in live._TERMINAL_CWD_SENTINELS
    if f == "expand_tilde":
        HOME[0] = r.get("home", "")
        oldjoin = live.os.path.join
        live.os.path.join = lambda a, b: a + "/" + b
        try:
            return live._expand_tilde(r.get("path", ""), r.get("task", "default"))
        finally:
            live.os.path.join = oldjoin
    return None

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w9_filetoolsp_5a4ce8: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w9_filetoolsp_5a4ce8 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w9_filetoolsp_5a4ce8: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
