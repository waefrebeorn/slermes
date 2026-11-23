#!/usr/bin/env python3
"""Differential oracle for gate w10_skillstool_3abc40 (tools/skills_tool_setup.py)."""
import importlib.util
import json
import sys
import types

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# Inject the plugin-registry / secret seams before the lazy imports run.
class UnscopedSecretError(Exception):
    pass

SECRET = {"value": None}
reg = types.ModuleType("agent.terminal_env_registry")
reg.provider_flag = lambda name, flag, default=False: default
sys.modules["agent.terminal_env_registry"] = reg
sec = types.ModuleType("agent.secret_scope")
sec.UnscopedSecretError = UnscopedSecretError
sec.get_secret = lambda name: SECRET["value"]
sys.modules["agent.secret_scope"] = sec

SRC = Q + "/tools/skills_tool_setup.py"
spec = importlib.util.spec_from_file_location("w10setup_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "remote":
        return live._is_remote_env_backend(r.get("backend", ""))
    if f == "aslist":
        return live._as_dict_list(r.get("raw"))
    if f == "clean":
        return live._clean_str(r.get("value"))
    if f == "required":
        return live._get_required_environment_variables(r.get("fm") or {})
    if f == "capture":
        return live._capture_result(r.get("missing") or [], bool(r.get("skipped", False)), r.get("hint"))
    if f == "note":
        return live._build_setup_note(live.SkillReadinessStatus(r.get("status", "")),
                                      r.get("missing") or [], r.get("help"))
    if f == "persisted":
        SECRET["value"] = r.get("secret")
        return live._is_env_var_persisted(r.get("var", ""), r.get("snapshot") or {})
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except BaseException as e:
        py = {"__raise__": type(e).__name__}
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w10_skillstool_3abc40: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or not cases) else 0)
