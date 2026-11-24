#!/usr/bin/env python3
"""Oracle for gate w16_climodern_279cc8 (hermes_cli/cli_modern_layout.py)."""
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

live = _load("hermes_cli.cli_modern_layout", "hermes_cli/cli_modern_layout.py")

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
    if func == "layout_from_config":
        py = live.layout_from_config(r.get("display"))
    elif func == "busy_input_mode_from_config":
        py = live.busy_input_mode_from_config(r.get("display"))
    elif func == "fg":
        py = live._fg(r.get("hex_color", ""), bool(r.get("bold", False)))
    elif func == "reasoning_text_line":
        py = live.reasoning_text_line(r.get("line", ""))
    elif func == "constants":
        py = {
            "spin": live._SPIN,
            "rst": live._RST,
            "bold": live._BOLD,
            "dim": live._DIM,
            "red": live._RED,
            "separator_width": live._SEPARATOR_WIDTH,
            "reasoning_preview_lines": live._REASONING_PREVIEW_LINES,
        }
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w16_climodern_279cc8: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
