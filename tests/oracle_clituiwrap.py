#!/usr/bin/env python3
"""Oracle for hermes_cli/cli_tui_wrap.py. ``wrap`` is pinned to textwrap.wrap."""
import importlib.util, json, sys, textwrap
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/hermes_cli/cli_tui_wrap.py"
spec = importlib.util.spec_from_file_location("clituiwrap_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

def wrap(text, width, subsequent_indent=""):
    return textwrap.wrap(text, width=width, subsequent_indent=subsequent_indent)

def canon(o):
    return json.dumps(o, sort_keys=True, ensure_ascii=False, separators=(",", ":"))

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
    if func == "wrap_rows":
        res = live._wrap_rows(wrap, r.get("items", []), r.get("width", 10), r.get("indent", ""))
        py = [[i, w] for i, w in res]
    elif func == "prefix_rows":
        py = live._prefix_wrapped_rows(wrap, r.get("label", ""), r.get("width", 10),
                                       r.get("first_prefix", ""), r.get("indent", ""))
    else:
        continue
    got = r.get("out")
    cases += 1
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH clituiwrap case={r.get('case')} func={func} py={canon(py)} c={canon(got)}", file=sys.stderr)
print(f"ORACLE clituiwrap: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
