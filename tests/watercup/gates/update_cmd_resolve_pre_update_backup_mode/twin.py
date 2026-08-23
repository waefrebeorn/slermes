"""twin for update_cmd_resolve_pre_update_backup_mode.

Imports the LIVE Python hermes_cli/update_cmd.py::_resolve_pre_update_backup_mode
and traces the SAME branch ladder as the traced-C harness. Inputs are encoded
"<no_backup:0|1> <backup:0|1> <raw|->" ("-": config key absent). The traced
result is asserted equal to the LIVE fn output on every input (config load is
satisfied by a stubbed hermes_cli.config.load_config).
"""
import os, sys, types

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_update_cmd", os.path.join(DEV, "hermes_cli", "update_cmd.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_update_cmd"] = mod
try:
    spec.loader.exec_module(mod)
except Exception:
    pass  # heavy CLI imports may fail; only _resolve_pre_update_backup_mode is needed

from watercup_py import Cup


class Args:
    def __init__(self, no_backup, backup):
        self.no_backup = no_backup
        self.backup = backup


def live(inp):
    nb_s, b_s, raw_s = inp.split(" ", 2)
    args = Args(nb_s == "1", b_s == "1")
    raw = None if raw_s == "-" else raw_s
    fake_cfg = {"updates": {"pre_update_backup": raw}} if raw is not None else {}
    stub = types.ModuleType("hermes_cli.config")
    stub.load_config = lambda: fake_cfg
    pkg = sys.modules.get("hermes_cli") or types.ModuleType("hermes_cli")
    saved = {k: sys.modules.get(k) for k in ("hermes_cli", "hermes_cli.config")}
    sys.modules["hermes_cli"] = pkg
    sys.modules["hermes_cli.config"] = stub
    try:
        return mod._resolve_pre_update_backup_mode(args)
    finally:
        for k, v in saved.items():
            if v is None:
                sys.modules.pop(k, None)
            else:
                sys.modules[k] = v


def traced(inp):
    c = Cup("resolve_pre_update_backup_mode")
    nb_s, b_s, raw_s = inp.split(" ", 2)
    nb, b = nb_s == "1", b_s == "1"
    raw = None if raw_s == "-" else raw_s

    def done(out):
        c.valstr(out)
        lv = live(inp)
        assert out == lv, f"{out!r} != LIVE {lv!r}"
        return c.finalize()

    c.branch(nb)
    if nb:
        return done("off")
    c.branch(b)
    if b:
        return done("full")

    missing = raw is None
    c.branch(missing)
    if missing:
        return done("quick")

    # Legacy boolean form (JSON booleans arrive as these exact strings).
    c.branch(raw == "true")
    if raw == "true":
        return done("full")
    c.branch(raw == "false")
    if raw == "false":
        return done("off")

    mode = str(raw).strip().lower()
    c.branch(mode in ("off", "false", "none", "disabled"))
    if mode in ("off", "false", "none", "disabled"):
        return done("off")
    c.branch(mode in ("full", "zip", "true"))
    if mode in ("full", "zip", "true"):
        return done("full")
    c.branch(mode == "quick")
    if mode == "quick":
        return done("quick")
    return done("quick")  # unknown -> warn + quick


def run(inputs):
    return [traced(v) for v in inputs]
