#!/usr/bin/env python3
"""Differential oracle for gateway/hosted_rooms_common.py (gate w14_hostedroom_818b6c).

The LIVE leaf validators run with the exact error class/message templates from
each case; their success value / raised message is compared to the C harness.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
SRC = "C:/Users/eman5/hermes-agent/gateway/hosted_rooms_common.py"
spec = importlib.util.spec_from_file_location("w14_hrcmn_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)


def call(fn):
    try:
        val = fn()
    except Exception as exc:
        return {"ok": False, "value": None, "error": str(exc)}
    return {"ok": True, "value": val, "error": None}


def want_for(r):
    func = r.get("func")
    if func == "identifier":
        return call(lambda: live.identifier(
            r.get("value"), label=r.get("label", "id"), error=ValueError,
            max_chars=r.get("max_chars", 128),
            pattern=(None if r.get("skip_pattern", False) else live.IDENTIFIER_RE),
            invalid=r.get("invalid")))
    if func == "bounded_int":
        return call(lambda: live.bounded_int(
            r.get("value"), error=ValueError, message=r.get("message", ""),
            low=r.get("low", 0),
            high=(r.get("high") if "high" in r else None)))
    if func == "exact_fields":
        return call(lambda: live.exact_fields(
            r.get("value"), label=r.get("label", "obj"),
            required=frozenset(r.get("required") or ()),
            optional=frozenset(r.get("optional") or ()),
            error=ValueError, not_object=r.get("not_object"),
            missing_fmt=r.get("missing_fmt", "{label} is missing fields: {fields}"),
            unknown_fmt=r.get("unknown_fmt", "{label} has unknown fields: {fields}")))
    if func == "text":
        return call(lambda: live.text(
            r.get("value"), error=ValueError, label=r.get("label", "note"),
            max_bytes=r.get("max_bytes", 32), strip=r.get("strip", True)))
    if func == "utf8_len":
        return call(lambda: live.utf8_len(*(r.get("parts") or [])))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    want = want_for(r)
    if want is None:
        continue
    got = r.get("out")
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w14_hostedroom_818b6c: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
