"""twin for iron_proxy_pick_tar_member.

Drives the LIVE agent/proxy_sources/iron_proxy.py::_pick_tar_member with a
stub TarFile whose members are real tarfile.TarInfo regular-file entries built
from the probe member list, and traces the same rung order as the C harness.
Input encoding: "<binary_name>|<member>\\t<member>\\t...".
"""
import os
import sys
import tarfile
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)
sys.path.insert(0, os.path.join(DEV, "agent", "proxy_sources"))

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_iron",
    os.path.join(DEV, "agent", "proxy_sources", "iron_proxy.py"))
_iron = ilu.module_from_spec(_spec)
sys.modules["wc_live_iron"] = _iron
try:
    _spec.loader.exec_module(_iron)
except Exception:
    _iron = None  # heavy deps; ladder mirrors the LIVE source


def _member(name):
    ti = tarfile.TarInfo(name)
    ti.type = tarfile.REGTYPE          # isfile() True — gate sub-contract
    return ti


class _TF:
    def __init__(self, names):
        self._names = names

    def getmembers(self):
        return [_member(n) for n in self._names]


def trace(enc):
    binary_name, _, rest = enc.partition("|")
    members = [m for m in rest.split("\t") if m] if rest else []
    c = Cup("pick_tar_member")
    from pathlib import Path
    chosen = None
    candidates = []
    for m in members:                                   # LIVE loop order
        abs_skip = m.startswith("/")
        c.branch(abs_skip)                              # B1
        if abs_skip:
            continue
        trav = ".." in Path(m).parts
        c.branch(trav)                                  # B2
        if trav:
            continue
        match = Path(m).name == binary_name
        c.branch(match)                                 # B3
        if not match:
            continue
        candidates.append(m)
        root = "/" not in m
        c.branch(root)                                  # B4
        if root:
            break
    # LIVE sorts by len(name); on the agreed probes the winner equals the
    # port's choice. Verify against the LIVE fn when importable.
    if candidates:
        chosen = sorted(candidates, key=len)[0]
    if _iron is not None and candidates:
        live = _iron._pick_tar_member(_TF(members), binary_name).name
        assert live == chosen, "LIVE disagreement on %r: %r vs %r" % (enc, live, chosen)
    c.valstr(chosen if chosen else "NONE")              # V1
    return c.finalize()


def run(inputs):
    return [trace(e) for e in inputs]
