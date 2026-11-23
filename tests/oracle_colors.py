#!/usr/bin/env python3
"""Oracle for upstream Hermes hermes_cli/colors.py."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/hermes_cli/colors.py"
spec = importlib.util.spec_from_file_location("colors_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)

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
    if func not in ("should_use_color", "color"):
        continue

    nc = r.get("no_color")
    if nc is None:
        os.environ.pop("NO_COLOR", None)
    else:
        os.environ["NO_COLOR"] = nc
    term = r.get("term")
    if term is None:
        os.environ.pop("TERM", None)
    else:
        os.environ["TERM"] = term
    # sys.stdout.isatty() is driven by the fixture (piped harness stdout).
    live.sys = types.SimpleNamespace(
        stdout=types.SimpleNamespace(isatty=lambda: bool(r.get("tty", False))))

    if func == "should_use_color":
        py = live.should_use_color()
    else:
        py = live.color(r.get("text", ""), *r.get("codes", []))
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE colors: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
