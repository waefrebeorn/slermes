#!/usr/bin/env python3
"""Differential oracle for upstream Hermes pm/worker.py (gate w5_worker_9c3898).

_read_controls is driven synchronously with a patched os.read/os.close over an
in-memory byte stream built from the fixture's decoded messages. The resulting
queue contents, pause state and error are compared to the C framing loop.
"""
import importlib.util
import json
import queue
import sys
import threading

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/pm/worker.py"
spec = importlib.util.spec_from_file_location("w5_worker_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w5_worker_live"] = live
spec.loader.exec_module(live)

def live_controls(messages):
    q = queue.Queue()
    pause = threading.Event()
    raw = b"".join(json.dumps(m, separators=(",", ":")).encode("utf-8") + b"\n"
                   for m in messages)
    state = {"buf": raw, "pos": 0}
    orig_read, orig_close = live.os.read, live.os.close
    def fake_read(fd, n):
        chunk = state["buf"][state["pos"]:state["pos"] + n]
        state["pos"] += len(chunk)
        return chunk
    live.os.read = fake_read
    live.os.close = lambda fd: None
    try:
        live._read_controls(q, pause, 0)
    finally:
        live.os.read = orig_read
        live.os.close = orig_close
    out, err = [], None
    while True:
        item = q.get_nowait()
        if item is None:
            break
        if isinstance(item, Exception):
            err = f"{type(item).__name__}: {item}"
        else:
            out.append(item)
    return {"messages": out, "paused": pause.is_set(), "error": err}

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
    args = {k: v for k, v in r.items() if k not in ("func", "out")}
    try:
        if func == "controls":
            py = live_controls(args.get("messages") or [])
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR func={func} args={args}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH func={func} args={args}\n  py={py!r}\n   c={got!r}", file=sys.stderr)

print(f"ORACLE w5_worker_9c3898: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
