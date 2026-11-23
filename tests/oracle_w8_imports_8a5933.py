#!/usr/bin/env python3
"""Differential oracle for upstream Hermes session_rules/imports.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, Q)
NAME = "w8_imports_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/session_rules/imports.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

MARK = {"NaN": float("nan"), "Infinity": float("inf"), "-Infinity": float("-inf")}
def decode(obj):
    if isinstance(obj, str):
        return MARK.get(obj, obj)
    if isinstance(obj, list):
        return [decode(x) for x in obj]
    if isinstance(obj, dict):
        return {k: decode(v) for k, v in obj.items()}
    return obj

def compute(r):
    f = r.get("func")
    if f == "text_or_none":
        field = r.get("field", "f")
        try:
            return {"ok": True, "value": live._import_text_or_none(r.get("value"), field)}
        except ValueError as e:
            return {"ok": False, "error": str(e)}
    if f == "int_or_none":
        field = r.get("field", "f")
        try:
            v = live._import_int_or_none(r.get("value"), field)
            return {"ok": True, "value": None if v is None else str(v)}
        except ValueError as e:
            return {"ok": False, "error": str(e)}
    if f == "non_finite_path":
        return live._import_non_finite_path(decode(r.get("data")), r.get("path", "root"))
    if f == "bounds":
        return {"min": str(live.IMPORT_INT_MIN), "max": str(live.IMPORT_INT_MAX)}
    if f == "field_tables":
        return {
            "session_text": list(live.IMPORT_SESSION_TEXT_FIELDS),
            "message_text": list(live.IMPORT_MESSAGE_TEXT_FIELDS),
            "message_json": list(live.IMPORT_MESSAGE_JSON_FIELDS),
            "int": list(live.IMPORT_INT_FIELDS),
            "float": list(live.IMPORT_FLOAT_FIELDS),
        }
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
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE imports: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)