#!/usr/bin/env python3
"""Differential oracle for hermes_cli/termination_guard.py (w16_termination_ef34ac).

The LIVE context manager is driven with an injected signal module and an
identity-correct thread sentinel; the handlers installed at the `yield` are the
expected `installed` list.  TERMINATION_SIGNALS is re-evaluated from the live
source AST under an injected signal namespace so platform availability is a
fixture input rather than a host assumption.
"""
import ast
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/hermes_cli/termination_guard.py"
spec = importlib.util.spec_from_file_location("w16_termination_ef34ac_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w16_termination_ef34ac_live"] = live
spec.loader.exec_module(live)

_TREE = ast.parse(open(SRC, encoding="utf-8").read(), SRC)
def _is_term_node(n):
    if isinstance(n, ast.Assign):
        return any(isinstance(t, ast.Name) and t.id == "TERMINATION_SIGNALS"
                   for t in n.targets)
    return (isinstance(n, ast.AnnAssign) and isinstance(n.target, ast.Name)
            and n.target.id == "TERMINATION_SIGNALS" and n.value is not None)


_TERM_NODE = next(n for n in ast.walk(_TREE) if _is_term_node(n))


class FakeSignal:
    SIG_DFL = "DFL"
    SIG_IGN = "IGN"

    def __init__(self, previous):
        self.previous = previous
        self.calls = []

    def getsignal(self, signum):
        return self.previous.get(str(signum), self.SIG_DFL)

    def signal(self, signum, handler):
        self.calls.append(int(signum))


class FakeThreading:
    def __init__(self, is_main):
        self.cur = object()
        self.main_obj = self.cur if is_main else object()

    def current_thread(self):
        return self.cur

    def main_thread(self):
        return self.main_obj


def compute(r):
    func = r.get("func")
    if func == "exit_status":
        try:
            live._exit_on_signal(int(r.get("signum", 0)), None)
        except SystemExit as exc:
            return exc.code
        return None
    if func == "signals":
        available = set(int(x) for x in r.get("available", []))
        fake = types.ModuleType("injected_signal")
        if 15 in available:
            fake.SIGTERM = 15
        if 1 in available:
            fake.SIGHUP = 1
        ns = {"signal": fake, "getattr": getattr}
        exec(compile(ast.Module(body=[_TERM_NODE], type_ignores=[]), SRC, "exec"), ns)
        return [int(x) for x in ns["TERMINATION_SIGNALS"]]
    if func == "guard":
        sigs = [int(x) for x in r.get("signals", [])]
        kinds = r.get("prev_kinds", [])
        live.TERMINATION_SIGNALS = tuple(sigs)
        fake_sig = FakeSignal({str(s): kinds[i] for i, s in enumerate(sigs)
                               if i < len(kinds)})
        old_signal, old_threading = live.signal, live.threading
        live.signal = fake_sig
        live.threading = FakeThreading(bool(r.get("is_main", False)))
        try:
            snapshot = []
            with live.unwind_on_termination():
                snapshot = list(fake_sig.calls)
        finally:
            live.signal, live.threading = old_signal, old_threading
        return {"installed": snapshot}
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w16_termination_ef34ac: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
