#!/usr/bin/env python3
"""Oracle for upstream Hermes hermes_cli/config_key_notices.py (gate keynotice)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# color() is a no-op when color output is unavailable (NO_COLOR / piped stdout);
# stub hermes_cli.colors so the oracle works from any cwd and compares plain text.
os.environ["NO_COLOR"] = "1"
if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli"); _pkg.__path__ = []; sys.modules["hermes_cli"] = _pkg
_colors = types.ModuleType("hermes_cli.colors")
class Colors:
    RED = YELLOW = DIM = ""
_colors.Colors = Colors
_colors.color = lambda text, *codes: text
sys.modules["hermes_cli.colors"] = _colors
SRC = "C:/Users/eman5/hermes-agent/hermes_cli/config_key_notices.py"
spec = importlib.util.spec_from_file_location("keynotice_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    if r.get("func") != "unknown_subkey_refusal": continue
    py = live.unknown_subkey_refusal(r.get("key", ""), r.get("suggestion"))
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE keynotice: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
