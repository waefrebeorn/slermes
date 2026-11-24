#!/usr/bin/env python3
"""Differential oracle for hermes_cli/dashboard_auth/listener_client.py
(gate w24_listenercl_afdef6).

The live module is driven through its real entry points -- ``offer``, ``use`` and
the undo ``use`` returns -- with the provider swap replaced by a counting
callable (the seam: no plugin, no provider, no portal is touched).  Each case
starts from a clean module state (``_current = _swap = None``), exactly the state
a fresh process has, so the compared sequence is the handshake itself:

  * a use before any offer changes nothing and returns None;
  * use after offer sets (client_id, portal_url) and runs the swap once;
  * the undo clears the tuple and runs the restore once (a second undo is a
    no-op);
  * a swap that raises leaves nothing current and propagates.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_listenercl_afdef6"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli.dashboard_auth import listener_client as live  # noqa: E402

WANTED = {"session"}


def session(req):
    live._current = None
    live._swap = None

    calls = {"swap": 0, "restore": 0}
    raises = bool(req.get("swap_raises"))

    def restore():
        calls["restore"] += 1

    def swap():
        calls["swap"] += 1
        if raises:
            raise RuntimeError("provider swap unavailable")
        return restore

    if req.get("offered"):
        live.offer(swap)

    def current():
        cur = live.current()
        return None if cur is None else list(cur)

    undos = []
    results = []
    ops = req.get("ops")
    for op in ops if isinstance(ops, list) else []:
        name = op.get("op") if isinstance(op, dict) else None
        entry = {"op": name, "raised": False, "returned_null": False,
                 "undo_id": None, "ran": False}
        if name == "offer":
            live.offer(swap)
        elif name == "use":
            client_id = op.get("client_id")
            portal_url = op.get("portal_url")
            try:
                undo = live.use(client_id, portal_url)
            except BaseException:  # noqa: BLE001 - the tested error seam
                entry["raised"] = True
                entry["returned_null"] = True
                entry["current"] = current()
                results.append(entry)
                continue
            if undo is None:
                entry["returned_null"] = True
            else:
                undos.append(undo)
                entry["undo_id"] = len(undos) - 1
        elif name == "undo":
            idx = op.get("id")
            if isinstance(idx, int) and not isinstance(idx, bool) and 0 <= idx < len(undos) \
                    and undos[idx] is not None:
                undos[idx]()
                undos[idx] = None
                entry["ran"] = True
        entry["current"] = current()
        results.append(entry)

    return {"current": current(), "results": results,
            "swap_calls": calls["swap"], "restore_calls": calls["restore"]}


def expected(r):
    f = r.get("func")
    if f == "session":
        return session(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
