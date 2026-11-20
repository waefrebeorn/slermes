#!/usr/bin/env python3
"""Generate tests/fixtures/web_server_flow5.jsonl — scripted-seam FLOW cases for
the web_server_flow5 port (hermes_cli/web_server.py FINAL UNCLAIMED surface:
_dashboard_selftest_loop, the async self-test driver).

Every case: {"func": <harness name>, "in": {... inputs ..., "seams": {name: script}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m} raises that exception class in the live
drive and is delivered as a raise descriptor to the C universal seam. A MISSING
seam key fail-closes (RuntimeError) on BOTH sides.

The live loop is `while True`; each case scripts the asyncio_sleep seam to raise
_LoopBreak on a chosen call index so the loop terminates deterministically.
The raise propagates out of asyncio.run exactly as in live Python (and through
the C seam), so the ret envelope is {"__raise__":"_LoopBreak","msg":...}.

Domains (documented in the port header):
- ASCII strings; app_state is a JSON object whose "auth_required" value is read
  with getattr-default-False semantics (absent/present-null/empty all falsy).
- import_httpx success scripted null (the module object is unused by the body).
- Exception hierarchy: ModuleNotFoundError->ImportError (caught -> None),
  plain ImportError (caught), KeyboardInterrupt/OSError (NOT caught -> ret),
  and _LoopBreak control-flow terminates the loop.
Run: python tests/gen_web_server_flow5_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "web_server_flow5.jsonl")
CASES = []


def add(func, inp, seams):
    inp = dict(inp)
    inp["seams"] = seams
    CASES.append({"func": func, "in": inp})


def rs(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def brk(msg="loop done"):
    return rs("_LoopBreak", msg)


# 1. httpx import fails with the canonical ModuleNotFoundError -> caught by
#    `except ImportError`; silent _log.debug; returns None (no loop entered).
add("dashboard_selftest_loop", {"app_state": {"auth_required": False}}, {
    "import_httpx": rs("ModuleNotFoundError", "No module named 'httpx'"),
})

# 2. plain ImportError is also caught -> None.
add("dashboard_selftest_loop", {"app_state": {"auth_required": False}}, {
    "import_httpx": rs("ImportError", "generic import failure"),
})

# 3. KeyboardInterrupt (BaseException trio) is NOT an ImportError -> propagates.
add("dashboard_selftest_loop", {"app_state": {"auth_required": False}}, {
    "import_httpx": rs("KeyboardInterrupt", "interrupted"),
})

# 4. OSError is a sibling of ImportError under Exception, NOT a subclass ->
#    propagates (hierarchy discrimination).
add("dashboard_selftest_loop", {"app_state": {"auth_required": False}}, {
    "import_httpx": rs("OSError", "io boom"),
})

# 5. import succeeds; auth_required is TRUE -> every iteration `continue`s
#    (dashboard_selftest_once NEVER fires). sleep breaks on call 2.
add("dashboard_selftest_loop", {"app_state": {"auth_required": True}}, {
    "import_httpx": None,
    "asyncio_sleep": [None, brk()],
})

# 6. auth_required FALSE -> each iteration awaits dashboard_selftest_once.
add("dashboard_selftest_loop", {"app_state": {"auth_required": False}}, {
    "import_httpx": None,
    "asyncio_sleep": [None, brk()],
    "dashboard_selftest_once": None,
})

# 7. auth_required ABSENT -> getattr default False -> selftest_once fires.
add("dashboard_selftest_loop", {"app_state": {}}, {
    "import_httpx": None,
    "asyncio_sleep": [None, brk()],
    "dashboard_selftest_once": None,
})

# 8. auth_required is an int 1 (truthy) -> continue; three sleeps before break.
add("dashboard_selftest_loop", {"app_state": {"auth_required": 1}}, {
    "import_httpx": None,
    "asyncio_sleep": [None, None, brk("third tick")],
})

# 9. asyncio.sleep itself raises a generic ValueError on the first call ->
#    propagates out (no selftest_once).
add("dashboard_selftest_loop", {"app_state": {"auth_required": False}}, {
    "import_httpx": None,
    "asyncio_sleep": rs("ValueError", "bad sleep"),
})

# 10. claimed sibling dashboard_selftest_once raises RuntimeError -> propagates.
add("dashboard_selftest_loop", {"app_state": {"auth_required": False}}, {
    "import_httpx": None,
    "asyncio_sleep": [None],
    "dashboard_selftest_once": rs("RuntimeError", "selftest boom"),
})

# 11. three full iterations with auth false -> two selftest_once calls, then break.
add("dashboard_selftest_loop", {"app_state": {"auth_required": False}}, {
    "import_httpx": None,
    "asyncio_sleep": [None, None, brk("third tick")],
    "dashboard_selftest_once": None,
})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(counts):
        print("  %-28s %d" % (k, counts[k]))
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("WARNING <5 cases: %s" % thin)


if __name__ == "__main__":
    main()
