#!/usr/bin/env python3
"""wave_queue.py -- work-conserving dispatch for the wave-porting pipeline.

The old shape was a barrier: dispatch 12 batches, wait for *all* of them, then
dispatch the next wave. Every child that finished early sat idle, and the whole
pipeline waited on its slowest member (the exact complaint reported for
parallel subagents in the wild).

This keeps one queue spanning waves. A finished batch frees a slot and the very
next claim hands out the next ready batch, so authoring never waits for the
tail of a wave.

    python scripts/wave_queue.py seed --wave 25 --size 5
    python scripts/wave_queue.py claim 3          # 3 free slots -> 3 batches
    python scripts/wave_queue.py done 17 --status pass
    python scripts/wave_queue.py status
    python scripts/wave_queue.py idle             # measured wait accounting

State lives in .testbin/wave_queue.json; every claim/done is also appended to
.testbin/wave_queue_events.jsonl so idle time is auditable after the fact
(`idle` reads that log -- a wait is a gap between a batch finishing and the
next claim while the queue still had ready work).
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STATE = os.path.join(REPO, ".testbin", "wave_queue.json")
EVENTS = os.path.join(REPO, ".testbin", "wave_queue_events.jsonl")


def load():
    if not os.path.exists(STATE):
        return {"next_id": 1, "capacity": 12, "batches": [], "waves": []}
    with open(STATE, encoding="utf-8") as fh:
        return json.load(fh)


def save(st):
    os.makedirs(os.path.dirname(STATE), exist_ok=True)
    tmp = STATE + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(st, fh, indent=1, sort_keys=True)
    os.replace(tmp, STATE)


def event(kind, **kw):
    os.makedirs(os.path.dirname(EVENTS), exist_ok=True)
    with open(EVENTS, "a", encoding="utf-8", newline="\n") as fh:
        fh.write(json.dumps(dict(kind=kind, t=time.time(), **kw), sort_keys=True) + "\n")


def assignment_lines(wave):
    """(gate, module) pairs across every docs/.w<wave>-assign-*.txt, in order."""
    pairs, seen = [], set()
    n = 0
    while True:
        path = os.path.join(REPO, "docs", ".w%d-assign-%d.txt" % (wave, n))
        if not os.path.isfile(path):
            if n > 24:
                break
            n += 1
            continue
        for line in open(path, encoding="utf-8"):
            line = line.strip()
            if not line or "\t" not in line:
                continue
            gate, mod = line.split("\t", 1)
            if gate in seen:
                continue
            seen.add(gate)
            pairs.append((gate.strip(), mod.strip()))
        n += 1
    return pairs


def cmd_seed(args):
    st = load()
    pairs = assignment_lines(args.wave)
    if not pairs:
        print("no docs/.w%d-assign-*.txt lines" % args.wave, file=sys.stderr)
        return 2
    if any(b["wave"] == args.wave for b in st["batches"]):
        print("wave %d already seeded" % args.wave)
        return 0
    made = 0
    for i in range(0, len(pairs), args.size):
        chunk = pairs[i:i + args.size]
        st["batches"].append({
            "id": st["next_id"], "wave": args.wave, "status": "ready",
            "gates": [g for g, _ in chunk], "modules": [m for _, m in chunk],
            "claimed_at": None, "done_at": None,
        })
        st["next_id"] += 1
        made += 1
    st["waves"].append(args.wave)
    save(st)
    print("seeded wave %d: %d batches of <=%d (%d gates)" % (args.wave, made, args.size, len(pairs)))
    return 0


def cmd_claim(args):
    st = load()
    ready = [b for b in st["batches"] if b["status"] == "ready"]
    inflight = [b for b in st["batches"] if b["status"] == "inflight"]
    free = max(0, st["capacity"] - len(inflight))
    take = min(args.n, free, len(ready))
    if take < min(args.n, free):
        event("starved", wanted=args.n, free=free, ready=len(ready))
    got = ready[:take]
    now = time.time()
    for b in got:
        b["status"] = "inflight"
        b["claimed_at"] = now
        event("claim", batch=b["id"], wave=b["wave"], gates=b["gates"])
        print("BATCH %d wave %d" % (b["id"], b["wave"]))
        for g, m in zip(b["gates"], b["modules"]):
            print("%s\t%s" % (g, m))
    save(st)
    if not got:
        print("no ready batches (inflight %d/%d)" % (len(inflight), st["capacity"]),
              file=sys.stderr)
    return 0


def cmd_done(args):
    st = load()
    for b in st["batches"]:
        if b["id"] == args.batch:
            b["status"] = "done" if args.status == "pass" else args.status
            b["done_at"] = time.time()
            b["evidence"] = args.note or ""
            event("done", batch=b["id"], status=b["status"], note=args.note or "",
                  seconds=round(b["done_at"] - (b["claimed_at"] or b["done_at"]), 1))
            save(st)
            print("batch %d -> %s (%s)" % (b["id"], b["status"], b["evidence"]))
            return 0
    print("no batch %s" % args.batch, file=sys.stderr)
    return 2


def cmd_status(_args):
    st = load()
    counts = {}
    for b in st["batches"]:
        counts[b["status"]] = counts.get(b["status"], 0) + 1
    now = time.time()
    print("capacity %d  waves %s" % (st["capacity"], st["waves"]))
    print("batches: " + "  ".join("%s=%d" % kv for kv in sorted(counts.items())))
    for b in st["batches"]:
        if b["status"] == "inflight":
            print("  inflight #%d wave %d  %.0fs  %s" % (
                b["id"], b["wave"], now - (b["claimed_at"] or now), " ".join(b["gates"])))
        elif b["status"] != "done":
            print("  %s #%d wave %d  %s" % (b["status"], b["id"], b["wave"], " ".join(b["gates"])))
    if counts.get("ready", 0) == 0 and counts.get("inflight", 0) < st["capacity"]:
        print("  WARN: no ready batch and free slots -- seed the next wave")
    return 0


def cmd_idle(_args):
    """Measured wait accounting from the event log."""
    if not os.path.exists(EVENTS):
        print("no events yet")
        return 0
    ev = [json.loads(l) for l in open(EVENTS, encoding="utf-8") if l.strip()]
    done = [e for e in ev if e["kind"] == "done"]
    claims = [e for e in ev if e["kind"] == "claim"]
    starved = [e for e in ev if e["kind"] == "starved"]
    total = sum(e.get("seconds", 0) for e in done)
    print("batches done: %d  (slot-seconds %.0f)" % (len(done), total))
    if done:
        slow = max(done, key=lambda e: e.get("seconds", 0))
        print("slowest batch #%d wave %s: %.0fs" % (slow["batch"], slow.get("wave", "?"),
                                                    slow.get("seconds", 0)))
    print("claims: %d   starvation events: %d" % (len(claims), len(starved)))
    # tail idle: a done event followed by a claim later than 2s is a slot that
    # had work available and did not take it.
    gaps = 0.0
    for prev, nxt in zip(done, claims):
        if nxt["t"] > prev["t"]:
            g = nxt["t"] - prev["t"]
            if g > 2:
                gaps += g
                print("  idle %.0fs between batch #%d done and next claim" % (g, prev["batch"]))
    print("tail idle (measured): %.0fs" % gaps)
    return 0


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("seed")
    s.add_argument("--wave", type=int, required=True)
    s.add_argument("--size", type=int, default=5)
    s.set_defaults(fn=cmd_seed)
    c = sub.add_parser("claim")
    c.add_argument("n", type=int, nargs="?", default=1)
    c.set_defaults(fn=cmd_claim)
    d = sub.add_parser("done")
    d.add_argument("batch", type=int)
    d.add_argument("--status", default="pass")
    d.add_argument("--note")
    d.set_defaults(fn=cmd_done)
    st = sub.add_parser("status")
    st.set_defaults(fn=cmd_status)
    i = sub.add_parser("idle")
    i.set_defaults(fn=cmd_idle)
    args = ap.parse_args()
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
