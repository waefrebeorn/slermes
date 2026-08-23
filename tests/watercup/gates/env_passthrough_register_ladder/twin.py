"""twin for env_passthrough_register_ladder.

Drives the LIVE tools/env_passthrough.py registry through the same ladder
as the C harness:
  B1 empty / B2 blocked credential / B3 already allowed / B4 registered.
V1 snapshots the live allowlist after each probe.
"""
import os
import sys
import importlib.util as ilu
import importlib

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_env_passthrough",
    os.path.join(DEV, "tools", "env_passthrough.py"))
_ep = importlib.util.module_from_spec(_spec)
sys.modules["wc_live_env_passthrough"] = _ep
_spec.loader.exec_module(_ep)


def _snapshot():
    return ",".join(sorted(_ep._get_allowed()))


def trace(raw, warn_capture):
    c = Cup("env_passthrough_register")
    # B1: empty after strip
    name = (raw or "").strip()
    if not name:
        c.branch(True)
        c.valstr(_snapshot())
        c.finalize()
        return False, c
    c.branch(False)

    # B2: Hermes provider credential (dynamic internal secret included)
    if _ep._is_hermes_provider_credential(name):
        c.branch(True)
        c.valstr(_snapshot())
        c.finalize()
        return False, c
    c.branch(False)

    # B3: already registered
    if _ep.is_env_passthrough(name):
        c.branch(True)
        c.valstr(_snapshot())
        c.finalize()
        return True, c
    c.branch(False)

    # B4: register (capture the module's logger warning noise)
    before = set(_ep._get_allowed())
    _ep.register_env_passthrough([name])
    registered = name in _ep._get_allowed() and name not in before
    c.branch(True)
    c.valstr(_snapshot())
    c.finalize()
    return registered, c


def run(inputs):
    _ep._get_allowed().clear()
    outs = []
    for p in inputs:
        outs.append(trace(p, None)[1].finalize())
    return outs
