#!/usr/bin/env python3
"""Differential oracle for w14_setupmigra_bd703d (hermes_cli/setup_migration.py).

Credential/env collaborators are injected through stubs: the C harness exposes
an explicit `has_creds` seam rather than mutating the process environment.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
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
    m = sys.modules.get(name)
    if m is None:
        m = _mod(name)
    if not hasattr(m, "__path__"):
        m.__path__ = []
    return m


_pkg("hermes_cli")


def cfg_get(cfg, *keys, default=None):
    """Faithful copy of hermes_cli.config.cfg_get (traverse, default on miss)."""
    if not isinstance(cfg, dict):
        return default
    node = cfg
    for key in keys:
        if not isinstance(node, dict) or key not in node:
            return default
        node = node[key]
    return node


# hermes_cli.setup is imported lazily inside _cfg_summary / _cfg helpers.
setup_stub = _mod("hermes_cli.setup", cfg_get=cfg_get)

SRC = Q + "/hermes_cli/setup_migration.py"
spec = importlib.util.spec_from_file_location("hermes_cli.setup_migration_w14_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def canon(v):
    def conv(x):
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


_HASC = [False]


def _has_creds(_config):
    return _HASC[0]


live._model_section_has_credentials = _has_creds


def compute(r):
    f = r.get("func")
    config = r.get("config")
    _HASC[0] = bool(r.get("has_creds", False))

    if f == "model_summary":
        return live._model_summary(config)
    if f == "cfg_summary":
        return live._cfg_summary(config, r.get("section", ""), r.get("key", ""),
                                 default=r.get("default"), prefix=r.get("prefix", ""))
    if f == "get_section_summary":
        return live._get_section_config_summary(config, r.get("section_key", ""))
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
        py = canon(compute(r))
    except Exception as exc:
        print(f"LIVE ERROR {r.get('func')} case={r.get('case')}: {exc!r}",
              file=sys.stderr)
        mism += 1
        continue
    got = canon(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)

print(f"ORACLE w14_setupmigra_bd703d: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
