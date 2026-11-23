#!/usr/bin/env python3
"""Oracle for upstream Hermes tools/file_operations_common.py (fileopscommon gate)."""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/tools/file_operations_common.py"
spec = importlib.util.spec_from_file_location("fileopscommon_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["fileopscommon_live"] = live
spec.loader.exec_module(live)

# get_max_lines() is a config read: inject it as a seam from the fixture.
CUR_MAX = [2000]
try:
    import tools
    tol = types.ModuleType("tools.tool_output_limits")
    tol.get_max_lines = lambda: CUR_MAX[0]
    sys.modules["tools.tool_output_limits"] = tol
    tools.tool_output_limits = tol
except Exception:
    pass


def coerce(v, d):
    try:
        return {"ok": True, "value": int(v)}
    except (TypeError, ValueError):
        return {"ok": False, "value": d}


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    if func == "conflict":
        py = live.count_conflict_blocks(r.get("text", ""))
    elif func == "fence":
        py = live._strip_terminal_fence_leaks(r.get("text", ""))
    elif func == "fname":
        py = live.filename_match_key(r.get("name", ""))
    elif func == "eol":
        py = live._detect_line_ending(r.get("sample", ""))
    elif func == "normeol":
        py = live._normalize_line_endings(r.get("text", ""), r.get("target", ""))
    elif func == "bom":
        t, b = live._strip_bom(r.get("text", ""))
        py = {"text": t, "had_bom": b}
    elif func == "hasbom":
        py = live._has_bom(r.get("text", ""))
    elif func == "coerce":
        py = coerce(r.get("value", ""), r.get("default", 1))
    elif func == "readpage":
        CUR_MAX[0] = r.get("max_lines", 2000)
        a, b = live.normalize_read_pagination(r.get("offset", ""), r.get("limit", ""))
        py = {"offset": a, "limit": b}
    elif func == "searchpage":
        a, b = live.normalize_search_pagination(r.get("offset", ""), r.get("limit", ""))
        py = {"offset": a, "limit": b}
    elif func == "lint":
        py = live.LintResult(success=r.get("success", True), skipped=r.get("skipped", False),
                             output=r.get("output", ""), message=r.get("message", "")).to_dict()
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE fileopscommon: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
