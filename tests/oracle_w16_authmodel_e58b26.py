#!/usr/bin/env python3
"""Oracle for gate w16_authmodel_e58b26 (hermes_cli/auth_model_picker.py)."""
import importlib.util
import json
import sys
import types
import contextlib
import io

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _load(dotted, rel):
    parts = dotted.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pkg = types.ModuleType(pn)
            pkg.__path__ = [QUARRY + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pkg
    spec = importlib.util.spec_from_file_location(dotted, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[dotted] = m
    with contextlib.redirect_stderr(io.StringIO()):
        spec.loader.exec_module(m)
    return m

live = _load("hermes_cli.auth_model_picker", "hermes_cli/auth_model_picker.py")

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
    if func == "picker_order":
        ordered, sections, start = live._picker_order(
            list(r.get("model_ids") or []), r.get("current_model") or "", None)
        py = [ordered, sections, start]
    elif func == "constants":
        py = {
            "custom_label": live._CUSTOM_LABEL,
            "skip_label": live._SKIP_LABEL,
            "current_suffix": live._CURRENT_SUFFIX,
            "cost_col": live._COST_COL,
            "gauge_gap": live._GAUGE_GAP,
            "level_style": dict(live._LEVEL_STYLE),
        }
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w16_authmodel_e58b26: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
