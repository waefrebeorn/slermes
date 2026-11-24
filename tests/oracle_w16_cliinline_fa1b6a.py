#!/usr/bin/env python3
"""Oracle for gate w16_cliinline_fa1b6a (hermes_cli/cli_inline_images.py)."""
import importlib.util
import json
import os
import sys
import types
import contextlib
import io

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)
os.environ.pop("TMUX", None)  # pin passthrough to identity

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

live = _load("hermes_cli.cli_inline_images", "hermes_cli/cli_inline_images.py")
# cli_terminal_mixin imports `rich` (absent in the gate interpreter); TMUX unset
# makes the live passthrough the identity, so patch it directly.
live._passthrough = lambda seq: seq

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
    if func == "zero_width":
        py = live._zero_width(r.get("seq", ""))
    elif func == "image_rows":
        img = types.SimpleNamespace(**(r.get("image") or {}))
        py = [list(pair) for pair in live.image_rows(img)]
    elif func == "constants":
        py = {
            "indent": live._INDENT,
            "max_cols": live._MAX_COLS,
            "max_rows": live._MAX_ROWS,
            "vscode_hint_odds": live._VSCODE_HINT_ODDS,
        }
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w16_cliinline_fa1b6a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
