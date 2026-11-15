#!/usr/bin/env python3
"""Oracle for the newly added slice of hermes_cli/gateway_windows.py."""
import json
import os
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

from hermes_cli import gateway_windows as gw   # noqa: E402


def main() -> int:
    total = mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fn = c.get("fn")
        i = c.get("in") or {}
        try:
            if fn == "quote_cmd":
                try:
                    want = gw._quote_cmd_script_arg(i["value"])
                except ValueError:
                    want = None
            elif fn == "stdin_interactive":
                cm = i.get("console_mode_ok")
                want = gw._stdin_is_interactive(isatty=bool(i.get("isatty")),
                                                console_mode_ok=cm)
            elif fn == "prepend_pythonpath":
                overlay = {}
                # _prepend_pythonpath reads os.environ, not a parameter.
                os.environ["PYTHONPATH"] = i.get("existing") or ""
                gw._prepend_pythonpath(overlay, i["entries"])
                want = overlay.get("PYTHONPATH")
            else:
                continue
        except Exception as exc:                       # noqa: BLE001
            print(f"MISMATCH {c.get('case')} ({fn}): python raised {exc!r}")
            mism += 1
            continue
        raw = c["out"]
        if isinstance(raw, dict) and set(raw) == {"str"}:
            raw = raw["str"]
        if raw != want:
            mism += 1
            print(f"MISMATCH {c.get('case')} ({fn}): C={str(c['out'])[:240]} "
                  f"PY={want!r}"[:400])
    if total == 0:
        print("ORACLE gateway_windows: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE gateway_windows: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
