"""Oracle for gateway/restart_loop_guard.py port.

Loads the LIVE upstream module and replays the fixture's cases against the same
temporary HERMES_HOME the C harness uses, then prints the identical compact
JSON array for byte-diffing.

argv[1] = fixture path. Fixture:
  {"home": "/tmp/xxx", "now": 1000.0, "cases":[{"op":..., "args":[...]}]}
"""
import sys, os, json
from pathlib import Path

ROOT = Path("/home/wubu/hermes-agent-dev")
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

import importlib.util
spec = importlib.util.spec_from_file_location(
    "restart_loop_guard_oracle", str(ROOT / "gateway" / "restart_loop_guard.py"))
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_restart_loop_guard.py <fixture>\n")
        return 1
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        data = json.load(f)

    home = data.get("home")
    if home:
        os.environ["HERMES_HOME"] = home
        Path(home).mkdir(parents=True, exist_ok=True)
    now = data.get("now", 1000.0)

    out = []
    for case in data.get("cases", []):
        op = case["op"]
        args = case.get("args", [])
        if op == "record":
            ws = args[0] if args else mod.DEFAULT_WINDOW_SECONDS
            out.append(mod.record_restart_interrupted_boot(window_seconds=ws, now=now))
        elif op == "is_tripped":
            mr = args[0] if args else mod.DEFAULT_MAX_RESTARTS
            ws = args[1] if len(args) > 1 else mod.DEFAULT_WINDOW_SECONDS
            out.append(mod.is_restart_loop_tripped(max_restarts=mr, window_seconds=ws, now=now))
        elif op == "check_and_record":
            mr = args[0] if args else mod.DEFAULT_MAX_RESTARTS
            ws = args[1] if len(args) > 1 else mod.DEFAULT_WINDOW_SECONDS
            out.append(mod.check_and_record(max_restarts=mr, window_seconds=ws, now=now))
        elif op == "clear":
            mod.clear()
            out.append("ok")
        elif op == "load":
            out.append(mod._load_boots())
    # Compact JSON, mirroring C output.
    parts = []
    for r in out:
        if isinstance(r, bool):
            parts.append("true" if r else "false")
        elif isinstance(r, list):
            parts.append("[" + ",".join("%.1f" % x for x in r) + "]")
        elif isinstance(r, (int, float)):
            parts.append(str(r))
        else:
            parts.append('"%s"' % r)
    sys.stdout.write("[" + ",".join(parts) + "]")


if __name__ == "__main__":
    sys.exit(main())
