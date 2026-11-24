#!/usr/bin/env python3
"""Differential oracle for gateway/runtime_downgrade_exit.py (gate w18_runtimedow_f303ab)."""
import ast, asyncio, json, sys, types, __future__
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "gateway" / "runtime_downgrade_exit.py"
_ANN = __future__.annotations.compiler_flag
GATE = "w18_runtimedow_f303ab"
MARKER = "CREATE TABLE IF NOT EXISTS session_admissions"


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "rde_live"}
    from pathlib import Path as PPath
    g.update(dict(asyncio=asyncio, logging=__import__("logging"), os=__import__("os"),
                  logger=__import__("logging").getLogger("oracle"), Path=PPath,
                  Callable=object, Optional=object,
                  GATEWAY_SERVICE_RESTART_EXIT_CODE=75,
                  SCHEMA_SOURCES=("*",), project_root_str=lambda: ".",
                  _PROJECT_ROOT=PPath("."),
                  defines_runtime=lambda sources: any(MARKER in s for s in sources)))
    for node in tree.body:
        if isinstance(node, ast.Assign):
            for t in node.targets:
                if isinstance(t, ast.Name) and t.id in ("POLL_S", "_CONFIRMATIONS"):
                    exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec"), g)
    wanted = {"tree_predates_runtime", "downgrade_reason", "_stop_or_exit",
              "runtime_downgrade_watcher"}
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in wanted:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()


class FakePath:
    def __init__(self, text): self.text = text
    def read_text(self, encoding=None, errors=None): return self.text


class FakeRoot:
    def __init__(self, sources, read_error):
        self.sources, self.read_error = sources, read_error
    def glob(self, pattern):
        if self.read_error: raise OSError("unreadable")
        return [FakePath(s) for s in self.sources]


def run_case(r):
    f = r.get("func")
    if f == "constants":
        return {"POLL_S": G["POLL_S"], "CONFIRMATIONS": G["_CONFIRMATIONS"],
                "SERVICE_RESTART_EXIT_CODE": G["GATEWAY_SERVICE_RESTART_EXIT_CODE"],
                "MARKER": MARKER}
    if f == "downgrade_reason":
        skew = r.get("skew") if r.get("has_skew") else None
        return G["downgrade_reason"](skew, update_in_progress=bool(r.get("update_in_progress")),
                                     predates=lambda: bool(r.get("predates")))
    if f == "tree_predates":
        return G["tree_predates_runtime"](FakeRoot(r.get("sources") or [], bool(r.get("read_error"))))
    if f == "watcher":
        running = bool(r.get("running"))
        max_polls = r.get("max_polls", -1)
        if max_polls == -1: max_polls = None
        polls = r.get("polls") or []
        idx = {"n": 0}
        runner = types.SimpleNamespace(_running=running)
        def skew_fn(): return (polls[idx["n"]].get("skew") or []) if idx["n"] < len(polls) else []
        def update_probe():
            p = polls[idx["n"]]
            if p.get("update_error"): raise RuntimeError("lock")
            return bool(p.get("update_busy"))
        def predates(): return bool(polls[idx["n"]].get("predates"))
        async def stop_or_exit(runner): pass
        class Loop:
            def create_task(self, coro):
                coro.close(); return "stop-task"
        async def to_thread(fn):
            v = fn(); idx["n"] += 1
            # The fixture's poll script is finite: running out of observations is
            # how this seam models the runner stopping, so the loop terminates.
            if idx["n"] >= len(polls): runner._running = False
            return v
        async def sleep(s): pass
        saved = (G.get("asyncio"), G.get("_stop_or_exit"))
        G["asyncio"] = types.SimpleNamespace(to_thread=to_thread, sleep=sleep,
                                             get_running_loop=lambda: Loop())
        G["_stop_or_exit"] = stop_or_exit
        try:
            asyncio.run(G["runtime_downgrade_watcher"](
                runner, poll_s=0, skew_fn=skew_fn, update_probe=update_probe,
                predates=predates, max_polls=max_polls))
        finally:
            G["asyncio"], G["_stop_or_exit"] = saved
        return {"polls": idx["n"],
                "reason": getattr(runner, "_exit_reason", None),
                "exit_code": getattr(runner, "_exit_code", None),
                "stop_scheduled": getattr(runner, "_runtime_downgrade_stop", None) is not None}
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        try: r = json.loads(line)
        except ValueError: continue
        f = r.get("func")
        if f not in {"constants", "downgrade_reason", "tree_predates", "watcher"}: continue
        cases += 1
        try: py = run_case(r)
        except BaseException as e:
            mism += 1; print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr); continue
        got = r.get("out")
        if py != got:
            mism += 1; print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__": main()
