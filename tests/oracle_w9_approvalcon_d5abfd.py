#!/usr/bin/env python3
"""Differential oracle for tools/approval_context.py (gate w9_approvalcon_d5abfd)."""
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
_mod("agent.i18n", t=lambda key, count=None: f"{key}|{count}")
hcli = _mod("hermes_cli"); hcli.__path__ = [Q + "/hermes_cli"]
def _cfg_get(config, section, key, default=None):
    try:
        return config[section][key]
    except Exception:
        return default
_cfg = {}
_mod("hermes_cli.config", cfg_get=_cfg_get, load_config_readonly=lambda: _cfg)
_mod("utils", env_var_enabled=lambda name: False, is_truthy_value=lambda v: bool(v))

NAME = "w9_approvalcon_d5abfd_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/tools/approval_context.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

def compute(r):
    global _cfg
    f = r.get("func")
    if f == "normalize_mode":
        return live._normalize_approval_mode(r.get("value"))
    if f == "format_window":
        return live.format_approval_window(int(r.get("seconds", "0")))
    if f == "binary_mode":
        _cfg = {"approvals": {"k": r.get("value")}}
        return live._binary_approval_mode("k")
    if f == "unattended_platform":
        live._get_session_platform = lambda: r.get("platform", "")
        return bool(live._is_unattended_platform_approval_context())
    if f == "prompt_authority":
        req = r.get("req") or {}
        env = r.get("env") or {}
        approvers = r.get("approvers")
        live._session_env = lambda name: env.get(name, "") or ""
        live._get_approval_config = lambda: ({"approvers": approvers} if approvers is not None else {})
        def pick(name):
            v = req.get(name)
            return None if v is None else str(v)
        return live.prompt_authority(requester_id=pick("requester_id"),
                                     requester_name=pick("requester_name"),
                                     chat_type=pick("chat_type"))
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
        print(f"ORACLE w9_approvalcon_d5abfd: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w9_approvalcon_d5abfd {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w9_approvalcon_d5abfd: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
