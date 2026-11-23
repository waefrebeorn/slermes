#!/usr/bin/env python3
"""Differential oracle for tools/terminal_tool_sudo.py (gate w11_terminalto_3baf4c)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

class _Any:
    def __init__(self, n="Any"): self._n = n
    def __call__(self, *a, **k): return _Any(self._n + "()")
    def __getattr__(self, n): return _Any(self._n + "." + n)
    def __iter__(self): return iter(())
    def __len__(self): return 0
    def __bool__(self): return True

for nm in ("hermes_yaml", "ruamel"):
    mod = types.ModuleType(nm)
    mod.__path__ = []
    mod.__getattr__ = lambda n: _Any(nm + "." + n)
    sys.modules[nm] = mod

tools = types.ModuleType("tools")
tools.__path__ = []
sys.modules["tools"] = tools
askpass = types.ModuleType("tools.terminal_tool_sudo_askpass")
askpass.SUDO_ASKPASS_PREFIX = 'SUDO_ASKPASS="$__hermes_sudo_dir/a" '
askpass.SUDO_ASKPASS_SUFFIX = ' "${__hermes_sudo_o[@]}"'
sys.modules["tools.terminal_tool_sudo_askpass"] = askpass
utils = types.ModuleType("utils")
utils.env_var_enabled = lambda n: False
sys.modules["utils"] = utils

spec = importlib.util.spec_from_file_location("w11_terminalto_live", QUARRY + "/tools/terminal_tool_sudo.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w11_terminalto_live"] = live
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
    got = r.get("out")
    try:
        if func == "env":
            want = live._looks_like_env_assignment(r.get("token", ""))
        elif func == "token":
            want = list(live._read_shell_token(r.get("command", ""), int(r.get("start", 0))))
        elif func == "scan":
            want = [list(e) for e in live._scan_shell(r.get("command", ""),
                                                       bool(r.get("background", False)))]
        elif func == "rewrite":
            want = list(live._rewrite_real_sudo_invocations(r.get("command", ""),
                                                            bool(r.get("askpass", False))))
        elif func == "count":
            want = live._count_real_sudo_invocations(r.get("command", ""))
        elif func == "bg":
            want = live._rewrite_compound_background(r.get("command", ""))
        elif func == "wrongpw":
            want = live._sudo_wrong_password_failure(r.get("output", ""))
        else:
            continue
    except BaseException as ex:
        print("MISMATCH %s oracle raised %s: %s" % (func, type(ex).__name__, ex), file=sys.stderr)
        cases += 1
        mism += 1
        continue
    cases += 1
    if want != got:
        mism += 1
        print("MISMATCH in=%s py=%r c=%r" % (json.dumps(r, ensure_ascii=False), want, got), file=sys.stderr)
print("ORACLE w11_terminalto_3baf4c: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or cases == 0 else 0)
