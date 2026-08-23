"""twin for config_normalize_root_model_keys.

The harness LINKS the real C port and traces the same rung ladder:
  degenerate-parse guard (B1) + V event on the compact JSON serialization
of the normalized result (insertion-ordered, matching libjson's compact
serialize format). LIVE hermes_cli/config.py::_normalize_root_model_keys
drives the Python side of the V event.
"""
import json as _json
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_hermes_cli_config", os.path.join(DEV, "hermes_cli", "config.py"))
_cfg = ilu.module_from_spec(_spec)
sys.modules["wc_live_hermes_cli_config"] = _cfg
_spec.loader.exec_module(_cfg)
live_fn = _cfg._normalize_root_model_keys


def _compact(obj) -> str:
    return _json.dumps(obj, separators=(",", ":"), ensure_ascii=False)


def trace(cfg):
    c = Cup("normalize_root_model_keys")
    if cfg is None:
        c.branch(True)                               # B1 degenerate guard
        res = {}
    else:
        c.branch(False)
        res = live_fn(cfg)
    c.valstr(_compact(res))
    return res, c.finalize()


def run(inputs):
    cups = []
    for s in inputs:
        try:
            cfg = _json.loads(s) if s else None
        except ValueError:
            cfg = None
        _, cup = trace(cfg)
        cups.append(cup)
    return cups
