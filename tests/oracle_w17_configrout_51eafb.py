#!/usr/bin/env python3
"""Differential oracle for gate w17_configrout_51eafb (hermes_cli/config_route_guard.py)."""
import copy
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


def _pkg(name):
    m = _mod(name)
    m.__path__ = []
    return m


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


hcli = _pkg("hermes_cli")
_mod("hermes_cli.colors", Colors=types.SimpleNamespace(YELLOW=""), color=lambda x, *a, **k: x)
_load("hermes_cli.route_identity", Q + "/hermes_cli/route_identity.py")
utils = _pkg("utils")
urls = _load("utils.urls", Q + "/utils/urls.py")
utils.same_base_url_origin = urls.same_base_url_origin


def _clear_creds(entry):
    for k in ("api_key", "api", "key_env", "api_key_env", "api_mode"):
        entry.pop(k, None)
    return entry


_mod("hermes_cli.config", clear_model_endpoint_credentials=_clear_creds)

NAME = "w17crg_live"
spec = importlib.util.spec_from_file_location(
    NAME, Q + "/hermes_cli/config_route_guard.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

G = "w17_configrout_51eafb"


def compute(r):
    inp = r.get("in") or {}
    func = r.get("func")
    if func == "route_of":
        return list(live._route_of(inp.get("provider", ""), inp.get("base_url", "")))
    if func == "url":
        return live._url(inp.get("value", ""))
    if func == "clear_aux_credentials":
        entry = copy.deepcopy(inp.get("entry") or {})
        changed = live.clear_aux_credentials_on_route_change(entry, inp.get("provider", ""),
                                                             inp.get("base_url", ""))
        return {"changed": changed, "entry": entry}
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
        mism += 1
        print(f"ORACLE {G}: live raised {exc!r} case={r.get('case')}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {G} {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
