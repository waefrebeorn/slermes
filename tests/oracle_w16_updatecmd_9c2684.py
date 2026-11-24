#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_cmd_config.py (w16_updatecmd_9c2684).

The two helpers are side-effecting in the live module. Builtins.print is captured
so the exact printed lines can be compared, and the interactive sources
(_gateway_prompt, input(), isatty) are injected from the fixture.
"""
import builtins
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/hermes_cli/update_cmd_config.py"
spec = importlib.util.spec_from_file_location("w16_updatecmd_9c2684_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


class Stream:
    def __init__(self, tty):
        self.tty = tty

    def isatty(self):
        return self.tty

    def reconfigure(self, *a, **k):
        pass


def ask_case(r):
    fake_uc = types.ModuleType("hermes_cli.update_cmd")
    fake_uc._gateway_prompt = lambda *a, **k: r.get("gateway_reply", "")
    old_mod = sys.modules.get("hermes_cli.update_cmd")
    sys.modules["hermes_cli.update_cmd"] = fake_uc

    def fake_input(prompt=""):
        v = r.get("input_reply")
        if v is None:
            raise EOFError()
        if v == "__UDE__":
            raise UnicodeDecodeError("utf-8", b"x", 0, 1, "bad")
        return v

    old_input, old_print = builtins.input, builtins.print
    old_in, old_out = sys.stdin, sys.stdout
    builtins.input = fake_input
    builtins.print = lambda *a, **k: None
    interactive = bool(r.get("is_interactive", False))
    sys.stdin, sys.stdout = Stream(interactive), Stream(interactive)
    try:
        return live._ask_configure_new_options(
            assume_yes=bool(r.get("assume_yes", False)),
            gateway_mode=bool(r.get("gateway_mode", False)))
    finally:
        builtins.input, builtins.print = old_input, old_print
        sys.stdin, sys.stdout = old_in, old_out
        if old_mod is not None:
            sys.modules["hermes_cli.update_cmd"] = old_mod
        else:
            sys.modules.pop("hermes_cli.update_cmd", None)


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
    if func not in ("print_items", "ask"):
        continue
    cases += 1
    try:
        if func == "print_items":
            lines = []
            old_print = builtins.print
            builtins.print = lambda *a, **k: lines.append(" ".join(str(x) for x in a))
            try:
                live._print_items(r.get("items", []), r.get("label", ""),
                                  r.get("key", ""), r.get("fallback_key"))
            finally:
                builtins.print = old_print
            py = lines
        else:
            py = ask_case(r)
    except Exception as exc:
        py = {"error": repr(exc)}
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print("ORACLE w16_updatecmd_9c2684: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
