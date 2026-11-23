#!/usr/bin/env python3
"""Differential oracle for w11_init_b19b81.

hermes_platform/__init__.py is documentation-only; the pure machine-fact
decoders it describes live in hermes_platform/host/facts.py, which is the
live source of truth here. Its one import (hermes_platform.host.darwin_pci)
is stubbed so the module loads on any OS.
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


pkg = _mod("hermes_platform")
pkg.__path__ = []
host = _mod("hermes_platform.host")
host.__path__ = []
_mod("hermes_platform.host.darwin_pci", display_vendor_ids=lambda: [])
host.darwin_pci = sys.modules["hermes_platform.host.darwin_pci"]

SRC = Q + "/hermes_platform/host/facts.py"
spec = importlib.util.spec_from_file_location("w11_init_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_init_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "normalize_arch":
            py = live.normalize_arch(r.get("raw", ""))
        elif func == "windows_native_arch":
            w = r.get("wow64_native")
            py = live.windows_native_arch(wow64_native=(None if w in (None, "", "none") else int(w, 10)),
                                          machine=r.get("machine", ""))
        elif func == "classify_gpu_vendors":
            py = live.classify_gpu_vendors(r.get("vendor_ids", []))
        elif func == "parse_cpuinfo":
            py = live.parse_cpuinfo(r.get("text", ""), device_tree_model=r.get("device_tree_model", ""))
        elif func == "parse_cpuinfo_vendor":
            py = live.parse_cpuinfo_vendor(r.get("text", ""))
        elif func == "parse_meminfo_total":
            py = live.parse_meminfo_total(r.get("text", ""))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w11_init_b19b81: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
