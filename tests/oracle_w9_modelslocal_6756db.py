#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/models_local.py
(w9_modelslocal_6756db gate). The live helpers do the real work."""
import importlib.util
import json
import sys
import types

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


agent = _mod("agent")
agent.__path__ = []
_mod("agent.secret_scope", get_secret_str=lambda *a, **k: "")
hcli = _mod("hermes_cli")
hcli.__path__ = []
_mod("hermes_cli.urllib_security", url_origin=lambda x: x)

SRC = Q + "/hermes_cli/models_local.py"
spec = importlib.util.spec_from_file_location("modelslocal_w9_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["modelslocal_w9_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "normalize":
            py = live._normalize_openai_base_url(r.get("base_url", ""))
        elif func == "root":
            py = live._root_for_ollama_native_api(r.get("base_url", ""))
        elif func == "host":
            py = live._ollama_host_from_env(r.get("env_host", ""))
        elif func == "suffix":
            py = live._strip_ollama_cloud_suffix(r.get("model_id", ""))
        elif func == "pint":
            py = live._positive_int(r.get("value"))
            if py is None:
                py = None
        elif func == "tags":
            py = live._parse_ollama_tags(r.get("payload"))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w9_modelslocal_6756db: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
