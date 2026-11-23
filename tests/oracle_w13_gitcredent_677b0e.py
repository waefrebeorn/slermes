#!/usr/bin/env python3
"""Differential oracle for gate w13_gitcredent_677b0e (hermes_cli/git_credentials.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_gitcredent_677b0e"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = sys.modules.get(".".join(parts[:i]))
        if parent is not None and not hasattr(parent, parts[i]):
            setattr(parent, parts[i], mod)
    return mod


_seed("hermes_cli")
_seed("hermes_cli._subprocess_compat",
      noninteractive_git_env=lambda: {},
      windows_hide_flags=lambda: 0)

SRC = QUARRY + "/hermes_cli/git_credentials.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    func = r.get("func")
    if func == "origin":
        return live._https_origin(r.get("url") or "")
    if func == "token_pair":
        return live._token_pair(r.get("token"))
    if func == "auth_headers":
        return sorted(live._auth_headers(r.get("env") or {}))
    if func == "credential_required":
        res = types.SimpleNamespace(stderr=r.get("err"), stdout=r.get("stdout"))
        return live.is_credential_required_error(res)
    if func == "with_git_auth":
        auth = r.get("auth")
        return live.with_git_auth(r.get("env") or {}, r.get("url") or "",
                                  tuple(auth) if isinstance(auth, list) else auth)
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
