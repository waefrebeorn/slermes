#!/usr/bin/env python3
"""Differential oracle for pm/termux_libs.py (gate w5_termuxlibs_3b3b3b).

Only the pure URL/index parsers are exercised; the network shells index()/repair()
are deliberately out of scope. pm.store / pm.update are stubbed so the live
module loads without pulling the package-manager stack.
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


pm = _mod("pm")
pm.__path__ = []
_mod("pm.store", hash_url=lambda url: "0" * 64)
_mod("pm.update", _get_text=lambda url: "")

SRC = Q + "/pm/termux_libs.py"
spec = importlib.util.spec_from_file_location("termuxlibs_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["termuxlibs_live"] = live
spec.loader.exec_module(live)


def rowdict(p):
    return {"name": p.name, "version": p.version, "url": p.url, "sha256": p.sha256}


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
        if func == "package_of":
            py = live.package_of(r.get("url", ""))
        elif func == "filename":
            py = live._filename(r.get("url", ""))
        elif func == "parse_index":
            py = {k: rowdict(v) for k, v in live.parse_index(r.get("text", "")).items()}
        elif func == "constants":
            py = {
                "POOL": live.POOL, "POOL_HOST": live.POOL_HOST,
                "INDEX_URL": live.INDEX_URL,
                "POOL_PATH_PREFIX": live.POOL_PATH_PREFIX,
                "TABLE_NAME": live.TABLE_NAME,
            }
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

print(f"ORACLE w5_termuxlibs_3b3b3b: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
