#!/usr/bin/env python3
"""Differential oracle for hermes_platform/host/gpu_adapters.py (w20_gpuadapter_3b3bcc).

The live module is imported as-is (it only touches ctypes at call time on
Windows), so the compared values come from the live ``Adapter`` dataclass --
``memory_bytes`` and the DXGI desc fields -- and the live constants
``_SOFTWARE_FLAG`` / ``_MAX_ADAPTERS`` / the vtable slot numbers.  The DXGI
enumeration walk is replayed over fixture-provided desc results, bounded by the
live ``_MAX_ADAPTERS``, because the real walk needs a live COM factory.
"""
import importlib.util
import json
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

sp = importlib.util.spec_from_file_location(
    "w20_gpuadapter_3b3bcc_live", os.path.join(Q, "hermes_platform", "host", "gpu_adapters.py"))
live = importlib.util.module_from_spec(sp)
sys.modules["w20_gpuadapter_3b3bcc_live"] = live
sp.loader.exec_module(live)


def _int(v):
    if isinstance(v, bool):
        return int(v)
    if isinstance(v, (int, float)):
        return int(v)
    if isinstance(v, str):
        return int(v.strip() or 0)
    return 0


def adapter_out(desc):
    if not isinstance(desc, dict):
        desc = {}
    description = desc.get("description", "")
    if not isinstance(description, str):
        description = ""
    flags = _int(desc.get("flags", 0))
    a = live.Adapter(
        description,
        _int(desc.get("vendor_id", 0)),
        _int(desc.get("dedicated_bytes", 0)),
        _int(desc.get("shared_bytes", 0)),
        bool(flags & live._SOFTWARE_FLAG),
    )
    return {
        "description": a.description,
        "vendor_id": str(a.vendor_id),
        "dedicated_bytes": str(a.dedicated_bytes),
        "shared_bytes": str(a.shared_bytes),
        "software": a.software,
        "memory_bytes": str(a.memory_bytes),
    }


def enumerate_out(items):
    out = []
    if not isinstance(items, list):
        return out
    for item in items[: live._MAX_ADAPTERS]:
        if not isinstance(item, dict):
            continue
        if _int(item.get("rc", 0)) != 0:
            break
        if item.get("got_desc") is False:
            continue
        out.append(adapter_out(item))
    return out


def expected(r):
    f = r.get("func")
    if f == "constants":
        return {
            "software_flag": live._SOFTWARE_FLAG,
            "max_adapters": live._MAX_ADAPTERS,
            "slots": {"release": live._RELEASE,
                      "enum_adapters1": live._ENUM_ADAPTERS1,
                      "get_desc1": live._GET_DESC1},
        }
    if f == "adapter":
        return adapter_out(r.get("desc"))
    if f == "enumerate":
        return enumerate_out(r.get("items"))
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
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_gpuadapter_3b3bcc: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
