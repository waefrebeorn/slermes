"""twin for agent_file_safety_classify_write_denial.

Imports LIVE agent/file_safety.py::_classify_write_denial and traces the
same predicate ladder as the C harness (which links the REAL port compiled
with -Dstatic=).  Each branch is computed with each side's own real logic;
the final verdict value comes from the REAL function on both sides.

Ladder: B1 approval(~/.ssh/config) B2 exact-denied B3 denied-prefix
B4 hermes .env B5 hermes mcp-tokens B6 hermes pairing B7 outside-safe-root.
Value: verdict string ("credential"/"safe_root"/"" for None).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu

spec = ilu.spec_from_file_location("wc_fs", os.path.join(DEV, "agent", "file_safety.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_fs"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def _resolve(path):
    return os.path.realpath(os.path.expanduser(str(path)))


def run(inputs):
    home = os.path.realpath(os.path.expanduser("~"))
    hh = str(mod._hermes_home_path())
    hr = str(mod._hermes_root_path())

    approval = mod.build_write_approval_paths(home)
    exact = mod.build_write_denied_paths(home)
    prefixes = mod.build_write_denied_prefixes(home)

    cups = []
    for raw in inputs:
        resolved = _resolve(raw)
        c = Cup("classify_write_denial")
        c.branch(resolved in approval)                       # B1
        c.branch(resolved in exact)                          # B2
        c.branch(any(resolved.startswith(p) for p in prefixes))  # B3
        p_env = resolved == os.path.realpath(os.path.join(hh, ".env")) or             resolved == os.path.realpath(os.path.join(hr, ".env"))
        p_mcp = p_pairing = False
        for base in (hh, hr):
            mcp = os.path.realpath(os.path.join(base, "mcp-tokens"))
            if resolved == mcp or resolved.startswith(mcp + os.sep):
                p_mcp = True
            pairing = os.path.realpath(os.path.join(base, "pairing"))
            if resolved == pairing or resolved.startswith(pairing + os.sep):
                p_pairing = True
        c.branch(p_env)                                      # B4
        c.branch(p_mcp)                                      # B5
        c.branch(p_pairing)                                  # B6
        roots = mod.get_safe_write_roots()
        outside = bool(roots) and not any(
            resolved == r or resolved.startswith(r + os.sep) for r in roots)
        c.branch(outside)                                    # B7

        verdict = mod._classify_write_denial(raw)
        c.valstr(verdict if verdict else "")
        cups.append(c.finalize())
    return cups
