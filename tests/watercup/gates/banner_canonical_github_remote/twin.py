"""twin for banner_canonical_github_remote.

Traces LIVE hermes_cli/banner.py::_canonical_github_remote with the same
rung order as the C harness.
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
    "wc_live_banner",
    os.path.join(DEV, "hermes_cli", "banner.py"))
_bn = importlib.util.module_from_spec(_spec)
sys.modules["wc_live_banner"] = _bn
_spec.loader.exec_module(_bn)


def trace(url):
    c = Cup("canonical_github_remote")
    value = ""
    if not url:
        c.branch(True)                                   # B1
        c.valstr("")                                     # V4
        return c.finalize()
    c.branch(False)
    value = url.strip()
    c.valstr(value)                                      # V1
    if value.startswith("git@github.com:"):
        c.branch(True)                                   # B2
        value = "github.com/" + value[len("git@github.com:"):]
    else:
        c.branch(False)
        if value.startswith("ssh://git@github.com/"):
            c.branch(True)                               # B3
            value = "github.com/" + value[len("ssh://git@github.com/"):]
        else:
            c.branch(False)
            parsed = _bn.urlparse(value)
            usable = bool(parsed.netloc) and bool(parsed.path)
            c.branch(usable)                             # B4
            if usable:
                value = f"{parsed.netloc}{parsed.path}"
    c.valstr(value)                                      # V2
    value = value.strip().rstrip("/")
    if value.endswith(".git"):
        c.branch(True)
        value = value[:-4]
    else:
        c.branch(False)
    c.valstr(value)                                      # V3
    value = value.lower()
    c.valstr(value)                                      # V4
    return c.finalize()


def run(inputs):
    return [trace(u) for u in inputs]
