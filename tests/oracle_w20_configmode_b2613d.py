#!/usr/bin/env python3
"""Differential oracle for hermes_cli/config_model_section.py (w20_configmode_b2613d).

The live module is imported against a stubbed ``hermes_cli.route_identity`` (its
only unrelated import-time dependency) and the *real* ``hermes_cli.config_providers``,
so ``coerce_provider_id`` and the per-provider warning dedupe are the live ones.
Compared values: the normalized ``model:`` mapping and the warning signatures the
flatten loop emits.
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _load(mod_name, path):
    sp = importlib.util.spec_from_file_location(mod_name, path)
    mod = importlib.util.module_from_spec(sp)
    sys.modules[mod_name] = mod
    sp.loader.exec_module(mod)
    return mod


if "hermes_cli" not in sys.modules:
    _mk("hermes_cli")
_mk("hermes_cli.route_identity", normalize_route_base_url=lambda value: value)

providers = _load("hermes_cli.config_providers", os.path.join(Q, "hermes_cli", "config_providers.py"))
live = _load("w20_configmode_b2613d_live",
             os.path.join(Q, "hermes_cli", "config_model_section.py"))

WARNINGS = []


class _Logger:
    def warning(self, msg, *args, **kw):
        # ``_warn_once_per_provider`` logs (msg, _key, _extra, _extra); the dedupe
        # signature the C port reports is "discarded:<key>.<extra>".
        if len(args) >= 2:
            WARNINGS.append("discarded:%s.%s" % (args[0], args[1]))
        else:
            WARNINGS.append(msg)

    def info(self, *a, **k):
        pass


providers.logger = _Logger()


def expected(r):
    f = r.get("func")
    if f == "coerce_provider":
        return providers.coerce_provider_id(r.get("value"))
    if f == "normalize":
        cfg = r.get("config")
        if not isinstance(cfg, dict):
            cfg = {}
        providers._PROVIDER_NORMALIZE_WARNED.clear()
        WARNINGS.clear()
        out = live._normalize_root_model_keys(json.loads(json.dumps(cfg)))
        return {"config": out, "warnings": list(WARNINGS)}
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001 - the diff is the point
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_configmode_b2613d: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
