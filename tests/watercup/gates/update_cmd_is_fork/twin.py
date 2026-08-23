"""twin for update_cmd_is_fork.

Traces LIVE hermes_cli/update_cmd.py::_is_fork with the same rung order as
the C harness.
"""
import os
import sys
import importlib.util as ilu
import importlib

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)
sys.path.insert(0, os.path.join(SLERMES, "tests", "watercup"))

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_update_cmd",
    os.path.join(DEV, "hermes_cli", "update_cmd.py"))
_uc = importlib.util.module_from_spec(_spec)
sys.modules["wc_live_update_cmd"] = _uc
_spec.loader.exec_module(_uc)


def trace(origin_url):
    c = Cup("is_fork")
    result = True
    if not origin_url:
        c.branch(True)                                   # B1
        result = False
    else:
        c.branch(False)
        normalized = origin_url.rstrip("/")
        if normalized.endswith(".git"):
            normalized = normalized[:-4]
        c.valstr(normalized)                             # V1
        matched = False
        # LIVE stores a set — pin deterministic (sorted) iteration for both sides
        for official in sorted(_uc.OFFICIAL_REPO_URLS):
            official_normalized = official.rstrip("/")
            if official_normalized.endswith(".git"):
                official_normalized = official_normalized[:-4]
            c.loop()
            eq = (normalized == official_normalized)
            c.branch(eq)                                 # B2
            if eq:
                result = False
                matched = True
                break
        if not matched:
            c.branch(True)                               # B3
    return c.finalize()


def run(inputs):
    return [trace(u) for u in inputs]
