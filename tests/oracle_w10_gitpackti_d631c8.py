#!/usr/bin/env python3
"""Differential oracle for gate w10_gitpackti_d631c8 (hermes_cli/git_pack_tidy.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)
def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items(): setattr(m, k, v)
    sys.modules[name] = m
    return m
h = _mod("hermes_cli"); h.__path__ = [os.path.join(Q, "hermes_cli")]
NAME = "w10_gitpackti_d631c8_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "hermes_cli", "git_pack_tidy.py"))
live = importlib.util.module_from_spec(sp); sys.modules[NAME] = live; sp.loader.exec_module(live)

class Idx:
    def __init__(self, vals): self.s = set(vals)
    def __contains__(self, oid): return oid in self.s

def make_index(idx_hex):
    idx = live._Index.__new__(live._Index)
    idx._map = bytes.fromhex(idx_hex)
    idx.count = live.struct.unpack_from(">I", idx._map, live._FANOUT + 255 * 4)[0]
    return idx

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    try:
        if func == "is_redundant":
            others = [Idx(x) for x in r.get("others", [])]
            py = live._is_redundant(iter(r.get("oids", [])), others, float("inf"))
        elif func == "index_contains":
            py = bytes.fromhex(r.get("oid_hex", "")) in make_index(r.get("idx_hex", ""))
        else: continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr); continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w10_gitpackti_d631c8: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
