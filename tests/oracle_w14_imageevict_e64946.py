#!/usr/bin/env python3
"""Differential oracle for gate w14_imageevict_e64946 (agent/image_eviction_policy.py).

Loads the LIVE stdlib-only module via importlib.spec_from_file_location and
recomputes each case from the live functions. Bytes inputs are base64 in the
fixture; retired-id sets are projected back as sorted arrays.
"""
import base64
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "image_eviction_policy.py").is_file():
    QUARRY = Path("C:/Users/eman5/hermes-agent")
sys.path.insert(0, str(QUARRY))
for _pkg in ("agent", "hermes_cli"):
    if _pkg not in sys.modules:
        _m = types.ModuleType(_pkg)
        _m.__path__ = []
        sys.modules[_pkg] = _m

_SRC = str(QUARRY / "agent" / "image_eviction_policy.py")
_SPEC = importlib.util.spec_from_file_location("w14_imageevict_live", _SRC)
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


def expected(rec):
    f = rec.get("func")
    if f == "image_dimensions_from_bytes":
        dims = mod.image_dimensions_from_bytes(base64.b64decode(rec.get("raw_b64") or ""))
        return list(dims) if dims is not None else None
    if f == "image_max_side_from_data":
        return mod.image_max_side_from_data(rec.get("data"))
    if f == "image_within_many_image_limit":
        return mod.image_within_many_image_limit(rec.get("data"))
    if f == "images_all_within_many_image_limit":
        return mod.images_all_within_many_image_limit(rec.get("datas") or [])
    if f == "outbound_image_block_limit":
        return mod.outbound_image_block_limit(bool(rec.get("many_image_safe")))
    if f == "outbound_image_retire_count":
        return mod.outbound_image_retire_count(
            rec.get("carrier_blocks") or [], rec.get("reserved_blocks") or 0,
            carrier_bytes_newest_first=rec.get("carrier_bytes"),
            reserved_bytes=rec.get("reserved_bytes") or 0,
            many_image_safe=bool(rec.get("many_image_safe")),
            limit=rec.get("limit"),
            budget=rec.get("budget", mod.OUTBOUND_IMAGE_BUDGET_BYTES),
            batch=rec.get("batch", mod.IMAGE_EVICTION_BATCH),
            floor=rec.get("floor", mod.OUTBOUND_IMAGE_FLOOR))
    if f == "latched_retire_count":
        raw = rec.get("retired")
        retired = set(raw) if raw is not None else None
        r = mod.latched_retire_count(int(rec.get("retire") or 0), rec.get("ids") or [], retired)
        return {"retire": r, "retired": (sorted(retired) if retired is not None else None)}
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        want = expected(rec)
        got = rec.get("out")
        if got != want:
            mism += 1
            if mism <= 10:
                print("MISMATCH %s\n  got : %s\n  want: %s" %
                      (rec.get("func"), json.dumps(got, ensure_ascii=False),
                       json.dumps(want, ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w14_imageevict_e64946: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
